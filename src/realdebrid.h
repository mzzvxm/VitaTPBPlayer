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

// Funções de gerenciamento de token
void rd_set_token(const char *token);
const char* rd_get_token();
int rd_load_token_from_file(const char* path);
int rd_save_token_to_file(const char* path);
int rd_get_user_info(RdUserInfo *userInfo, char *error_out, size_t error_out_size);

// Funções do fluxo de torrent
int rd_add_magnet(const char *magnet, char *id_out, char *error_out, size_t error_out_size);
int rd_select_all_files(const char *id, char *error_out, size_t error_out_size);
int rd_get_intermediate_link(const char *id, char *link_out, char *error_out, size_t error_out_size);
int rd_unrestrict_link(const char *link, char *url_out, char *error_out, size_t error_out_size);

#endif