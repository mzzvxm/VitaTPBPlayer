#ifndef REALDEBRID_H
#define REALDEBRID_H

int rd_add_magnet(const char *magnet, char *id_out);
int rd_wait_for_ready(const char *id);
int rd_get_file_url(const char *id, char *url_out);

#endif