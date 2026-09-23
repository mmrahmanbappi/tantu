/* tantu serve: a small static file server for previewing a site.
 *
 * It binds to 127.0.0.1 only, so nobody else on the network can reach it.
 * Only GET and HEAD are allowed, paths containing ".." are refused, and
 * files are served from the output folder and nowhere else.
 */
#define _POSIX_C_SOURCE 200809L
#include "serve.h"
#include "util.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void serve_static(int fd, const char *root, const char *base, const char *method, char *raw);

static const char *mime(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    struct { const char *ext, *type; } t[] = {
        {".html", "text/html; charset=utf-8"}, {".css", "text/css; charset=utf-8"},
        {".js", "text/javascript; charset=utf-8"}, {".json", "application/json"},
        {".xml", "application/xml"}, {".txt", "text/plain; charset=utf-8"},
        {".svg", "image/svg+xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"}, {".webp", "image/webp"}, {".gif", "image/gif"},
        {".ico", "image/x-icon"}, {".woff2", "font/woff2"}, {".pdf", "application/pdf"},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(dot, t[i].ext)) return t[i].type;
    return "application/octet-stream";
}

void send_all(int fd, const char *d, size_t n) {
    while (n) {
        ssize_t w = write(fd, d, n);
        if (w <= 0) return;
        d += w;
        n -= (size_t)w;
    }
}

void http_respond(int fd, int code, const char *status, const char *type, const char *body,
                    size_t len, int head, const char *extra) {
    buf h = {0};
    buf_printf(&h,
               "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
               "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n%sConnection: close\r\n\r\n",
               code, status, type, len, extra ? extra : "");
    send_all(fd, h.s, h.len);
    if (!head && body) send_all(fd, body, len);
    buf_free(&h);
}

static int url_decode(char *s) {
    char *o = s;
    for (; *s; s++) {
        if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char hex[3] = {s[1], s[2], 0};
            int v = (int)strtol(hex, NULL, 16);
            if (v == 0) return -1;
            *o++ = (char)v;
            s += 2;
        } else {
            *o++ = *s;
        }
    }
    *o = '\0';
    return 0;
}

static void handle(int fd, const char *root, const char *base) {
    char req[8192];
    ssize_t n = read(fd, req, sizeof req - 1);
    if (n <= 0) return;
    req[n] = '\0';
    char method[8] = {0}, raw[4096] = {0};
    if (sscanf(req, "%7s %4095s", method, raw) != 2) return;
    serve_static(fd, root, base, method, raw);
}

void serve_static(int fd, const char *root, const char *base, const char *method, char *raw) {
    int head = !strcmp(method, "HEAD");
    if (strcmp(method, "GET") && !head) {
        http_respond(fd, 405, "Method Not Allowed", "text/plain", "Method not allowed\n", 19, 0,
                "Allow: GET, HEAD\r\n");
        return;
    }
    char *q = strpbrk(raw, "?#");
    if (q) *q = '\0';
    if (url_decode(raw) != 0 || raw[0] != '/' || strstr(raw, "..") || strchr(raw, '\\')) {
        http_respond(fd, 400, "Bad Request", "text/plain", "Bad request\n", 12, head, NULL);
        return;
    }
    const char *path = raw;
    size_t bl = strlen(base);
    if (bl && !strncmp(path, base, bl) && (path[bl] == '/' || path[bl] == '\0')) path += bl;
    if (!*path) path = "/";

    char *file = path_join(root, path);
    if (is_dir(file)) {
        if (path[strlen(path) - 1] != '/') {
            buf loc = {0};
            buf_printf(&loc, "Location: %s/\r\n", raw);
            http_respond(fd, 301, "Moved Permanently", "text/plain", "", 0, head, loc.s);
            buf_free(&loc);
            free(file);
            printf("301 %s\n", raw);
            return;
        }
        char *idx = path_join(file, "index.html");
        free(file);
        file = idx;
    }
    size_t len;
    char *data = is_file(file) ? read_file(file, &len) : NULL;
    if (data) {
        http_respond(fd, 200, "OK", mime(file), data, len, head, NULL);
        printf("200 %s\n", raw);
    } else {
        char *nf = path_join(root, "404.html");
        char *body = read_file(nf, &len);
        if (body) http_respond(fd, 404, "Not Found", "text/html; charset=utf-8", body, len, head, NULL);
        else http_respond(fd, 404, "Not Found", "text/plain", "Not found\n", 10, head, NULL);
        printf("404 %s\n", raw);
        free(body);
        free(nf);
    }
    fflush(stdout);
    free(data);
    free(file);
}

int serve_dir(const char *root, const char *base_path, int port) {
    signal(SIGPIPE, SIG_IGN);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) {
        fprintf(stderr, "tantu: port %d is busy. Try: tantu serve --port %d\n", port, port + 1);
        close(s);
        return 1;
    }
    listen(s, 16);
    printf("\nPreview your site at http://127.0.0.1:%d%s/\nPress Ctrl+C to stop.\n\n", port, base_path);
    fflush(stdout);
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) continue;
        handle(c, root, base_path);
        close(c);
    }
}
