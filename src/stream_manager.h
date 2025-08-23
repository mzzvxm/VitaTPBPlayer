#ifndef STREAM_MANAGER_H
#define STREAM_MANAGER_H

#include <psp2/kernel/threadmgr.h>

typedef struct {
    // Status e Controle
    volatile int is_active;
    volatile int buffer_ready;
    volatile int is_cancelled;
    volatile int download_complete;
    
    // Progresso e Informações
    char status_message[256];
    char error_message[256];
    float progress_percent;
    long long total_downloaded;
    long long total_size;

    // Internals
    char download_url[2048];
    char temp_path[512];
    SceUID thread_id;
    SceUID mutex;

} StreamManager;

// Inicializa o gerenciador de streaming (chame isso no main)
void stream_manager_init();

// Finaliza o gerenciador de streaming (chame na saída do app)
void stream_manager_finish();

// Inicia uma nova sessão de streaming
// url: O link de download direto do Real-Debrid
// dest_path: O caminho para salvar o arquivo temporário (ex: "ux0:data/VitaTPBPlayer/stream.mp4")
int stream_manager_start(const char *url, const char *dest_path);

// Para a sessão de streaming e limpa os arquivos
void stream_manager_stop();

// Retorna um ponteiro para o estado atual do gerenciador (para a UI)
StreamManager* stream_manager_get_state();

#endif