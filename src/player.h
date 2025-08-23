#ifndef PLAYER_H
#define PLAYER_H

// Forward declaration para a estrutura de progresso
struct ProgressData;

// Inicia o download completo de um arquivo
int download_file(const char *url, const char *dest_path, struct ProgressData *progress);

// Inicia o player de vídeo nativo do Vita
void player_play(const char *file_path);

#endif