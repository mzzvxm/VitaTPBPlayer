#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <vita2d.h>
#include "ui.h"
#include "i18n.h"

// ======= Tema Moderno - Fundo Preto =======
#define UI_W 960
#define UI_H 544

// Cores do tema dark elegante
#define COL_BG           RGBA8(0,   0,   0,   255)  // Preto puro
#define COL_CARD         RGBA8(18,  18,  18,  255)  // Cinza muito escuro
#define COL_CARD_HOVER   RGBA8(28,  28,  28,  255)  // Cinza escuro (hover)
#define COL_ACCENT       RGBA8(255, 255, 255, 255)  // Branco puro (accent)
#define COL_TEXT         RGBA8(255, 255, 255, 255)  // Branco
#define COL_TEXT_DIM     RGBA8(160, 160, 160, 255)  // Cinza médio
#define COL_TEXT_MUTED   RGBA8(100, 100, 100, 255)  // Cinza escuro
#define COL_OK           RGBA8(76,  175, 80,  255)  // Verde
#define COL_WARN         RGBA8(255, 152, 0,   255)  // Laranja

// Layout moderno
static const float FONT_TITLE    = 32.0f;
static const float FONT_SUBTITLE = 18.0f;
static const float FONT_NORMAL   = 20.0f;
static const float FONT_SMALL    = 16.0f;

static const int CENTER_X = UI_W / 2;
static const int CENTER_Y = UI_H / 2;

// Dimensões dos botões do menu (centralizados)
#define MENU_BUTTON_W 400
#define MENU_BUTTON_H 60
#define MENU_BUTTON_SPACING 20
#define MENU_START_Y 200

// ======= Estado =======
static vita2d_font *g_font = NULL;
static int g_ui_inited = 0;

// ======= Touch Helper =======
typedef struct {
    int x, y, w, h;
    int index;
} TouchArea;

static TouchArea g_touch_areas[32];
static int g_touch_area_count = 0;

void ui_clear_touch_areas(void) {
    g_touch_area_count = 0;
}

void ui_add_touch_area(int x, int y, int w, int h, int index) {
    if (g_touch_area_count < 32) {
        g_touch_areas[g_touch_area_count].x = x;
        g_touch_areas[g_touch_area_count].y = y;
        g_touch_areas[g_touch_area_count].w = w;
        g_touch_areas[g_touch_area_count].h = h;
        g_touch_areas[g_touch_area_count].index = index;
        g_touch_area_count++;
    }
}

int ui_check_touch(int touch_x, int touch_y) {
    for (int i = 0; i < g_touch_area_count; i++) {
        TouchArea *area = &g_touch_areas[i];
        if (touch_x >= area->x && touch_x <= area->x + area->w &&
            touch_y >= area->y && touch_y <= area->y + area->h) {
            return area->index;
        }
    }
    return -1;
}

// ======= Helpers de Desenho =======
static void draw_centered_text(float y, unsigned int color, float size, const char *text) {
    int text_width = vita2d_font_text_width(g_font, size, text);
    vita2d_font_draw_text(g_font, CENTER_X - text_width / 2, y, color, size, text);
}

static void draw_card(int x, int y, int w, int h, int is_selected) {
    unsigned int bg_color = is_selected ? COL_CARD_HOVER : COL_CARD;
    vita2d_draw_rectangle(x, y, w, h, bg_color);
    
    // Borda branca se selecionado
    if (is_selected) {
        vita2d_draw_rectangle(x, y, w, 3, COL_ACCENT);
    }
}

static void draw_centered_card_button(int y, int w, int h, int is_selected, const char *text) {
    int x = CENTER_X - w / 2;
    draw_card(x, y, w, h, is_selected);
    
    int text_width = vita2d_font_text_width(g_font, FONT_NORMAL, text);
    int text_x = CENTER_X - text_width / 2;
    int text_y = y + h / 2 + 7;
    
    vita2d_font_draw_text(g_font, text_x, text_y, COL_TEXT, FONT_NORMAL, text);
    
    // Adiciona área de toque
    ui_add_touch_area(x, y, w, h, -1);
}

static void draw_footer(const char *hint) {
    int y = UI_H - 40;
    vita2d_font_draw_text(g_font, 40, y, COL_TEXT_MUTED, FONT_SMALL, hint);
}

// ======= API =======
int ui_init(const char *font_app0_path, const char *font_fallback_path) {
    if (g_ui_inited) return 1;
    if (vita2d_init() < 0) return 0;

    vita2d_set_clear_color(COL_BG);

    g_font = vita2d_load_font_file(font_app0_path);
    if (!g_font && font_fallback_path) {
        g_font = vita2d_load_font_file(font_fallback_path);
    }
    if (!g_font) {
        vita2d_fini();
        return 0;
    }
    
    i18n_init();
    g_ui_inited = 1;
    return 1;
}

void ui_shutdown(void) {
    if (!g_ui_inited) return;
    if (g_font) vita2d_free_font(g_font);
    vita2d_fini();
    g_font = NULL;
    g_ui_inited = 0;
}

vita2d_font* ui_font(void) {
    return g_font;
}

void ui_begin_frame(void) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    ui_clear_touch_areas();
}

void ui_end_frame(void) {
    vita2d_end_drawing();
    vita2d_common_dialog_update();
    vita2d_swap_buffers();
}

// ======= Telas =======
void ui_draw_main_menu(int selection) {
    // Título centralizado no topo
    draw_centered_text(100, COL_TEXT, FONT_TITLE, i18n_get(STR_APP_TITLE));
    draw_centered_text(140, COL_TEXT_DIM, FONT_SUBTITLE, i18n_get(STR_APP_SUBTITLE));
    
    // Botões centralizados
    const char *labels[] = {
        i18n_get(STR_MENU_SEARCH),
        i18n_get(STR_MENU_SETTINGS),
        i18n_get(STR_MENU_EXIT)
    };
    
    for (int i = 0; i < 3; i++) {
        int y = MENU_START_Y + i * (MENU_BUTTON_H + MENU_BUTTON_SPACING);
        draw_centered_card_button(y, MENU_BUTTON_W, MENU_BUTTON_H, selection == i, labels[i]);
        
        // Área de toque
        int x = CENTER_X - MENU_BUTTON_W / 2;
        g_touch_areas[g_touch_area_count - 1].index = i;
    }
    
    // Footer com hints
    char hint[128];
    snprintf(hint, sizeof(hint), "%s   %s   %s", 
             i18n_get(STR_HINT_NAVIGATE),
             i18n_get(STR_HINT_SELECT),
             i18n_get(STR_HINT_TOUCH));
    draw_footer(hint);
}

void ui_draw_settings_screen(
    int selection,
    const RdUserInfo *user_info,
    const char *masked_token,
    const char *status_line
) {
    // Header
    vita2d_font_draw_text(g_font, 40, 50, COL_TEXT, FONT_TITLE, i18n_get(STR_SETTINGS_TITLE));
    vita2d_font_draw_text(g_font, 40, 80, COL_TEXT_DIM, FONT_SUBTITLE, i18n_get(STR_SETTINGS_SUBTITLE));
    
    // Lista de opções
    const char *options[] = {
        i18n_get(STR_SETTINGS_TOKEN_EDIT),
        i18n_get(STR_SETTINGS_TOKEN_TEST),
        i18n_get(STR_SETTINGS_TOKEN_WIFI),
        i18n_get(STR_SETTINGS_LANGUAGE),
        i18n_get(STR_SETTINGS_SAVE)
    };
    
    int start_y = 120;
    int item_h = 50;
    
    for (int i = 0; i < 5; i++) {
        int y = start_y + i * (item_h + 10);
        int is_selected = (selection == i);
        
        draw_card(40, y, UI_W - 80, item_h, is_selected);
        vita2d_font_draw_text(g_font, 60, y + 30, COL_TEXT, FONT_NORMAL, options[i]);
        
        // Subtítulo para o idioma
        if (i == 3) {
            const char *lang_name = i18n_get_language_name(i18n_get_language());
            int text_w = vita2d_font_text_width(g_font, FONT_SMALL, lang_name);
            vita2d_font_draw_text(g_font, UI_W - 100 - text_w, y + 30, COL_TEXT_DIM, FONT_SMALL, lang_name);
        }
        
        ui_add_touch_area(40, y, UI_W - 80, item_h, i);
    }
    
    // Info do usuário (se houver)
    if (user_info) {
        int info_y = start_y + 6 * (item_h + 10);
        vita2d_font_draw_textf(g_font, 60, info_y, COL_TEXT_DIM, FONT_SMALL,
                               "%s %s", i18n_get(STR_USER_INFO), 
                               user_info->username[0] ? user_info->username : "-");
        vita2d_font_draw_textf(g_font, 60, info_y + 22, COL_TEXT_DIM, FONT_SMALL,
                               "%s %s", i18n_get(STR_USER_EMAIL),
                               user_info->email[0] ? user_info->email : "-");
        
        unsigned int status_color = user_info->is_premium ? COL_OK : COL_WARN;
        const char *status_text = user_info->is_premium ? 
            i18n_get(STR_USER_PREMIUM) : i18n_get(STR_USER_FREE);
        vita2d_font_draw_textf(g_font, 60, info_y + 44, status_color, FONT_SMALL,
                               "%s %s", i18n_get(STR_USER_STATUS), status_text);
    }
    
    char hint[128];
    snprintf(hint, sizeof(hint), "%s   %s", i18n_get(STR_HINT_SELECT), i18n_get(STR_HINT_BACK));
    draw_footer(hint);
}

void ui_draw_token_server_screen(const char *ip_addr) {
    vita2d_font_draw_text(g_font, 40, 50, COL_TEXT, FONT_TITLE, i18n_get(STR_WIFI_TITLE));
    
    vita2d_font_draw_text(g_font, 60, 120, COL_TEXT_DIM, FONT_NORMAL, i18n_get(STR_WIFI_STEP1));
    vita2d_font_draw_text(g_font, 60, 160, COL_TEXT_DIM, FONT_NORMAL, i18n_get(STR_WIFI_STEP2));
    
    char url[128];
    snprintf(url, sizeof(url), "http://%s:8080", ip_addr ? ip_addr : "0.0.0.0");
    vita2d_font_draw_text(g_font, 80, 200, COL_ACCENT, FONT_NORMAL + 2, url);
    
    vita2d_font_draw_text(g_font, 60, 240, COL_TEXT_DIM, FONT_NORMAL, i18n_get(STR_WIFI_STEP3));
    
    draw_footer(i18n_get(STR_HINT_BACK));
}

void ui_draw_results(
    const char *query,
    const TpbResult *results, int num_results,
    int selection, int current_page, int items_per_page
) {
    char title[256];
    snprintf(title, sizeof(title), "%s: \"%s\"", i18n_get(STR_SEARCH_RESULTS_FOR), query ? query : "");
    vita2d_font_draw_text(g_font, 40, 50, COL_TEXT, FONT_TITLE, title);
    
    int total_pages = (num_results + items_per_page - 1) / items_per_page;
    if (total_pages <= 0) total_pages = 1;
    
    char page_info[64];
    snprintf(page_info, sizeof(page_info), "%s %d/%d", i18n_get(STR_SEARCH_PAGE), current_page + 1, total_pages);
    vita2d_font_draw_text(g_font, 40, 80, COL_TEXT_DIM, FONT_SUBTITLE, page_info);
    
    if (num_results <= 0) {
        vita2d_font_draw_text(g_font, 60, 150, COL_TEXT_DIM, FONT_NORMAL, i18n_get(STR_SEARCH_NO_RESULTS));
    } else {
        int start = current_page * items_per_page;
        int end = start + items_per_page;
        if (end > num_results) end = num_results;
        
        int y = 120;
        int item_h = 70;
        
        for (int i = start; i < end; i++) {
            int is_selected = (selection == i);
            
            draw_card(40, y, UI_W - 80, item_h, is_selected);
            
            // Nome do torrent
            vita2d_font_draw_text(g_font, 60, y + 25, COL_TEXT, FONT_NORMAL, results[i].name);
            
            // Info (tamanho e seeders)
            char info[128];
            snprintf(info, sizeof(info), "%s %s  |  %s %d", 
                     i18n_get(STR_SIZE), results[i].size,
                     i18n_get(STR_SEEDERS), results[i].seeders);
            vita2d_font_draw_text(g_font, 60, y + 50, COL_TEXT_DIM, FONT_SMALL, info);
            
            ui_add_touch_area(40, y, UI_W - 80, item_h, i);
            y += item_h + 10;
        }
    }
    
    char hint[256];
    snprintf(hint, sizeof(hint), "%s   %s   %s   %s", 
             i18n_get(STR_HINT_SELECT), i18n_get(STR_HINT_BACK),
             i18n_get(STR_HINT_PAGE), i18n_get(STR_HINT_NEW_SEARCH));
    draw_footer(hint);
}

void ui_draw_file_selection(
    const RdFileInfo *files, int num_files,
    int selection, int current_page, int items_per_page
) {
    vita2d_font_draw_text(g_font, 40, 50, COL_TEXT, FONT_TITLE, i18n_get(STR_DOWNLOAD_SELECT_FILE));
    
    int total_pages = (num_files + items_per_page - 1) / items_per_page;
    if (total_pages <= 0) total_pages = 1;
    
    char page_info[64];
    snprintf(page_info, sizeof(page_info), "%s %d/%d", i18n_get(STR_SEARCH_PAGE), current_page + 1, total_pages);
    vita2d_font_draw_text(g_font, 40, 80, COL_TEXT_DIM, FONT_SUBTITLE, page_info);
    
    if (num_files <= 0) {
        vita2d_font_draw_text(g_font, 60, 150, COL_TEXT_DIM, FONT_NORMAL, i18n_get(STR_SEARCH_NO_RESULTS));
    } else {
        int start = current_page * items_per_page;
        int end = start + items_per_page;
        if (end > num_files) end = num_files;
        
        int y = 120;
        int item_h = 60;
        
        for (int i = start; i < end; i++) {
            int is_selected = (selection == i);
            
            draw_card(40, y, UI_W - 80, item_h, is_selected);
            
            const char *p = files[i].path;
            const char *slash = strrchr(p, '/');
            const char *fname = slash ? slash + 1 : p;
            
            vita2d_font_draw_text(g_font, 60, y + 25, COL_TEXT, FONT_NORMAL, fname);
            
            if (files[i].bytes > 0) {
                double mb = (double)files[i].bytes / (1024.0 * 1024.0);
                char size_info[64];
                snprintf(size_info, sizeof(size_info), "%.1f %s", mb, i18n_get(STR_MB));
                vita2d_font_draw_text(g_font, 60, y + 45, COL_TEXT_DIM, FONT_SMALL, size_info);
            }
            
            ui_add_touch_area(40, y, UI_W - 80, item_h, i);
            y += item_h + 10;
        }
    }
    
    char hint[128];
    snprintf(hint, sizeof(hint), "%s   %s   %s", 
             i18n_get(STR_HINT_CONFIRM), i18n_get(STR_HINT_BACK), i18n_get(STR_HINT_PAGE));
    draw_footer(hint);
}

void ui_draw_progress(const UiProgress *p) {
    vita2d_font_draw_text(g_font, 40, 50, COL_TEXT, FONT_TITLE, i18n_get(STR_DOWNLOAD_PROGRESS));
    
    if (p) {
        vita2d_font_draw_text(g_font, 60, 120, COL_TEXT_DIM, FONT_NORMAL, p->status);
        
        // Barra de progresso moderna
        int bar_x = 60;
        int bar_y = 180;
        int bar_w = UI_W - 120;
        int bar_h = 20;
        
        // Fundo da barra
        vita2d_draw_rectangle(bar_x, bar_y, bar_w, bar_h, COL_CARD);
        
        // Progresso
        float pct = p->percent;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        vita2d_draw_rectangle(bar_x, bar_y, (bar_w * pct) / 100.0f, bar_h, COL_ACCENT);
        
        // Porcentagem
        char pct_text[16];
        snprintf(pct_text, sizeof(pct_text), "%.1f%%", pct);
        int text_w = vita2d_font_text_width(g_font, FONT_NORMAL, pct_text);
        vita2d_font_draw_text(g_font, CENTER_X - text_w / 2, bar_y + 50, COL_TEXT, FONT_NORMAL, pct_text);
    }
    
    draw_footer(i18n_get(STR_HINT_CANCEL));
}

void ui_draw_status(const char *title, const char *message) {
    vita2d_font_draw_text(g_font, 40, 50, COL_TEXT, FONT_TITLE, title ? title : i18n_get(STR_STATUS));
    vita2d_font_draw_text(g_font, 60, 120, COL_TEXT_DIM, FONT_NORMAL, message ? message : "");
}

void ui_draw_error(const char *message) {
    vita2d_font_draw_text(g_font, 40, 50, COL_WARN, FONT_TITLE, i18n_get(STR_ERROR));
    vita2d_font_draw_text(g_font, 60, 120, COL_TEXT, FONT_NORMAL, message ? message : "");
    draw_footer(i18n_get(STR_HINT_BACK));
}