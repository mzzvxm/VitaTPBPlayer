#include <stdio.h>
#include <string.h>
#include "debugScreen.h"
#include "osk.h"

static int cursor_x = 0;
static int cursor_y = 0;
static unsigned int old_buttons = 0;
static char* text_buffer;

static const char* keyboard_layout[] = {
    "1234567890-=",
    "qwertyuiop[]",
    "asdfghjkl;'",
    "zxcvbnm,./",
    " " // Barra de espaço
};
static const int layout_rows = 5;

void osk_init(char* initial_text) {
    cursor_x = 0;
    cursor_y = 0;
    old_buttons = 0;
    text_buffer = initial_text;
}

int osk_update(SceCtrlData *pad) {
    unsigned int pressed_buttons = pad->buttons & ~old_buttons;
    old_buttons = pad->buttons;

    if (pressed_buttons & SCE_CTRL_UP) {
        if (cursor_y > 0) cursor_y--;
        if (cursor_x >= strlen(keyboard_layout[cursor_y])) {
            cursor_x = strlen(keyboard_layout[cursor_y]) - 1;
        }
    }
    if (pressed_buttons & SCE_CTRL_DOWN) {
        if (cursor_y < layout_rows - 1) cursor_y++;
        if (cursor_x >= strlen(keyboard_layout[cursor_y])) {
            cursor_x = strlen(keyboard_layout[cursor_y]) - 1;
        }
    }
    if (pressed_buttons & SCE_CTRL_LEFT) {
        if (cursor_x > 0) cursor_x--;
    }
    if (pressed_buttons & SCE_CTRL_RIGHT) {
        if (cursor_x < strlen(keyboard_layout[cursor_y]) - 1) cursor_x++;
    }

    if (pressed_buttons & SCE_CTRL_CROSS) {
        size_t len = strlen(text_buffer);
        if (len < OSK_INPUT_MAX_LENGTH - 1) {
            text_buffer[len] = keyboard_layout[cursor_y][cursor_x];
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
        return 1; // Confirmação de entrada
    }
    
    return 0; // Continua no teclado
}

void osk_draw(char* buffer) {
    psvDebugScreenPrintf("Digite sua busca:\n");
    psvDebugScreenPrintf("----------------------------------\n");
    psvDebugScreenPrintf("[%s_]\n", buffer);
    psvDebugScreenPrintf("----------------------------------\n\n");

    for (int y = 0; y < layout_rows; y++) {
        for (int x = 0; x < strlen(keyboard_layout[y]); x++) {
            if (y == cursor_y && x == cursor_x) {
                psvDebugScreenPrintf("[%c]", keyboard_layout[y][x]);
            } else {
                psvDebugScreenPrintf(" %c ", keyboard_layout[y][x]);
            }
        }
        psvDebugScreenPrintf("\n");
    }

    psvDebugScreenPrintf("\n\n(X) Adicionar | (O) Apagar | (START) Buscar\n");
}