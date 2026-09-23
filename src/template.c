#include "template.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *lookup(const tctx *c, const char *name) {
    for (; c; c = c->parent) {
        const char *v = map_get(c->vars, name);
        if (v) return v;
    }
    return NULL;
}

static const tlist *find_list(const tctx *c, const char *name) {
    for (; c; c = c->parent)
        for (int i = 0; i < c->nlists; i++)
            if (!strcmp(c->lists[i].name, name)) return &c->lists[i];
    return NULL;
}

static const map *find_partials(const tctx *c) {
    for (; c; c = c->parent)
        if (c->partials) return c->partials;
    return NULL;
}

static int truthy(const tctx *c, const char *name) {
    const tlist *l = find_list(c, name);
    if (l) return l->n > 0;
    const char *v = lookup(c, name);
    return v && *v && strcmp(v, "false") != 0 && strcmp(v, "0") != 0;
}

static int at(const char *t, size_t n, size_t i, const char *s) {
    size_t l = strlen(s);
    return i + l <= n && !memcmp(t + i, s, l);
}

/* Returns the position of the matching close tag, or n if missing.
 * Sets *elsep to the position of a top-level {{else}} if one is found. */
static size_t find_close(const char *t, size_t n, size_t i, const char *open, const char *close,
                         size_t *elsep) {
    int depth = 1;
    while (i < n) {
        if (at(t, n, i, open)) {
            depth++;
            i += strlen(open);
        } else if (at(t, n, i, close)) {
            if (--depth == 0) return i;
            i += strlen(close);
        } else if (elsep && depth == 1 && at(t, n, i, "{{else}}")) {
            *elsep = i;
            i += 8;
        } else {
            i++;
        }
    }
    return n;
}

static char *tag_name(const char *s, size_t n) {
    while (n && isspace((unsigned char)*s)) { s++; n--; }
    while (n && isspace((unsigned char)s[n - 1])) n--;
    return xstrndup(s, n);
}

void tpl_render(const char *t, size_t n, const tctx *c, buf *o) {
    size_t i = 0;
    while (i < n) {
        const char *open = NULL;
        for (size_t j = i; j + 1 < n; j++)
            if (t[j] == '{' && t[j + 1] == '{') { open = t + j; break; }
        if (!open) {
            buf_put(o, t + i, n - i);
            return;
        }
        size_t p = (size_t)(open - t);
        buf_put(o, t + i, p - i);

        if (at(t, n, p, "{{{")) {
            const char *e = strstr(t + p + 3, "}}}");
            if (!e || (size_t)(e - t) > n) { buf_put(o, t + p, n - p); return; }
            char *name = tag_name(t + p + 3, (size_t)(e - t) - p - 3);
            const char *v = lookup(c, name);
            if (v) buf_puts(o, v);
            free(name);
            i = (size_t)(e - t) + 3;
            continue;
        }

        const char *e = strstr(t + p + 2, "}}");
        if (!e || (size_t)(e - t) > n) { buf_put(o, t + p, n - p); return; }
        char *tag = tag_name(t + p + 2, (size_t)(e - t) - p - 2);
        size_t after = (size_t)(e - t) + 2;

        if (!strncmp(tag, "#if ", 4)) {
            char *name = trim(tag + 4);
            size_t elsep = 0;
            size_t cl = find_close(t, n, after, "{{#if ", "{{/if}}", &elsep);
            int ok = truthy(c, name);
            if (elsep) {
                if (ok) tpl_render(t + after, elsep - after, c, o);
                else tpl_render(t + elsep + 8, cl - elsep - 8, c, o);
            } else if (ok) {
                tpl_render(t + after, cl - after, c, o);
            }
            i = cl < n ? cl + 7 : n;
        } else if (!strncmp(tag, "#each ", 6)) {
            char *name = trim(tag + 6);
            size_t cl = find_close(t, n, after, "{{#each ", "{{/each}}", NULL);
            const tlist *l = find_list(c, name);
            if (l) {
                for (int k = 0; k < l->n; k++) {
                    tctx child = {l->items[k], c, NULL, 0, NULL};
                    tpl_render(t + after, cl - after, &child, o);
                }
            }
            i = cl < n ? cl + 9 : n;
        } else if (tag[0] == '>') {
            char *name = trim(tag + 1);
            const map *parts = find_partials(c);
            const char *body = map_get(parts, name);
            if (body) tpl_render(body, strlen(body), c, o);
            i = after;
        } else if (!strcmp(tag, "else") || !strcmp(tag, "/if") || !strcmp(tag, "/each")) {
            i = after; /* stray tag, ignore */
        } else {
            const char *v = lookup(c, tag);
            if (v) esc_html_s(o, v);
            i = after;
        }
        free(tag);
    }
}
