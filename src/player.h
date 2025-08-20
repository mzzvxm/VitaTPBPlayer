#ifndef PLAYER_H
#define PLAYER_H

// Forward declaration para evitar include circular
struct DownloadProgress;

int download_file(const char *url, const char *dest_path, struct DownloadProgress *progress);
void player_play(const char *file_path);

#endif