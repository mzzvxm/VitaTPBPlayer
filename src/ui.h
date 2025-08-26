#ifndef UI_H
#define UI_H 544

#include <vita2d.h>
#include "tpb_scraper.h"
#include "realdebrid.h"

// Mini snapshot de progresso para evitar tocar em mutex aqui
typedef struct {
    float percent;              // 0..100
    char  status[256];          // mensagem de status
} UiProgress;

// Inicializa vida2d, fonte e tema. Retorna 1 OK / 0 erro.
int  ui_init(const char *font_app0_path, const char *font_fallback_path);

// Finaliza font/vida2d
void ui_shutdown(void);

// Início/fim de frame. (limpa fundo e troca buffers)
void ui_begin_frame(void);
void ui_end_frame(void);

// Acesso à fonte interna (útil para o seu OSK custom)
vita2d_font* ui_font(void);

// Telas
void ui_draw_main_menu(int selection);

void ui_draw_settings_screen(
    int selection,
    const RdUserInfo *user_info,         // pode ser NULL
    const char *masked_token,            // string já mascarada
    const char *status_line              // dicas/resultado de teste
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
