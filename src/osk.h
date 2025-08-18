#ifndef OSK_H
#define OSK_H

#include <psp2/ctrl.h>

#define OSK_INPUT_MAX_LENGTH 64

// Inicializa o estado do teclado
void osk_init(char *initial_text);

// Processa a entrada do controle para o teclado
// Retorna 1 se o usuário confirmou a entrada (START), 0 caso contrário
int osk_update(SceCtrlData *pad);

// Desenha o teclado na tela
void osk_draw(char *buffer);

#endif // OSK_H