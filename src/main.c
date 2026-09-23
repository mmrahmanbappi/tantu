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

#include "markdown.h"
#include "seo.h"
#include "serve.h"
#include "template.h"
#include "util.h"

#define VERSION "0.1.0"
#define OUT "public"

/* ---------- config and front matter ---------- */

static void parse_conf(const char *text, map *m) {
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
static const char *front_matter(const char *src, map *m) {
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

static int valid_date(const char *d) {
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

static int by_date_desc(const void *a, const void *b) {
    const map *x = *(map *const *)a, *y = *(map *const *)b;
    int c = strcmp(get(y, "date"), get(x, "date"));
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

static void render(site_t *s, const char *tpl, map *page, const char *out_path) {
    buf head = {0};
    seo_head(&s->cfg, page, &head);
    map_set(page, "seo", head.s ? head.s : "");
    buf_free(&head);

    tctx root = {&s->tvars, NULL, s->lists, s->nlists, &s->partials};
    tctx ctx = {page, &root, NULL, 0, NULL};
    buf o = {0};
    tpl_render(tpl, strlen(tpl), &ctx, &o);
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

static int cmd_build(const char *dir, int quiet) {
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
    qsort(posts.items, (size_t)posts.n, sizeof(map *), by_date_desc);

    for (int i = 0; i < posts.n; i++) {
        buf u = {0};
        buf_printf(&u, "%s/%s/%s/", base, section, get(posts.items[i], "slug"));
        map_set(posts.items[i], "url", u.s);
        map_set(posts.items[i], "kind", "post");
        map_set(posts.items[i], "list_url", list_url);
        buf_free(&u);
    }
    for (int i = 0; i < pages.n; i++) {
        const char *sl = get(pages.items[i], "slug");
        if (!strcmp(sl, section) || !strcmp(sl, "assets") || !strcmp(sl, "index"))
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
    render(&s, t_list, &list, list_out);
    free(list_out);
    push(&all, &list);

    for (int i = 0; i < pages.n; i++) {
        buf p = {0};
        buf_printf(&p, "/%s/", get(pages.items[i], "slug"));
        char *out = out_for(p.s);
        render(&s, t_page, pages.items[i], out);
        free(out);
        buf_free(&p);
        push(&all, pages.items[i]);
    }
    for (int i = 0; i < posts.n; i++) {
        buf p = {0};
        buf_printf(&p, "/%s/%s/", section, get(posts.items[i], "slug"));
        char *out = out_for(p.s);
        render(&s, t_post, posts.items[i], out);
        free(out);
        buf_free(&p);
        push(&all, posts.items[i]);
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
    seo_headers_file(&b);
    write_text(OUT "/_headers", &b);
    write_file(OUT "/.nojekyll", "", 0);

    char *assets = path_join(s.theme_dir, "assets");
    if (is_dir(assets)) copy_tree(assets, OUT "/assets");
    free(assets);
    if (is_dir("static")) copy_tree("static", OUT);

    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (double)(t1.tv_sec - t0.tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
    if (!quiet) {
        printf("Built %d pages (%d %s, %d %s) in %.1f ms", pages_written, posts.n,
               posts.n == 1 ? "post" : "posts", pages.n, pages.n == 1 ? "page" : "pages", ms);
        if (skipped) printf(", skipped %d drafts", skipped);
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

static char *tantu_home(const char *argv0) {
    const char *env = getenv("TANTU_HOME");
    if (env && is_dir(env)) return xstrdup(env);
    char exe[PATH_MAX];
    char *found = NULL;
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) {
        exe[n] = '\0';
        found = xstrdup(exe);
    } else if (realpath(argv0, exe)) {
        found = xstrdup(exe);
    }
    if (found) {
        char *slash = strrchr(found, '/');
        if (slash) *slash = '\0';
        char *t = path_join(found, "themes");
        int ok = is_dir(t);
        free(t);
        if (ok) return found;
        free(found);
    }
    if (is_dir("themes") && is_dir("starters")) return xstrdup(".");
    return NULL;
}

static int cmd_new(const char *dir, const char *theme, const char *argv0) {
    if (is_dir(dir) || is_file(dir)) die("\"%s\" already exists. Pick a new folder name.", dir);
    char *home = tantu_home(argv0);
    if (!home) die("cannot find the themes folder. Keep it next to the tantu program, or set TANTU_HOME.");
    char *tsrc = path_join(home, "themes");
    char *tsrc2 = path_join(tsrc, theme);
    char *ssrc = path_join(home, "starters");
    char *ssrc2 = path_join(ssrc, theme);
    if (!is_dir(tsrc2) || !is_dir(ssrc2)) die("unknown theme \"%s\". Try: blog or portfolio", theme);
    if (copy_tree(ssrc2, dir) != 0) die("could not create %s", dir);
    char *tdst = path_join(dir, "themes");
    char *tdst2 = path_join(tdst, theme);
    copy_tree(tsrc2, tdst2);
    printf("Created a new %s site in \"%s\".\n\nNext steps:\n  cd %s\n  tantu serve\n\n"
           "Then edit site.conf and the files in content/.\n", theme, dir, dir);
    free(home); free(tsrc); free(tsrc2); free(ssrc); free(ssrc2); free(tdst); free(tdst2);
    return 0;
}

/* ---------- main ---------- */

static void usage(void) {
    puts("tantu " VERSION ": build fast, secure websites from Markdown\n\n"
         "Usage:\n"
         "  tantu new <folder> [--theme blog|portfolio]   start a new site\n"
         "  tantu build [folder]                          build the site into public/\n"
         "  tantu serve [folder] [--port 8000]            build and preview on your computer\n"
         "  tantu version                                 show the version\n\n"
         "Upload the public/ folder to any web host, GitHub Pages or Cloudflare Pages.");
}

int main(int argc, char **argv) {
    if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage();
        return 0;
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) {
        puts("tantu " VERSION);
        return 0;
    }
    const char *folder = NULL, *theme = "blog";
    int port = 8000;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--theme") && i + 1 < argc) theme = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (argv[i][0] != '-' && !folder) folder = argv[i];
        else die("unknown option: %s", argv[i]);
    }
    if (!strcmp(cmd, "new")) {
        if (!folder) die("give the new site a folder name, for example: tantu new mysite");
        return cmd_new(folder, theme, argv[0]);
    }
    if (!strcmp(cmd, "build")) return cmd_build(folder, 0);
    if (!strcmp(cmd, "serve")) {
        if (port < 1 || port > 65535) die("port must be between 1 and 65535");
        cmd_build(folder, 0);
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
        int rc = serve_dir(OUT, base, port);
        free(base);
        map_free(&cfg);
        return rc;
    }
    usage();
    return 1;
}
