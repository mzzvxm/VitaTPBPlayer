#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include "stream_manager.h"

// Tamanho do buffer inicial em bytes antes de iniciar a reprodução (15 MB)
#define INITIAL_BUFFER_SIZE (15 * 1024 * 1024)

static StreamManager g_stream_manager;

// Função de callback de escrita do cURL
// Escreve os dados no arquivo e atualiza o total baixado
static size_t write_data_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);

    sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);
    g_stream_manager.total_downloaded += written * size;

    // Se o buffer inicial ainda não foi atingido, verifica
    if (!g_stream_manager.buffer_ready) {
        if (g_stream_manager.total_downloaded >= INITIAL_BUFFER_SIZE) {
            g_stream_manager.buffer_ready = 1;
        }
    }
    sceKernelUnlockMutex(g_stream_manager.mutex, 1);

    return written;
}

// Função de callback de progresso do cURL
// Usada para obter o tamanho total do arquivo e calcular a porcentagem
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);

    if (g_stream_manager.is_cancelled) {
        sceKernelUnlockMutex(g_stream_manager.mutex, 1);
        return 1; // Retornar 1 cancela o download do cURL
    }

    if (dltotal > 0) {
        g_stream_manager.total_size = dltotal;
        g_stream_manager.progress_percent = ((float)dlnow / (float)dltotal) * 100.0f;
        snprintf(g_stream_manager.status_message, sizeof(g_stream_manager.status_message),
                 "Buffering... %.2f%% (%.1f / %.1f MB)",
                 g_stream_manager.progress_percent,
                 (float)dlnow / (1024.0f * 1024.0f),
                 (float)dltotal / (1024.0f * 1024.0f));
    }
    
    sceKernelUnlockMutex(g_stream_manager.mutex, 1);
    return 0;
}


static int stream_thread_func(SceSize args, void *argp) {
    FILE *fp = fopen(g_stream_manager.temp_path, "wb");
    if (!fp) {
        sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);
        snprintf(g_stream_manager.error_message, sizeof(g_stream_manager.error_message), "Erro ao criar arquivo temporario.");
        g_stream_manager.is_active = 0;
        sceKernelUnlockMutex(g_stream_manager.mutex, 1);
        return sceKernelExitDeleteThread(0);
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);
        snprintf(g_stream_manager.error_message, sizeof(g_stream_manager.error_message), "Falha ao iniciar cURL.");
        g_stream_manager.is_active = 0;
        sceKernelUnlockMutex(g_stream_manager.mutex, 1);
        return sceKernelExitDeleteThread(0);
    }
    
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, g_stream_manager.download_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);
    if (res == CURLE_OK) {
        g_stream_manager.download_complete = 1;
        snprintf(g_stream_manager.status_message, sizeof(g_stream_manager.status_message), "Download concluido.");
    } else if (res != CURLE_ABORTED_BY_CALLBACK) {
        snprintf(g_stream_manager.error_message, sizeof(g_stream_manager.error_message), "Falha no download: %s", curl_easy_strerror(res));
    }
    g_stream_manager.is_active = 0; // O thread terminou, mas o manager ainda pode ser consultado.
    sceKernelUnlockMutex(g_stream_manager.mutex, 1);
    
    return sceKernelExitDeleteThread(0);
}

void stream_manager_init() {
    memset(&g_stream_manager, 0, sizeof(StreamManager));
    g_stream_manager.mutex = sceKernelCreateMutex("stream_mutex", 0, 0, NULL);
}

void stream_manager_finish() {
    sceKernelDeleteMutex(g_stream_manager.mutex);
}

int stream_manager_start(const char *url, const char *dest_path) {
    sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);

    if (g_stream_manager.is_active) {
        sceKernelUnlockMutex(g_stream_manager.mutex, 1);
        return 0; // Já está em execução
    }

    // Reseta o estado
    memset(&g_stream_manager, 0, sizeof(StreamManager)); // Mantem o mutex
    SceUID old_mutex = g_stream_manager.mutex;
    memset(&g_stream_manager, 0, sizeof(StreamManager));
    g_stream_manager.mutex = old_mutex;
    
    g_stream_manager.is_active = 1;
    strncpy(g_stream_manager.download_url, url, sizeof(g_stream_manager.download_url) - 1);
    strncpy(g_stream_manager.temp_path, dest_path, sizeof(g_stream_manager.temp_path) - 1);
    snprintf(g_stream_manager.status_message, sizeof(g_stream_manager.status_message), "Iniciando...");

    g_stream_manager.thread_id = sceKernelCreateThread("stream_thread", stream_thread_func, 0x10000100, 0x10000, 0, 0, NULL);
    
    if (g_stream_manager.thread_id < 0) {
        g_stream_manager.is_active = 0;
        snprintf(g_stream_manager.error_message, sizeof(g_stream_manager.error_message), "Erro ao criar thread de stream.");
        sceKernelUnlockMutex(g_stream_manager.mutex, 1);
        return 0;
    }

    sceKernelStartThread(g_stream_manager.thread_id, 0, NULL);
    sceKernelUnlockMutex(g_stream_manager.mutex, 1);
    return 1;
}

void stream_manager_stop() {
    sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);
    if (g_stream_manager.is_active) {
        g_stream_manager.is_cancelled = 1;
    }
    // Espera o thread terminar para evitar race conditions
    sceKernelUnlockMutex(g_stream_manager.mutex, 1);

    SceUInt timeout = 500000; // 500ms timeout
    sceKernelWaitThreadEnd(g_stream_manager.thread_id, NULL, &timeout);

    // Limpa o arquivo temporário
    sceIoRemove(g_stream_manager.temp_path);
    
    sceKernelLockMutex(g_stream_manager.mutex, 1, NULL);
    g_stream_manager.is_active = 0;
    g_stream_manager.is_cancelled = 0;
    sceKernelUnlockMutex(g_stream_manager.mutex, 1);
}

StreamManager* stream_manager_get_state() {
    return &g_stream_manager;
}