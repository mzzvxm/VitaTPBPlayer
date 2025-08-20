#ifndef REALDEBRID_H
#define REALDEBRID_H

#include <stddef.h> // Para size_t

#ifndef RD_TOKEN
#define RD_TOKEN "COLOQUE_SEU_TOKEN_AQUI"
#endif

int rd_add_magnet(const char *magnet, char *id_out, char *error_out, size_t error_out_size);
int rd_wait_for_ready(const char *id, char *error_out, size_t error_out_size);
int rd_get_file_url(const char *id, char *url_out, char *error_out, size_t error_out_size);

#endif