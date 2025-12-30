#ifndef UI_H
#define UI_H

#include <vita2d.h>
#include "tpb_scraper.h"
#include "realdebrid.h"

#define UI_W 960
#define UI_H 544

// Progresso de download
typedef struct {
    float percent;              // 0..100
    char  status[256];          // mensagem de status
} UiProgress;

// Inicializa vita2d, fonte e tema. Retorna 1 OK / 0 erro.
int  ui_init(const char *font_app0_path, const char *font_fallback_path);

// Finaliza font/vita2d
void ui_shutdown(void);

// Início/fim de frame
void ui_begin_frame(void);
void ui_end_frame(void);

// Acesso à fonte interna
vita2d_font* ui_font(void);

// ======= Sistema de Touch =======
// Limpa áreas de toque (chamar no início do frame)
void ui_clear_touch_areas(void);

// Adiciona uma área tocável (x, y, width, height, index)
void ui_add_touch_area(int x, int y, int w, int h, int index);

// Verifica se um toque está em alguma área registrada
// Retorna o index da área ou -1 se não tocou em nada
int ui_check_touch(int touch_x, int touch_y);

// ======= Telas =======
void ui_draw_main_menu(int selection);

void ui_draw_settings_screen(
    int selection,
    const RdUserInfo *user_info,
    const char *masked_token,
    const char *status_line
);

void ui_draw_token_server_screen(const char *ip_addr);

void ui_draw_results(
    const char *query,
    const TpbResult *results, int num_results,
    int selection, int current_page, int items_per_page
);

void ui_draw_file_selection(
    const RdFileInfo *files, int num_files,
    int selection, int current_page, int items_per_page
);

void ui_draw_progress(const UiProgress *p);

void ui_draw_status(const char *title, const char *message);

void ui_draw_error(const char *message);

#endif // UI_H