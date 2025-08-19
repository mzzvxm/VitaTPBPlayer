#ifndef REALDEBRID_H
#define REALDEBRID_H

// aqui ce vai colocar teu token Real-Debrid
// para obter um token, crie uma conta em https://real-debrid.com e gere um token na seção "API" do painel de usuário.
// NÃO compartilhe esse token publicamente,
// ele é único e vinculado à sua conta Real-Debrid.
#ifndef RD_TOKEN
#define RD_TOKEN "XXXX"
#endif

int rd_add_magnet(const char *magnet, char *id_out);
int rd_wait_for_ready(const char *id);
int rd_get_file_url(const char *id, char *url_out);

#endif
