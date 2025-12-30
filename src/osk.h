#ifndef OSK_H
#define OSK_H

#define MAX_INPUT_LEN 256

// Status do IME Dialog
typedef enum {
    OSK_STATUS_RUNNING,   // Diálogo ainda aberto
    OSK_STATUS_FINISHED,  // Usuário confirmou (Enter)
    OSK_STATUS_CANCELED,  // Usuário cancelou
    OSK_STATUS_ERROR      // Erro
} OskStatus;

/**
 * Inicializa e abre o IME Dialog nativo do PS Vita
 * @param title Título do diálogo (em UTF-8)
 * @param initial_text Texto inicial do campo de entrada (em UTF-8)
 */
void osk_init_ime(const char* title, const char* initial_text);

/**
 * Atualiza o estado do IME Dialog
 * @return Status atual do diálogo
 */
OskStatus osk_update_ime(void);

/**
 * Obtém o texto digitado pelo usuário (válido após OSK_STATUS_FINISHED)
 * @return Ponteiro para o texto em UTF-8
 */
char* osk_get_text(void);

#endif // OSK_H