#include "zip.h"

#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];

static void crc_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
}

static uint32_t crc32(const unsigned char *d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void u16(buf *b, unsigned v) {
    char x[2] = {(char)(v & 0xFF), (char)((v >> 8) & 0xFF)};
    buf_put(b, x, 2);
}

static void u32(buf *b, uint32_t v) {
    char x[4] = {(char)(v & 0xFF), (char)((v >> 8) & 0xFF), (char)((v >> 16) & 0xFF), (char)((v >> 24) & 0xFF)};
    buf_put(b, x, 4);
}

typedef struct {
    char *name;
    uint32_t crc, size, offset;
} entry;

typedef struct {
    entry *e;
    int n, cap;
} entries;

static int add_dir(const char *root, const char *rel, buf *out, entries *es) {
    char *full = *rel ? path_join(root, rel) : xstrdup(root);
    DIR *d = opendir(full);
    if (!d) { free(full); return -1; }
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") || !strcmp(de->d_name, ".tantu")) continue;
        char *r = *rel ? path_join(rel, de->d_name) : xstrdup(de->d_name);
        char *p = path_join(root, r);
        if (is_dir(p)) {
            add_dir(root, r, out, es);
        } else if (is_file(p)) {
            size_t n;
            char *data = read_file(p, &n);
            if (data && n < 0xFFFFFFFFu) {
                if (es->n == es->cap) {
                    es->cap = es->cap ? es->cap * 2 : 64;
                    es->e = xrealloc(es->e, sizeof(entry) * (size_t)es->cap);
                }
                entry *e = &es->e[es->n++];
                e->name = xstrdup(r);
                e->crc = crc32((unsigned char *)data, n);
                e->size = (uint32_t)n;
                e->offset = (uint32_t)out->len;
                u32(out, 0x04034b50);
                u16(out, 20); u16(out, 0x0800); u16(out, 0); /* version, UTF-8 flag, stored */
                u16(out, 0); u16(out, 0x21);                 /* time, date (1980-01-01) */
                u32(out, e->crc); u32(out, e->size); u32(out, e->size);
                u16(out, (unsigned)strlen(r)); u16(out, 0);
                buf_puts(out, r);
                buf_put(out, data, n);
            }
            free(data);
        }
        free(p);
        free(r);
    }
    closedir(d);
    free(full);
    return 0;
}

int zip_dir(const char *dir, buf *out) {
    crc_init();
    entries es = {0};
    if (add_dir(dir, "", out, &es) != 0) return -1;
    uint32_t cd_start = (uint32_t)out->len;
    for (int i = 0; i < es.n; i++) {
        entry *e = &es.e[i];
        u32(out, 0x02014b50);
        u16(out, 20); u16(out, 20); u16(out, 0x0800); u16(out, 0);
        u16(out, 0); u16(out, 0x21);
        u32(out, e->crc); u32(out, e->size); u32(out, e->size);
        u16(out, (unsigned)strlen(e->name)); u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0);
        u32(out, 0); u32(out, e->offset);
        buf_puts(out, e->name);
    }
    uint32_t cd_size = (uint32_t)out->len - cd_start;
    u32(out, 0x06054b50);
    u16(out, 0); u16(out, 0);
    u16(out, (unsigned)es.n); u16(out, (unsigned)es.n);
    u32(out, cd_size); u32(out, cd_start); u16(out, 0);
    int n = es.n;
    for (int i = 0; i < es.n; i++) free(es.e[i].name);
    free(es.e);
    return n;
}
