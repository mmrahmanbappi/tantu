/* tantu ftp: upload the built site to shared hosting. */
#ifndef TT_FTP_H
#define TT_FTP_H

#include <stdio.h>

typedef struct {
    const char *host;
    int port;
    const char *user;
    const char *pass;
    const char *dir; /* remote folder, for example public_html */
    int all;         /* 1 = upload every file, not only changed ones */
} ftp_opts;

/* Uploads every changed file under local. Progress goes to log.
 * Returns 0 on success. */
int ftp_publish(const char *local, const ftp_opts *opts, FILE *log);

#endif
