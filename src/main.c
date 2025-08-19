#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h> // Biblioteca para manipulação de arquivos
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debugScreen.h"
#include "tpb_scraper.h"
#include "realdebrid.h"
#include "player.h"
#include "osk.h"

#define MAX_RESULTS 50
#define ITEMS_PER_PAGE 8

typedef enum {
    STATE_MAIN_MENU,
    STATE_INPUT,
    STATE_SHOW_RESULTS,
    STATE_PROCESSING
} AppState;

static char search_query[OSK_INPUT_MAX_LENGTH] = "matrix";

void show_status_screen(const char* title, const char* message) {
    psvDebugScreenInit();
    psvDebugScreenPrintf("--- %s ---\n\n", title);
    psvDebugScreenPrintf("%s\n\n", message);
    psvDebugScreenPrintf("Por favor, aguarde...");
}

void draw_main_menu(int selection) {
    psvDebugScreenInit();
    psvDebugScreenPrintf("VitaTPBPlayer v0.1.0-alpha\n");
    psvDebugScreenPrintf("----------------------------------\n\n");
    psvDebugScreenPrintf(" %s Buscar Torrents\n", (selection == 0) ? ">>" : "  ");
    psvDebugScreenPrintf(" %s Sair\n", (selection == 1) ? ">>" : "  ");
    psvDebugScreenPrintf("\n\n----------------------------------\n");
    psvDebugScreenPrintf("Use CIMA/BAIXO e (X) para selecionar.\n");
}

void draw_results_menu(TpbResult* results, int num_results, int selection, int current_page) {
    psvDebugScreenInit();
    psvDebugScreenPrintf("Resultados para: '%s' (Pagina %d/%d)\n", search_query, current_page + 1, (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    psvDebugScreenPrintf("--------------------------------------------------------------------------------\n");
    int start_index = current_page * ITEMS_PER_PAGE;
    int end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > num_results) end_index = num_results;
    for (int i = start_index; i < end_index; i++) {
        if (i == selection) psvDebugScreenPrintf(">>");
        else psvDebugScreenPrintf("  ");
        char truncated_name[50];
        strncpy(truncated_name, results[i].name, sizeof(truncated_name) - 1);
        truncated_name[sizeof(truncated_name) - 1] = '\0';
        psvDebugScreenPrintf(" %-50s [%-9s] (S:%d L:%d)\n", truncated_name, results[i].size, results[i].seeders, results[i].leechers);
    }
    psvDebugScreenPrintf("\n--------------------------------------------------------------------------------\n");
    psvDebugScreenPrintf("(X) Baixar com RD | (L/R) Pagina | (TRIANGULO) Nova Busca | (START) Sair\n");
}

int main(int argc, char *argv[]) {
    int init_error = psvDebugScreenInit();
    if (init_error < 0) {
        // Se a inicialização da tela falhar, vamos criar um log com o código do erro.
        SceUID fd = sceIoOpen("ux0:data/vitatpb_log.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (fd >= 0) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "psvDebugScreenInit failed with error code: 0x%08X\n", init_error);
            sceIoWrite(fd, error_msg, strlen(error_msg));
            sceIoClose(fd);
        }
        sceKernelDelayThread(3 * 1000 * 1000);
        sceKernelExitProcess(0);
        return 0;
    }

    AppState app_state = STATE_MAIN_MENU;
    TpbResult results[MAX_RESULTS];
    int num_results = 0;
    int menu_selection = 0;
    int results_selection = 0;
    int current_page = 0;
    SceCtrlData pad;
    unsigned int old_buttons = 0;

    while (1) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed_buttons = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;

        switch (app_state) {
            case STATE_MAIN_MENU: {
                draw_main_menu(menu_selection);
                if (pressed_buttons & SCE_CTRL_UP) { if (menu_selection > 0) menu_selection--; }
                if (pressed_buttons & SCE_CTRL_DOWN) { if (menu_selection < 1) menu_selection++; }
                if (pressed_buttons & SCE_CTRL_CROSS) {
                    if (menu_selection == 0) { app_state = STATE_INPUT; } 
                    else if (menu_selection == 1) { sceKernelExitProcess(0); }
                }
                break;
            }
            case STATE_INPUT: {
                psvDebugScreenInit();
                osk_init(search_query);
                while(1) {
                    sceCtrlPeekBufferPositive(0, &pad, 1);
                    if (osk_update(&pad)) break;
                    psvDebugScreenInit();
                    osk_draw(search_query);
                    sceDisplayWaitVblankStart();
                }
                show_status_screen("Buscando", search_query);
                num_results = tpb_search(search_query, results, MAX_RESULTS);
                results_selection = 0;
                current_page = 0;
                app_state = STATE_SHOW_RESULTS;
                break;
            }
            case STATE_SHOW_RESULTS: {
                draw_results_menu(results, num_results, results_selection, current_page);
                if (num_results == 0) {
                    psvDebugScreenPrintf("\nNenhum resultado encontrado. Pressione (O) para voltar.\n");
                    if (pressed_buttons & SCE_CTRL_CIRCLE) app_state = STATE_MAIN_MENU;
                } else {
                    if (pressed_buttons & SCE_CTRL_UP) { if (results_selection > 0) results_selection--; }
                    if (pressed_buttons & SCE_CTRL_DOWN) { if (results_selection < num_results - 1) results_selection++; }
                    if (pressed_buttons & SCE_CTRL_LTRIGGER) {
                        if (current_page > 0) current_page--;
                        results_selection = current_page * ITEMS_PER_PAGE;
                    }
                    if (pressed_buttons & SCE_CTRL_RTRIGGER) {
                        int total_pages = (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
                        if (current_page < total_pages - 1) current_page++;
                        results_selection = current_page * ITEMS_PER_PAGE;
                    }
                    if (pressed_buttons & SCE_CTRL_CROSS) app_state = STATE_PROCESSING;
                    if (pressed_buttons & SCE_CTRL_TRIANGLE) app_state = STATE_INPUT;
                    if (pressed_buttons & SCE_CTRL_START) sceKernelExitProcess(0);
                    if (results_selection < current_page * ITEMS_PER_PAGE || results_selection >= (current_page + 1) * ITEMS_PER_PAGE) {
                        current_page = results_selection / ITEMS_PER_PAGE;
                    }
                }
                break;
            }
            case STATE_PROCESSING: {
                if (strlen(RD_TOKEN) < 10 || strcmp(RD_TOKEN, "COLOQUE_SEU_TOKEN_AQUI") == 0) {
                    show_status_screen("ERRO", "Token do Real-Debrid nao configurado!\nEdite src/realdebrid.h e compile novamente.");
                    sceKernelDelayThread(5 * 1000 * 1000);
                    app_state = STATE_SHOW_RESULTS;
                    break;
                }
                show_status_screen("Real-Debrid", "Enviando magnet...");
                char torrent_id[128];
                if (!rd_add_magnet(results[results_selection].magnet, torrent_id)) {
                    show_status_screen("ERRO", "Falha ao enviar magnet ao Real-Debrid.");
                    sceKernelDelayThread(5 * 1000 * 1000);
                    app_state = STATE_SHOW_RESULTS;
                    break;
                }
                show_status_screen("Real-Debrid", "Aguardando torrent ficar pronto...");
                if (!rd_wait_for_ready(torrent_id)) {
                    show_status_screen("ERRO", "Timeout ou falha ao aguardar o torrent.");
                    sceKernelDelayThread(5 * 1000 * 1000);
                    app_state = STATE_SHOW_RESULTS;
                    break;
                }
                show_status_screen("Download", "Obtendo link de download...");
                char download_url[2048];
                if (!rd_get_file_url(torrent_id, download_url)) {
                    show_status_screen("ERRO", "Falha ao obter URL de download.");
                    sceKernelDelayThread(5 * 1000 * 1000);
                    app_state = STATE_SHOW_RESULTS;
                    break;
                }
                show_status_screen("Download", "Baixando o arquivo para o Vita...");
                char local_path[256] = "ux0:data/movie.mp4";
                if (!download_file(download_url, local_path)) {
                    show_status_screen("ERRO", "Falha ao baixar o arquivo.");
                    sceKernelDelayThread(5 * 1000 * 1000);
                    app_state = STATE_SHOW_RESULTS;
                    break;
                }
                show_status_screen("Player", "Iniciando o player externo...");
                player_play(local_path);
                sceKernelDelayThread(2 * 1000 * 1000);
                sceKernelExitProcess(0);
                break;
            }
        }
        sceDisplayWaitVblankStart();
    }
    sceKernelExitProcess(0);
    return 0;
}
