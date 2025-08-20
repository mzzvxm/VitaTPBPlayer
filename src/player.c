#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#endif

#include "player.h"

// A biblioteca curl já é incluída pelo vitasdk,
// mas se precisar de forma explícita, use a flag -DHAVE_LIBCURL
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int download_file(const char *url, const char *dest_path) {
    if (!url || !dest_path) return 0;

#ifdef HAVE_LIBCURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        // Falha silenciosa se o curl não puder ser inicializado
        return 0;
    }

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    // Adiciona um timeout para evitar que o download fique preso indefinidamente
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minutos de timeout para o download

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        // Se o download falhar, remove o arquivo incompleto
        remove(dest_path);
        return 0;
    }

    return 1;
#else
    // Fallback para curl via system() caso a libcurl não esteja linkada
    char command[2048];
    snprintf(command, sizeof(command), "curl -L -f -s -o \"%s\" \"%s\"", dest_path, url);
    int rc = system(command);
    if (rc != 0 || !file_exists(dest_path)) {
        return 0;
    }
    return 1;
#endif
}

void player_play(const char *file_path) {
    if (!file_path) return;

#ifdef __vita__
    // No Vita, chama um player externo ou um aplicativo para o arquivo.
    sceAppMgrLoadExec(file_path, NULL, NULL);
#else
    // Em outros sistemas (Linux, etc.), usa o abridor padrão.
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", file_path);
    system(cmd);
#endif
}