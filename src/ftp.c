/* tantu ftp: uploads the built site to shared hosting over FTP.
 *
 * Only files that changed since the last upload are sent. A list of what was
 * uploaded is kept in .cache/ftp-<host>.txt. Remote files are never deleted.
 *
 * Plain FTP does not encrypt the password. tantu never stores the password.
 */
#define _XOPEN_SOURCE 700
#include "ftp.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "util.h"
#include "zip.h"

typedef struct {
    int fd;
    char rbuf[8192];
    size_t rlen;
    FILE *log;
    char last[1024];
} ftp_t;

static void set_timeouts(int fd) {
    struct timeval tv = {30, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

static int tcp_connect(const char *host, int port) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) return -1;
    int fd = -1;
    for (struct addrinfo *a = res; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        set_timeouts(fd);
        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static int send_all_fd(int fd, const char *d, size_t n) {
    while (n) {
        ssize_t w = send(fd, d, n, 0);
        if (w <= 0) return -1;
        d += w;
        n -= (size_t)w;
    }
    return 0;
}

/* Reads one line from the control connection. */
static int read_line(ftp_t *f, char *line, size_t cap) {
    for (;;) {
        char *nl = memchr(f->rbuf, '\n', f->rlen);
        if (nl) {
            size_t n = (size_t)(nl - f->rbuf) + 1;
            size_t c = n < cap - 1 ? n : cap - 1;
            memcpy(line, f->rbuf, c);
            line[c] = '\0';
            memmove(f->rbuf, f->rbuf + n, f->rlen - n);
            f->rlen -= n;
            return 0;
        }
        if (f->rlen == sizeof f->rbuf) f->rlen = 0;
        ssize_t r = recv(f->fd, f->rbuf + f->rlen, sizeof f->rbuf - f->rlen, 0);
        if (r <= 0) return -1;
        f->rlen += (size_t)r;
    }
}

/* Reads a full (possibly multi-line) reply and returns its code. */
static int reply(ftp_t *f) {
    char line[1024];
    if (read_line(f, line, sizeof line) != 0) return -1;
    if (strlen(line) < 4 || !isdigit((unsigned char)line[0])) return -1;
    int code = atoi(line);
    if (line[3] == '-') {
        char end[5];
        snprintf(end, sizeof end, "%.3s ", line);
        do {
            if (read_line(f, line, sizeof line) != 0) return -1;
        } while (strncmp(line, end, 4) != 0);
    }
    snprintf(f->last, sizeof f->last, "%s", line);
    size_t l = strlen(f->last);
    while (l && (f->last[l - 1] == '\n' || f->last[l - 1] == '\r')) f->last[--l] = '\0';
    return code;
}

static int cmd(ftp_t *f, const char *fmt, ...) {
    char line[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line - 3, fmt, ap);
    va_end(ap);
    /* never allow CR or LF inside a command */
    for (char *c = line; *c; c++) if (*c == '\r' || *c == '\n') *c = ' ';
    strcat(line, "\r\n");
    if (send_all_fd(f->fd, line, strlen(line)) != 0) return -1;
    return reply(f);
}

static int open_data(ftp_t *f) {
    int code = cmd(f, "PASV");
    if (code != 227) return -1;
    const char *p = strchr(f->last, '(');
    int h[6];
    if (!p || sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &h[0], &h[1], &h[2], &h[3], &h[4], &h[5]) != 6) return -1;
    int port = h[4] * 256 + h[5];
    /* Connect to the same address as the control connection. Servers behind
     * NAT often report a private address in the PASV reply. */
    struct sockaddr_in peer;
    socklen_t pl = sizeof peer;
    if (getpeername(f->fd, (struct sockaddr *)&peer, &pl) != 0) return -1;
    peer.sin_port = htons((unsigned short)port);
    int d = socket(AF_INET, SOCK_STREAM, 0);
    if (d < 0) return -1;
    set_timeouts(d);
    if (connect(d, (struct sockaddr *)&peer, sizeof peer) != 0) { close(d); return -1; }
    return d;
}

typedef struct {
    char **paths;
    unsigned *crcs;
    size_t *sizes;
    int n, cap;
} filelist;

static void walk(const char *root, const char *rel, filelist *fl) {
    char *full = *rel ? path_join(root, rel) : xstrdup(root);
    DIR *d = opendir(full);
    if (!d) { free(full); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || !strcmp(e->d_name, ".tantu")) continue;
        char *r = *rel ? path_join(rel, e->d_name) : xstrdup(e->d_name);
        char *p = path_join(root, r);
        if (is_dir(p)) walk(root, r, fl);
        else if (is_file(p)) {
            size_t n;
            char *data = read_file(p, &n);
            if (data) {
                if (fl->n == fl->cap) {
                    fl->cap = fl->cap ? fl->cap * 2 : 64;
                    fl->paths = xrealloc(fl->paths, sizeof(char *) * (size_t)fl->cap);
                    fl->crcs = xrealloc(fl->crcs, sizeof(unsigned) * (size_t)fl->cap);
                    fl->sizes = xrealloc(fl->sizes, sizeof(size_t) * (size_t)fl->cap);
                }
                fl->paths[fl->n] = xstrdup(r);
                fl->crcs[fl->n] = zip_crc32((unsigned char *)data, n);
                fl->sizes[fl->n] = n;
                fl->n++;
                free(data);
            }
        }
        free(p);
        free(r);
    }
    closedir(d);
    free(full);
}

static void logf_(FILE *log, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log, fmt, ap);
    va_end(ap);
    fflush(log);
}

int ftp_publish(const char *local, const ftp_opts *o, FILE *log) {
    if (!*o->host || !*o->user) { logf_(log, "Enter the FTP host and user name.\n"); return 1; }
    for (const char *c = o->host; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '.' && *c != '-') { logf_(log, "The FTP host looks wrong: use a name like ftp.example.com\n"); return 1; }

    filelist fl = {0};
    walk(local, "", &fl);
    if (!fl.n) { logf_(log, "Nothing to upload. Build the site first.\n"); return 1; }

    /* What was uploaded last time */
    char *hslug = slugify(o->host);
    buf mpath = {0};
    buf_printf(&mpath, ".cache/ftp-%s.txt", hslug);
    free(hslug);
    map prev = {0};
    if (!o->all) {
        char *m = read_file(mpath.s, NULL);
        if (m) {
            char *save = NULL;
            for (char *line = strtok_r(m, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
                char *sp = strchr(line, ' ');
                if (!sp) continue;
                *sp = '\0';
                map_set(&prev, sp + 1, line);
            }
            free(m);
        }
    }
    int todo = 0;
    char **need = xmalloc(sizeof(char *) * (size_t)fl.n);
    for (int i = 0; i < fl.n; i++) {
        char sig[32];
        snprintf(sig, sizeof sig, "%08x-%zu", fl.crcs[i], fl.sizes[i]);
        const char *was = map_get(&prev, fl.paths[i]);
        need[i] = (was && !strcmp(was, sig)) ? NULL : fl.paths[i];
        if (need[i]) todo++;
    }
    if (!todo) {
        logf_(log, "Everything is already online. Nothing changed since the last upload.\n");
        goto done_ok;
    }

    logf_(log, "Connecting to %s...\n", o->host);
    ftp_t f;
    memset(&f, 0, sizeof f);
    f.log = log;
    f.fd = tcp_connect(o->host, o->port ? o->port : 21);
    if (f.fd < 0) { logf_(log, "Could not connect to %s. Check the host name and port.\n", o->host); goto fail; }
    if (reply(&f) != 220) { logf_(log, "The server did not answer like an FTP server: %s\n", f.last); goto fail_close; }
    int code = cmd(&f, "USER %s", o->user);
    if (code == 331) code = cmd(&f, "PASS %s", o->pass);
    if (code != 230) { logf_(log, "Login failed: %s\n", f.last); goto fail_close; }
    logf_(log, "Logged in. Uploading %d of %d files...\n", todo, fl.n);
    if (cmd(&f, "TYPE I") != 200) { logf_(log, "Server refused binary mode: %s\n", f.last); goto fail_close; }

    const char *rd = *o->dir ? o->dir : "";
    map made = {0};
    int sent = 0, failed = 0;
    for (int i = 0; i < fl.n; i++) {
        if (!need[i]) continue;
        buf remote = {0};
        if (*rd) {
            buf_puts(&remote, rd);
            if (remote.s[remote.len - 1] != '/') buf_putc(&remote, '/');
        }
        buf_puts(&remote, need[i]);
        /* create parent folders */
        for (char *s = remote.s + 1; *s; s++) {
            if (*s != '/') continue;
            *s = '\0';
            if (!map_get(&made, remote.s)) {
                cmd(&f, "MKD %s", remote.s); /* fails harmlessly if it exists */
                map_set(&made, remote.s, "1");
            }
            *s = '/';
        }
        char *lp = path_join(local, need[i]);
        size_t n;
        char *data = read_file(lp, &n);
        free(lp);
        int d = data ? open_data(&f) : -1;
        int ok = 0;
        if (d >= 0) {
            code = cmd(&f, "STOR %s", remote.s);
            if (code == 150 || code == 125) {
                ok = send_all_fd(d, data, n) == 0;
                close(d);
                d = -1;
                code = reply(&f);
                ok = ok && (code == 226 || code == 250);
            }
            if (d >= 0) close(d);
        }
        if (ok) {
            sent++;
            logf_(log, "  sent %s\n", need[i]);
        } else {
            failed++;
            need[i] = NULL; /* do not record it as uploaded */
            logf_(log, "  FAILED %s (%s)\n", fl.paths[i], f.last);
        }
        free(data);
        buf_free(&remote);
    }
    cmd(&f, "QUIT");
    close(f.fd);
    map_free(&made);

    /* Remember what is online now */
    {
        buf m = {0};
        for (int i = 0; i < fl.n; i++) {
            const char *was = map_get(&prev, fl.paths[i]);
            char sig[32];
            snprintf(sig, sizeof sig, "%08x-%zu", fl.crcs[i], fl.sizes[i]);
            if (need[i]) buf_printf(&m, "%s %s\n", sig, fl.paths[i]);
            else if (was) buf_printf(&m, "%s %s\n", was, fl.paths[i]);
        }
        mkdir_p(".cache");
        write_file(mpath.s, m.s ? m.s : "", m.len);
        buf_free(&m);
    }
    logf_(log, "\nDone. Uploaded %d file%s%s.\n", sent, sent == 1 ? "" : "s",
          failed ? ", some failed (see above)" : "");
    for (int i = 0; i < fl.n; i++) free(fl.paths[i]);
    free(fl.paths); free(fl.crcs); free(fl.sizes); free(need);
    map_free(&prev);
    buf_free(&mpath);
    return failed ? 1 : 0;

fail_close:
    close(f.fd);
fail:
    for (int i = 0; i < fl.n; i++) free(fl.paths[i]);
    free(fl.paths); free(fl.crcs); free(fl.sizes); free(need);
    map_free(&prev);
    buf_free(&mpath);
    return 1;
done_ok:
    for (int i = 0; i < fl.n; i++) free(fl.paths[i]);
    free(fl.paths); free(fl.crcs); free(fl.sizes); free(need);
    map_free(&prev);
    buf_free(&mpath);
    return 0;
}
