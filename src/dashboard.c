/* tantu dashboard: edit a site in the browser.
 *
 * Runs a small server on 127.0.0.1. The dashboard lives at /_tantu/ and
 * talks to a JSON API at /_tantu/api/. Everything else is a live preview of
 * the built site.
 *
 * Security:
 *   - Listens on 127.0.0.1 only.
 *   - Every API call needs a random token that is printed when the
 *     dashboard starts, sent in the X-Tantu-Token header. Other websites
 *     cannot send that header, which blocks cross-site requests.
 *   - The Host header must be 127.0.0.1 or localhost, which blocks DNS
 *     rebinding attacks.
 *   - File access is limited to Markdown files in content, images in
 *     static/images, and site.conf.
 *   - Uploads are limited to real PNG, JPEG, GIF and WebP images up to 10 MB.
 */
#define _XOPEN_SOURCE 700
#include "dashboard.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "embedded.h"
#include "markdown.h"
#include "serve.h"
#include "site.h"
#include "util.h"
#include "zip.h"

#ifdef __COSMOPOLITAN__
#include <cosmo.h>
#endif

#define MAX_BODY (12 * 1024 * 1024)
#define MAX_UPLOAD (10 * 1024 * 1024)

static char token[33];
static int dash_port;

static const char *get(const map *m, const char *k) {
    const char *v = map_get(m, k);
    return v ? v : "";
}

/* ---------- helpers ---------- */

static void make_token(void) {
    unsigned char r[16] = {0};
    int ok = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ok = read(fd, r, sizeof r) == (ssize_t)sizeof r;
        close(fd);
    }
    if (!ok) {
        srand((unsigned)time(NULL) ^ (unsigned)getpid());
        for (int i = 0; i < 16; i++) r[i] = (unsigned char)(rand() & 0xFF);
    }
    for (int i = 0; i < 16; i++) snprintf(token + i * 2, 3, "%02x", r[i]);
}

static void json_str(buf *o, const char *key, const char *val, int comma) {
    if (comma) buf_putc(o, ',');
    buf_printf(o, "\"%s\":\"", key);
    esc_json(o, val ? val : "");
    buf_putc(o, '"');
}

static void send_json(int fd, int code, buf *b) {
    const char *status = code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden"
                       : code == 404 ? "Not Found" : "Error";
    http_respond(fd, code, status, "application/json; charset=utf-8", b->s ? b->s : "{}", b->len, 0, NULL);
}

static void send_error(int fd, int code, const char *msg) {
    buf b = {0};
    buf_puts(&b, "{");
    json_str(&b, "error", msg, 0);
    buf_puts(&b, "}");
    send_json(fd, code, &b);
    buf_free(&b);
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *url_decode_dup(const char *s, size_t n) {
    buf o = {0};
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '+') buf_putc(&o, ' ');
        else if (s[i] == '%' && i + 2 < n && hexval(s[i + 1]) >= 0 && hexval(s[i + 2]) >= 0) {
            char c = (char)(hexval(s[i + 1]) * 16 + hexval(s[i + 2]));
            if (c) buf_putc(&o, c);
            i += 2;
        } else buf_putc(&o, s[i]);
    }
    return buf_take(&o);
}

/* Parses "a=1&b=2" (query strings and form bodies). */
static void form_parse(const char *s, size_t n, map *m) {
    size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && s[j] != '&') j++;
        const char *eq = memchr(s + i, '=', j - i);
        if (eq) {
            char *k = url_decode_dup(s + i, (size_t)(eq - (s + i)));
            char *v = url_decode_dup(eq + 1, (size_t)((s + j) - (eq + 1)));
            map_set(m, k, v);
            free(k);
            free(v);
        }
        i = j + 1;
    }
}

/* content/posts/x.md, content/pages/x.md or content/index.md only */
static int safe_content_path(const char *p) {
    if (strncmp(p, "content/", 8) != 0) return 0;
    size_t l = strlen(p);
    if (l < 12 || l > 200 || strcmp(p + l - 3, ".md") != 0) return 0;
    if (strstr(p, "..") || strstr(p, "//")) return 0;
    for (const char *c = p; *c; c++)
        if (!isalnum((unsigned char)*c) && !strchr("._-/", *c)) return 0;
    if (!strcmp(p, "content/index.md")) return 1;
    const char *rest = NULL;
    if (!strncmp(p, "content/posts/", 14)) rest = p + 14;
    else if (!strncmp(p, "content/pages/", 14)) rest = p + 14;
    return rest && *rest && !strchr(rest, '/');
}

static char *read_conf(map *cfg) {
    char *conf = read_file("site.conf", NULL);
    if (conf) parse_conf(conf, cfg);
    return conf;
}

static char *base_path(const map *cfg) {
    const char *bu = get(cfg, "base_url");
    const char *sc = strstr(bu, "://");
    if (!sc) return xstrdup("");
    const char *sl = strchr(sc + 3, '/');
    char *b = xstrdup(sl ? sl : "");
    size_t l = strlen(b);
    while (l && b[l - 1] == '/') b[--l] = '\0';
    return b;
}

/* Same rule as the build: a date prefix in the file name is not part of the URL. */
static char *doc_slug(const char *fname, const map *fm) {
    if (*get(fm, "slug")) return slugify(get(fm, "slug"));
    char *stem = xstrndup(fname, strlen(fname) - 3);
    const char *name = stem;
    if (strlen(stem) > 11 && stem[10] == '-') {
        char d[11];
        memcpy(d, stem, 10);
        d[10] = '\0';
        if (valid_date(d)) name = stem + 11;
    }
    char *s = slugify(name);
    free(stem);
    return s;
}

/* ---------- SEO checks ---------- */

typedef struct {
    char *path, *title, *desc;
} seen_t;

static int count_sub(const char *h, const char *needle) {
    int n = 0;
    for (const char *p = h; (p = strstr(p, needle)); p += strlen(needle)) n++;
    return n;
}

static int seo_checks(const map *fm, const char *html, int is_post, int dup_title, int dup_desc, buf *issues) {
    int score = 100, first = 1;
#define ISSUE(pts, msg) do { score -= (pts); if (!first) buf_putc(issues, ','); first = 0; \
        buf_putc(issues, '"'); esc_json(issues, msg); buf_putc(issues, '"'); } while (0)
    const char *t = get(fm, "title"), *d = get(fm, "description");
    size_t tl = strlen(t), dl = strlen(d);
    char msg[160];
    if (!tl) ISSUE(20, "Add a title.");
    else if (tl > 60) { snprintf(msg, sizeof msg, "Title is %zu characters. Google shows about 60.", tl); ISSUE(8, msg); }
    else if (tl < 15) ISSUE(4, "Title is very short. Say what the page is about.");
    if (!dl) ISSUE(15, "Add a description. Google and social networks show it under your title.");
    else if (dl > 160) { snprintf(msg, sizeof msg, "Description is %zu characters. Keep it under 160.", dl); ISSUE(8, msg); }
    else if (dl < 50) ISSUE(5, "Description is short. Aim for 50 to 160 characters.");
    int imgs_no_alt = count_sub(html, "alt=\"\"");
    if (imgs_no_alt) { snprintf(msg, sizeof msg, "%d image%s without alt text. Describe each image.", imgs_no_alt, imgs_no_alt > 1 ? "s" : ""); ISSUE(imgs_no_alt > 2 ? 20 : imgs_no_alt * 8, msg); }
    if (strstr(html, "<h1")) ISSUE(5, "Use ## for headings. The title is already the main heading.");
    int words = word_count(html);
    if (is_post && words < 300) { snprintf(msg, sizeof msg, "Only %d words. Pages with 300 or more words usually rank better.", words); ISSUE(words < 100 ? 12 : 6, msg); }
    if (is_post && !*get(fm, "date")) ISSUE(6, "Add a date, for example date: 2026-09-23.");
    if (*get(fm, "image") && !*get(fm, "image_alt")) ISSUE(6, "Add image_alt to describe the cover image.");
    if (dup_title) ISSUE(10, "Another page has the same title.");
    if (dup_desc) ISSUE(8, "Another page has the same description.");
#undef ISSUE
    return score < 0 ? 0 : score;
}

/* ---------- API: state ---------- */

static void add_docs(buf *o, const char *dir, int is_post, const char *base, const char *section,
                     seen_t *seen, int nseen) {
    int n = 0;
    char **files = list_files(dir, ".md", &n);
    for (int i = 0; i < n; i++) {
        char *p = path_join(dir, files[i]);
        char *src = read_file(p, NULL);
        map fm = {0};
        const char *body = front_matter(src ? src : "", &fm);
        buf html = {0};
        md_to_html(body, base, &html);
        int dt = 0, dd = 0;
        for (int k = 0; k < nseen; k++) {
            if (!strcmp(seen[k].path, p)) continue;
            if (*get(&fm, "title") && !strcmp(seen[k].title, get(&fm, "title"))) dt = 1;
            if (*get(&fm, "description") && !strcmp(seen[k].desc, get(&fm, "description"))) dd = 1;
        }
        buf issues = {0};
        int score = seo_checks(&fm, html.s ? html.s : "", is_post, dt, dd, &issues);
        char *slug = doc_slug(files[i], &fm);
        buf url = {0};
        if (is_post) buf_printf(&url, "%s/%s/%s/", base, section, slug);
        else buf_printf(&url, "%s/%s/", base, slug);

        if (i) buf_putc(o, ',');
        buf_puts(o, "{");
        json_str(o, "path", p, 0);
        json_str(o, "title", *get(&fm, "title") ? get(&fm, "title") : files[i], 1);
        json_str(o, "date", get(&fm, "date"), 1);
        json_str(o, "description", get(&fm, "description"), 1);
        json_str(o, "url", url.s, 1);
        buf_printf(o, ",\"draft\":%s,\"score\":%d,\"words\":%d,\"issues\":[%s]}",
                   !strcmp(get(&fm, "draft"), "true") ? "true" : "false", score,
                   word_count(html.s ? html.s : ""), issues.s ? issues.s : "");
        buf_free(&issues);
        buf_free(&url);
        buf_free(&html);
        free(slug);
        map_free(&fm);
        free(src);
        free(p);
    }
    free_list(files, n);
}

static void collect_seen(const char *dir, seen_t **seen, int *n, int *cap) {
    int c = 0;
    char **files = list_files(dir, ".md", &c);
    for (int i = 0; i < c; i++) {
        char *p = path_join(dir, files[i]);
        char *src = read_file(p, NULL);
        map fm = {0};
        front_matter(src ? src : "", &fm);
        if (*n == *cap) {
            *cap = *cap ? *cap * 2 : 32;
            *seen = xrealloc(*seen, sizeof(seen_t) * (size_t)*cap);
        }
        (*seen)[*n].path = p;
        (*seen)[*n].title = xstrdup(get(&fm, "title"));
        (*seen)[*n].desc = xstrdup(get(&fm, "description"));
        (*n)++;
        map_free(&fm);
        free(src);
    }
    free_list(files, c);
}

static void api_state(int fd) {
    map cfg = {0};
    char *conf = read_conf(&cfg);
    char *base = base_path(&cfg);
    char *section = slugify(*get(&cfg, "section") ? get(&cfg, "section") : "blog");
    buf o = {0};
    buf_puts(&o, "{\"site\":{");
    for (int i = 0; i < cfg.n; i++) json_str(&o, cfg.items[i].k, cfg.items[i].v, i > 0);
    buf_puts(&o, "}");
    json_str(&o, "base_path", base, 1);
    json_str(&o, "section", section, 1);

    seen_t *seen = NULL;
    int ns = 0, cs = 0;
    collect_seen("content/posts", &seen, &ns, &cs);
    collect_seen("content/pages", &seen, &ns, &cs);
    buf_puts(&o, ",\"posts\":[");
    add_docs(&o, "content/posts", 1, base, section, seen, ns);
    buf_puts(&o, "],\"pages\":[");
    add_docs(&o, "content/pages", 0, base, section, seen, ns);
    buf_puts(&o, "]");
    for (int i = 0; i < ns; i++) { free(seen[i].path); free(seen[i].title); free(seen[i].desc); }
    free(seen);

    buf_puts(&o, ",\"media\":[");
    int nm = 0;
    char **media = list_files("static/images", NULL, &nm);
    for (int i = 0; i < nm; i++) {
        if (i) buf_putc(&o, ',');
        buf_putc(&o, '"');
        esc_json(&o, media[i]);
        buf_putc(&o, '"');
    }
    free_list(media, nm);
    buf_puts(&o, "],\"themes\":[");
    int first = 1;
    for (size_t i = 0; i < tt_nfiles; i++) {
        const char *p = tt_files[i].path;
        size_t l = strlen(p);
        if (strncmp(p, "themes/", 7) != 0 || l < 18 || strcmp(p + l - 11, "/theme.conf") != 0) continue;
        char *text = xstrndup((const char *)tt_files[i].data, tt_files[i].len);
        map tm = {0};
        parse_conf(text, &tm);
        if (!first) buf_putc(&o, ',');
        first = 0;
        buf_puts(&o, "{");
        json_str(&o, "id", get(&tm, "id"), 0);
        json_str(&o, "name", get(&tm, "name"), 1);
        json_str(&o, "description", get(&tm, "description"), 1);
        buf_puts(&o, "}");
        map_free(&tm);
        free(text);
    }
    buf_puts(&o, "]}");
    send_json(fd, 200, &o);
    buf_free(&o);
    free(section);
    free(base);
    free(conf);
    map_free(&cfg);
}

/* ---------- API: build ---------- */

/* Builds in a child process so a bad file can never stop the dashboard. */
static int run_build(buf *output) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(pipefd[1], 1);
        dup2(pipefd[1], 2);
        close(pipefd[0]);
        close(pipefd[1]);
        int rc = cmd_build(NULL, 0);
        fflush(stdout);
        _exit(rc);
    }
    close(pipefd[1]);
    char tmp[4096];
    ssize_t r;
    while ((r = read(pipefd[0], tmp, sizeof tmp)) > 0) buf_put(output, tmp, (size_t)r);
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static void api_build(int fd) {
    buf out = {0};
    int rc = run_build(&out);
    buf o = {0};
    buf_printf(&o, "{\"ok\":%s", rc == 0 ? "true" : "false");
    json_str(&o, "output", out.s ? out.s : "", 1);
    buf_puts(&o, "}");
    send_json(fd, 200, &o);
    buf_free(&o);
    buf_free(&out);
}

/* ---------- API: files ---------- */

static void api_file_get(int fd, const char *path) {
    if (!safe_content_path(path)) { send_error(fd, 400, "That file cannot be opened here."); return; }
    size_t n = 0;
    char *text = read_file(path, &n);
    if (!text) { send_error(fd, 404, "File not found."); return; }
    http_respond(fd, 200, "OK", "text/plain; charset=utf-8", text, n, 0, NULL);
    free(text);
}

static void api_file_save(int fd, const char *path, const char *body, size_t n) {
    if (!safe_content_path(path)) { send_error(fd, 400, "That file cannot be saved here."); return; }
    if (write_file(path, body, n) != 0) { send_error(fd, 500, "Could not save the file."); return; }
    buf o = {0};
    buf_puts(&o, "{\"ok\":true}");
    send_json(fd, 200, &o);
    buf_free(&o);
}

static void api_file_delete(int fd, const char *path) {
    if (!safe_content_path(path) || !strcmp(path, "content/index.md")) { send_error(fd, 400, "That file cannot be deleted."); return; }
    mkdir_p("content/.trash");
    const char *name = strrchr(path, '/') + 1;
    buf dst = {0};
    buf_printf(&dst, "content/.trash/%ld-%s", (long)time(NULL), name);
    if (rename(path, dst.s) != 0) { buf_free(&dst); send_error(fd, 500, "Could not delete the file."); return; }
    buf_free(&dst);
    buf o = {0};
    buf_puts(&o, "{\"ok\":true}");
    send_json(fd, 200, &o);
    buf_free(&o);
}

static void api_new(int fd, const map *q) {
    const char *kind = get(q, "kind");
    const char *title = get(q, "title");
    if (!*title) title = "Untitled";
    if (strlen(title) > 200) { send_error(fd, 400, "Title is too long."); return; }
    int is_post = !strcmp(kind, "post");
    char *slug = slugify(title);
    if (strlen(slug) > 60) slug[60] = '\0';
    char date[11];
    time_t now = time(NULL);
    strftime(date, sizeof date, "%Y-%m-%d", localtime(&now));
    buf path = {0};
    for (int k = 1; k < 100; k++) {
        path.len = 0;
        if (path.s) path.s[0] = '\0';
        buf_printf(&path, "content/%s/", is_post ? "posts" : "pages");
        if (is_post) buf_printf(&path, "%s-", date);
        buf_puts(&path, slug);
        if (k > 1) buf_printf(&path, "-%d", k);
        buf_puts(&path, ".md");
        if (!is_file(path.s)) break;
    }
    buf text = {0};
    buf_puts(&text, "---\ntitle: ");
    for (const char *c = title; *c; c++) buf_putc(&text, (*c == '\n' || *c == '\r') ? ' ' : *c);
    buf_puts(&text, "\ndescription: \n");
    if (is_post) buf_printf(&text, "date: %s\ntags: \ndraft: true\n", date);
    buf_puts(&text, "---\n\nStart writing here.\n");
    write_file(path.s, text.s, text.len);
    buf o = {0};
    buf_puts(&o, "{");
    json_str(&o, "path", path.s, 0);
    buf_puts(&o, "}");
    send_json(fd, 200, &o);
    buf_free(&o);
    buf_free(&text);
    buf_free(&path);
    free(slug);
}

static void api_preview(int fd, const char *body, size_t n) {
    map cfg = {0};
    char *conf = read_conf(&cfg);
    char *base = base_path(&cfg);
    char *src = xstrndup(body, n);
    map fm = {0};
    const char *md = front_matter(src, &fm);
    buf html = {0};
    md_to_html(md, base, &html);
    http_respond(fd, 200, "OK", "text/html; charset=utf-8", html.s ? html.s : "", html.len, 0, NULL);
    buf_free(&html);
    map_free(&fm);
    free(src);
    free(base);
    free(conf);
    map_free(&cfg);
}

/* ---------- API: settings and theme ---------- */

static int safe_key(const char *k) {
    size_t l = strlen(k);
    if (!l || l > 40) return 0;
    for (const char *c = k; *c; c++)
        if (!islower((unsigned char)*c) && !isdigit((unsigned char)*c) && *c != '_') return 0;
    return 1;
}

static int update_conf(const map *changes) {
    char *conf = read_file("site.conf", NULL);
    if (!conf) return -1;
    buf out = {0};
    int *done = xmalloc(sizeof(int) * (size_t)(changes->n + 1));
    memset(done, 0, sizeof(int) * (size_t)(changes->n + 1));
    char *save = NULL;
    size_t len = strlen(conf);
    int ends_nl = len && conf[len - 1] == '\n';
    for (char *line = strtok_r(conf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *copy = xstrdup(line);
        char *t = trim(copy);
        char *eq = strchr(t, '=');
        int replaced = 0;
        if (*t && *t != '#' && eq) {
            *eq = '\0';
            char *k = trim(t);
            for (int i = 0; i < changes->n; i++) {
                if (!strcmp(changes->items[i].k, k)) {
                    buf_printf(&out, "%s = %s\n", k, changes->items[i].v);
                    done[i] = 1;
                    replaced = 1;
                    break;
                }
            }
        }
        if (!replaced) { buf_puts(&out, line); buf_putc(&out, '\n'); }
        free(copy);
    }
    (void)ends_nl;
    for (int i = 0; i < changes->n; i++)
        if (!done[i]) buf_printf(&out, "%s = %s\n", changes->items[i].k, changes->items[i].v);
    int rc = write_file("site.conf", out.s, out.len);
    buf_free(&out);
    free(done);
    free(conf);
    return rc;
}

static void api_settings(int fd, const char *body, size_t n) {
    map in = {0}, clean = {0};
    form_parse(body, n, &in);
    for (int i = 0; i < in.n; i++) {
        if (!safe_key(in.items[i].k) || !strcmp(in.items[i].k, "theme")) continue;
        char *v = xstrdup(in.items[i].v);
        for (char *c = v; *c; c++) if (*c == '\n' || *c == '\r') *c = ' ';
        map_set(&clean, in.items[i].k, trim(v));
        free(v);
    }
    int rc = update_conf(&clean);
    map_free(&in);
    map_free(&clean);
    if (rc != 0) { send_error(fd, 500, "Could not save site.conf."); return; }
    buf o = {0};
    buf_puts(&o, "{\"ok\":true}");
    send_json(fd, 200, &o);
    buf_free(&o);
}

static void api_theme(int fd, const map *q) {
    const char *name = get(q, "name");
    if (!theme_exists(name)) { send_error(fd, 400, "Unknown theme."); return; }
    buf dir = {0}, prefix = {0};
    buf_printf(&dir, "themes/%s", name);
    buf_printf(&prefix, "themes/%s/", name);
    if (!is_dir(dir.s)) extract_prefix(prefix.s, dir.s);
    map ch = {0};
    map_set(&ch, "theme", name);
    update_conf(&ch);
    map_free(&ch);
    buf_free(&dir);
    buf_free(&prefix);
    buf o = {0};
    buf_puts(&o, "{\"ok\":true}");
    send_json(fd, 200, &o);
    buf_free(&o);
}

/* ---------- API: media ---------- */

static int image_type_ok(const char *ext, const unsigned char *d, size_t n) {
    if (n < 12) return 0;
    if (!strcmp(ext, "png")) return !memcmp(d, "\x89PNG\r\n\x1a\n", 8);
    if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg")) return d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF;
    if (!strcmp(ext, "gif")) return !memcmp(d, "GIF8", 4);
    if (!strcmp(ext, "webp")) return !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WEBP", 4);
    return 0;
}

static void api_upload(int fd, const map *q, const char *body, size_t n) {
    if (n > MAX_UPLOAD) { send_error(fd, 400, "Images must be 10 MB or smaller."); return; }
    const char *raw = get(q, "name");
    buf name = {0};
    for (const char *c = raw; *c && name.len < 80; c++) {
        unsigned char ch = (unsigned char)tolower((unsigned char)*c);
        if (isalnum(ch) || ch == '.' || ch == '-' || ch == '_') buf_putc(&name, (char)ch);
        else if (ch == ' ') buf_putc(&name, '-');
    }
    char *nm = buf_take(&name);
    char *dot = strrchr(nm, '.');
    if (!dot || dot == nm || !image_type_ok(dot + 1, (const unsigned char *)body, n)) {
        free(nm);
        send_error(fd, 400, "Upload a PNG, JPEG, GIF or WebP image.");
        return;
    }
    *dot = '\0';
    const char *ext = dot + 1;
    buf path = {0};
    for (int k = 1; k < 1000; k++) {
        path.len = 0;
        if (path.s) path.s[0] = '\0';
        if (k == 1) buf_printf(&path, "static/images/%s.%s", nm, ext);
        else buf_printf(&path, "static/images/%s-%d.%s", nm, k, ext);
        if (!is_file(path.s)) break;
    }
    if (write_file(path.s, body, n) != 0) {
        free(nm);
        buf_free(&path);
        send_error(fd, 500, "Could not save the image.");
        return;
    }
    buf o = {0};
    buf_puts(&o, "{");
    json_str(&o, "path", path.s + 6, 0); /* "/images/..." relative to the site root */
    buf_puts(&o, "}");
    send_json(fd, 200, &o);
    buf_free(&o);
    buf_free(&path);
    free(nm);
}

static void api_export(int fd) {
    buf out = {0};
    if (run_build(&out) != 0) {
        buf_free(&out);
        send_error(fd, 500, "The build failed. Open Publish to see why.");
        return;
    }
    buf_free(&out);
    buf z = {0};
    if (zip_dir("public", &z) < 0) { send_error(fd, 500, "Could not create the ZIP file."); return; }
    http_respond(fd, 200, "OK", "application/zip", z.s, z.len, 0,
                 "Content-Disposition: attachment; filename=\"site.zip\"\r\n");
    buf_free(&z);
}

/* ---------- request handling ---------- */

typedef struct {
    char method[8];
    char path[4096];
    char query[4096];
    char host[256];
    char tok[64];
    size_t clen;
    char *body;
    size_t blen;
} request;

static void header_value(const char *headers, const char *name, char *out, size_t cap) {
    out[0] = '\0';
    size_t nl = strlen(name);
    for (const char *p = headers; (p = strchr(p, '\n')); ) {
        p++;
        if (strncasecmp(p, name, nl) == 0 && p[nl] == ':') {
            const char *v = p + nl + 1;
            while (*v == ' ') v++;
            size_t i = 0;
            while (v[i] && v[i] != '\r' && v[i] != '\n' && i + 1 < cap) { out[i] = v[i]; i++; }
            out[i] = '\0';
            return;
        }
    }
}

static int read_request(int fd, request *r) {
    buf in = {0};
    char tmp[16384];
    char *end = NULL;
    while (!end) {
        ssize_t n = read(fd, tmp, sizeof tmp);
        if (n <= 0) { buf_free(&in); return -1; }
        buf_put(&in, tmp, (size_t)n);
        end = strstr(in.s, "\r\n\r\n");
        if (!end && in.len > 65536) { buf_free(&in); return -1; }
    }
    size_t hlen = (size_t)(end - in.s) + 4;
    char target[4096] = {0};
    if (sscanf(in.s, "%7s %4095s", r->method, target) != 2) { buf_free(&in); return -1; }
    char *qm = strchr(target, '?');
    if (qm) {
        *qm = '\0';
        snprintf(r->query, sizeof r->query, "%s", qm + 1);
    }
    snprintf(r->path, sizeof r->path, "%s", target);
    in.s[hlen - 2] = '\0';
    header_value(in.s, "Host", r->host, sizeof r->host);
    header_value(in.s, "X-Tantu-Token", r->tok, sizeof r->tok);
    char cl[32];
    header_value(in.s, "Content-Length", cl, sizeof cl);
    r->clen = (size_t)strtoul(cl, NULL, 10);
    if (r->clen > MAX_BODY) { buf_free(&in); return -2; }
    buf body = {0};
    size_t have = in.len - hlen;
    if (have) buf_put(&body, in.s + hlen, have);
    while (body.len < r->clen) {
        ssize_t n = read(fd, tmp, sizeof tmp);
        if (n <= 0) break;
        buf_put(&body, tmp, (size_t)n);
    }
    if (body.len > r->clen) body.len = r->clen;
    r->blen = body.len;
    r->body = body.s ? body.s : xstrdup("");
    buf_free(&in);
    return 0;
}

static int host_ok(const char *host) {
    char a[64], b[64];
    snprintf(a, sizeof a, "127.0.0.1:%d", dash_port);
    snprintf(b, sizeof b, "localhost:%d", dash_port);
    return !strcmp(host, a) || !strcmp(host, b);
}

static void handle(int fd, const char *base) {
    request r;
    memset(&r, 0, sizeof r);
    int rc = read_request(fd, &r);
    if (rc == -2) { send_error(fd, 400, "Request is too large."); return; }
    if (rc != 0) return;

    if (!strcmp(r.path, "/_tantu") || !strcmp(r.path, "/_tantu/")) {
        size_t n = 0;
        const unsigned char *ui = embedded_get("ui/dashboard.html", &n);
        http_respond(fd, 200, "OK", "text/html; charset=utf-8", (const char *)ui, n, 0,
                     "Content-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; "
                     "style-src 'self' 'unsafe-inline'; img-src 'self' data:; frame-src 'self'; "
                     "connect-src 'self'; frame-ancestors 'none'\r\nX-Frame-Options: DENY\r\n");
    } else if (!strncmp(r.path, "/_tantu/api/", 12)) {
        const char *ep = r.path + 12;
        if (!host_ok(r.host)) send_error(fd, 403, "Wrong host.");
        else if (strcmp(r.tok, token) != 0) send_error(fd, 403, "Missing or wrong token. Open the link printed in the terminal.");
        else {
            map q = {0};
            form_parse(r.query, strlen(r.query), &q);
            int post = !strcmp(r.method, "POST"), get_ = !strcmp(r.method, "GET");
            if (get_ && !strcmp(ep, "state")) api_state(fd);
            else if (get_ && !strcmp(ep, "file")) api_file_get(fd, get(&q, "path"));
            else if (post && !strcmp(ep, "file")) api_file_save(fd, get(&q, "path"), r.body, r.blen);
            else if (post && !strcmp(ep, "delete")) api_file_delete(fd, get(&q, "path"));
            else if (post && !strcmp(ep, "new")) api_new(fd, &q);
            else if (post && !strcmp(ep, "preview")) api_preview(fd, r.body, r.blen);
            else if (post && !strcmp(ep, "build")) api_build(fd);
            else if (post && !strcmp(ep, "settings")) api_settings(fd, r.body, r.blen);
            else if (post && !strcmp(ep, "theme")) api_theme(fd, &q);
            else if (post && !strcmp(ep, "upload")) api_upload(fd, &q, r.body, r.blen);
            else if (get_ && !strcmp(ep, "export")) api_export(fd);
            else send_error(fd, 404, "Unknown request.");
            map_free(&q);
        }
    } else if (!strcmp(r.method, "GET") || !strcmp(r.method, "HEAD")) {
        char raw[4200];
        snprintf(raw, sizeof raw, "%s", r.path);
        if (!strcmp(raw, "/")) {
            /* The site root without a base path redirects to the dashboard or the site */
            char loc[128];
            snprintf(loc, sizeof loc, "Location: %s/\r\n", *base ? base : "/_tantu");
            http_respond(fd, 302, "Found", "text/plain", "", 0, 0, loc);
        } else {
            serve_static(fd, "public", base, r.method, raw);
        }
    } else {
        send_error(fd, 405, "Method not allowed.");
    }
    free(r.body);
}

static void open_browser(const char *url) {
    pid_t pid = fork();
    if (pid != 0) return;
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); }
#ifdef __COSMOPOLITAN__
    if (IsWindows()) execlp("cmd.exe", "cmd.exe", "/c", "start", "", url, (char *)NULL);
    else if (IsXnu()) execlp("open", "open", url, (char *)NULL);
    else execlp("xdg-open", "xdg-open", url, (char *)NULL);
#elif defined(__APPLE__)
    execlp("open", "open", url, (char *)NULL);
#else
    execlp("xdg-open", "xdg-open", url, (char *)NULL);
#endif
    _exit(0);
}

int dashboard_run(const char *dir, int port, int browser) {
    if (dir && chdir(dir) != 0) die("folder not found: %s", dir);
    if (!is_file("site.conf")) die("no site.conf here. Create a site first with: tantu new mysite");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_DFL);
    make_token();

    buf out = {0};
    run_build(&out);
    buf_free(&out);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) die("cannot open a network socket");
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int bound = 0;
    for (int p = port; p < port + 20; p++) {
        a.sin_port = htons((unsigned short)p);
        if (bind(s, (struct sockaddr *)&a, sizeof a) == 0) { dash_port = p; bound = 1; break; }
    }
    if (!bound) die("ports %d to %d are busy", port, port + 19);
    listen(s, 16);

    map cfg = {0};
    char *conf = read_conf(&cfg);
    char *base = base_path(&cfg);
    char url[128];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/_tantu/?t=%s", dash_port, token);
    printf("\ntantu dashboard is running.\n\n  Open: %s\n\nKeep this window open while you work. Press Ctrl+C to stop.\n\n", url);
    fflush(stdout);
    if (browser) open_browser(url);

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        handle(c, base);
        close(c);
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }
    free(base);
    free(conf);
    map_free(&cfg);
    return 0;
}
