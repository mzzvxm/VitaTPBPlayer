#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/kernel/threadmgr/mutex.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/apputil.h>       // Necessário para inicialização correta do sistema
#include <psp2/common_dialog.h> // Necessário para o sistema de diálogos (IME)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "token_server.h"
#include "tpb_scraper.h"
#include "realdebrid.h"
#include "player.h"
#include "osk.h" // <<< VOLTAMOS PARA OSK.H (Nova implementação)
#include "ui.h"

#define MAX_RESULTS 50
#define MAX_FILES_PER_TORRENT 100
#define ITEMS_PER_PAGE 12
#define TOKEN_CONFIG_PATH "ux0:data/VitaTPBPlayer/token.txt"
#define DOWNLOAD_FOLDER "ux0:data/VitaTPBPlayer/"
#define MAX_INPUT_LEN 256 

typedef enum {
    STATE_MAIN_MENU,
    STATE_INPUT,
    STATE_SETTINGS,
    STATE_SHOW_RESULTS,
    STATE_SELECT_FILE,
    STATE_DOWNLOADING,
    STATE_ERROR,
    STATE_TOKEN_SERVER
} AppState;

static char search_query[MAX_INPUT_LEN] = "matrix";
static char error_message[256] = "";
static AppState state_before_osk = STATE_MAIN_MENU;

static RdUserInfo g_user_info;
static char g_settings_status[128] = "";
static int g_settings_info_valid = 0;
static char vita_ip_address[32] = "Buscando IP...";

// Seleção de arquivos do torrent (Real-Debrid)
static RdFileInfo g_torrent_files[MAX_FILES_PER_TORRENT];
static int g_num_torrent_files = 0;
static char g_current_torrent_id[128] = {0};
static int g_selected_file_id = -1;

// Progresso global do download
struct ProgressData {
    char status_message[256];
    char error_message[256];
    float progress_percent;
    volatile int is_running;
    volatile int is_done;
    volatile int is_cancelled;
    char final_filepath[512];
} g_progress;

SceUID g_progress_mutex;
SceUID g_worker_thread_id;

// Worker de download (usa seleção de arquivo já feita)
static int download_worker_thread(SceSize args, void *argp) {
    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 1/4: Selecionando arquivo...");
    g_progress.error_message[0] = '\0';
    g_progress.is_running = 1;
    sceKernelUnlockMutex(g_progress_mutex, 1);

    if (!rd_select_specific_file(g_current_torrent_id, g_selected_file_id, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 2/4: Processando torrent...");
    sceKernelUnlockMutex(g_progress_mutex, 1);

    RdTorrentInfo torrent_info;
    if (!rd_wait_for_torrent_ready(g_current_torrent_id, &torrent_info, NULL, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 3/4: Desbloqueando link...");
    sceKernelUnlockMutex(g_progress_mutex, 1);

    char download_url[2048];
    if (!rd_unrestrict_link(torrent_info.original_link, download_url, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    // Deriva nome do arquivo a partir da lista global
    char safe_filename[256] = "video";
    for (int i = 0; i < g_num_torrent_files; ++i) {
        if (g_torrent_files[i].id == g_selected_file_id) {
            char *last_slash = strrchr(g_torrent_files[i].path, '/');
            char *filename_to_use = last_slash ? last_slash + 1 : g_torrent_files[i].path;

            int j = 0;
            for (int k = 0; filename_to_use[k] != '\0' && j < (int)sizeof(safe_filename) - 5; k++) {
                unsigned char c = (unsigned char)filename_to_use[k];
                if (isalnum(c) || c == '.' || c == '-' || c == ' ') {
                    safe_filename[j++] = (char)c;
                }
            }
            safe_filename[j] = '\0';
            break;
        }
    }
    snprintf(g_progress.final_filepath, sizeof(g_progress.final_filepath), "%s%s", DOWNLOAD_FOLDER, safe_filename);

    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 4/4: Iniciando download...");
    sceKernelUnlockMutex(g_progress_mutex, 1);

    if (!download_file(download_url, g_progress.final_filepath, &g_progress)) {
        goto thread_error;
    }

    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    g_progress.is_done = 1;
    g_progress.is_running = 0;
    sceKernelUnlockMutex(g_progress_mutex, 1);
    return sceKernelExitDeleteThread(0);

thread_error:
    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    g_progress.is_running = 0;
    snprintf(error_message, sizeof(error_message), "%s", g_progress.error_message);
    sceKernelUnlockMutex(g_progress_mutex, 1);
    return sceKernelExitDeleteThread(0);
}

static void get_ip_address() {
    SceNetCtlInfo info;
    if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) == 0) {
        strncpy(vita_ip_address, info.ip_address, sizeof(vita_ip_address) - 1);
        vita_ip_address[sizeof(vita_ip_address) - 1] = '\0';
    } else {
        strcpy(vita_ip_address, "IP nao encontrado");
    }
}

static void start_download() {
    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    g_progress.is_running = 1;
    g_progress.is_done = 0;
    g_progress.is_cancelled = 0;
    g_progress.progress_percent = 0.0f;
    g_progress.error_message[0] = '\0';
    g_progress.final_filepath[0] = '\0';
    sceKernelUnlockMutex(g_progress_mutex, 1);

    g_worker_thread_id = sceKernelCreateThread("download_worker", download_worker_thread, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(g_worker_thread_id, 0, NULL);
}

int main(int argc, char *argv[]) {
    // 1. INICIALIZAÇÃO DE SISTEMA 
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    sceCommonDialogSetConfigParam(&(SceCommonDialogConfigParam){});

    // Módulos e rede
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    // Nota: O módulo IME é carregado sob demanda no novo osk.c, mas não faz mal carregar aqui também
    sceSysmoduleLoadModule(SCE_SYSMODULE_IME); 

    static char net_memory[1024 * 1024];
    SceNetInitParam net_init_param = { .memory = net_memory, .size = sizeof(net_memory), .flags = 0 };
    sceNetInit(&net_init_param);
    sceNetCtlInit();

    // UI centralizada
    if (!ui_init("app0:font.ttf", "ux0:app/VTPB00001/font.ttf")) {
        sceKernelExitProcess(0);
        return 0;
    }

    g_progress_mutex = sceKernelCreateMutex("progress_mutex", 0, 0, NULL);
    sceIoMkdir(DOWNLOAD_FOLDER, 0777);
    rd_load_token_from_file(TOKEN_CONFIG_PATH);
    
    // Touch (front)
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    AppState app_state = STATE_MAIN_MENU;
    TpbResult results[MAX_RESULTS];
    int num_results = 0;
    int menu_selection = 0;
    int settings_selection = 0;
    int results_selection = 0;
    int file_selection = 0;
    int current_page = 0;
    SceCtrlData pad;
    unsigned int old_buttons = 0;

    SceTouchData touch_data;
    int touch_x = -1, touch_y = -1, touch_held = 0;

    int input_initialized = 0; // Flag para controlar se o IME já foi aberto

    while (1) {
        sceCommonDialogUpdate(&(SceCommonDialogUpdateParam){});

        // Input polling
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed_buttons = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;

        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch_data, 1);
        int touch_tapped = 0;
        if (touch_data.reportNum > 0 && !touch_held) {
            touch_held = 1;
            touch_x = touch_data.report[0].x / 2;
            touch_y = touch_data.report[0].y / 2;
        } else if (touch_data.reportNum == 0 && touch_held) {
            touch_held = 0;
            touch_tapped = 1;
        } else if (touch_data.reportNum == 0) {
            touch_x = -1;
            touch_y = -1;
        }

        ui_begin_frame();

        switch (app_state) {
            case STATE_MAIN_MENU: {
                input_initialized = 0;
                ui_draw_main_menu(menu_selection);
                if (touch_tapped) {
                    if (touch_x > 40 && touch_x < 400 && touch_y > 90  && touch_y < 120) { menu_selection = 0; pressed_buttons |= SCE_CTRL_CROSS; }
                    else if (touch_x > 40 && touch_x < 400 && touch_y > 120 && touch_y < 150) { menu_selection = 1; pressed_buttons |= SCE_CTRL_CROSS; }
                    else if (touch_x > 40 && touch_x < 400 && touch_y > 150 && touch_y < 180) { menu_selection = 2; pressed_buttons |= SCE_CTRL_CROSS; }
                }

                if (pressed_buttons & SCE_CTRL_UP)   { if (menu_selection > 0) menu_selection--; }
                if (pressed_buttons & SCE_CTRL_DOWN) { if (menu_selection < 2) menu_selection++; }
                if (pressed_buttons & SCE_CTRL_CROSS) {
                    if (menu_selection == 0) { 
                        state_before_osk = app_state;
                        app_state = STATE_INPUT; 
                    }
                    else if (menu_selection == 1) { 
                        settings_selection = 0;
                        app_state = STATE_SETTINGS; 
                    }
                    else if (menu_selection == 2) goto exit_loop;
                }
                break;
            }

            case STATE_INPUT: {
            // ========== CÓDIGO NOVO ==========
            if (!input_initialized) {
                const char *title = (state_before_osk == STATE_SETTINGS) 
                    ? "Insira o Token" 
                    : "Buscar Torrent";
                const char *initial = (state_before_osk == STATE_SETTINGS) 
                    ? rd_get_token() 
                    : search_query;
                
                osk_init_ime(title, initial);
                input_initialized = 1;
            }

            // UI de fundo (aparece atrás do teclado do sistema)
            ui_draw_status("Teclado Aberto", "Digite no teclado do sistema...");

            // Processa o IME
            OskStatus status = osk_update_ime();
            
            if (status == OSK_STATUS_FINISHED) {
                char *text_result = osk_get_text();
                
                if (state_before_osk == STATE_SETTINGS) {
                    rd_set_token(text_result);
                    app_state = STATE_SETTINGS;
                } else {
                    strncpy(search_query, text_result, sizeof(search_query)-1);
                    search_query[sizeof(search_query) - 1] = '\0';

                    // Feedback visual
                    ui_end_frame();
                    ui_begin_frame();
                    ui_draw_status("Buscando...", search_query);
                    ui_end_frame();
                    ui_begin_frame(); 

                    num_results = tpb_search(search_query, results, MAX_RESULTS, 
                                            error_message, sizeof(error_message));
                    if (num_results < 0) {
                        app_state = STATE_ERROR;
                    } else {
                        results_selection = 0;
                        current_page = 0;
                        app_state = STATE_SHOW_RESULTS;
                    }
                }
                input_initialized = 0;
                
            } else if (status == OSK_STATUS_CANCELED || status == OSK_STATUS_ERROR) {
                app_state = state_before_osk;
                input_initialized = 0;
            }
            // ========== FIM DO CÓDIGO NOVO ==========
            break;
        }

            case STATE_SETTINGS: {
                input_initialized = 0;
                char masked_token[64] = "Nao configurado";
                const char* token = rd_get_token();
                if (token && strlen(token) > 8)
                    snprintf(masked_token, sizeof(masked_token), "%.4s************************%.4s", token, token + strlen(token) - 4);
                else if (token && strlen(token) > 0)
                    snprintf(masked_token, sizeof(masked_token), "Token muito curto");
                
                ui_draw_settings_screen(settings_selection, g_settings_info_valid ? &g_user_info : NULL, masked_token, g_settings_status);

                if (pressed_buttons & SCE_CTRL_UP)   { if (settings_selection > 0) settings_selection--; }
                if (pressed_buttons & SCE_CTRL_DOWN) { if (settings_selection < 3) settings_selection++; }
                if (pressed_buttons & SCE_CTRL_CROSS) {
                    switch (settings_selection) {
                        case 0: // Editar Token Manualmente
                            state_before_osk = app_state;
                            app_state = STATE_INPUT;
                            break;
                        case 1: // Testar Token
                            g_settings_info_valid = 0;
                            snprintf(g_settings_status, sizeof(g_settings_status), "Testando token...");
                            
                            ui_end_frame(); 
                            ui_begin_frame();
                            ui_draw_settings_screen(settings_selection, g_settings_info_valid ? &g_user_info : NULL, masked_token, g_settings_status);
                            ui_end_frame();
                            ui_begin_frame();

                            if (rd_get_user_info(&g_user_info, g_settings_status, sizeof(g_settings_status))) {
                                snprintf(g_settings_status, sizeof(g_settings_status), "Teste bem-sucedido!");
                                g_settings_info_valid = 1;
                            }
                            break;
                        case 2: // Servidor Wi-Fi
                            get_ip_address();
                            token_server_start(8080);
                            app_state = STATE_TOKEN_SERVER;
                            break;
                        case 3: // Salvar e Sair
                            rd_save_token_to_file(TOKEN_CONFIG_PATH);
                            app_state = STATE_MAIN_MENU;
                            break;
                    }
                }
                if (pressed_buttons & SCE_CTRL_CIRCLE) app_state = STATE_MAIN_MENU;
                break;
            }

            case STATE_TOKEN_SERVER: {
                ui_draw_token_server_screen(vita_ip_address);
                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    token_server_stop();
                    rd_load_token_from_file(TOKEN_CONFIG_PATH);
                    app_state = STATE_SETTINGS;
                }
                break;
            }

            case STATE_SHOW_RESULTS: {
                input_initialized = 0;
                if (num_results == 0) {
                    ui_draw_results(search_query, results, 0, 0, 0, ITEMS_PER_PAGE);
                    if (pressed_buttons & SCE_CTRL_CIRCLE) app_state = STATE_MAIN_MENU;
                } else {
                    ui_draw_results(search_query, results, num_results, results_selection, current_page, ITEMS_PER_PAGE);
                    if (pressed_buttons & SCE_CTRL_UP)   { if (results_selection > 0) results_selection--; }
                    if (pressed_buttons & SCE_CTRL_DOWN) { if (results_selection < num_results - 1) results_selection++; }
                    if (pressed_buttons & SCE_CTRL_LTRIGGER) {
                        if (current_page > 0) { current_page--; results_selection = current_page * ITEMS_PER_PAGE; }
                    }
                    if (pressed_buttons & SCE_CTRL_RTRIGGER) {
                        int total_pages = (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
                        if (current_page < total_pages - 1) { current_page++; results_selection = current_page * ITEMS_PER_PAGE; }
                    }
                    if (pressed_buttons & SCE_CTRL_CIRCLE) app_state = STATE_MAIN_MENU;
                    if (pressed_buttons & SCE_CTRL_CROSS) {
                        // Adiciona magnet, pega arquivos
                        ui_end_frame();
                        ui_begin_frame();
                        ui_draw_status("Processando...", "Adicionando magnet ao Real-Debrid..."); 
                        ui_end_frame();
                        ui_begin_frame();

                        if (!rd_add_magnet(results[results_selection].magnet, g_current_torrent_id, error_message, sizeof(error_message))) {
                            app_state = STATE_ERROR;
                            break;
                        }

                        ui_end_frame();
                        ui_begin_frame();
                        ui_draw_status("Processando...", "Buscando lista de arquivos do torrent..."); 
                        ui_end_frame();
                        ui_begin_frame();

                        g_num_torrent_files = rd_get_torrent_files(g_current_torrent_id, g_torrent_files, MAX_FILES_PER_TORRENT, error_message, sizeof(error_message));
                        if (g_num_torrent_files < 0) {
                            app_state = STATE_ERROR;
                        } else if (g_num_torrent_files == 0) {
                            snprintf(error_message, sizeof(error_message), "Nenhum arquivo encontrado neste torrent.");
                            app_state = STATE_ERROR;
                        } else if (g_num_torrent_files == 1) {
                            g_selected_file_id = g_torrent_files[0].id;
                            start_download();
                            app_state = STATE_DOWNLOADING;
                        } else {
                            file_selection = 0;
                            current_page = 0;
                            app_state = STATE_SELECT_FILE;
                        }
                    }

                    if (pressed_buttons & SCE_CTRL_TRIANGLE) { 
                        state_before_osk = STATE_MAIN_MENU;
                        app_state = STATE_INPUT; 
                    }

                    if (results_selection < current_page * ITEMS_PER_PAGE) current_page = results_selection / ITEMS_PER_PAGE;
                    if (results_selection >= (current_page + 1) * ITEMS_PER_PAGE) current_page = results_selection / ITEMS_PER_PAGE;
                }
                break;
            }

            case STATE_SELECT_FILE: {
                ui_draw_file_selection(g_torrent_files, g_num_torrent_files, file_selection, current_page, ITEMS_PER_PAGE);
                if (pressed_buttons & SCE_CTRL_UP)   { if (file_selection > 0) file_selection--; }
                if (pressed_buttons & SCE_CTRL_DOWN) { if (file_selection < g_num_torrent_files - 1) file_selection++; }
                if (pressed_buttons & SCE_CTRL_LTRIGGER) {
                    if (current_page > 0) { current_page--; file_selection = current_page * ITEMS_PER_PAGE; }
                }
                if (pressed_buttons & SCE_CTRL_RTRIGGER) {
                    int total_pages = (g_num_torrent_files + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
                    if (current_page < total_pages - 1) { current_page++; file_selection = current_page * ITEMS_PER_PAGE; }
                }
                if (pressed_buttons & SCE_CTRL_CIRCLE) app_state = STATE_SHOW_RESULTS;
                if (pressed_buttons & SCE_CTRL_CROSS) {
                    g_selected_file_id = g_torrent_files[file_selection].id;
                    start_download();
                    app_state = STATE_DOWNLOADING;
                }

                if (file_selection < current_page * ITEMS_PER_PAGE) current_page = file_selection / ITEMS_PER_PAGE;
                if (file_selection >= (current_page + 1) * ITEMS_PER_PAGE) current_page = file_selection / ITEMS_PER_PAGE;
                break;
            }

            case STATE_DOWNLOADING: {
                UiProgress p;
                sceKernelLockMutex(g_progress_mutex, 1, NULL);
                p.percent = g_progress.progress_percent;
                strncpy(p.status, g_progress.status_message, sizeof(p.status)-1);
                p.status[sizeof(p.status)-1] = '\0';
                int is_done = g_progress.is_done;
                int is_running = g_progress.is_running;
                int was_cancelled = g_progress.is_cancelled;
                char final_path[512]; snprintf(final_path, sizeof(final_path), "%s", g_progress.final_filepath);
                sceKernelUnlockMutex(g_progress_mutex, 1);

                ui_draw_progress(&p);
                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    sceKernelLockMutex(g_progress_mutex, 1, NULL);
                    g_progress.is_cancelled = 1;
                    sceKernelUnlockMutex(g_progress_mutex, 1);
                }

                if (is_done) {
                    player_play(final_path);
                    app_state = STATE_SHOW_RESULTS;
                } else if (!is_running) {
                    if (strlen(error_message) > 0) app_state = STATE_ERROR;
                    else if (was_cancelled) app_state = STATE_SHOW_RESULTS;
                }
                break;
            }

            case STATE_ERROR: {
                ui_draw_error(error_message);
                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    error_message[0] = '\0';
                    app_state = STATE_SHOW_RESULTS;
                }
                break;
            }
        }

        ui_end_frame();
    }

exit_loop:
    if (token_server_is_running()) token_server_stop();

    sceNetCtlTerm();
    sceNetTerm();

    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);

    ui_shutdown();
    sceKernelDeleteMutex(g_progress_mutex);
    sceKernelExitProcess(0);
    return 0;
}