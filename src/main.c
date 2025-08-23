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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vita2d.h>

#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include "token_server.h"

#include "tpb_scraper.h"
#include "realdebrid.h"
#include "player.h"
#include "osk.h"

#define MAX_RESULTS 50
#define ITEMS_PER_PAGE 12
#define FONT_SIZE 18.0f
#define TOKEN_CONFIG_PATH "ux0:data/VitaTPBPlayer/token.txt"
#define DOWNLOAD_FOLDER "ux0:data/VitaTPBPlayer/"

typedef enum {
    STATE_MAIN_MENU,
    STATE_INPUT,
    STATE_SETTINGS,
    STATE_SHOW_RESULTS,
    STATE_DOWNLOADING,
    STATE_ERROR,
    STATE_TOKEN_SERVER
} AppState;

vita2d_font *font;
#define COLOR_WHITE RGBA8(255, 255, 255, 255)
#define COLOR_YELLOW RGBA8(255, 255, 0, 255)
#define COLOR_GREY RGBA8(170, 170, 170, 255)
#define COLOR_GREEN RGBA8(0, 255, 0, 255)

static char search_query[OSK_INPUT_MAX_LENGTH] = "matrix";
static char error_message[256] = "";
static char temp_osk_buffer[OSK_INPUT_MAX_LENGTH] = "";
static AppState state_before_osk = STATE_MAIN_MENU;

static RdUserInfo g_user_info;
static char g_settings_status[128] = "";
static int g_settings_info_valid = 0;
static char vita_ip_address[32] = "Buscando IP...";

// Estrutura de progresso global
struct ProgressData {
    char status_message[256];
    char error_message[256];
    float progress_percent;
    volatile int is_running;
    volatile int is_done;
    volatile int is_cancelled;
    TpbResult selected_torrent;
    char torrent_id[128];
    char final_filepath[512];
} g_progress;

SceUID g_progress_mutex;
SceUID g_worker_thread_id;

// Thread único para todo o processo de download
static int download_worker_thread(SceSize args, void *argp) {
    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 1/4: Adicionando magnet...");
    g_progress.error_message[0] = '\0';
    g_progress.is_running = 1;
    sceKernelUnlockMutex(g_progress_mutex, 1);

    if (!rd_add_magnet(g_progress.selected_torrent.magnet, g_progress.torrent_id, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 2/4: Processando torrent...");
    sceKernelUnlockMutex(g_progress_mutex, 1);
    
    if (!rd_select_all_files(g_progress.torrent_id, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    RdTorrentInfo torrent_info;
    if (!rd_wait_for_torrent_ready(g_progress.torrent_id, &torrent_info, NULL, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 3/4: Desbloqueando link...");
    sceKernelUnlockMutex(g_progress_mutex, 1);

    char download_url[2048];
    if (!rd_unrestrict_link(torrent_info.original_link, download_url, g_progress.error_message, sizeof(g_progress.error_message))) {
        goto thread_error;
    }

    // Limpa o nome do arquivo para criar um caminho seguro
    char safe_filename[256];
    int j = 0;
    for (int i = 0; g_progress.selected_torrent.name[i] != '\0' && j < sizeof(safe_filename) - 5; i++) {
        if (isalnum((unsigned char)g_progress.selected_torrent.name[i]) || g_progress.selected_torrent.name[i] == '.' || g_progress.selected_torrent.name[i] == '-' || g_progress.selected_torrent.name[i] == ' ') {
            safe_filename[j++] = g_progress.selected_torrent.name[i];
        }
    }
    safe_filename[j] = '\0';
    snprintf(g_progress.final_filepath, sizeof(g_progress.final_filepath), "%s%s.mp4", DOWNLOAD_FOLDER, safe_filename);
    
    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    snprintf(g_progress.status_message, sizeof(g_progress.status_message), "Passo 4/4: Iniciando download...");
    sceKernelUnlockMutex(g_progress_mutex, 1);

    // Inicia o download do arquivo completo
    if (!download_file(download_url, g_progress.final_filepath, &g_progress)) {
        goto thread_error;
    }
    
    // Sucesso!
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

void show_status_screen(const char* title, const char* message) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_font_draw_text(font, 40, 272, COLOR_WHITE, FONT_SIZE + 4.0f, title);
    vita2d_font_draw_text(font, 40, 300, COLOR_GREY, FONT_SIZE, message);
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void draw_main_menu(int selection) {
    vita2d_font_draw_text(font, 40, 40, COLOR_WHITE, FONT_SIZE + 6.0f, "VitaTPBPlayer");
    vita2d_font_draw_text(font, 40, 100, (selection == 0) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Buscar Torrents");
    vita2d_font_draw_text(font, 40, 130, (selection == 1) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Configuracoes");
    vita2d_font_draw_text(font, 40, 160, (selection == 2) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Sair");
    vita2d_font_draw_text(font, 40, 500, COLOR_GREY, FONT_SIZE - 2.0f, "Use CIMA/BAIXO, (X) ou o toque para selecionar.");
}

void draw_settings_screen(int selection) {
    vita2d_font_draw_text(font, 40, 40, COLOR_WHITE, FONT_SIZE + 4.0f, "Configuracoes");
    const char* token = rd_get_token();
    char masked_token[64] = "Nao configurado";
    if (strlen(token) > 8) {
        snprintf(masked_token, sizeof(masked_token), "%.4s************************%.4s", token, token + strlen(token) - 4);
    } else if (strlen(token) > 0) {
        snprintf(masked_token, sizeof(masked_token), "Token muito curto");
    }
    vita2d_font_draw_text(font, 40, 100, COLOR_GREY, FONT_SIZE, "Token Real-Debrid:");
    vita2d_font_draw_text(font, 40, 125, COLOR_WHITE, FONT_SIZE, masked_token);

    vita2d_font_draw_text(font, 40, 180, (selection == 0) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Editar Token Manualmente");
    vita2d_font_draw_text(font, 40, 210, (selection == 1) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Testar Token");
    vita2d_font_draw_text(font, 40, 240, (selection == 2) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Receber Token via Wi-Fi");
    vita2d_font_draw_text(font, 40, 270, (selection == 3) ? COLOR_YELLOW : COLOR_WHITE, FONT_SIZE, ">> Salvar e Voltar");
    vita2d_font_draw_text(font, 40, 320, COLOR_GREY, FONT_SIZE, g_settings_status);

    if (g_settings_info_valid) {
        vita2d_font_draw_textf(font, 40, 360, COLOR_WHITE, FONT_SIZE, "Usuario: %s", g_user_info.username);
        vita2d_font_draw_textf(font, 40, 390, COLOR_WHITE, FONT_SIZE, "Email: %s", g_user_info.email);
        vita2d_font_draw_textf(font, 40, 420, g_user_info.is_premium ? COLOR_GREEN : COLOR_YELLOW, FONT_SIZE,
            "Status: %s (Expira em: %s)", g_user_info.is_premium ? "Premium" : "Gratuito", g_user_info.expiration);
    }
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

void draw_token_server_screen() {
    vita2d_font_draw_text(font, 40, 150, COLOR_WHITE, FONT_SIZE + 4.0f, "Receber Token via Wi-Fi");
    vita2d_font_draw_text(font, 40, 200, COLOR_GREY, FONT_SIZE, "1. Conecte seu celular/PC na mesma rede Wi-Fi.");
    vita2d_font_draw_text(font, 40, 230, COLOR_GREY, FONT_SIZE, "2. Abra o navegador e acesse o seguinte endereco:");
    vita2d_font_draw_textf(font, 40, 270, COLOR_YELLOW, FONT_SIZE + 2.0f, "http://%s:8080", vita_ip_address);
    vita2d_font_draw_text(font, 40, 310, COLOR_GREY, FONT_SIZE, "3. Cole seu token e clique em Salvar.");
    vita2d_font_draw_text(font, 40, 500, COLOR_GREY, FONT_SIZE - 2.0f, "Pressione (O) para cancelar e voltar.");
}

void draw_results_menu(TpbResult* results, int num_results, int selection, int current_page) {
    int y_pos = 40;
    int total_pages = (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (total_pages == 0) total_pages = 1;
    vita2d_font_draw_textf(font, 40, y_pos, COLOR_WHITE, FONT_SIZE, "Resultados para: '%s' (Pagina %d/%d)", search_query, current_page + 1, total_pages);
    y_pos += 40;

    int start_index = current_page * ITEMS_PER_PAGE;
    int end_index = start_index + ITEMS_PER_PAGE;
    if (end_index > num_results) end_index = num_results;

    for (int i = start_index; i < end_index; i++) {
        unsigned int color = (i == selection) ? COLOR_YELLOW : COLOR_WHITE;
        char truncated_name[60];
        strncpy(truncated_name, results[i].name, sizeof(truncated_name) -1);
        truncated_name[sizeof(truncated_name) - 1] = '\0';

        vita2d_font_draw_textf(font, 40, y_pos, color, FONT_SIZE, "%s %s", (i == selection) ? ">>" : "  ", truncated_name);
        y_pos += 20;
        vita2d_font_draw_textf(font, 70, y_pos, color, FONT_SIZE - 2.0f, "Tamanho: %s | Seeders: %d", results[i].size, results[i].seeders);
        y_pos += 25;
    }

    vita2d_font_draw_text(font, 40, 500, COLOR_GREY, FONT_SIZE - 2.0f, "(X) Baixar e Assistir | (O) Voltar | (L/R) Pagina | (TRIANGULO) Nova Busca");
}

void draw_progress_screen() {
    sceKernelLockMutex(g_progress_mutex, 1, NULL);
    char status[256];
    float progress = g_progress.progress_percent;
    strncpy(status, g_progress.status_message, sizeof(status));
    status[sizeof(status)-1] = '\0';
    sceKernelUnlockMutex(g_progress_mutex, 1);

    vita2d_font_draw_text(font, 40, 200, COLOR_WHITE, FONT_SIZE + 4.0f, "Progresso do Download");
    vita2d_font_draw_text(font, 40, 240, COLOR_GREY, FONT_SIZE, status);

    float bar_width = 880;
    float progress_width = (bar_width / 100.0f) * progress;
    vita2d_draw_rectangle(40, 270, bar_width, 30, COLOR_GREY);
    vita2d_draw_rectangle(40, 270, progress_width, 30, COLOR_GREEN);

    vita2d_font_draw_text(font, 40, 500, COLOR_GREY, FONT_SIZE - 2.0f, "Pressione (O) para cancelar.");
}

int main(int argc, char *argv[]) {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

    static char net_memory[1024 * 1024];
    SceNetInitParam net_init_param = { .memory = net_memory, .size = sizeof(net_memory), .flags = 0 };
    sceNetInit(&net_init_param);
    sceNetCtlInit();

    vita2d_init();
    vita2d_set_clear_color(RGBA8(25, 25, 25, 255));
    font = vita2d_load_font_file("app0:font.ttf");
    if (!font) {
        font = vita2d_load_font_file("ux0:app/VTPB00001/font.ttf");
    }
    if (!font) {
        sceKernelExitProcess(0);
        return 0;
    }

    g_progress_mutex = sceKernelCreateMutex("progress_mutex", 0, 0, NULL);
    sceIoMkdir(DOWNLOAD_FOLDER, 0777);
    rd_load_token_from_file(TOKEN_CONFIG_PATH);

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    AppState app_state = STATE_MAIN_MENU;
    TpbResult results[MAX_RESULTS];
    int num_results = 0;

    int menu_selection = 0;
    int settings_selection = 0;
    int results_selection = 0;
    int current_page = 0;
    
    SceCtrlData pad;
    unsigned int old_buttons = 0;

    SceTouchData touch_data;
    int touch_x = -1;
    int touch_y = -1;
    int touch_held = 0;

    while (1) {
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

        vita2d_start_drawing();
        vita2d_clear_screen();

        switch (app_state) {
            case STATE_MAIN_MENU: {
                draw_main_menu(menu_selection);

                if (touch_tapped) {
                    if (touch_x > 40 && touch_x < 400 && touch_y > 90 && touch_y < 120) {
                        menu_selection = 0;
                        pressed_buttons |= SCE_CTRL_CROSS;
                    } 
                    else if (touch_x > 40 && touch_x < 400 && touch_y > 120 && touch_y < 150) {
                        menu_selection = 1;
                        pressed_buttons |= SCE_CTRL_CROSS;
                    }
                    else if (touch_x > 40 && touch_x < 400 && touch_y > 150 && touch_y < 180) {
                        menu_selection = 2;
                        pressed_buttons |= SCE_CTRL_CROSS;
                    }
                }

                if (pressed_buttons & SCE_CTRL_UP) { if (menu_selection > 0) menu_selection--; }
                if (pressed_buttons & SCE_CTRL_DOWN) { if (menu_selection < 2) menu_selection++; }
                if (pressed_buttons & SCE_CTRL_CROSS) {
                    if (menu_selection == 0) {
                        state_before_osk = app_state;
                        app_state = STATE_INPUT;
                    } else if (menu_selection == 1) {
                        settings_selection = 0;
                        app_state = STATE_SETTINGS;
                    } else if (menu_selection == 2) goto exit_loop;
                }
                break;
            }
            case STATE_INPUT: {
                if (state_before_osk == STATE_SETTINGS) {
                    strncpy(temp_osk_buffer, rd_get_token(), sizeof(temp_osk_buffer)-1);
                } else {
                     strncpy(temp_osk_buffer, search_query, sizeof(temp_osk_buffer)-1);
                }
                temp_osk_buffer[sizeof(temp_osk_buffer) - 1] = '\0';
                osk_init(temp_osk_buffer);

                while(app_state == STATE_INPUT) {
                    sceCtrlPeekBufferPositive(0, &pad, 1);
                    if (osk_update(&pad)) {
                        if (state_before_osk == STATE_SETTINGS) {
                            rd_set_token(temp_osk_buffer);
                            app_state = STATE_SETTINGS;
                        } else {
                            strncpy(search_query, temp_osk_buffer, sizeof(search_query)-1);
                            search_query[sizeof(search_query) - 1] = '\0';
                            show_status_screen("Buscando...", search_query);
                            num_results = tpb_search(search_query, results, MAX_RESULTS, error_message, sizeof(error_message));
                            if (num_results < 0) {
                                app_state = STATE_ERROR;
                            } else {
                                results_selection = 0;
                                current_page = 0;
                                app_state = STATE_SHOW_RESULTS;
                            }
                        }
                    } else if ((pad.buttons & ~old_buttons) & SCE_CTRL_TRIANGLE) {
                         app_state = state_before_osk;
                    }
                    old_buttons = pad.buttons;
                    vita2d_start_drawing();
                    vita2d_clear_screen();
                    osk_draw(font, temp_osk_buffer);
                    vita2d_end_drawing();
                    vita2d_swap_buffers();
                }
                continue;
            }
            case STATE_SETTINGS: {
                draw_settings_screen(settings_selection);
                if (pressed_buttons & SCE_CTRL_UP) { if (settings_selection > 0) settings_selection--; }
                if (pressed_buttons & SCE_CTRL_DOWN) { if (settings_selection < 3) settings_selection++; }
                if (pressed_buttons & SCE_CTRL_CROSS) {
                    switch (settings_selection) {
                        case 0:
                            state_before_osk = app_state;
                            app_state = STATE_INPUT;
                            break;
                        case 1:
                            g_settings_info_valid = 0;
                            snprintf(g_settings_status, sizeof(g_settings_status), "Testando token...");
                            vita2d_start_drawing();
                            vita2d_clear_screen();
                            draw_settings_screen(settings_selection);
                            vita2d_end_drawing();
                            vita2d_swap_buffers();
                            if (rd_get_user_info(&g_user_info, g_settings_status, sizeof(g_settings_status))) {
                                snprintf(g_settings_status, sizeof(g_settings_status), "Teste bem-sucedido!");
                                g_settings_info_valid = 1;
                            }
                            break;
                        case 2:
                            get_ip_address();
                            token_server_start(8080);
                            app_state = STATE_TOKEN_SERVER;
                            break;
                        case 3:
                            rd_save_token_to_file(TOKEN_CONFIG_PATH);
                            app_state = STATE_MAIN_MENU;
                            break;
                    }
                }
                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    app_state = STATE_MAIN_MENU;
                }
                break;
            }
            case STATE_TOKEN_SERVER: {
                draw_token_server_screen();
                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    token_server_stop();
                    rd_load_token_from_file(TOKEN_CONFIG_PATH);
                    app_state = STATE_SETTINGS;
                }
                break;
            }
            case STATE_SHOW_RESULTS: {
                if (num_results == 0) {
                    vita2d_font_draw_text(font, 40, 80, COLOR_WHITE, FONT_SIZE, "Nenhum resultado encontrado.");
                    vita2d_font_draw_text(font, 40, 500, COLOR_GREY, FONT_SIZE - 2.0f, "(O) Voltar");
                    if (pressed_buttons & SCE_CTRL_CIRCLE) app_state = STATE_MAIN_MENU;
                } else {
                    draw_results_menu(results, num_results, results_selection, current_page);
                    if (pressed_buttons & SCE_CTRL_UP) { if (results_selection > 0) results_selection--; }
                    if (pressed_buttons & SCE_CTRL_DOWN) { if (results_selection < num_results - 1) results_selection++; }
                    if (pressed_buttons & SCE_CTRL_LTRIGGER) {
                        if (current_page > 0) {
                            current_page--;
                            results_selection = current_page * ITEMS_PER_PAGE;
                        }
                    }
                    if (pressed_buttons & SCE_CTRL_RTRIGGER) {
                        int total_pages = (num_results + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
                        if (current_page < total_pages - 1) {
                            current_page++;
                            results_selection = current_page * ITEMS_PER_PAGE;
                        }
                    }
                    if (pressed_buttons & SCE_CTRL_CIRCLE) {
                        app_state = STATE_MAIN_MENU;
                    }
                    if (pressed_buttons & SCE_CTRL_CROSS) {
                        sceKernelLockMutex(g_progress_mutex, 1, NULL);
                        g_progress.selected_torrent = results[results_selection];
                        g_progress.is_running = 1;
                        g_progress.is_done = 0;
                        g_progress.is_cancelled = 0;
                        g_progress.progress_percent = 0.0f;
                        g_progress.error_message[0] = '\0';
                        g_progress.final_filepath[0] = '\0';
                        sceKernelUnlockMutex(g_progress_mutex, 1);
                        
                        g_worker_thread_id = sceKernelCreateThread("download_worker", download_worker_thread, 0x10000100, 0x10000, 0, 0, NULL);
                        sceKernelStartThread(g_worker_thread_id, 0, NULL);
                        
                        app_state = STATE_DOWNLOADING;
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
            case STATE_DOWNLOADING: {
                draw_progress_screen();

                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    sceKernelLockMutex(g_progress_mutex, 1, NULL);
                    g_progress.is_cancelled = 1;
                    sceKernelUnlockMutex(g_progress_mutex, 1);
                }

                sceKernelLockMutex(g_progress_mutex, 1, NULL);
                int is_done = g_progress.is_done;
                int is_running = g_progress.is_running;
                int was_cancelled = g_progress.is_cancelled;
                char final_path[512];
                snprintf(final_path, sizeof(final_path), "%s", g_progress.final_filepath);
                sceKernelUnlockMutex(g_progress_mutex, 1);

                if (is_done) {
                    player_play(final_path);
                    app_state = STATE_SHOW_RESULTS;
                } else if (!is_running) {
                    if (strlen(error_message) > 0) {
                         app_state = STATE_ERROR;
                    } else if (was_cancelled) {
                        app_state = STATE_SHOW_RESULTS;
                    }
                }
                break;
            }
            case STATE_ERROR: {
                vita2d_font_draw_text(font, 40, 272, COLOR_YELLOW, FONT_SIZE, "--- ERRO ---");
                vita2d_font_draw_text(font, 40, 300, COLOR_WHITE, FONT_SIZE, error_message);
                vita2d_font_draw_text(font, 40, 330, COLOR_GREY, FONT_SIZE, "Pressione (O) para voltar.");
                if (pressed_buttons & SCE_CTRL_CIRCLE) {
                    error_message[0] = '\0'; // Limpa o erro ao voltar
                    app_state = STATE_SHOW_RESULTS;
                }
                break;
            }
        }
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

exit_loop:
    if (token_server_is_running()) {
        token_server_stop();
    }

    sceNetCtlTerm();
    sceNetTerm();
    
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    
    vita2d_free_font(font);
    vita2d_fini();
    sceKernelDeleteMutex(g_progress_mutex);
    sceKernelExitProcess(0);
    return 0;
}