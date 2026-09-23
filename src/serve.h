/* tantu serve: a local preview server. Listens on 127.0.0.1 only. */
#ifndef TT_SERVE_H
#define TT_SERVE_H

#include <stddef.h>

int serve_dir(const char *root, const char *base_path, int port);

/* Shared with the dashboard */
void serve_static(int fd, const char *root, const char *base, const char *method, char *raw);
void http_respond(int fd, int code, const char *status, const char *type, const char *body, size_t len,
                  int head, const char *extra);
void send_all(int fd, const char *d, size_t n);

#endif
