#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#include <psp2/kernel/threadmgr/mutex.h>
#endif

#include <curl/curl.h>
#include "player.h"
#include "realdebrid.h" // Necessário para a definição de RdUserInfo, etc.
#include "tpb_scraper.h"  // Necessário para a definição de TpbResult

// A declaração da estrutura precisa estar disponível aqui
struct ProgressData {
    char status_message[256];
    char error_message[256];
    float progress_percent;
    volatile int is_running;
    volatile int is_done;
    volatile int is_cancelled;
    TpbResult selected_torrent;
    char torrent_id[128];
    char final_filepath[512];
};

// Precisamos acessar o mutex global definido no main.c
extern SceUID g_progress_mutex;

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    struct ProgressData *progress = (struct ProgressData *)clientp;

    sceKernelLockMutex(g_progress_mutex, 1, NULL);

    if (progress->is_cancelled) {
        sceKernelUnlockMutex(g_progress_mutex, 1);
        return 1; // Retornar 1 cancela o download
    }

    if (dltotal > 0) {
        progress->progress_percent = ((float)dlnow / (float)dltotal) * 100.0f;
        snprintf(progress->status_message, sizeof(progress->status_message), "Baixando... %.1f%% (%.1f / %.1f MB)",
                 progress->progress_percent,
                 (float)dlnow / (1024.0f * 1024.0f),
                 (float)dltotal / (1024.0f * 1024.0f));
    }

    sceKernelUnlockMutex(g_progress_mutex, 1);
    
    return 0;
}

int download_file(const char *url, const char *dest_path, struct ProgressData *progress) {
    if (!url || !dest_path) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(progress->error_message, sizeof(progress->error_message), "Falha ao iniciar cURL.");
        return 0;
    }

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        snprintf(progress->error_message, sizeof(progress->error_message), "Nao foi possivel criar o arquivo de video.");
        curl_easy_cleanup(curl);
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progress);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        // Se não foi cancelado pelo usuário, registra o erro
        if (res != CURLE_ABORTED_BY_CALLBACK) {
            snprintf(progress->error_message, sizeof(progress->error_message), "Falha no download: %s", curl_easy_strerror(res));
        }
        remove(dest_path); // Apaga o arquivo incompleto
        curl_easy_cleanup(curl);
        return 0;
    }

    curl_easy_cleanup(curl);
    return 1;
}

void player_play(const char *file_path) {
    if (!file_path) return;

#ifdef __vita__
    sceAppMgrLoadExec(file_path, NULL, NULL);
#else
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", file_path);
    system(cmd);
#endif
}