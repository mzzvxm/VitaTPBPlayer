#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debugScreen.h"
#include "tpb_scraper.h"
#include "realdebrid.h"
#include "player.h"
#include "osk.h" // Inclui nosso teclado

#define MAX_RESULTS 50 // Aumentamos para ter mais resultados para paginar
#define ITEMS_PER_PAGE 8 // Resultados por página

// Enum para gerenciar os diferentes estados do aplicativo
typedef enum {
    STATE_INPUT,
    STATE_SHOW_RESULTS,
    STATE_PROCESSING
} AppState;

// Buffer para o texto da busca
static char search_query[OSK_INPUT_MAX_LENGTH] = "matrix";

// Função para mostrar telas de status (feedback visual)
void show_status_screen(const char* title, const char* message) {
    psvDebugScreenInit();
    psvDebugScreenPrintf("--- %s ---\n\n", title);
    psvDebugScreenPrintf("%s\n\n", message);
    psvDebugScreenPrintf("Por favor, aguarde...");
}

// Função de menu modificada para incluir paginação
void draw_menu(TpbResult* results, int num_results, int selection, int current_page) {
    psvDebugScreenInit();

    psvDebugScreenPrintf("Resultados para: '%s' (Pagina %d/%d)\n", search_query, current_page + 1, (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    psvDebugScreenPrintf("--------------------------------------------------------------------------------\n\n");

    int start_index = current_page * ITEMS_PER_PAGE;
    int end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > num_results) {
        end_index = num_results;
    }

    for (int i = start_index; i < end_index; i++) {
        if (i == selection) {
            psvDebugScreenPrintf(">> ");
        } else {
            psvDebugScreenPrintf("   ");
        }
        
        char truncated_name[60];
        strncpy(truncated_name, results[i].name, sizeof(truncated_name) - 1);
        truncated_name[sizeof(truncated_name) - 1] = '\0';
        
        psvDebugScreenPrintf("%-60s (S:%d L:%d)\n", truncated_name, results[i].seeders, results[i].leechers);
    }
    
    psvDebugScreenPrintf("\n--------------------------------------------------------------------------------\n");
    psvDebugScreenPrintf("(X) Selecionar | (L/R) Pagina | (TRIANGULO) Nova Busca | (START) Sair\n");
}

int main(int argc, char *argv[]) {
    psvDebugScreenInit();

    AppState app_state = STATE_INPUT;
    TpbResult results[MAX_RESULTS];
    int num_results = 0;
    
    int selection = 0;
    int current_page = 0;
    SceCtrlData pad;

    while (1) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);

        switch (app_state) {
            case STATE_INPUT: {
                psvDebugScreenInit();
                osk_init(search_query);
                
                // Loop do teclado
                while(1) {
                    sceCtrlPeekBufferPositive(0, &pad, 1);
                    if (osk_update(&pad)) {
                        app_state = STATE_PROCESSING; // Vai para a tela de busca
                        break;
                    }
                    psvDebugScreenInit();
                    osk_draw(search_query);
                    sceDisplayWaitVblankStart();
                }
                // Prepara para a busca
                show_status_screen("Buscando", search_query);
                num_results = tpb_search(search_query, results, MAX_RESULTS);
                selection = 0;
                current_page = 0;
                app_state = STATE_SHOW_RESULTS;
                break;
            }

            case STATE_SHOW_RESULTS: {
                draw_menu(results, num_results, selection, current_page);
                
                static unsigned int old_buttons_menu = 0;
                unsigned int pressed_buttons = pad.buttons & ~old_buttons_menu;
                old_buttons_menu = pad.buttons;

                if (num_results == 0) {
                    psvDebugScreenPrintf("Nenhum resultado encontrado.\n");
                    if (pressed_buttons & (SCE_CTRL_TRIANGLE | SCE_CTRL_START)) {
                        app_state = STATE_INPUT;
                    }
                } else {
                     if (pressed_buttons & SCE_CTRL_UP) {
                        if (selection > 0) selection--;
                    }
                    if (pressed_buttons & SCE_CTRL_DOWN) {
                        if (selection < num_results - 1) selection++;
                    }
                    if (pressed_buttons & SCE_CTRL_LTRIGGER) {
                        if (current_page > 0) current_page--;
                        selection = current_page * ITEMS_PER_PAGE;
                    }
                    if (pressed_buttons & SCE_CTRL_RTRIGGER) {
                        int total_pages = (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
                        if (current_page < total_pages - 1) current_page++;
                        selection = current_page * ITEMS_PER_PAGE;
                    }
                    if (pressed_buttons & SCE_CTRL_CROSS) {
                        app_state = STATE_PROCESSING;
                    }
                    if (pressed_buttons & SCE_CTRL_TRIANGLE) {
                        app_state = STATE_INPUT;
                    }
                    if (pressed_buttons & SCE_CTRL_START) {
                        sceKernelExitProcess(0);
                    }
                    
                    // Ajusta a seleção para estar sempre na página visível
                    if (selection < current_page * ITEMS_PER_PAGE || selection >= (current_page + 1) * ITEMS_PER_PAGE) {
                        current_page = selection / ITEMS_PER_PAGE;
                    }
                }
                break;
            }

            case STATE_PROCESSING: {
                show_status_screen("Real-Debrid", "Enviando magnet...");
                char torrent_id[128];
                if (!rd_add_magnet(results[selection].magnet, torrent_id)) {
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
                
                // Após o player fechar (se fechar), o app encerra.
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