#define _XOPEN_SOURCE 700
#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("tantu: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) die("out of memory");
    return q;
}

char *xstrndup(const char *s, size_t n) {
    char *d = xmalloc(n + 1);
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

char *xstrdup(const char *s) { return xstrndup(s, strlen(s)); }

/* ---------- buffer ---------- */

static void buf_grow(buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra + 1) cap *= 2;
    b->s = xrealloc(b->s, cap);
    b->cap = cap;
}

void buf_put(buf *b, const char *s, size_t n) {
    buf_grow(b, n);
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

void buf_puts(buf *b, const char *s) { buf_put(b, s, strlen(s)); }

void buf_putc(buf *b, char c) { buf_put(b, &c, 1); }

void buf_printf(buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return;
    }
    buf_grow(b, (size_t)n);
    vsnprintf(b->s + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

char *buf_take(buf *b) {
    char *s = b->s ? b->s : xstrdup("");
    b->s = NULL;
    b->len = b->cap = 0;
    return s;
}

void buf_free(buf *b) {
    free(b->s);
    b->s = NULL;
    b->len = b->cap = 0;
}

/* ---------- escaping ---------- */

void esc_html(buf *b, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        switch (s[i]) {
        case '&': buf_puts(b, "&amp;"); break;
        case '<': buf_puts(b, "&lt;"); break;
        case '>': buf_puts(b, "&gt;"); break;
        case '"': buf_puts(b, "&quot;"); break;
        case '\'': buf_puts(b, "&#39;"); break;
        default: buf_putc(b, s[i]);
        }
    }
}

void esc_html_s(buf *b, const char *s) { esc_html(b, s, strlen(s)); }

void esc_json(buf *b, const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"': buf_puts(b, "\\\""); break;
        case '\\': buf_puts(b, "\\\\"); break;
        case '\n': buf_puts(b, "\\n"); break;
        case '\r': buf_puts(b, "\\r"); break;
        case '\t': buf_puts(b, "\\t"); break;
        case '<': buf_puts(b, "\\u003c"); break; /* keeps </script> out of JSON-LD */
        case '>': buf_puts(b, "\\u003e"); break;
        case '&': buf_puts(b, "\\u0026"); break;
        default:
            if (c < 0x20) buf_printf(b, "\\u%04x", c);
            else buf_putc(b, (char)c);
        }
    }
}

/* ---------- files ---------- */

char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    buf b = {0};
    char tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) buf_put(&b, tmp, n);
    fclose(f);
    if (len) *len = b.len;
    return buf_take(&b);
}

int mkdir_p(const char *path) {
    char *p = xstrdup(path);
    for (char *q = p + 1; *q; q++) {
        if (*q == '/') {
            *q = '\0';
            if (mkdir(p, 0755) != 0 && errno != EEXIST) {
                free(p);
                return -1;
            }
            *q = '/';
        }
    }
    int rc = (mkdir(p, 0755) != 0 && errno != EEXIST) ? -1 : 0;
    free(p);
    return rc;
}

int write_file(const char *path, const char *data, size_t len) {
    char *dir = xstrdup(path);
    char *slash = strrchr(dir, '/');
    if (slash && slash != dir) {
        *slash = '\0';
        mkdir_p(dir);
    }
    free(dir);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, len, f);
    int rc = fclose(f);
    return (w == len && rc == 0) ? 0 : -1;
}

int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

char *path_join(const char *a, const char *b) {
    buf o = {0};
    buf_puts(&o, a);
    if (o.len && o.s[o.len - 1] != '/') buf_putc(&o, '/');
    while (*b == '/') b++;
    buf_puts(&o, b);
    return buf_take(&o);
}

int copy_tree(const char *src, const char *dst) {
    DIR *d = opendir(src);
    if (!d) return -1;
    mkdir_p(dst);
    struct dirent *e;
    int rc = 0;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || !strcmp(e->d_name, ".git"))
            continue;
        char *s = path_join(src, e->d_name);
        char *t = path_join(dst, e->d_name);
        if (is_dir(s)) {
            if (copy_tree(s, t) != 0) rc = -1;
        } else if (is_file(s)) {
            size_t n;
            char *data = read_file(s, &n);
            if (!data || write_file(t, data, n) != 0) rc = -1;
            free(data);
        }
        free(s);
        free(t);
    }
    closedir(d);
    return rc;
}

static int rm_cb(const char *p, const struct stat *st, int flag, struct FTW *ftw) {
    (void)st;
    (void)flag;
    (void)ftw;
    return remove(p);
}

int remove_tree(const char *path) {
    if (!is_dir(path)) return 0;
    return nftw(path, rm_cb, 16, FTW_DEPTH | FTW_PHYS);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

char **list_files(const char *dir, const char *suffix, int *count) {
    *count = 0;
    DIR *d = opendir(dir);
    if (!d) return NULL;
    int cap = 16, n = 0;
    char **list = xmalloc(sizeof(char *) * (size_t)cap);
    struct dirent *e;
    size_t sl = suffix ? strlen(suffix) : 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        size_t nl = strlen(e->d_name);
        if (suffix && (nl < sl || strcmp(e->d_name + nl - sl, suffix) != 0)) continue;
        char *full = path_join(dir, e->d_name);
        int ok = is_file(full);
        free(full);
        if (!ok) continue;
        if (n == cap) {
            cap *= 2;
            list = xrealloc(list, sizeof(char *) * (size_t)cap);
        }
        list[n++] = xstrdup(e->d_name);
    }
    closedir(d);
    qsort(list, (size_t)n, sizeof(char *), cmp_str);
    *count = n;
    return list;
}

void free_list(char **list, int n) {
    for (int i = 0; i < n; i++) free(list[i]);
    free(list);
}

/* ---------- text ---------- */

char *slugify(const char *s) {
    buf o = {0};
    int dash = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (isalnum(c)) {
            if (dash && o.len) buf_putc(&o, '-');
            buf_putc(&o, (char)tolower(c));
            dash = 0;
        } else {
            dash = 1;
        }
    }
    if (!o.len) buf_puts(&o, "page");
    return buf_take(&o);
}

char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

char *strip_tags(const char *html, size_t max) {
    buf o = {0};
    int in_tag = 0, space = 0;
    for (const char *p = html; *p; p++) {
        if (*p == '<') { in_tag = 1; space = 1; continue; }
        if (*p == '>') { in_tag = 0; continue; }
        if (in_tag) continue;
        if (*p == '&') {
            const char *ents[][2] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
                                     {"&quot;", "\""}, {"&#39;", "'"}};
            int hit = 0;
            for (int i = 0; i < 5; i++) {
                size_t l = strlen(ents[i][0]);
                if (!strncmp(p, ents[i][0], l)) {
                    if (space && o.len) buf_putc(&o, ' ');
                    space = 0;
                    buf_puts(&o, ents[i][1]);
                    p += l - 1;
                    hit = 1;
                    break;
                }
            }
            if (hit) continue;
        }
        if (isspace((unsigned char)*p)) { space = 1; continue; }
        if (space && o.len) buf_putc(&o, ' ');
        space = 0;
        buf_putc(&o, *p);
    }
    char *s = buf_take(&o);
    if (max && strlen(s) > max) {
        /* Prefer ending on a full sentence when one fits */
        for (size_t k = max; k > 60; k--) {
            if (s[k - 1] == '.' && s[k] == ' ') {
                s[k] = '\0';
                return s;
            }
        }
        size_t cut = max;
        while (cut > 0 && s[cut] != ' ') cut--;
        if (cut == 0) cut = max;
        s[cut] = '\0';
        /* drop trailing punctuation fragments */
        while (cut && (s[cut - 1] == ',' || s[cut - 1] == ';' || s[cut - 1] == ':')) s[--cut] = '\0';
    }
    return s;
}

int word_count(const char *html) {
    char *t = strip_tags(html, 0);
    int n = 0, in = 0;
    for (char *p = t; *p; p++) {
        if (isspace((unsigned char)*p)) in = 0;
        else if (!in) { in = 1; n++; }
    }
    free(t);
    return n;
}

/* ---------- map ---------- */

void map_set(map *m, const char *k, const char *v) {
    for (int i = 0; i < m->n; i++) {
        if (!strcmp(m->items[i].k, k)) {
            free(m->items[i].v);
            m->items[i].v = xstrdup(v ? v : "");
            return;
        }
    }
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 16;
        m->items = xrealloc(m->items, sizeof(kv) * (size_t)m->cap);
    }
    m->items[m->n].k = xstrdup(k);
    m->items[m->n].v = xstrdup(v ? v : "");
    m->n++;
}

const char *map_get(const map *m, const char *k) {
    if (!m) return NULL;
    for (int i = 0; i < m->n; i++)
        if (!strcmp(m->items[i].k, k)) return m->items[i].v;
    return NULL;
}

void map_free(map *m) {
    for (int i = 0; i < m->n; i++) {
        free(m->items[i].k);
        free(m->items[i].v);
    }
    free(m->items);
    m->items = NULL;
    m->n = m->cap = 0;
}
