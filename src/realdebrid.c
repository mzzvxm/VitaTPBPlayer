#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "realdebrid.h"
#include "debugScreen.h"
#include <unistd.h> // Para a função sleep()

// aqui ce vai colocar teu token Real-Debrid
// para obter um token, crie uma conta em https://real-debrid.com e gere um token na seção "API" do painel de usuário.
// NÃO compartilhe esse token publicamente,
// ele é único e vinculado à sua conta Real-Debrid.
#ifndef RD_TOKEN
#define RD_TOKEN "COLOQUE_SEU_TOKEN_AQUI"
#endif

// buffer growth for curl write callback
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
    if (strlen(RD_TOKEN) < 10) return 0;
    return 1;
}

/**
 * rd_add_magnet:
 *   envia magnet para Real-Debrid e extrai o id retornado para id_out.
 *   id_out deve ter espaço para pelo menos 128 bytes.
 *   Retorna 1 em sucesso, 0 em falha.
 */
int rd_add_magnet(const char *magnet, char *id_out) {
    if (!magnet || !id_out) return 0;
    if (!has_token()) {
        psvDebugScreenPrintf("Real-Debrid token não encontrado. Edite o código e coloque RD_TOKEN.\n");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        psvDebugScreenPrintf("curl_easy_init failed\n");
        return 0;
    }

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
        psvDebugScreenPrintf("rd_add_magnet: curl error: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    psvDebugScreenPrintf("rd_add_magnet: response=%s\n", response.data);

    // procura "id":"..."
    char *id_ptr = strstr(response.data, "\"id\"");
    if (!id_ptr) {
        // às vezes a API retorna id sem a chave "id", tentar procurar "id":
        id_ptr = strstr(response.data, "id\":\"");
        if (!id_ptr) {
            psvDebugScreenPrintf("rd_add_magnet: id não encontrado na resposta\n");
            curl_slist_free_all(headers);
            free(response.data);
            curl_easy_cleanup(curl);
            return 0;
        }
    }

    // achar a sequência dentro de aspas
    char *start = strchr(response.data, '\"');
    // busca mais robusta: procurar "id":"...".
    char *id_start = strstr(response.data, "\"id\":\"");
    if (!id_start) id_start = strstr(response.data, "id\":\"");
    if (!id_start) {
        psvDebugScreenPrintf("rd_add_magnet: formato inesperado da resposta\n");
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }
    id_start += strlen("\"id\":\"");
    char *id_end = strchr(id_start, '\"');
    if (!id_end) {
        psvDebugScreenPrintf("rd_add_magnet: fim do id não encontrado\n");
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    size_t id_len = id_end - id_start;
    if (id_len >= 128) id_len = 127;
    strncpy(id_out, id_start, id_len);
    id_out[id_len] = '\0';

    psvDebugScreenPrintf("rd_add_magnet: id=%s\n", id_out);

    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 1;
}

/**
 * rd_wait_for_ready:
 *   Polling simples do status da torrent até 'ready' ou até timeout.
 *   Retorna 1 se pronto, 0 falha/timeout.
 *
 *   NOTE: É uma implementação mínima — em produção você deve checar o status do torrent
 *   via endpoint /torrents/info/{id} (ver docs Real-Debrid).
 */
int rd_wait_for_ready(const char *id) {
    if (!id) return 0;
    if (!has_token()) return 0;

    // Implementação simples: tente algumas vezes pedir info até achar "status":"ready"
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char url[512];
    snprintf(url, sizeof(url), "https://api.real-debrid.com/rest/1.0/torrents/info/%s", id);

    struct mem_buffer response;
    response.data = malloc(1);
    response.size = 0;

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", RD_TOKEN);
    headers = curl_slist_append(headers, auth_header);

    int attempts = 0;
    int max_attempts = 30; // timeout conservador
    while (attempts++ < max_attempts) {
        free(response.data);
        response.data = malloc(1);
        response.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            psvDebugScreenPrintf("rd_wait_for_ready: curl error: %s\n", curl_easy_strerror(res));
            // tentar novamente
        } else {
            psvDebugScreenPrintf("rd_wait_for_ready: info=%s\n", response.data);
            if (strstr(response.data, "\"status\":\"ready\"") || strstr(response.data, "\"status\": \"ready\"")) {
                curl_slist_free_all(headers);
                free(response.data);
                curl_easy_cleanup(curl);
                return 1;
            }
            // se tiver "status":"downloaded" também pode ser considerado pronto
            if (strstr(response.data, "\"status\":\"downloaded\"") || strstr(response.data, "\"status\": \"downloaded\"")) {
                curl_slist_free_all(headers);
                free(response.data);
                curl_easy_cleanup(curl);
                return 1;
            }
        }

#ifdef _WIN32
        Sleep(3000);
#else
        sleep(3);
#endif
    }

    psvDebugScreenPrintf("rd_wait_for_ready: timeout\n");
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 0;
}

/**
 * rd_get_file_url:
 *   Retorna a URL direto do arquivo a ser baixado (via endpoint /torrents/info ou /torrents/hosts).
 *   url_out precisa ter espaço para >= 1024 bytes.
 *
 *   Implementação mínima: tenta extrair um "link" ou "links" na resposta.
 */
int rd_get_file_url(const char *id, char *url_out) {
    if (!id || !url_out) return 0;
    if (!has_token()) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

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
        psvDebugScreenPrintf("rd_get_file_url: curl error: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 0;
    }

    psvDebugScreenPrintf("rd_get_file_url: info=%s\n", response.data);

    // Procurar por "link":"http..." ou "links":[{"file":"..."}]
    char *p = strstr(response.data, "\"link\":\"");
    if (!p) p = strstr(response.data, "\"file\":\"");
    if (p) {
        p += strchr(p, ':') ? 2 : 0; // move para depois :"
        // ajustar para pegar entre aspas
        char *start = strchr(p, '\"');
        if (!start) start = p;
        else start++;
        char *end = strchr(start, '\"');
        if (end && (end - start) < 1024) {
            size_t n = end - start;
            strncpy(url_out, start, n);
            url_out[n] = '\0';
            curl_slist_free_all(headers);
            free(response.data);
            curl_easy_cleanup(curl);
            return 1;
        }
    }

    // fallback: às vezes RD fornece 'links' array com host / link. tentar procurar http:// ou https://
    char *http = strstr(response.data, "http://");
    if (!http) http = strstr(response.data, "https://");
    if (http) {
        // extrair até aspas, vírgula ou espaço
        char *term = strpbrk(http, "\"', ]}");
        size_t n = term ? (size_t)(term - http) : strlen(http);
        if (n >= 1024) n = 1023;
        strncpy(url_out, http, n);
        url_out[n] = '\0';
        curl_slist_free_all(headers);
        free(response.data);
        curl_easy_cleanup(curl);
        return 1;
    }

    psvDebugScreenPrintf("rd_get_file_url: nenhuma URL encontrada\n");
    curl_slist_free_all(headers);
    free(response.data);
    curl_easy_cleanup(curl);
    return 0;
}
