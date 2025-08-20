#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#include <psp2/kernel/threadmgr/mutex.h>
#endif

#include <curl/curl.h>
#include "player.h"
#include "tpb_scraper.h"

extern struct DownloadProgress {
    char status_message[256];
    char error_message[256];
    float progress_percent;
    int is_running;
    int is_done;
    int is_cancelled;
    TpbResult selected_torrent;
} g_progress;

extern SceUID g_progress_mutex;

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    struct DownloadProgress *progress = (struct DownloadProgress *)clientp;

    sceKernelLockMutex(g_progress_mutex, 1, NULL);

    if (progress->is_cancelled) {
        sceKernelUnlockMutex(g_progress_mutex, 1);
        return 1;
    }

    if (dltotal > 0) {
        progress->progress_percent = ((float)dlnow / (float)dltotal) * 100.0f;
        snprintf(progress->status_message, sizeof(progress->status_message), "Baixando... %.2f%%", progress->progress_percent);
    }

    sceKernelUnlockMutex(g_progress_mutex, 1);
    
    return 0;
}

int download_file(const char *url, const char *dest_path, struct DownloadProgress *progress) {
    if (!url || !dest_path) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        return 0;
    }

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
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
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (res != CURLE_ABORTED_BY_CALLBACK) {
            sceKernelLockMutex(g_progress_mutex, 1, NULL);
            snprintf(progress->error_message, sizeof(progress->error_message), "Falha no download: %s", curl_easy_strerror(res));
            sceKernelUnlockMutex(g_progress_mutex, 1);
        }
        remove(dest_path);
        return 0;
    }

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