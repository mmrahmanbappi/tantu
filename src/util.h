/* tantu: small utilities shared by all modules. */
#ifndef TT_UTIL_H
#define TT_UTIL_H

#include <stddef.h>

/* Growable string buffer. Always nul-terminated after the first write. */
typedef struct {
    char *s;
    size_t len, cap;
} buf;

void buf_put(buf *b, const char *s, size_t n);
void buf_puts(buf *b, const char *s);
void buf_putc(buf *b, char c);
void buf_printf(buf *b, const char *fmt, ...);
char *buf_take(buf *b);
void buf_free(buf *b);

/* Escaping */
void esc_html(buf *b, const char *s, size_t n);
void esc_html_s(buf *b, const char *s);
void esc_json(buf *b, const char *s);

/* Memory */
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* Files */
char *read_file(const char *path, size_t *len);
int write_file(const char *path, const char *data, size_t len);
int mkdir_p(const char *path);
int copy_tree(const char *src, const char *dst);
int remove_tree(const char *path);
int is_dir(const char *path);
int is_file(const char *path);
char **list_files(const char *dir, const char *suffix, int *count);
void free_list(char **list, int n);
char *path_join(const char *a, const char *b);

/* Text */
char *slugify(const char *s);
char *trim(char *s);
char *strip_tags(const char *html, size_t max);
int word_count(const char *html);

/* Small string map (insertion ordered) */
typedef struct {
    char *k, *v;
} kv;
typedef struct {
    kv *items;
    int n, cap;
} map;

void map_set(map *m, const char *k, const char *v);
const char *map_get(const map *m, const char *k);
void map_free(map *m);

void die(const char *fmt, ...);

#endif
