/* tantu serve: a local preview server. Listens on 127.0.0.1 only. */
#ifndef TT_SERVE_H
#define TT_SERVE_H

int serve_dir(const char *root, const char *base_path, int port);

#endif
