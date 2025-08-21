#ifndef OSK_H
#define OSK_H

#include <psp2/ctrl.h>
#include <vita2d.h>

#define OSK_INPUT_MAX_LENGTH 64

void osk_init(char *initial_text);
int osk_update(SceCtrlData *pad);
void osk_draw(vita2d_font *font, char *buffer);

#endif // OSK_H