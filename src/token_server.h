#ifndef TOKEN_SERVER_H
#define TOKEN_SERVER_H

#include <stdbool.h>

// Inicia o servidor na porta especificada. Retorna 0 em caso de sucesso.
int token_server_start(int port);

// Para o servidor.
void token_server_stop(void);

// Verifica se o servidor está rodando.
bool token_server_is_running(void);

#endif