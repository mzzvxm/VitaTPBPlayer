#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "osk.h"

#define OSK_FONT_SIZE 24.0f
#define OSK_KEY_SPACING_X 38
#define OSK_KEY_SPACING_Y 38

static int cursor_x = 0;
static int cursor_y = 0;
static unsigned int old_buttons = 0;
static char* text_buffer;
static int is_caps = 0;

static const char* keyboard_layout_lower[] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm,.-_",
    " "
};

static const char* keyboard_layout_upper[] = {
    "!@#$%^&*()",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM,.-_",
    " "
};

static const int layout_rows = 5;

void osk_init(char* initial_text) {
    cursor_x = 0;
    cursor_y = 0;
    old_buttons = 0;
    text_buffer = initial_text;
    is_caps = 0;
}

int osk_update(SceCtrlData *pad) {
    unsigned int pressed_buttons = pad->buttons & ~old_buttons;
    old_buttons = pad->buttons;

    // MODIFICADO: Gatilho R para alternar entre maiúsculas e minúsculas
    if (pressed_buttons & SCE_CTRL_RTRIGGER) {
        is_caps = !is_caps;
    }

    const char** current_layout = is_caps ? keyboard_layout_upper : keyboard_layout_lower;

    if (pressed_buttons & SCE_CTRL_UP) {
        if (cursor_y > 0) cursor_y--;
        if (cursor_x >= strlen(current_layout[cursor_y])) {
            cursor_x = strlen(current_layout[cursor_y]) - 1;
        }
    }
    if (pressed_buttons & SCE_CTRL_DOWN) {
        if (cursor_y < layout_rows - 1) cursor_y++;
        if (cursor_x >= strlen(current_layout[cursor_y])) {
            cursor_x = strlen(current_layout[cursor_y]) - 1;
        }
    }
    if (pressed_buttons & SCE_CTRL_LEFT) {
        if (cursor_x > 0) cursor_x--;
    }
    if (pressed_buttons & SCE_CTRL_RIGHT) {
        if (cursor_x < strlen(current_layout[cursor_y]) - 1) cursor_x++;
    }

    if (pressed_buttons & SCE_CTRL_CROSS) {
        size_t len = strlen(text_buffer);
        if (len < OSK_INPUT_MAX_LENGTH - 1) {
            text_buffer[len] = current_layout[cursor_y][cursor_x];
            text_buffer[len + 1] = '\0';
        }
    }

    if (pressed_buttons & SCE_CTRL_CIRCLE) {
        size_t len = strlen(text_buffer);
        if (len > 0) {
            text_buffer[len - 1] = '\0';
        }
    }

    if (pressed_buttons & SCE_CTRL_START) {
        return 1;
    }

    return 0;
}

void osk_draw(vita2d_font *font, char *buffer) {
    vita2d_font_draw_text(font, 40, 40, RGBA8(255, 255, 255, 255), 20.0f, "Digite o texto:");
    vita2d_font_draw_textf(font, 40, 80, RGBA8(255, 255, 255, 255), 20.0f, "[%s_]", buffer);

    const char** current_layout = is_caps ? keyboard_layout_upper : keyboard_layout_lower;

    int y_pos = 180;
    for (int y = 0; y < layout_rows; y++) {
        int x_pos = (960 - (strlen(current_layout[y]) * OSK_KEY_SPACING_X)) / 2;

        for (int x = 0; x < strlen(current_layout[y]); x++) {
            unsigned int color = (y == cursor_y && x == cursor_x) ? RGBA8(255, 255, 0, 255) : RGBA8(255, 255, 255, 255);

            if (y == 4) {
                 vita2d_draw_rectangle(x_pos, y_pos - 20, OSK_KEY_SPACING_X * 5, 30, color);
            } else {
                 vita2d_font_draw_textf(font, x_pos, y_pos, color, OSK_FONT_SIZE, "%c", current_layout[y][x]);
            }
            x_pos += OSK_KEY_SPACING_X;
        }
        y_pos += OSK_KEY_SPACING_Y;
    }

    // MODIFICADO: Texto de ajuda atualizado para mostrar (R) Caps
    vita2d_font_draw_text(font, 40, 480, RGBA8(180, 180, 180, 255), 18.0f, "(X) Adicionar | (O) Apagar | (R) Caps | (START) Confirmar");
}