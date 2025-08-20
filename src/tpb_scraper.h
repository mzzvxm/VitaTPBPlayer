#ifndef TPB_SCRAPER_H
#define TPB_SCRAPER_H

#include <stddef.h> // Para size_t

typedef struct {
    char name[256];
    char magnet[512];
    char size[32];
    int seeders;
    int leechers;
} TpbResult;

int tpb_search(const char *query, TpbResult *results, int max_results, char *error_out, size_t error_out_size);

#endif