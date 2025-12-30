#include <psp2/ime_dialog.h>
#include <psp2/common_dialog.h>
#include <psp2/types.h>
#include <string.h>
#include <wchar.h>
#include "osk.h"

// Buffer para o texto de entrada (UTF-16)
static uint16_t g_input_text[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
static uint16_t g_initial_text[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
static uint16_t g_title_text[SCE_IME_DIALOG_MAX_TITLE_LENGTH + 1];

// Buffer para resultado em UTF-8 (para retornar ao usuário)
static char g_result_text[MAX_INPUT_LEN];

// Flag para saber se o IME foi inicializado
static int g_ime_initialized = 0;

// Converte UTF-8 para UTF-16
static void utf8_to_utf16(const char* utf8, uint16_t* utf16, size_t max_len) {
    size_t i = 0;
    size_t j = 0;
    
    while (utf8[i] && j < max_len - 1) {
        unsigned char c = (unsigned char)utf8[i];
        
        if (c < 0x80) {
            // ASCII de 1 byte
            utf16[j++] = (uint16_t)c;
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            // UTF-8 de 2 bytes
            if (utf8[i + 1]) {
                utf16[j++] = (uint16_t)(((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F));
                i += 2;
            } else {
                break;
            }
        } else if ((c & 0xF0) == 0xE0) {
            // UTF-8 de 3 bytes
            if (utf8[i + 1] && utf8[i + 2]) {
                utf16[j++] = (uint16_t)(((c & 0x0F) << 12) | 
                                        ((utf8[i + 1] & 0x3F) << 6) | 
                                        (utf8[i + 2] & 0x3F));
                i += 3;
            } else {
                break;
            }
        } else {
            // Ignora caracteres UTF-8 de 4+ bytes (não suportados pelo Vita)
            i++;
        }
    }
    utf16[j] = 0;
}

// Converte UTF-16 para UTF-8
static void utf16_to_utf8(const uint16_t* utf16, char* utf8, size_t max_len) {
    size_t i = 0;
    size_t j = 0;
    
    while (utf16[i] && j < max_len - 4) {
        uint16_t c = utf16[i];
        
        if (c < 0x80) {
            // ASCII de 1 byte
            utf8[j++] = (char)c;
        } else if (c < 0x800) {
            // UTF-8 de 2 bytes
            utf8[j++] = (char)(0xC0 | (c >> 6));
            utf8[j++] = (char)(0x80 | (c & 0x3F));
        } else {
            // UTF-8 de 3 bytes
            utf8[j++] = (char)(0xE0 | (c >> 12));
            utf8[j++] = (char)(0x80 | ((c >> 6) & 0x3F));
            utf8[j++] = (char)(0x80 | (c & 0x3F));
        }
        i++;
    }
    utf8[j] = 0;
}

void osk_init_ime(const char* title, const char* initial_text) {
    // Se já foi inicializado, termina primeiro
    if (g_ime_initialized) {
        sceImeDialogTerm();
        g_ime_initialized = 0;
    }
    
    // Limpa os buffers
    memset(g_input_text, 0, sizeof(g_input_text));
    memset(g_initial_text, 0, sizeof(g_initial_text));
    memset(g_title_text, 0, sizeof(g_title_text));
    
    // Converte título e texto inicial de UTF-8 para UTF-16
    utf8_to_utf16(title, g_title_text, SCE_IME_DIALOG_MAX_TITLE_LENGTH);
    utf8_to_utf16(initial_text, g_initial_text, SCE_IME_DIALOG_MAX_TEXT_LENGTH);
    
    // Copia o texto inicial para o buffer de entrada
    memcpy(g_input_text, g_initial_text, sizeof(g_input_text));
    
    // Configura os parâmetros do IME Dialog
    SceImeDialogParam param;
    sceImeDialogParamInit(&param);
    
    param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH | 
                               SCE_IME_LANGUAGE_PORTUGUESE | 
                               SCE_IME_LANGUAGE_SPANISH |
                               SCE_IME_LANGUAGE_FRENCH |
                               SCE_IME_LANGUAGE_GERMAN;
    param.languagesForced = SCE_FALSE;
    param.type = SCE_IME_TYPE_DEFAULT;
    param.option = 0;
    param.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
    param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
    param.title = g_title_text;
    param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
    param.initialText = g_initial_text;
    param.inputTextBuffer = g_input_text;
    
    // Inicializa o diálogo
    int ret = sceImeDialogInit(&param);
    if (ret >= 0) {
        g_ime_initialized = 1;
    }
}

OskStatus osk_update_ime(void) {
    if (!g_ime_initialized) {
        return OSK_STATUS_ERROR;
    }
    
    // Verifica o status do diálogo
    SceCommonDialogStatus status = sceImeDialogGetStatus();
    
    switch (status) {
        case SCE_COMMON_DIALOG_STATUS_RUNNING:
            // Diálogo ainda está aberto
            return OSK_STATUS_RUNNING;
            
        case SCE_COMMON_DIALOG_STATUS_FINISHED:
            // Diálogo foi finalizado, pega o resultado
            {
                SceImeDialogResult result;
                memset(&result, 0, sizeof(SceImeDialogResult));
                sceImeDialogGetResult(&result);
                
                // Termina o diálogo
                sceImeDialogTerm();
                g_ime_initialized = 0;
                
                // Verifica se o usuário confirmou ou cancelou
                if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {
                    // Converte o texto UTF-16 para UTF-8 e armazena
                    utf16_to_utf8(g_input_text, g_result_text, sizeof(g_result_text));
                    return OSK_STATUS_FINISHED;
                } else {
                    // Usuário cancelou
                    g_result_text[0] = '\0';
                    return OSK_STATUS_CANCELED;
                }
            }
            
        default:
            // Erro ou estado inválido
            if (g_ime_initialized) {
                sceImeDialogTerm();
                g_ime_initialized = 0;
            }
            g_result_text[0] = '\0';
            return OSK_STATUS_ERROR;
    }
}

char* osk_get_text(void) {
    return g_result_text;
}