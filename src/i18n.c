#include "i18n.h"
#include <string.h>

static Language current_language = LANG_EN;

// Tabela de traduções [IDIOMA][STRING_ID]
static const char* translations[LANG_COUNT][STR_COUNT] = {
    // PORTUGUÊS BRASILEIRO
    [LANG_PT_BR] = {
        [STR_APP_TITLE] = "VitaTPBPlayer",
        [STR_APP_SUBTITLE] = "made by @mzzvxm",
        [STR_MENU_SEARCH] = "Buscar",
        [STR_MENU_SETTINGS] = "Configurações",
        [STR_MENU_EXIT] = "Sair",
        
        [STR_SETTINGS_TITLE] = "Configurações",
        [STR_SETTINGS_SUBTITLE] = "Token, Conta e Idioma",
        [STR_SETTINGS_TOKEN_EDIT] = "Editar Token Manualmente",
        [STR_SETTINGS_TOKEN_TEST] = "Testar Token",
        [STR_SETTINGS_TOKEN_WIFI] = "Receber Token via Wi-Fi",
        [STR_SETTINGS_SAVE] = "Salvar e Voltar",
        [STR_SETTINGS_TOKEN_STATUS] = "Token Real-Debrid:",
        [STR_SETTINGS_LANGUAGE] = "Idioma",
        
        [STR_USER_INFO] = "Usuário:",
        [STR_USER_EMAIL] = "Email:",
        [STR_USER_STATUS] = "Status:",
        [STR_USER_PREMIUM] = "Premium",
        [STR_USER_FREE] = "Gratuito",
        [STR_USER_EXPIRES] = "Expira:",
        
        [STR_SEARCH_TITLE] = "Buscar Torrents",
        [STR_SEARCH_RESULTS_FOR] = "Resultados para",
        [STR_SEARCH_NO_RESULTS] = "Nenhum resultado encontrado.",
        [STR_SEARCH_PAGE] = "Página",
        
        [STR_DOWNLOAD_TITLE] = "Download",
        [STR_DOWNLOAD_SELECT_FILE] = "Selecione o Arquivo",
        [STR_DOWNLOAD_PROGRESS] = "Progresso do Download",
        [STR_DOWNLOAD_STEP] = "Passo",
        
        [STR_WIFI_TITLE] = "Receber Token via Wi-Fi",
        [STR_WIFI_STEP1] = "1) Conecte seu celular/PC na mesma rede.",
        [STR_WIFI_STEP2] = "2) Acesse no navegador:",
        [STR_WIFI_STEP3] = "3) Cole o token e clique em Salvar.",
        
        [STR_HINT_SELECT] = "(X) Selecionar",
        [STR_HINT_BACK] = "(O) Voltar",
        [STR_HINT_PAGE] = "(L/R) Página",
        [STR_HINT_NEW_SEARCH] = "(△) Nova Busca",
        [STR_HINT_CONFIRM] = "(X) Confirmar",
        [STR_HINT_CANCEL] = "(O) Cancelar",
        [STR_HINT_NAVIGATE] = "(↑/↓) Cima/Baixo",
        [STR_HINT_TOUCH] = "(Toque) Tocar",
        
        [STR_ERROR] = "ERRO",
        [STR_STATUS] = "Status",
        [STR_SIZE] = "Tamanho:",
        [STR_SEEDERS] = "Seeders:",
        [STR_MB] = "MB",
        [STR_NOT_CONFIGURED] = "Não configurado",
    },
    
    // ENGLISH
    [LANG_EN] = {
        [STR_APP_TITLE] = "VitaTPBPlayer",
        [STR_APP_SUBTITLE] = "made by @mzzvxm",
        [STR_MENU_SEARCH] = "Search",
        [STR_MENU_SETTINGS] = "Settings",
        [STR_MENU_EXIT] = "Exit",
        
        [STR_SETTINGS_TITLE] = "Settings",
        [STR_SETTINGS_SUBTITLE] = "Token, Account and Language",
        [STR_SETTINGS_TOKEN_EDIT] = "Edit Token Manually",
        [STR_SETTINGS_TOKEN_TEST] = "Test Token",
        [STR_SETTINGS_TOKEN_WIFI] = "Receive Token via Wi-Fi",
        [STR_SETTINGS_SAVE] = "Save and Return",
        [STR_SETTINGS_TOKEN_STATUS] = "Real-Debrid Token:",
        [STR_SETTINGS_LANGUAGE] = "Language",
        
        [STR_USER_INFO] = "User:",
        [STR_USER_EMAIL] = "Email:",
        [STR_USER_STATUS] = "Status:",
        [STR_USER_PREMIUM] = "Premium",
        [STR_USER_FREE] = "Free",
        [STR_USER_EXPIRES] = "Expires:",
        
        [STR_SEARCH_TITLE] = "Search Torrents",
        [STR_SEARCH_RESULTS_FOR] = "Results for",
        [STR_SEARCH_NO_RESULTS] = "No results found.",
        [STR_SEARCH_PAGE] = "Page",
        
        [STR_DOWNLOAD_TITLE] = "Download",
        [STR_DOWNLOAD_SELECT_FILE] = "Select File",
        [STR_DOWNLOAD_PROGRESS] = "Download Progress",
        [STR_DOWNLOAD_STEP] = "Step",
        
        [STR_WIFI_TITLE] = "Receive Token via Wi-Fi",
        [STR_WIFI_STEP1] = "1) Connect your phone/PC to the same network.",
        [STR_WIFI_STEP2] = "2) Open in browser:",
        [STR_WIFI_STEP3] = "3) Paste the token and click Save.",
        
        [STR_HINT_SELECT] = "(X) Select",
        [STR_HINT_BACK] = "(O) Back",
        [STR_HINT_PAGE] = "(L/R) Page",
        [STR_HINT_NEW_SEARCH] = "(△) New Search",
        [STR_HINT_CONFIRM] = "(X) Confirm",
        [STR_HINT_CANCEL] = "(O) Cancel",
        [STR_HINT_NAVIGATE] = "(↑/↓) Up/Down",
        [STR_HINT_TOUCH] = "(Touch) Tap",
        
        [STR_ERROR] = "ERROR",
        [STR_STATUS] = "Status",
        [STR_SIZE] = "Size:",
        [STR_SEEDERS] = "Seeders:",
        [STR_MB] = "MB",
        [STR_NOT_CONFIGURED] = "Not configured",
    }
};

static const char* language_names[LANG_COUNT] = {
    [LANG_PT_BR] = "Português (BR)",
    [LANG_EN] = "English"
};

void i18n_init(void) {
    current_language = LANG_PT_BR;
}

void i18n_set_language(Language lang) {
    if (lang >= 0 && lang < LANG_COUNT) {
        current_language = lang;
    }
}

Language i18n_get_language(void) {
    return current_language;
}

const char* i18n_get(StringId id) {
    if (id >= 0 && id < STR_COUNT && current_language >= 0 && current_language < LANG_COUNT) {
        return translations[current_language][id];
    }
    return "";
}

const char* i18n_get_language_name(Language lang) {
    if (lang >= 0 && lang < LANG_COUNT) {
        return language_names[lang];
    }
    return "";
}