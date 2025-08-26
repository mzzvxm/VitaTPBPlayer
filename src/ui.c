#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <vita2d.h>
#include "ui.h"

// ======= Tema / Layout =======
#define UI_W 960
#define UI_SCREEN_HEIGHT 544

#define COL_BG        RGBA8(18,  20,  22, 255)
#define COL_PANEL     RGBA8(28,  31,  34, 255)
#define COL_ACCENT    RGBA8(255, 205, 64, 255)   // amarelo
#define COL_TEXT      RGBA8(245, 245, 245, 255)
#define COL_MUTED     RGBA8(170, 170, 170, 255)
#define COL_OK        RGBA8(  0, 205, 120, 255)
#define COL_WARN      RGBA8(255, 170,  60, 255)

static const float FONT_BASE = 20.0f;
static const int   PAD_X     = 36;
static const int   ROW_H     = 40;   // altura de linha nas listas
static const int   HEADER_H  = 64;
static const int   FOOTER_H  = 28;

// ======= Estado interno =======
static vita2d_font *g_font = NULL;
static int g_ui_inited = 0;

// ======= Helpers =======
static void draw_header(const char *title, const char *subtitle) {
    vita2d_draw_rectangle(0, 0, UI_W, HEADER_H, COL_PANEL);
    float y = 24.0f;
    vita2d_font_draw_text(g_font, PAD_X, y, COL_TEXT, FONT_BASE + 6.0f, title);
    if (subtitle && subtitle[0]) {
        vita2d_font_draw_text(g_font, PAD_X, y + 24.0f, COL_MUTED, FONT_BASE - 2.0f, subtitle);
    }
}

static void draw_footer_hint(const char *hint) {
    vita2d_draw_rectangle(0, UI_H - FOOTER_H, UI_W, FOOTER_H, COL_PANEL);
    vita2d_font_draw_text(g_font, PAD_X, UI_H - FOOTER_H + 19, COL_MUTED, FONT_BASE - 4.0f, hint);
}

static void draw_list_item(int index, int is_selected, const char *title, const char *subtitle) {
    int y = HEADER_H + 12 + index * ROW_H;
    unsigned int bg = is_selected ? RGBA8(60, 60, 60, 255) : 0;
    if (is_selected) {
        vita2d_draw_rectangle(PAD_X - 4, y - 4, UI_W - PAD_X*2 + 8, ROW_H, bg);
        vita2d_draw_rectangle(PAD_X - 8, y - 4, 4, ROW_H, COL_ACCENT); // régua lateral
    }
    vita2d_font_draw_text(g_font, PAD_X, y + 16, is_selected ? COL_ACCENT : COL_TEXT, FONT_BASE, title ? title : "");
    if (subtitle && subtitle[0]) {
        vita2d_font_draw_text(g_font, PAD_X, y + 16 + 18, COL_MUTED, FONT_BASE - 4.0f, subtitle);
    }
}

static void draw_progress_bar(float x, float y, float w, float h, float pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    vita2d_draw_rectangle(x, y, w, h, RGBA8(60, 60, 60, 255));
    vita2d_draw_rectangle(x, y, (w * pct) / 100.0f, h, COL_OK);
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
}

void ui_end_frame(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

// ======= Telas =======
void ui_draw_main_menu(int selection) {
    draw_header("VitaTPBPlayer", "Busca + Debrid + Player no Vita");
    const char *items[] = {"Buscar Torrents", "Configuracoes", "Sair"};
    for (int i = 0; i < 3; ++i) {
        draw_list_item(i, selection == i, items[i], NULL);
    }
    draw_footer_hint("CIMA/BAIXO   (X) Selecionar   (Toque) Tocar");
}

void ui_draw_settings_screen(
    int selection,
    const RdUserInfo *user_info,
    const char *masked_token,
    const char *status_line
) {
    draw_header("Configuracoes", "Token, conta e recebimento via Wi-Fi");

    // Token
    char line[96] = "Token Real-Debrid: ";
    strncat(line, (masked_token && masked_token[0]) ? masked_token : "Nao configurado", sizeof(line)-1);
    draw_list_item(0, selection == 0, "Editar Token Manualmente", line);
    draw_list_item(1, selection == 1, "Testar Token", (status_line && status_line[0]) ? status_line : "Verifica usuario/premium");
    draw_list_item(2, selection == 2, "Receber Token via Wi-Fi", "Abrir servidor local na mesma rede");
    draw_list_item(3, selection == 3, "Salvar e Voltar", NULL);

    // Info do usuário (se existir)
    if (user_info) {
        int y = HEADER_H + 12 + 5 * ROW_H + 8;
        vita2d_font_draw_textf(g_font, PAD_X, y, COL_TEXT, FONT_BASE,
                               "Usuario: %s", user_info->username[0] ? user_info->username : "-");
        vita2d_font_draw_textf(g_font, PAD_X, y + 22, COL_TEXT,   FONT_BASE,
                               "Email: %s", user_info->email[0] ? user_info->email : "-");
        vita2d_font_draw_textf(g_font, PAD_X, y + 44, user_info->is_premium ? COL_OK : COL_WARN, FONT_BASE,
                               "Status: %s%s%s",
                               user_info->is_premium ? "Premium" : "Gratuito",
                               user_info->expiration[0] ? " (Expira: " : "",
                               user_info->expiration[0] ? user_info->expiration : "");
        if (user_info->expiration[0]) {
            vita2d_font_draw_text(g_font, PAD_X + 300, y + 44, user_info->is_premium ? COL_OK : COL_WARN, FONT_BASE, ")");
        }
    }

    draw_footer_hint("(X) Selecionar    (O) Voltar");
}

void ui_draw_token_server_screen(const char *ip_addr) {
    draw_header("Receber Token via Wi-Fi", NULL);
    vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 28, COL_MUTED, FONT_BASE, "1) Conecte seu celular/PC na mesma rede.");
    vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 56, COL_MUTED, FONT_BASE, "2) Acesse no navegador:");
    vita2d_font_draw_textf(g_font, PAD_X, HEADER_H + 88, COL_ACCENT, FONT_BASE + 2.0f,
                           "http://%s:8080", ip_addr ? ip_addr : "0.0.0.0");
    vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 120, COL_MUTED, FONT_BASE, "3) Cole o token e clique em Salvar.");
    draw_footer_hint("(O) Cancelar/Voltar");
}

void ui_draw_results(
    const char *query,
    const TpbResult *results, int num_results,
    int selection, int current_page, int items_per_page
) {
    char subtitle[128];
    int total_pages = (num_results + items_per_page - 1) / items_per_page;
    if (total_pages <= 0) total_pages = 1;
    snprintf(subtitle, sizeof(subtitle), "Resultados para \"%s\"  —  Pagina %d/%d",
             query ? query : "", current_page + 1, total_pages);
    draw_header("Busca de Torrents", subtitle);

    if (num_results <= 0) {
        vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 40, COL_TEXT, FONT_BASE, "Nenhum resultado encontrado.");
    } else {
        int start = current_page * items_per_page;
        int end   = start + items_per_page;
        if (end > num_results) end = num_results;

        int row = 0;
        for (int i = start; i < end; ++i, ++row) {
            char sub[96];
            snprintf(sub, sizeof(sub), "Tamanho: %s  |  Seeders: %d", results[i].size, results[i].seeders);
            draw_list_item(row, selection == i, results[i].name, sub);
        }
    }
    draw_footer_hint("(X) Baixar/Selecionar   (O) Voltar   (L/R) Pagina   (△) Nova Busca");
}

void ui_draw_file_selection(
    const RdFileInfo *files, int num_files,
    int selection, int current_page, int items_per_page
) {
    char subtitle[96];
    int total_pages = (num_files + items_per_page - 1) / items_per_page;
    if (total_pages <= 0) total_pages = 1;
    snprintf(subtitle, sizeof(subtitle), "Selecione o arquivo  —  Pagina %d/%d",
             current_page + 1, total_pages);
    draw_header("Arquivos do Torrent", subtitle);

    if (num_files <= 0) {
        vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 40, COL_TEXT, FONT_BASE, "Nenhum arquivo listado.");
    } else {
        int start = current_page * items_per_page;
        int end   = start + items_per_page;
        if (end > num_files) end = num_files;

        int row = 0;
        for (int i = start; i < end; ++i, ++row) {
            const char *p = files[i].path;
            // título: só o nome do arquivo (se tiver /)
            const char *slash = strrchr(p, '/');
            const char *fname = slash ? slash + 1 : p;

            char sub[96];
            if (files[i].bytes > 0) {
                double mb = (double)files[i].bytes / (1024.0 * 1024.0);
                snprintf(sub, sizeof(sub), "%.1f MB", mb);
            } else {
                sub[0] = '\0';
            }
            draw_list_item(row, selection == i, fname, sub);
        }
    }
    draw_footer_hint("(X) Confirmar   (O) Voltar   (L/R) Pagina");
}

void ui_draw_progress(const UiProgress *p) {
    draw_header("Progresso do Download", NULL);
    if (p) {
        vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 40, COL_MUTED, FONT_BASE, p->status);
        draw_progress_bar(PAD_X, HEADER_H + 76, UI_W - PAD_X*2, 30, p->percent);
    }
    draw_footer_hint("(O) Cancelar");
}

void ui_draw_status(const char *title, const char *message) {
    draw_header(title ? title : "", NULL);
    vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 40, COL_MUTED, FONT_BASE, message ? message : "");
}

void ui_draw_error(const char *message) {
    draw_header("--- ERRO ---", NULL);
    vita2d_font_draw_text(g_font, PAD_X, HEADER_H + 40, COL_ACCENT, FONT_BASE, message ? message : "");
    draw_footer_hint("(O) Voltar");
}
