#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "realdebrid.h"

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

static int has_token() {
    if (strlen(RD_TOKEN) < 10 || strcmp(RD_TOKEN, "COLOQUE_SEU_TOKEN_AQUI") == 0) return 0;
    return 1;
}

int rd_add_magnet(const char *magnet, char *id_out, char *error_out, size_t error_out_size) {
    if (!magnet || !id_out) return 0;
    if (!has_token()) {
        snprintf(error_out, error_out_size, "Token do Real-Debrid nao configurado.");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");

    char url[] = "https://api.real-debrid.com/rest/1.0/torrents/addMagnet";
    struct mem_buffer response;
    response.data = malloc(1);
    response.size = 0;

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", RD_TOKEN);
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, auth_header);

    char postfields[2048];
    snprintf(postfields, sizeof(postfields), "magnet=%s", magnet);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD Magnet: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    char *id_start = strstr(response.data, "\"id\":\"");
    if (!id_start) {
        snprintf(error_out, error_out_size, "RD Magnet: ID nao encontrado na resposta da API.");
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }
    id_start += strlen("\"id\":\"");
    char *id_end = strchr(id_start, '\"');
    if (!id_end) {
        snprintf(error_out, error_out_size, "RD Magnet: Resposta da API mal formatada.");
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    size_t id_len = id_end - id_start;
    if (id_len >= 128) id_len = 127;
    strncpy(id_out, id_start, id_len);
    id_out[id_len] = '\0';

    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 1;
}

int rd_wait_for_ready(const char *id, char *error_out, size_t error_out_size) {
    if (!id) return 0;
    if (!has_token()) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");

    char url[512];
    snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/info/%s", id);

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", RD_TOKEN);
    headers = curl_slist_append(headers, auth_header);

    int attempts = 0;
    int max_attempts = 30;
    while (attempts++ < max_attempts) {
        struct mem_buffer response;
        response.data = malloc(1);
        response.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            snprintf(error_out, error_out_size, "RD Wait: %s", curl_easy_strerror(res));
            curl_slist_free_all(headers);
            free(response.data);
            curl_easy_cleanup(curl);
            return 0;
        }

        if (strstr(response.data, "\"status\":\"downloaded\"") || strstr(response.data, "\"status\":\"ready\"")) {
            curl_slist_free_all(headers);
            free(response.data);
            curl_easy_cleanup(curl);
            return 1;
        }
        
        free(response.data);
        sleep(3);
    }

    snprintf(error_out, error_out_size, "RD Wait: Timeout esperando o torrent.");
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;
}

int rd_get_file_url(const char *id, char *url_out, char *error_out, size_t error_out_size) {
    if (!id || !url_out) return 0;
    if (!has_token()) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    
    curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:cacert.pem");

    char url[512];
    snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/info/%s", id);

    struct mem_buffer response;
    response.data = malloc(1);
    response.size = 0;

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", RD_TOKEN);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        snprintf(error_out, error_out_size, "RD Get URL: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    char *link_start = strstr(response.data, "\"unrestricted_link\":\"");
    if (link_start) {
        link_start += strlen("\"unrestricted_link\":\"");
        char *link_end = strchr(link_start, '\"');
        if (link_end && (link_end - link_start) < 2048) {
            size_t n = link_end - link_start;
            strncpy(url_out, link_start, n);
            url_out[n] = '\0';
            
            char *p1 = url_out;
            char *p2 = url_out;
            while (*p1) {
                if (*p1 == '\\' && *(p1 + 1) == '/') p1++;
                *p2++ = *p1++;
            }
            *p2 = '\0';

            curl_slist_free_all(headers);
            free(response.data);
            curl_easy_cleanup(curl);
            return 1;
        }
    }

    snprintf(error_out, error_out_size, "RD Get URL: Link nao encontrado na resposta da API.");
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 0;
}