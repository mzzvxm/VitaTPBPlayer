#ifndef I18N_H
#define I18N_H

typedef enum {
    LANG_PT_BR = 0,
    LANG_EN,
    LANG_COUNT
} Language;

// IDs de strings traduzíveis
typedef enum {
    // Menu Principal
    STR_APP_TITLE,
    STR_APP_SUBTITLE,
    STR_MENU_SEARCH,
    STR_MENU_SETTINGS,
    STR_MENU_EXIT,
    
    // Configurações
    STR_SETTINGS_TITLE,
    STR_SETTINGS_SUBTITLE,
    STR_SETTINGS_TOKEN_EDIT,
    STR_SETTINGS_TOKEN_TEST,
    STR_SETTINGS_TOKEN_WIFI,
    STR_SETTINGS_SAVE,
    STR_SETTINGS_TOKEN_STATUS,
    STR_SETTINGS_LANGUAGE,
    
    // Informações de usuário
    STR_USER_INFO,
    STR_USER_EMAIL,
    STR_USER_STATUS,
    STR_USER_PREMIUM,
    STR_USER_FREE,
    STR_USER_EXPIRES,
    
    // Busca
    STR_SEARCH_TITLE,
    STR_SEARCH_RESULTS_FOR,
    STR_SEARCH_NO_RESULTS,
    STR_SEARCH_PAGE,
    
    // Download
    STR_DOWNLOAD_TITLE,
    STR_DOWNLOAD_SELECT_FILE,
    STR_DOWNLOAD_PROGRESS,
    STR_DOWNLOAD_STEP,
    
    // Servidor Wi-Fi
    STR_WIFI_TITLE,
    STR_WIFI_STEP1,
    STR_WIFI_STEP2,
    STR_WIFI_STEP3,
    
    // Hints de controle
    STR_HINT_SELECT,
    STR_HINT_BACK,
    STR_HINT_PAGE,
    STR_HINT_NEW_SEARCH,
    STR_HINT_CONFIRM,
    STR_HINT_CANCEL,
    STR_HINT_NAVIGATE,
    STR_HINT_TOUCH,
    
    // Misc
    STR_ERROR,
    STR_STATUS,
    STR_SIZE,
    STR_SEEDERS,
    STR_MB,
    STR_NOT_CONFIGURED,
    
    STR_COUNT
} StringId;

// Inicializa o sistema de i18n
void i18n_init(void);

// Define o idioma atual
void i18n_set_language(Language lang);

// Obtém o idioma atual
Language i18n_get_language(void);

// Obtém uma string traduzida
const char* i18n_get(StringId id);

// Obtém o nome do idioma
const char* i18n_get_language_name(Language lang);

#endif // I18N_H