#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#endif

#include "player.h"
#include "debugScreen.h"

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int download_file(const char *url, const char *dest_path) {
    if (!url || !dest_path) return 0;

    psvDebugScreenPrintf("download_file: url=%s\n", url);

#ifdef HAVE_LIBCURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        psvDebugScreenPrintf("curl init failed, falling back to system curl\n");
        goto fallback;
    }

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        psvDebugScreenPrintf("failed to open dest file: %s\n", dest_path);
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        psvDebugScreenPrintf("curl download error: %s\n", curl_easy_strerror(res));
        // remove incomplete file
        remove(dest_path);
        return 0;
    }

    psvDebugScreenPrintf("download finished: %s\n", dest_path);
    return 1;
#else
fallback:
    {
        char command[2048];
        // -L follow redirects, -f fail silently on HTTP errors
        snprintf(command, sizeof(command), "curl -L -f \"%s\" -o \"%s\"", url, dest_path);
        psvDebugScreenPrintf("running: %s\n", command);
        int rc = system(command);
        if (rc != 0 || !file_exists(dest_path)) {
            psvDebugScreenPrintf("system curl failed (rc=%d)\n", rc);
            return 0;
        }
        psvDebugScreenPrintf("download finished (system curl): %s\n", dest_path);
        return 1;
    }
#endif
}

void player_play(const char *file_path) {
    if (!file_path) return;

    psvDebugScreenPrintf("player_play: %s\n", file_path);

#ifdef __vita__
    // On Vita we can try to call load/exec for an ELF, or open with an external player.
    // Keep original behaviour: try to load exec the file path (as you had).
    int ret = sceAppMgrLoadExec(file_path, NULL, NULL);
    if (ret < 0) {
        psvDebugScreenPrintf("sceAppMgrLoadExec failed: 0x%08X\n", ret);
    }
#else
    // On native POSIX, fallback to default opener
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", file_path);
    system(cmd);
#endif
}
