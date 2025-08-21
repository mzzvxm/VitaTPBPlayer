#include "token_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <psp2/io/fcntl.h>

// MODIFICADO: Usando .txt para consistência com o resto do app
#define TOKEN_PATH "ux0:data/VitaTPBPlayer/token.txt"

static int server_sock = -1;
static pthread_t server_thread;
static volatile int server_running = 0;

static const char *html_page =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=utf-8\r\n\r\n"
"<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><meta charset='utf-8'><title>Vita Token</title><style>body{font-family:sans-serif;background-color:#111;color:#eee;text-align:center;padding-top:20px;} input{width:90%;padding:10px;margin-bottom:15px;border-radius:5px;border:1px solid #555;background-color:#333;color:#eee;} input[type=submit]{width:auto;cursor:pointer;background-color:#007bff;color:white;}</style></head><body>"
"<h3>VitaTPBPlayer</h3>"
"<h4>Insira seu token Real-Debrid</h4>"
"<form id='f' method='post' action='/save'>"
"<input id='token' name='token' placeholder='Cole seu token aqui...' autofocus><br>"
"<input type='submit' value='Salvar Token'>"
"</form>"
"<script>document.getElementById('f').onsubmit = async function(e){e.preventDefault();"
"let t = document.getElementById('token').value;"
"if(!t){alert('O campo do token nao pode estar vazio.');return;}"
"let res = await fetch('/save', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'token='+encodeURIComponent(t)});"
"let j = await res.json(); alert(j.msg || j.status); if(j.status==='ok'){document.body.innerHTML='<h1>Sucesso!</h1><p>Token salvo no PS Vita. Voce ja pode fechar esta pagina.</p>'}};</script>"
"</body></html>";

static const char *ok_json_fmt = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\",\"msg\":\"%s\"}";
static const char *err_json_fmt = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n{\"status\":\"error\",\"msg\":\"%s\"}";

static void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if (*src == '%' && (a = src[1]) && (b = src[2]) && isxdigit(a) && isxdigit(b)) {
            char hex[3] = {a,b,0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
}

static int save_token_to_file(const char *token) {
    int fd = sceIoOpen(TOKEN_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd < 0) return -1;
    sceIoWrite(fd, token, strlen(token));
    sceIoClose(fd);
    return 0;
}

static void send_response(int client, const char *response) {
    send(client, response, strlen(response), 0);
}

static void handle_client(int client) {
    char buf[4096];
    int r = recv(client, buf, sizeof(buf)-1, 0);
    if (r <= 0) { close(client); return; }
    buf[r] = 0;

    if (strncmp(buf, "GET / ", 6) == 0) {
        send_response(client, html_page);
    } else if (strncmp(buf, "POST /save", 10) == 0) {
        char response_buffer[256];
        char *body = strstr(buf, "\r\n\r\n");
        if (!body) {
            snprintf(response_buffer, sizeof(response_buffer), err_json_fmt, "Corpo da requisicao invalido.");
            send_response(client, response_buffer);
            close(client); return;
        }
        body += 4;
        
        char *tokpos = strstr(body, "token=");
        if (!tokpos) {
            snprintf(response_buffer, sizeof(response_buffer), err_json_fmt, "Parametro 'token' ausente.");
            send_response(client, response_buffer);
            close(client); return;
        }
        
        char decoded[512];
        urldecode(decoded, tokpos + 6);
        
        if (save_token_to_file(decoded) == 0) {
            snprintf(response_buffer, sizeof(response_buffer), ok_json_fmt, "Token salvo com sucesso!");
            send_response(client, response_buffer);
        } else {
            snprintf(response_buffer, sizeof(response_buffer), err_json_fmt, "Falha ao salvar o arquivo no PS Vita.");
            send_response(client, response_buffer);
        }
    } else {
        const char *notf = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot found";
        send_response(client, notf);
    }
    close(client);
}

static void *server_thread_fn(void *arg) {
    int port = (int)(intptr_t)arg;
    struct sockaddr_in addr;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { server_running = 0; return NULL; }
    
    int yes = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(server_sock); server_sock = -1; server_running = 0; return NULL; }
    if (listen(server_sock, 4) < 0) { close(server_sock); server_sock = -1; server_running = 0; return NULL; }

    while (server_running) {
        int client = accept(server_sock, NULL, NULL);
        if (client >= 0) {
            handle_client(client);
        } else {
            break; 
        }
    }
    
    close(server_sock);
    server_sock = -1;
    return NULL;
}

int token_server_start(int port) {
    if (server_running) return 0;
    server_running = 1;
    if (pthread_create(&server_thread, NULL, server_thread_fn, (void*)(intptr_t)port) != 0) {
        server_running = 0;
        return -1;
    }
    return 0;
}

void token_server_stop(void) {
    if (!server_running) return;
    server_running = 0;
    if (server_sock >= 0) {
        shutdown(server_sock, SHUT_RDWR);
    }
    pthread_join(server_thread, NULL);
}

bool token_server_is_running(void) {
    return server_running != 0;
}