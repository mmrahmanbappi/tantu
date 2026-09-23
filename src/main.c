/* tantu: build fast, secure websites from Markdown.
 *
 *   tantu new <folder> [--theme blog|portfolio]
 *   tantu build [folder]
 *   tantu serve [folder] [--port 8000]
 */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <termios.h>
#include <sys/stat.h>

#include "embedded.h"
#include "image.h"
#include "zip.h"
#include "ftp.h"
#include "markdown.h"
#include "site.h"
#include "dashboard.h"
#include "seo.h"
#include "serve.h"
#include "template.h"
#include "util.h"

#define VERSION "1.0.0"
#define OUT "public"

/* ---------- config and front matter ---------- */

void parse_conf(const char *text, map *m) {
    char *copy = xstrdup(text);
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *t = trim(line);
        if (!*t || *t == '#') continue;
        char *eq = strchr(t, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = trim(t), *v = trim(eq + 1);
        size_t vl = strlen(v);
        if (vl >= 2 && ((v[0] == '"' && v[vl - 1] == '"') || (v[0] == '\'' && v[vl - 1] == '\''))) {
            v[vl - 1] = '\0';
            v++;
        }
        map_set(m, k, v);
    }
    free(copy);
}

/* Reads "---\nkey: value\n---" at the top of a file. Returns the body. */
const char *front_matter(const char *src, map *m) {
    if (strncmp(src, "---", 3) != 0) return src;
    const char *p = strchr(src, '\n');
    if (!p) return src;
    p++;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char *line = xstrndup(p, len);
        char *t = trim(line);
        p = nl ? nl + 1 : p + len;
        if (!strcmp(t, "---")) {
            free(line);
            return p;
        }
        char *colon = strchr(t, ':');
        if (colon) {
            *colon = '\0';
            char *k = trim(t), *v = trim(colon + 1);
            size_t vl = strlen(v);
            if (vl >= 2 && ((v[0] == '"' && v[vl - 1] == '"') || (v[0] == '\'' && v[vl - 1] == '\''))) {
                v[vl - 1] = '\0';
                v++;
            }
            map_set(m, k, v);
        }
        free(line);
    }
    return p;
}

static const char *get(const map *m, const char *k) {
    const char *v = map_get(m, k);
    return v ? v : "";
}

int valid_date(const char *d) {
    if (strlen(d) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) { if (d[i] != '-') return 0; }
        else if (!isdigit((unsigned char)d[i])) return 0;
    }
    return 1;
}

static char *date_display(const char *d) {
    static const char *months[] = {"January", "February", "March", "April", "May", "June", "July",
                                   "August", "September", "October", "November", "December"};
    if (!valid_date(d)) return xstrdup(d);
    int y = atoi(d), mo = atoi(d + 5), day = atoi(d + 8);
    if (mo < 1 || mo > 12) return xstrdup(d);
    buf o = {0};
    buf_printf(&o, "%d %s %d", day, months[mo - 1], y);
    return buf_take(&o);
}

/* ---------- documents ---------- */

typedef struct {
    map **items;
    int n, cap;
} doclist;

static void push(doclist *l, map *m) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->items = xrealloc(l->items, sizeof(map *) * (size_t)l->cap);
    }
    l->items[l->n++] = m;
}

static const char *sort_key = "date";

/* "date" sorts newest first. Any other key (order, start, ...) sorts
 * ascending, numerically when both values are numbers. */
static int by_sort_key(const void *a, const void *b) {
    const map *x = *(map *const *)a, *y = *(map *const *)b;
    int c;
    if (!strcmp(sort_key, "date")) {
        c = strcmp(get(y, "date"), get(x, "date"));
    } else {
        const char *vx = get(x, sort_key), *vy = get(y, sort_key);
        char *ex, *ey;
        double dx = strtod(vx, &ex), dy = strtod(vy, &ey);
        if (*vx && *vy && !*ex && !*ey) c = (dx > dy) - (dx < dy);
        else if (!*vx || !*vy) c = (!*vx) - (!*vy); /* items without a value go last */
        else c = strcmp(vx, vy);
    }
    return c ? c : strcmp(get(x, "title"), get(y, "title"));
}

static map *load_doc(const char *path, const char *fname, const char *base, int *skipped) {
    char *src = read_file(path, NULL);
    if (!src) die("cannot read %s", path);
    map *m = xmalloc(sizeof(map));
    memset(m, 0, sizeof(map));
    const char *body = front_matter(src, m);
    if (!strcmp(get(m, "draft"), "true")) {
        (*skipped)++;
        map_free(m);
        free(m);
        free(src);
        return NULL;
    }
    buf html = {0};
    md_to_html(body, base, &html);
    map_set(m, "content", html.s ? html.s : "");

    char *stem = xstrndup(fname, strlen(fname) - 3);
    /* "2026-09-20-welcome.md": the date prefix becomes the date, not the URL */
    const char *name = stem;
    if (strlen(stem) > 11 && stem[10] == '-') {
        char d[11];
        memcpy(d, stem, 10);
        d[10] = '\0';
        if (valid_date(d)) {
            if (!*get(m, "date")) map_set(m, "date", d);
            name = stem + 11;
        }
    }
    if (!*get(m, "title")) map_set(m, "title", name);
    char *slug = slugify(*get(m, "slug") ? get(m, "slug") : name);
    map_set(m, "slug", slug);

    if (*get(m, "date") && !valid_date(get(m, "date")))
        fprintf(stderr, "tantu: warning: %s: date should look like 2026-09-23\n", path);
    char *dd = date_display(get(m, "date"));
    map_set(m, "date_display", dd);
    /* Event times: "2026-11-14T09:00" -> "14 November 2026, 09:00" */
    const char *st = get(m, "start");
    if (strlen(st) >= 16 && st[10] == 'T') {
        char d[11];
        memcpy(d, st, 10);
        d[10] = '\0';
        if (valid_date(d)) {
            char *sd = date_display(d);
            buf t = {0};
            buf_printf(&t, "%s, %.5s", sd, st + 11);
            map_set(m, "start_display", t.s);
            buf_free(&t);
            free(sd);
        }
    }

    if (!*get(m, "description")) {
        char *d = strip_tags(html.s ? html.s : "", 155);
        map_set(m, "description", d);
        free(d);
    }
    int words = word_count(html.s ? html.s : "");
    char num[32];
    snprintf(num, sizeof num, "%d", words);
    map_set(m, "word_count", num);
    snprintf(num, sizeof num, "%d min read", words / 200 > 0 ? words / 200 : 1);
    map_set(m, "reading_time", num);

    free(dd);
    free(slug);
    free(stem);
    buf_free(&html);
    free(src);
    return m;
}

static void load_dir(const char *dir, const char *base, doclist *out, int *skipped) {
    int n = 0;
    char **files = list_files(dir, ".md", &n);
    for (int i = 0; i < n; i++) {
        char *p = path_join(dir, files[i]);
        map *m = load_doc(p, files[i], base, skipped);
        if (m) push(out, m);
        free(p);
    }
    free_list(files, n);
}

/* ---------- rendering ---------- */

typedef struct {
    map cfg;      /* raw site.conf values plus origin and base_path */
    map tvars;    /* site_* variables for templates */
    map partials;
    char *theme_dir;
    tlist lists[3];
    int nlists;
} site_t;

static char *load_template(site_t *s, const char *name) {
    char *p = path_join(s->theme_dir, name);
    char *t = read_file(p, NULL);
    if (!t) die("theme is missing %s", p);
    free(p);
    return t;
}

static int pages_written = 0;

/* ---------- images, social images, search ---------- */

static map imgmap; /* "/images/a.jpg" -> "srcset|width|height" */
static int og_made = 0, img_made = 0;

static int is_raster(const char *n) {
    size_t l = strlen(n);
    const char *e = strrchr(n, '.');
    if (!e || l < 5) return 0;
    return !strcasecmp(e, ".jpg") || !strcasecmp(e, ".jpeg") || !strcasecmp(e, ".png");
}

/* Makes 480, 960 and 1600 pixel wide copies of every JPEG and PNG in
 * static/images, cached in .cache/images so later builds are fast. */
static void process_images(const char *base) {
    static const int widths[] = {480, 960, 1600};
    int n = 0;
    char **files = list_files("static/images", NULL, &n);
    for (int i = 0; i < n; i++) {
        if (!is_raster(files[i])) continue;
        char *src = path_join("static/images", files[i]);
        int w = 0, h = 0;
        if (img_info(src, &w, &h) != 0 || w <= 0) { free(src); continue; }
        struct stat st;
        stat(src, &st);
        const char *dot = strrchr(files[i], '.');
        char *stem = xstrndup(files[i], (size_t)(dot - files[i]));
        buf set = {0};
        for (int k = 0; k < 3; k++) {
            if (widths[k] >= w) continue;
            buf cache = {0}, out = {0};
            buf_printf(&cache, ".cache/images/%s-%d-%lld-%lld%s", stem, widths[k], (long long)st.st_size,
                       (long long)st.st_mtime, dot);
            buf_printf(&out, OUT "/images/w/%s-%d%s", stem, widths[k], dot);
            if (!is_file(cache.s)) {
                if (img_resize(src, cache.s, widths[k]) != 0) { buf_free(&cache); buf_free(&out); continue; }
                img_made++;
            }
            size_t len;
            char *data = read_file(cache.s, &len);
            if (data) write_file(out.s, data, len);
            free(data);
            buf_printf(&set, "%s/images/w/%s-%d%s %dw, ", base, stem, widths[k], dot, widths[k]);
            buf_free(&cache);
            buf_free(&out);
        }
        if (set.len) {
            buf_printf(&set, "%s/images/%s %dw", base, files[i], w);
            buf v = {0}, key = {0};
            buf_printf(&v, "%s|%d|%d", set.s, w, h);
            buf_printf(&key, "/images/%s", files[i]);
            map_set(&imgmap, key.s, v.s);
            buf_free(&v);
            buf_free(&key);
        } else {
            buf v = {0}, key = {0};
            buf_printf(&v, "|%d|%d", w, h);
            buf_printf(&key, "/images/%s", files[i]);
            map_set(&imgmap, key.s, v.s);
            buf_free(&v);
            buf_free(&key);
        }
        buf_free(&set);
        free(stem);
        free(src);
    }
    free_list(files, n);
}

/* Adds srcset, sizes, width and height to <img> tags for local images. */
static char *enhance_images(const char *html, const char *base) {
    if (!imgmap.n) return NULL;
    buf o = {0};
    const char *p = html;
    size_t bl = strlen(base);
    int changed = 0;
    for (;;) {
        const char *tag = strstr(p, "<img ");
        if (!tag) break;
        const char *end = strchr(tag, '>');
        if (!end) break;
        buf_put(&o, p, (size_t)(tag - p));
        char *t = xstrndup(tag, (size_t)(end - tag));
        const char *src = strstr(t, " src=\"");
        const char *v = NULL;
        if (src && !strstr(t, "srcset=")) {
            src += 6;
            const char *q = strchr(src, '"');
            if (q) {
                char *u = xstrndup(src, (size_t)(q - src));
                const char *key = u;
                if (bl && !strncmp(u, base, bl)) key = u + bl;
                v = map_get(&imgmap, key);
                if (v) {
                    char *copy = xstrdup(v);
                    char *a = strchr(copy, '|');
                    char *b2 = a ? strchr(a + 1, '|') : NULL;
                    if (a && b2) {
                        *a = '\0';
                        *b2 = '\0';
                        buf_puts(&o, t);
                        if (*copy) buf_printf(&o, " srcset=\"%s\" sizes=\"(max-width: 900px) 100vw, 900px\"", copy);
                        if (!strstr(t, "width=")) buf_printf(&o, " width=\"%s\" height=\"%s\"", a + 1, b2 + 1);
                        changed = 1;
                    } else v = NULL;
                    free(copy);
                }
                free(u);
            }
        }
        if (!v) buf_puts(&o, t);
        buf_putc(&o, '>');
        free(t);
        p = end + 1;
    }
    buf_puts(&o, p);
    if (!changed) { buf_free(&o); return NULL; }
    return buf_take(&o);
}

static map themecfg;

/* Creates /og/<key>.png for pages without their own image. */
static void make_og(site_t *s, map *page, const char *key) {
    if (!strcmp(get(&s->cfg, "auto_social_images"), "false")) return;
    if (*get(page, "image")) return;
    const char *title = get(page, "title");
    if (!strcmp(get(page, "kind"), "home") && *get(&s->cfg, "tagline")) title = get(&s->cfg, "tagline");
    const char *bg = *get(&themecfg, "og_bg") ? get(&themecfg, "og_bg") : "#ffffff";
    const char *fg = *get(&themecfg, "og_fg") ? get(&themecfg, "og_fg") : "#1c1f24";
    const char *ac = *get(&themecfg, "og_accent") ? get(&themecfg, "og_accent") : "#1f2a5c";
    const char *origin = get(&s->cfg, "origin");
    const char *host = strstr(origin, "://");
    host = host ? host + 3 : origin;
    buf footer = {0}, sig = {0}, cache = {0}, out = {0}, url = {0};
    buf_printf(&footer, "%s%s", host, get(&s->cfg, "base_path"));
    buf_printf(&sig, "v1|%s|%s|%s|%s|%s|%s", get(&s->cfg, "title"), title, footer.s, bg, fg, ac);
    char *slug = slugify(key);
    buf_printf(&cache, ".cache/og/%08x.png", zip_crc32((const unsigned char *)sig.s, sig.len));
    buf_printf(&out, OUT "/og/%s.png", slug);
    int ok = 1;
    if (!is_file(cache.s)) {
        int rc = og_render(cache.s, get(&s->cfg, "title"), title, footer.s, bg, fg, ac);
        ok = rc == 0;
        if (ok) og_made++;
    }
    if (ok) {
        size_t len;
        char *data = read_file(cache.s, &len);
        if (data && write_file(out.s, data, len) == 0) {
            buf_printf(&url, "/og/%s.png", slug);
            map_set(page, "og_auto", url.s);
        }
        free(data);
    }
    free(slug);
    buf_free(&footer); buf_free(&sig); buf_free(&cache); buf_free(&out); buf_free(&url);
}

static void json_field(buf *o, const char *k, const char *v, int comma) {
    if (comma) buf_putc(o, ',');
    buf_printf(o, "\"%s\":\"", k);
    esc_json(o, v);
    buf_putc(o, '"');
}

static void add_to_index(buf *o, const map *d, int *first) {
    if (!strcmp(get(d, "robots"), "noindex")) return;
    if (!*first) buf_puts(o, ",\n");
    *first = 0;
    char *text = strip_tags(get(d, "content"), 1500);
    buf_putc(o, '{');
    json_field(o, "t", get(d, "title"), 0);
    json_field(o, "u", get(d, "url"), 1);
    json_field(o, "d", get(d, "description"), 1);
    json_field(o, "g", get(d, "tags"), 1);
    json_field(o, "x", text, 1);
    buf_putc(o, '}');
    free(text);
}

static void write_embedded(const char *path, const char *dst) {
    size_t n = 0;
    const unsigned char *d = embedded_get(path, &n);
    if (d && write_file(dst, (const char *)d, n) != 0) die("cannot write %s", dst);
}

static int valid_ga(const char *id) {
    size_t l = strlen(id);
    if (l < 6 || l > 20 || strncmp(id, "G-", 2) != 0) return 0;
    for (size_t i = 2; i < l; i++)
        if (!isupper((unsigned char)id[i]) && !isdigit((unsigned char)id[i])) return 0;
    return 1;
}



static void render(site_t *s, const char *tpl, map *page, const char *out_path) {
    buf head = {0};
    seo_head(&s->cfg, page, &head);
    map_set(page, "seo", head.s ? head.s : "");
    buf_free(&head);

    tctx root = {&s->tvars, NULL, s->lists, s->nlists, &s->partials};
    tctx ctx = {page, &root, NULL, 0, NULL};
    buf o = {0};
    tpl_render(tpl, strlen(tpl), &ctx, &o);
    char *better = enhance_images(o.s ? o.s : "", get(&s->cfg, "base_path"));
    if (better) {
        buf_free(&o);
        o.s = better;
        o.len = strlen(better);
    }
    if (write_file(out_path, o.s ? o.s : "", o.len) != 0) die("cannot write %s", out_path);
    buf_free(&o);
    pages_written++;
}

static char *out_for(const char *url_path_no_base) {
    /* "/about/" -> "public/about/index.html" */
    buf o = {0};
    buf_puts(&o, OUT);
    buf_puts(&o, url_path_no_base);
    if (o.s[o.len - 1] != '/') buf_putc(&o, '/');
    buf_puts(&o, "index.html");
    return buf_take(&o);
}

static void load_partials(site_t *s) {
    char *dir = path_join(s->theme_dir, "partials");
    int n = 0;
    char **files = list_files(dir, ".html", &n);
    for (int i = 0; i < n; i++) {
        char *p = path_join(dir, files[i]);
        char *body = read_file(p, NULL);
        char *name = xstrndup(files[i], strlen(files[i]) - 5);
        map_set(&s->partials, name, body ? body : "");
        free(name);
        free(body);
        free(p);
    }
    free_list(files, n);
    free(dir);
}

static void write_text(const char *path, buf *b) {
    if (write_file(path, b->s ? b->s : "", b->len) != 0) die("cannot write %s", path);
    buf_free(b);
}

int cmd_build(const char *dir, int quiet) {
    if (dir && chdir(dir) != 0) die("folder not found: %s", dir);
    char *conf = read_file("site.conf", NULL);
    if (!conf) die("no site.conf here. Run this inside a site folder, or create one with: tantu new mysite");

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    site_t s;
    memset(&s, 0, sizeof s);
    parse_conf(conf, &s.cfg);
    free(conf);

    if (!*get(&s.cfg, "title")) die("site.conf needs a title");
    if (!*get(&s.cfg, "base_url")) die("site.conf needs a base_url, for example https://yourname.github.io");
    if (!*get(&s.cfg, "language")) map_set(&s.cfg, "language", "en");
    if (!*get(&s.cfg, "locale")) map_set(&s.cfg, "locale", "en_US");
    if (!*get(&s.cfg, "author")) map_set(&s.cfg, "author", get(&s.cfg, "title"));
    if (!*get(&s.cfg, "theme")) map_set(&s.cfg, "theme", "blog");
    if (!*get(&s.cfg, "section")) map_set(&s.cfg, "section", "blog");
    if (!*get(&s.cfg, "section_title")) map_set(&s.cfg, "section_title", "Blog");

    /* Split base_url into origin and base path */
    char *bu = xstrdup(get(&s.cfg, "base_url"));
    size_t bl = strlen(bu);
    while (bl && bu[bl - 1] == '/') bu[--bl] = '\0';
    char *scheme = strstr(bu, "://");
    if (!scheme) die("base_url must start with https:// or http://");
    char *slash = strchr(scheme + 3, '/');
    if (slash) {
        map_set(&s.cfg, "base_path", slash);
        *slash = '\0';
    } else {
        map_set(&s.cfg, "base_path", "");
    }
    map_set(&s.cfg, "origin", bu);
    free(bu);
    const char *base = get(&s.cfg, "base_path");
    const char *section = get(&s.cfg, "section");
    char *sec_slug = slugify(section);
    map_set(&s.cfg, "section", sec_slug);
    free(sec_slug);
    section = get(&s.cfg, "section");

    for (int i = 0; i < s.cfg.n; i++) {
        char key[256];
        snprintf(key, sizeof key, "site_%s", s.cfg.items[i].k);
        map_set(&s.tvars, key, s.cfg.items[i].v);
    }
    map_set(&s.tvars, "base_path", base);
    char year[8];
    time_t now = time(NULL);
    strftime(year, sizeof year, "%Y", localtime(&now));
    map_set(&s.tvars, "year", year);
    buf lu = {0};
    buf_printf(&lu, "%s/%s/", base, section);
    char *list_url = buf_take(&lu);
    map_set(&s.tvars, "list_url", list_url);

    s.theme_dir = path_join("themes", get(&s.cfg, "theme"));
    if (!is_dir(s.theme_dir)) die("theme not found: %s", s.theme_dir);
    load_partials(&s);
    {
        map_free(&themecfg);
        char *tc = path_join(s.theme_dir, "theme.conf");
        char *tt = read_file(tc, NULL);
        if (tt) parse_conf(tt, &themecfg);
        free(tt);
        free(tc);
    }
    int search_on = strcmp(get(&s.cfg, "search"), "false") != 0;
    if (search_on) {
        buf su = {0};
        buf_printf(&su, "%s/search/", base);
        map_set(&s.tvars, "search_url", su.s);
        buf_free(&su);
    }
    if (*get(&s.cfg, "google_analytics") && !valid_ga(get(&s.cfg, "google_analytics"))) {
        fprintf(stderr, "tantu: warning: google_analytics should look like G-XXXXXXXXXX, ignoring it\n");
        map_set(&s.cfg, "google_analytics", "");
    }
    char *t_home = load_template(&s, "home.html");
    char *t_page = load_template(&s, "page.html");
    char *t_post = load_template(&s, "post.html");
    char *t_list = load_template(&s, "list.html");

    /* Menu: "Home:/, Blog:/blog/, About:/about/" */
    doclist menu = {0};
    char *mcopy = xstrdup(get(&s.cfg, "menu"));
    char *save = NULL;
    for (char *tok = strtok_r(mcopy, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char *colon = strchr(tok, ':');
        if (!colon) continue;
        *colon = '\0';
        char *label = trim(tok), *url = trim(colon + 1);
        map *m = xmalloc(sizeof(map));
        memset(m, 0, sizeof(map));
        map_set(m, "label", label);
        buf u = {0};
        if (url[0] == '/') buf_puts(&u, base);
        buf_puts(&u, url);
        map_set(m, "url", u.s);
        buf_free(&u);
        push(&menu, m);
    }
    free(mcopy);

    /* Content */
    int skipped = 0;
    doclist posts = {0}, pages = {0};
    load_dir("content/posts", base, &posts, &skipped);
    load_dir("content/pages", base, &pages, &skipped);
    sort_key = *get(&s.cfg, "sort") ? get(&s.cfg, "sort") : "date";
    qsort(posts.items, (size_t)posts.n, sizeof(map *), by_sort_key);

    for (int i = 0; i < posts.n; i++) {
        buf u = {0};
        buf_printf(&u, "%s/%s/%s/", base, section, get(posts.items[i], "slug"));
        map_set(posts.items[i], "url", u.s);
        map_set(posts.items[i], "kind", "post");
        map_set(posts.items[i], "list_url", list_url);
        buf_free(&u);
    }
    for (int i = 0; i < posts.n; i++) {
        if (i > 0) {
            map_set(posts.items[i], "prev_url", get(posts.items[i - 1], "url"));
            map_set(posts.items[i], "prev_title", get(posts.items[i - 1], "title"));
        }
        if (i + 1 < posts.n) {
            map_set(posts.items[i], "next_url", get(posts.items[i + 1], "url"));
            map_set(posts.items[i], "next_title", get(posts.items[i + 1], "title"));
        }
        char idx[16];
        snprintf(idx, sizeof idx, "%d", i + 1);
        map_set(posts.items[i], "position", idx);
    }
    for (int i = 0; i < pages.n; i++) {
        const char *sl = get(pages.items[i], "slug");
        if (!strcmp(sl, section) || !strcmp(sl, "assets") || !strcmp(sl, "index") || !strcmp(sl, "search") ||
            !strcmp(sl, "og") || !strcmp(sl, "images"))
            die("page slug \"%s\" is reserved. Pick another slug in content/pages.", sl);
        buf u = {0};
        buf_printf(&u, "%s/%s/", base, sl);
        map_set(pages.items[i], "url", u.s);
        map_set(pages.items[i], "kind", "page");
        buf_free(&u);
    }

    doclist recent = {0};
    int max_recent = atoi(get(&s.cfg, "home_posts"));
    if (max_recent <= 0) max_recent = 6;
    for (int i = 0; i < posts.n && i < max_recent; i++) push(&recent, posts.items[i]);

    s.lists[0] = (tlist){"menu", menu.items, menu.n};
    s.lists[1] = (tlist){"posts", posts.items, posts.n};
    s.lists[2] = (tlist){"recent", recent.items, recent.n};
    s.nlists = 3;

    /* Fresh output folder. Only folders created by tantu are removed. */
    if (is_dir(OUT)) {
        if (!is_file(OUT "/.tantu"))
            die("the folder \"" OUT "\" exists but was not made by tantu. Move it away and try again.");
        remove_tree(OUT);
    }
    mkdir_p(OUT);
    write_file(OUT "/.tantu", "made by tantu\n", 14);

    {
        char *assets_dir = path_join(s.theme_dir, "assets");
        if (is_dir(assets_dir)) copy_tree(assets_dir, OUT "/assets");
        free(assets_dir);
    }
    if (is_dir("static")) copy_tree("static", OUT);
    map_free(&imgmap);
    og_made = img_made = 0;
    if (is_dir("static/images")) process_images(base);
    if (search_on) {
        write_embedded("assets/site/tantu-search.js", OUT "/assets/tantu-search.js");
        write_embedded("assets/site/tantu-search.css", OUT "/assets/tantu-search.css");
    }
    if (*get(&s.cfg, "google_analytics")) write_embedded("assets/site/tantu-analytics.js", OUT "/assets/tantu-analytics.js");

    doclist all = {0};

    /* Home page */
    map home = {0};
    char *home_src = read_file("content/index.md", NULL);
    if (home_src) {
        const char *body = front_matter(home_src, &home);
        buf h = {0};
        md_to_html(body, base, &h);
        map_set(&home, "content", h.s ? h.s : "");
        buf_free(&h);
        free(home_src);
    }
    map_set(&home, "kind", "home");
    map_set(&home, "title", get(&s.cfg, "title"));
    if (!*get(&home, "description")) map_set(&home, "description", get(&s.cfg, "description"));
    buf hu = {0};
    buf_printf(&hu, "%s/", base);
    map_set(&home, "url", hu.s);
    buf_free(&hu);
    make_og(&s, &home, "home");
    render(&s, t_home, &home, OUT "/index.html");
    push(&all, &home);

    /* Section list */
    map list = {0};
    map_set(&list, "kind", "list");
    map_set(&list, "title", get(&s.cfg, "section_title"));
    buf ld = {0};
    buf_printf(&ld, "All %s from %s.", get(&s.cfg, "section_title"), get(&s.cfg, "title"));
    map_set(&list, "description", *get(&s.cfg, "section_description") ? get(&s.cfg, "section_description") : ld.s);
    buf_free(&ld);
    map_set(&list, "url", list_url);
    if (posts.n) map_set(&list, "updated", get(posts.items[0], "date"));
    buf lo = {0};
    buf_printf(&lo, "/%s/", section);
    char *list_out = out_for(lo.s);
    buf_free(&lo);
    make_og(&s, &list, section);
    render(&s, t_list, &list, list_out);
    free(list_out);
    push(&all, &list);

    for (int i = 0; i < pages.n; i++) {
        buf p = {0};
        buf_printf(&p, "/%s/", get(pages.items[i], "slug"));
        char *out = out_for(p.s);
        make_og(&s, pages.items[i], get(pages.items[i], "slug"));
        render(&s, t_page, pages.items[i], out);
        free(out);
        buf_free(&p);
        push(&all, pages.items[i]);
    }
    for (int i = 0; i < posts.n; i++) {
        buf p = {0};
        buf_printf(&p, "/%s/%s/", section, get(posts.items[i], "slug"));
        char *out = out_for(p.s);
        buf ok = {0};
        buf_printf(&ok, "%s-%s", section, get(posts.items[i], "slug"));
        make_og(&s, posts.items[i], ok.s);
        buf_free(&ok);
        render(&s, t_post, posts.items[i], out);
        free(out);
        buf_free(&p);
        push(&all, posts.items[i]);
    }

    /* Search page and index */
    if (search_on) {
        buf idx = {0};
        int first = 1;
        buf_puts(&idx, "[\n");
        add_to_index(&idx, &home, &first);
        for (int i = 0; i < pages.n; i++) add_to_index(&idx, pages.items[i], &first);
        for (int i = 0; i < posts.n; i++) add_to_index(&idx, posts.items[i], &first);
        buf_puts(&idx, "\n]\n");
        write_text(OUT "/search-index.json", &idx);
        map sp = {0};
        map_set(&sp, "kind", "page");
        map_set(&sp, "title", "Search");
        map_set(&sp, "robots", "noindex");
        buf sd = {0};
        buf_printf(&sd, "Search %s.", get(&s.cfg, "title"));
        map_set(&sp, "description", sd.s);
        buf_free(&sd);
        buf su = {0};
        buf_printf(&su, "%s/search/", base);
        map_set(&sp, "url", su.s);
        buf_free(&su);
        buf sc = {0};
        buf_printf(&sc,
            "<link rel=\"stylesheet\" href=\"%s/assets/tantu-search.css\">\n"
            "<div id=\"tantu-search\" data-index=\"%s/search-index.json\">\n"
            "<form role=\"search\"><label for=\"ts-q\">Search this site</label>"
            "<input id=\"ts-q\" type=\"search\" name=\"q\" placeholder=\"Type to search\" autocomplete=\"off\">"
            "<button type=\"submit\">Search</button></form>\n"
            "<p class=\"ts-status\" role=\"status\" aria-live=\"polite\"></p>\n<ol></ol>\n"
            "<noscript><p>Search needs JavaScript. You can browse all pages from the menu.</p></noscript>\n"
            "</div>\n<script src=\"%s/assets/tantu-search.js\" defer></script>\n", base, base, base);
        map_set(&sp, "content", sc.s);
        buf_free(&sc);
        render(&s, t_page, &sp, OUT "/search/index.html");
        map_free(&sp);
    }

    /* 404 page */
    map nf = {0};
    map_set(&nf, "kind", "404");
    map_set(&nf, "title", "Page not found");
    map_set(&nf, "robots", "noindex");
    map_set(&nf, "description", "The page you are looking for does not exist.");
    buf nfu = {0};
    buf_printf(&nfu, "%s/404.html", base);
    map_set(&nf, "url", nfu.s);
    buf_free(&nfu);
    buf nfc = {0};
    buf_printf(&nfc, "<p>This page does not exist or has moved.</p>\n<p><a href=\"%s/\">Go to the home page</a></p>\n", base);
    map_set(&nf, "content", nfc.s);
    buf_free(&nfc);
    render(&s, t_page, &nf, OUT "/404.html");

    /* Site-wide files */
    buf b = {0};
    seo_sitemap(&s.cfg, all.items, all.n, &b);
    write_text(OUT "/sitemap.xml", &b);
    seo_robots(&s.cfg, &b);
    write_text(OUT "/robots.txt", &b);
    seo_feed(&s.cfg, posts.items, posts.n, &b);
    write_text(OUT "/feed.xml", &b);
    seo_htaccess(&s.cfg, &b);
    write_text(OUT "/.htaccess", &b);
    seo_headers_file(&s.cfg, &b);
    write_text(OUT "/_headers", &b);
    write_file(OUT "/.nojekyll", "", 0);


    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (double)(t1.tv_sec - t0.tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
    if (!quiet) {
        printf("Built %d pages (%d %s, %d %s) in %.1f ms", pages_written, posts.n,
               posts.n == 1 ? "post" : "posts", pages.n, pages.n == 1 ? "page" : "pages", ms);
        if (skipped) printf(", skipped %d drafts", skipped);
        if (img_made) printf(", resized %d images", img_made);
        if (og_made) printf(", drew %d social images", og_made);
        printf("\nYour site is ready in the \"%s\" folder.\n", OUT);
    }

    /* cleanup */
    for (int i = 0; i < posts.n; i++) { map_free(posts.items[i]); free(posts.items[i]); }
    for (int i = 0; i < pages.n; i++) { map_free(pages.items[i]); free(pages.items[i]); }
    for (int i = 0; i < menu.n; i++) { map_free(menu.items[i]); free(menu.items[i]); }
    free(posts.items);
    free(pages.items);
    free(menu.items);
    free(recent.items);
    free(all.items);
    map_free(&home);
    map_free(&list);
    map_free(&nf);
    free(t_home);
    free(t_page);
    free(t_post);
    free(t_list);
    free(list_url);
    free(s.theme_dir);
    map_free(&s.partials);
    map_free(&s.tvars);
    map_free(&s.cfg);
    return 0;
}

/* ---------- new ---------- */

static int cmd_new(const char *dir, const char *theme) {
    if (is_dir(dir) || is_file(dir)) die("\"%s\" already exists. Pick a new folder name.", dir);
    if (!theme_exists(theme)) {
        fprintf(stderr, "tantu: unknown theme \"%s\". Available themes:\n", theme);
        list_themes(stderr);
        return 1;
    }
    buf sp = {0}, tp = {0}, td = {0};
    buf_printf(&sp, "starters/%s/", theme);
    buf_printf(&tp, "themes/%s/", theme);
    buf_printf(&td, "%s/themes/%s", dir, theme);
    if (extract_prefix(sp.s, dir) <= 0) die("could not create %s", dir);
    extract_prefix(tp.s, td.s);
    buf_free(&sp);
    buf_free(&tp);
    buf_free(&td);
    printf("Created a new %s site in \"%s\".\n\nNext steps:\n  cd %s\n  tantu dashboard\n\n"
           "Or edit site.conf and the files in content/, then run: tantu serve\n", theme, dir, dir);
    return 0;
}

/* ---------- main ---------- */

static char *ask_password(const char *prompt) {
    const char *env = getenv("TANTU_FTP_PASSWORD");
    if (env) return xstrdup(env);
    fputs(prompt, stderr);
    fflush(stderr);
    struct termios old, quiet;
    int tty = tcgetattr(0, &old) == 0;
    if (tty) {
        quiet = old;
        quiet.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(0, TCSANOW, &quiet);
    }
    char line[512] = {0};
    if (!fgets(line, sizeof line, stdin)) line[0] = '\0';
    if (tty) tcsetattr(0, TCSANOW, &old);
    fputc('\n', stderr);
    size_t l = strlen(line);
    while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = '\0';
    return xstrdup(line);
}

static int cmd_publish(const char *dir, int all) {
    if (cmd_build(dir, 0) != 0) return 1;
    map cfg = {0};
    char *conf = read_file("site.conf", NULL);
    parse_conf(conf ? conf : "", &cfg);
    free(conf);
    if (!*get(&cfg, "ftp_host") || !*get(&cfg, "ftp_user"))
        die("add ftp_host, ftp_user and ftp_dir to site.conf, or publish from the dashboard");
    fprintf(stderr, "Note: plain FTP does not encrypt your password. Use it on a network you trust.\n");
    char *pass = ask_password("FTP password: ");
    ftp_opts o = {get(&cfg, "ftp_host"), atoi(get(&cfg, "ftp_port")), get(&cfg, "ftp_user"), pass,
                  get(&cfg, "ftp_dir"), all};
    int rc = ftp_publish(OUT, &o, stdout);
    memset(pass, 0, strlen(pass));
    free(pass);
    map_free(&cfg);
    return rc;
}

static void usage(void) {
    puts("tantu " VERSION ": build fast, secure websites from Markdown\n\n"
         "Usage:\n"
         "  tantu                                  open the dashboard (creates \"mysite\" the first time)\n"
         "  tantu new <folder> [--theme NAME]      start a new site\n"
         "  tantu dashboard [folder] [--port 8080] edit your site in the browser\n"
         "  tantu build [folder]                   build the site into public/\n"
         "  tantu serve [folder] [--port 8000]     build and preview on your computer\n"
         "  tantu publish [folder] [--all]         upload the site to your hosting over FTP\n"
         "  tantu themes                           list the built-in themes\n"
         "  tantu version                          show the version\n\n"
         "Upload the public/ folder to any web host, GitHub Pages or Cloudflare Pages.");
}

static char *base_path_of(void) {
    map cfg = {0};
    char *conf = read_file("site.conf", NULL);
    parse_conf(conf ? conf : "", &cfg);
    free(conf);
    const char *bu = get(&cfg, "base_url");
    const char *sc = strstr(bu, "://");
    char *base = xstrdup("");
    if (sc) {
        const char *sl = strchr(sc + 3, '/');
        if (sl) {
            free(base);
            base = xstrdup(sl);
            size_t l = strlen(base);
            while (l && base[l - 1] == '/') base[--l] = '\0';
        }
    }
    map_free(&cfg);
    return base;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        /* Double click or plain "tantu": open the dashboard */
        if (is_file("site.conf")) return dashboard_run(NULL, 8080, 1);
        if (!is_file("mysite/site.conf")) {
            if (is_dir("mysite")) die("a folder named mysite exists without a site.conf");
            cmd_new("mysite", "blog");
        }
        return dashboard_run("mysite", 8080, 1);
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
        usage();
        return 0;
    }
    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) {
        puts("tantu " VERSION);
        return 0;
    }
    if (!strcmp(cmd, "themes")) {
        list_themes(stdout);
        return 0;
    }
    const char *folder = NULL, *theme = "blog";
    int port = 0, no_browser = 0, all = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--theme") && i + 1 < argc) theme = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-browser")) no_browser = 1;
        else if (!strcmp(argv[i], "--all")) all = 1;
        else if (argv[i][0] != '-' && !folder) folder = argv[i];
        else die("unknown option: %s", argv[i]);
    }
    if (port < 0 || port > 65535) die("port must be between 1 and 65535");
    if (!strcmp(cmd, "new")) {
        if (!folder) die("give the new site a folder name, for example: tantu new mysite");
        return cmd_new(folder, theme);
    }
    if (!strcmp(cmd, "build")) return cmd_build(folder, 0);
    if (!strcmp(cmd, "publish")) return cmd_publish(folder, all);
    if (!strcmp(cmd, "dashboard")) return dashboard_run(folder, port ? port : 8080, !no_browser);
    if (!strcmp(cmd, "serve")) {
        cmd_build(folder, 0);
        char *base = base_path_of();
        int rc = serve_dir(OUT, base, port ? port : 8000);
        free(base);
        return rc;
    }
    usage();
    return 1;
}
