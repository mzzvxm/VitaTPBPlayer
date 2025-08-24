#ifndef REALDEBRID_H
#define REALDEBRID_H

#include <stddef.h>

// Estrutura para guardar informações do usuário do Real-Debrid
typedef struct {
    char username[64];
    char email[128];
    char expiration[64];
    int is_premium;
} RdUserInfo;

// Estrutura para guardar informações detalhadas do torrent
typedef struct {
    char id[128];
    char filename[256];
    char original_link[2048]; // Link intermediário
} RdTorrentInfo;

// NOVA ESTRUTURA: Para guardar informações de um único arquivo dentro de um torrent
typedef struct {
    int id;
    char path[512];
    long long bytes;
} RdFileInfo;


// Define um "ponteiro de função" para o nosso callback de progresso
// Isso permitirá que a função de espera se comunique com a interface principal.
typedef void (*rd_progress_callback)(int current_attempt, int max_attempts);

// Funções de gerenciamento de token
void rd_set_token(const char *token);
const char* rd_get_token();
int rd_load_token_from_file(const char* path);
int rd_save_token_to_file(const char* path);
int rd_get_user_info(RdUserInfo *userInfo, char *error_out, size_t error_out_size);

// Funções do fluxo de torrent
int rd_add_magnet(const char *magnet, char *id_out, char *error_out, size_t error_out_size);

// FUNÇÃO ANTIGA (rd_select_all_files) FOI REMOVIDA E SUBSTITUÍDA
// NOVA FUNÇÃO: Pega a lista de arquivos de um torrent. Retorna o número de arquivos encontrados.
int rd_get_torrent_files(const char *id, RdFileInfo *files, int max_files, char *error_out, size_t error_out_size);
// NOVA FUNÇÃO: Seleciona um arquivo específico para download.
int rd_select_specific_file(const char *id, int file_id, char *error_out, size_t error_out_size);

// A função agora aceita um callback para reportar o progresso
int rd_wait_for_torrent_ready(const char *id, RdTorrentInfo *torrentInfo, rd_progress_callback callback, char *error_out, size_t error_out_size);
int rd_unrestrict_link(const char *link, char *url_out, char *error_out, size_t error_out_size);

#endif