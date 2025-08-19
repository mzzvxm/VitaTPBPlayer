#include <curl/curl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tpb_scraper.h"
#include "debugScreen.h"

// Estrutura interna para o callback do cURL
struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        psvDebugScreenPrintf("Erro: sem memória para o buffer da resposta da API\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Função auxiliar para extrair um valor de uma chave JSON simples (ex: "chave":"valor")
static int json_extract_string(const char* json, const char* key, char* out, size_t out_size) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);

    const char* key_ptr = strstr(json, search_key);
    if (!key_ptr) return 0;

    const char* value_start = key_ptr + strlen(search_key);
    const char* value_end = strchr(value_start, '\"');
    if (!value_end) return 0;

    size_t len = value_end - value_start;
    if (len >= out_size) {
        len = out_size - 1;
    }

    strncpy(out, value_start, len);
    out[len] = '\0';
    return 1;
}

// Função auxiliar para extrair um valor numérico (ex: "chave":"123")
static int json_extract_int(const char* json, const char* key, int* out) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    
    const char* key_ptr = strstr(json, search_key);
    if (!key_ptr) return 0;

    const char* value_start = key_ptr + strlen(search_key);
    *out = atoi(value_start);
    return 1;
}

// Função para formatar o tamanho em bytes para KB, MB, GB...
static void format_size(long long bytes, char* out, size_t out_size) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double d_bytes = bytes;

    if (bytes == 0) {
        snprintf(out, out_size, "0 B");
        return;
    }

    while (d_bytes >= 1024 && i < 4) {
        d_bytes /= 1024;
        i++;
    }

    snprintf(out, out_size, "%.2f %s", d_bytes, suffixes[i]);
}


int tpb_search(const char *query, TpbResult *results, int max_results) {
    CURL *curl;
    CURLcode res;
    char url[512];

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    snprintf(url, sizeof(url), "https://apibay.org/q.php?q=%s&cat=0", query);

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        psvDebugScreenPrintf("curl_easy_perform() falhou: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 0;
    }
    
    int count = 0;
    const char* current_pos = chunk.memory;

    if (strstr(current_pos, "\"name\":\"No results\"")) {
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 0;
    }

    while (count < max_results && (current_pos = strstr(current_pos, "\"id\":"))) {
        char name[256] = "N/A";
        char info_hash[41] = "";
        char size_str[32] = "0";
        int seeders = 0, leechers = 0;

        json_extract_string(current_pos, "name", name, sizeof(name));
        json_extract_string(current_pos, "info_hash", info_hash, sizeof(info_hash));
        json_extract_string(current_pos, "size", size_str, sizeof(size_str)); // Extrai o tamanho como string
        json_extract_int(current_pos, "seeders", &seeders);
        json_extract_int(current_pos, "leechers", &leechers);

        if (strlen(info_hash) == 40) {
            strncpy(results[count].name, name, sizeof(results[count].name) - 1);
            results[count].name[sizeof(results[count].name) - 1] = '\0';
            
            results[count].seeders = seeders;
            results[count].leechers = leechers;
            format_size(atoll(size_str), results[count].size, sizeof(results[count].size));
            
            snprintf(results[count].magnet, sizeof(results[count].magnet), "magnet:?xt=urn:btih:%s", info_hash);
            
            count++;
        }

        current_pos = strchr(current_pos, '}');
        if (!current_pos) break;
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return count;
}
