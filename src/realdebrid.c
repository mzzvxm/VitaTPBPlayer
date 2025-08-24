#define _XOPEN_SOURCE
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <psp2/kernel/threadmgr.h>
#include "realdebrid.h"
#include "cJSON.h" // <<< GARANTA QUE ESTE INCLUDE ESTÁ AQUI

static char g_rd_token[256] = {0};

struct mem_buffer {
    char *data;
    size_t size;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t realsz = size * nmemb;
    struct mem_buffer *buf = (struct mem_buffer *)userdata;
    char *tmp = realloc(buf->data, buf->size + realsz + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, ptr, realsz);
    buf->size += realsz;
    buf->data[buf->size] = '\0';
    return realsz;
}

static int has_valid_token() {
    if (strlen(g_rd_token) < 10) return 0;
    return 1;
}

void rd_set_token(const char *token) {
    if (token) {
        strncpy(g_rd_token, token, sizeof(g_rd_token) - 1);
        g_rd_token[sizeof(g_rd_token) - 1] = '\0';
    }
}

const char* rd_get_token() {
    return g_rd_token;
}

int rd_load_token_from_file(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) return 0;
    fgets(g_rd_token, sizeof(g_rd_token), fp);
    fclose(fp);
    g_rd_token[strcspn(g_rd_token, "\r\n")] = 0;
    return 1;
}

int rd_save_token_to_file(const char* path) {
    FILE* fp = fopen(path, "w");
    if (!fp) return 0;
    fprintf(fp, "%s", g_rd_token);
    fclose(fp);
    return 1;
}

int rd_get_user_info(RdUserInfo *userInfo, char *error_out, size_t error_out_size) {
    memset(userInfo, 0, sizeof(RdUserInfo));
    if (!has_valid_token()) {
        snprintf(error_out, error_out_size, "Token do Real-Debrid nao configurado.");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char url[] = "https://api.real-debrid.com/rest/1.0/user";
    struct mem_buffer response;
    response.data = malloc(1); response.size = 0;

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    int success = 0;
    if (res == CURLE_OK && http_code == 200) {
        cJSON *json = cJSON_Parse(response.data);
        if (json) {
            const cJSON *username = cJSON_GetObjectItemCaseSensitive(json, "username");
            const cJSON *email = cJSON_GetObjectItemCaseSensitive(json, "email");
            const cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
            const cJSON *expiration = cJSON_GetObjectItemCaseSensitive(json, "expiration");

            if (cJSON_IsString(username)) strncpy(userInfo->username, username->valuestring, sizeof(userInfo->username) - 1);
            if (cJSON_IsString(email)) strncpy(userInfo->email, email->valuestring, sizeof(userInfo->email) - 1);
            if (cJSON_IsString(type) && strcmp(type->valuestring, "premium") == 0) userInfo->is_premium = 1;
            
            if (cJSON_IsString(expiration)) {
                struct tm tm = {0};
                if (strptime(expiration->valuestring, "%Y-%m-%dT%H:%M:%S", &tm)) {
                    strftime(userInfo->expiration, sizeof(userInfo->expiration), "%d/%m/%Y", &tm);
                }
            }
            success = 1;
            cJSON_Delete(json);
        }
    } else {
        snprintf(error_out, error_out_size, "RD User: Token invalido ou API falhou (HTTP: %ld)", http_code);
    }

    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return success;
}

int rd_add_magnet(const char *magnet, char *id_out, char *error_out, size_t error_out_size) {
    if (!magnet || !id_out) return 0;
    if (!has_valid_token()) {
        snprintf(error_out, error_out_size, "Token do Real-Debrid nao configurado.");
        return 0;
    }
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    
    struct mem_buffer response;
    response.data = malloc(1); response.size = 0;
    
    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, auth_header);
    
    char postfields[2048];
    snprintf(postfields, sizeof(postfields), "magnet=%s", magnet);
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.real-debrid.com/rest/1.0/torrents/addMagnet");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    int success = 0;
    if (res == CURLE_OK) {
        cJSON *json = cJSON_Parse(response.data);
        if (json) {
            const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(json, "id");
            if (cJSON_IsString(id_item) && id_item->valuestring != NULL) {
                strncpy(id_out, id_item->valuestring, 128 - 1);
                success = 1;
            } else {
                snprintf(error_out, error_out_size, "RD Magnet: ID nao encontrado na resposta da API.");
            }
            cJSON_Delete(json);
        }
    } else {
         snprintf(error_out, error_out_size, "RD Magnet (cURL): %s", curl_easy_strerror(res));
    }
    
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return success;
}

int rd_get_torrent_files(const char *id, RdFileInfo *files, int max_files, char *error_out, size_t error_out_size) {
    if (!id || !has_valid_token()) {
        snprintf(error_out, error_out_size, "ID ou Token invalido.");
        return -1;
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    
    char url[512];
    snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/info/%s", id);
    
    struct mem_buffer response = { .data = malloc(1), .size = 0 };
    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    int files_found = -1;
    if (res == CURLE_OK && http_code == 200) {
        cJSON *json = cJSON_Parse(response.data);
        if (json) {
            const cJSON *files_array = cJSON_GetObjectItemCaseSensitive(json, "files");
            if (cJSON_IsArray(files_array)) {
                files_found = 0;
                int count = cJSON_GetArraySize(files_array);
                for (int i = 0; i < count && i < max_files; i++) {
                    cJSON *file_item = cJSON_GetArrayItem(files_array, i);
                    const cJSON *file_id = cJSON_GetObjectItemCaseSensitive(file_item, "id");
                    const cJSON *file_path = cJSON_GetObjectItemCaseSensitive(file_item, "path");
                    const cJSON *file_bytes = cJSON_GetObjectItemCaseSensitive(file_item, "bytes");
                    
                    if (cJSON_IsNumber(file_id) && cJSON_IsString(file_path)) {
                        files[files_found].id = file_id->valueint;
                        strncpy(files[files_found].path, file_path->valuestring, sizeof(files[files_found].path) - 1);
                        files[files_found].bytes = cJSON_IsNumber(file_bytes) ? file_bytes->valueint : 0;
                        files_found++;
                    }
                }
            } else {
                 snprintf(error_out, error_out_size, "RD GetFiles: Array 'files' nao encontrado no JSON.");
            }
            cJSON_Delete(json);
        } else {
            snprintf(error_out, error_out_size, "RD GetFiles: Falha ao parsear JSON da API.");
        }
    } else {
        snprintf(error_out, error_out_size, "RD GetFiles: API falhou (HTTP: %ld)", http_code);
    }
    
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    
    return files_found;
}

// --- FUNÇÃO CORRIGIDA ---
int rd_select_specific_file(const char *id, int file_id, char *error_out, size_t error_out_size) {
    if (!id || !has_valid_token()) {
        snprintf(error_out, error_out_size, "ID ou Token invalido.");
        return 0;
    }
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    char url[512];
    snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/selectFiles/%s", id);
    
    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
    headers = curl_slist_append(headers, auth_header);
    
    char postfields[64];
    snprintf(postfields, sizeof(postfields), "files=%d", file_id);
    
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    
    // LINHA PROBLEMÁTICA REMOVIDA
    // curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); 
    
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD SelectFile: %s", curl_easy_strerror(res));
        return 0;
    }
    
    if (http_code == 202 || http_code == 204) {
        return 1;
    }
    
    snprintf(error_out, error_out_size, "RD SelectFile: API retornou erro HTTP %ld", http_code);
    return 0;
}

int rd_wait_for_torrent_ready(const char *id, RdTorrentInfo *torrentInfo, rd_progress_callback callback, char *error_out, size_t error_out_size) {
    if (!id || !has_valid_token()) {
        snprintf(error_out, error_out_size, "ID ou Token invalido.");
        return 0;
    }

    const int max_retries = 24;
    for (int i = 0; i < max_retries; i++) {
        if (callback) {
            callback(i + 1, max_retries);
        }

        CURL *curl = curl_easy_init();
        if (!curl) return 0;

        char url[512];
        snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/info/%s", id);
        
        struct mem_buffer response = { .data = malloc(1), .size = 0 };
        struct curl_slist *headers = NULL;
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        int torrent_ready = 0;
        if (res == CURLE_OK && http_code == 200) {
            cJSON *json = cJSON_Parse(response.data);
            if (json) {
                const cJSON *status_item = cJSON_GetObjectItemCaseSensitive(json, "status");
                if (cJSON_IsString(status_item) && (strcmp(status_item->valuestring, "downloaded") == 0)) {
                    const cJSON *links_array = cJSON_GetObjectItemCaseSensitive(json, "links");
                    if (cJSON_IsArray(links_array) && cJSON_GetArraySize(links_array) > 0) {
                        const cJSON *first_link_item = cJSON_GetArrayItem(links_array, 0);
                        if (cJSON_IsString(first_link_item) && (first_link_item->valuestring != NULL)) {
                            strncpy(torrentInfo->original_link, first_link_item->valuestring, sizeof(torrentInfo->original_link) - 1);
                            torrent_ready = 1;
                        }
                    }
                }
                cJSON_Delete(json);
            }
        }
        
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);

        if (torrent_ready) {
            return 1;
        }
        sceKernelDelayThread(5 * 1000 * 1000);
    }
    snprintf(error_out, error_out_size, "RD Info: Timeout. O torrent nao ficou pronto.");
    return 0;
}

int rd_unrestrict_link(const char *link, char *url_out, char *error_out, size_t error_out_size) {
    if (!link || !has_valid_token()) {
        snprintf(error_out, error_out_size, "Link intermediario ou Token invalido.");
        return 0;
    }
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    struct mem_buffer response;
    response.data = malloc(1); response.size = 0;
    
    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
    headers = curl_slist_append(headers, auth_header);
    
    char *encoded_link = curl_easy_escape(curl, link, 0);
    if (!encoded_link) {
        snprintf(error_out, error_out_size, "RD Unrestrict: Falha ao codificar (URL escape) o link.");
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(response.data);
        return 0;
    }

    char postfields[2048];
    snprintf(postfields, sizeof(postfields), "link=%s", encoded_link);
    curl_free(encoded_link);

    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.real-debrid.com/rest/1.0/unrestrict/link");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    int success = 0;
    if (res == CURLE_OK && http_code == 200) {
        cJSON *json = cJSON_Parse(response.data);
        if (json) {
            const cJSON *download_item = cJSON_GetObjectItemCaseSensitive(json, "download");
            if (cJSON_IsString(download_item) && download_item->valuestring != NULL) {
                strncpy(url_out, download_item->valuestring, 2048 - 1);
                success = 1;
            } else {
                 snprintf(error_out, error_out_size, "RD Unrestrict: Nao foi possivel extrair link final (JSON inesperado).");
            }
            cJSON_Delete(json);
        }
    } else {
        cJSON *json = cJSON_Parse(response.data);
        char api_error_msg[256] = "Erro desconhecido da API.";
        if (json) {
            const cJSON *error_item = cJSON_GetObjectItemCaseSensitive(json, "error");
            if(cJSON_IsString(error_item)) {
                strncpy(api_error_msg, error_item->valuestring, sizeof(api_error_msg) - 1);
            }
            cJSON_Delete(json);
        }
        snprintf(error_out, error_out_size, "RD Unrestrict: Falha (HTTP %ld) - %s", http_code, api_error_msg);
    }
    
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return success;
}