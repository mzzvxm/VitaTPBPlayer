#define _XOPEN_SOURCE
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <psp2/kernel/threadmgr.h>
#include "realdebrid.h"

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

static int manual_json_extract(const char* json_buffer, const char* key, char* out, size_t out_size) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\": \"", key);

    const char* key_ptr = strstr(json_buffer, search_key);
    if (!key_ptr) {
        snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
        key_ptr = strstr(json_buffer, search_key);
        if (!key_ptr) return 0;
    }

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
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD User: %s", curl_easy_strerror(res));
        goto cleanup;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        snprintf(error_out, error_out_size, "RD User: Token invalido ou API falhou (HTTP: %ld)", http_code);
        goto cleanup;
    }

    char temp_buffer[256];
    manual_json_extract(response.data, "username", userInfo->username, sizeof(userInfo->username));
    manual_json_extract(response.data, "email", userInfo->email, sizeof(userInfo->email));
    if (manual_json_extract(response.data, "type", temp_buffer, sizeof(temp_buffer))) {
        if (strcmp(temp_buffer, "premium") == 0) {
            userInfo->is_premium = 1;
        }
    }
    if (manual_json_extract(response.data, "expiration", temp_buffer, sizeof(temp_buffer))) {
        struct tm tm = {0};
        if (strptime(temp_buffer, "%Y-%m-%dT%H:%M:%S", &tm)) {
            strftime(userInfo->expiration, sizeof(userInfo->expiration), "%d/%m/%Y", &tm);
        } else {
            strncpy(userInfo->expiration, "Data invalida", sizeof(userInfo->expiration)-1);
        }
    }

    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 1;

cleanup:
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 0;
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
    char url[] = "https://api.real-debrid.com/rest/1.0/torrents/addMagnet";
    struct mem_buffer response;
    response.data = malloc(1); response.size = 0;
    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_rd_token);
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, auth_header);
    char postfields[2048];
    snprintf(postfields, sizeof(postfields), "magnet=%s", magnet);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD Magnet: %s", curl_easy_strerror(res));
        goto cleanup;
    }
    if (!manual_json_extract(response.data, "id", id_out, 128)) {
        snprintf(error_out, error_out_size, "RD Magnet: ID nao encontrado na resposta da API.");
        goto cleanup;
    }
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 1;
cleanup:
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 0;
}

int rd_select_all_files(const char *id, char *error_out, size_t error_out_size) {
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
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "files=all");
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD SelectFiles: %s", curl_easy_strerror(res));
        return 0;
    }
    if (http_code == 202 || http_code == 204) {
        return 1;
    }
    snprintf(error_out, error_out_size, "RD SelectFiles: API retornou erro HTTP %ld", http_code);
    return 0;
}

int rd_get_intermediate_link(const char *id, char *link_out, char *error_out, size_t error_out_size) {
    if (!id || !has_valid_token()) {
        snprintf(error_out, error_out_size, "ID ou Token invalido.");
        return 0;
    }
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    char url[512];
    snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/info/%s", id);
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
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD Info: %s", curl_easy_strerror(res));
        goto cleanup;
    }
    const char *links_key = "\"links\": [\"";
    char *link_start = strstr(response.data, links_key);
    if (link_start) {
        link_start += strlen(links_key);
        char *link_end = strchr(link_start, '\"');
        if (link_end) {
            size_t len = link_end - link_start;
            if (len > 0 && len < 2048) {
                strncpy(link_out, link_start, len);
                link_out[len] = '\0';
                curl_slist_free_all(headers);
                free(response.data);
                curl_easy_cleanup(curl);
                return 1;
            }
        }
    }
    snprintf(error_out, error_out_size, "RD Info: Link intermediario nao disponivel ainda.");
cleanup:
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
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
    char postfields[2048];
    snprintf(postfields, sizeof(postfields), "link=%s", link);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.real-debrid.com/rest/1.0/unrestrict/link");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD Unrestrict: %s", curl_easy_strerror(res));
        goto cleanup;
    }
    if (!manual_json_extract(response.data, "download", url_out, 2048)) {
        snprintf(error_out, error_out_size, "RD Unrestrict: Nao foi possivel extrair link final.");
        goto cleanup;
    }
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 1;
cleanup:
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 0;
}