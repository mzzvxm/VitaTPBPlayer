#ifndef TPB_SCRAPER_H
#define TPB_SCRAPER_H

// Estrutura para armazenar um único resultado da busca do The Pirate Bay
typedef struct {
    char name[256];
    char magnet[512];
    int seeders;
    int leechers;
} TpbResult;

/**
 * @brief Busca no The Pirate Bay (via apibay) e preenche uma lista de resultados.
 *
 * @param query O termo a ser buscado.
 * @param results Um array de TpbResult a ser preenchido com os resultados.
 * @param max_results O tamanho máximo do array 'results'.
 * @return O número de resultados encontrados e preenchidos no array.
 */
int tpb_search(const char *query, TpbResult *results, int max_results);

#endif