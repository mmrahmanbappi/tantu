/* tantu markdown: a small, safe Markdown to HTML converter.
 *
 * Supported: headings, paragraphs, bold, italic, inline code, fenced code
 * blocks, links, images, bullet and numbered lists, block quotes and
 * horizontal rules. Raw HTML is always escaped, so content can never inject
 * scripts into a page. Links with javascript:, vbscript: or data: URLs are
 * rendered as plain text.
 */
#include "markdown.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void inline_md(buf *o, const char *s, size_t n, const char *base);

static int url_is_safe(const char *u, size_t n) {
    while (n && isspace((unsigned char)*u)) { u++; n--; }
    const char *bad[] = {"javascript:", "vbscript:", "data:"};
    for (int i = 0; i < 3; i++) {
        size_t l = strlen(bad[i]);
        if (n >= l && strncasecmp(u, bad[i], l) == 0) return 0;
    }
    return 1;
}

/* Root-relative URLs get the site's base path, so sites work in sub-folders
 * such as username.github.io/project/. */
static void put_url(buf *o, const char *u, size_t n, const char *base) {
    if (n && u[0] == '/' && !(n > 1 && u[1] == '/') && base && *base) esc_html_s(o, base);
    esc_html(o, u, n);
}

/* Parses [text](url "title") starting at s[i] == '['. */
static int parse_link(const char *s, size_t n, size_t i, size_t *ts, size_t *tl, size_t *us,
                      size_t *ul, size_t *end) {
    int depth = 0;
    size_t j = i;
    for (; j < n; j++) {
        if (s[j] == '\\') { j++; continue; }
        if (s[j] == '[') depth++;
        else if (s[j] == ']' && --depth == 0) break;
    }
    if (j >= n || j + 1 >= n || s[j + 1] != '(') return 0;
    size_t k = j + 2;
    size_t close = k;
    while (close < n && s[close] != ')') close++;
    if (close >= n) return 0;
    size_t ue = k;
    while (ue < close && !isspace((unsigned char)s[ue])) ue++;
    *ts = i + 1;
    *tl = j - i - 1;
    *us = k;
    *ul = ue - k;
    *end = close + 1;
    return 1;
}

static const char *find_str(const char *s, size_t n, size_t from, const char *pat) {
    size_t pl = strlen(pat);
    for (size_t i = from; i + pl <= n; i++)
        if (!memcmp(s + i, pat, pl)) return s + i;
    return NULL;
}

static void inline_md(buf *o, const char *s, size_t n, const char *base) {
    size_t i = 0;
    while (i < n) {
        char c = s[i];
        if (c == '\\' && i + 1 < n && ispunct((unsigned char)s[i + 1])) {
            esc_html(o, s + i + 1, 1);
            i += 2;
            continue;
        }
        if (c == '`') {
            const char *e = memchr(s + i + 1, '`', n - i - 1);
            if (e) {
                buf_puts(o, "<code>");
                esc_html(o, s + i + 1, (size_t)(e - (s + i + 1)));
                buf_puts(o, "</code>");
                i = (size_t)(e - s) + 1;
                continue;
            }
        }
        if ((c == '[') || (c == '!' && i + 1 < n && s[i + 1] == '[')) {
            int img = (c == '!');
            size_t ts, tl, us, ul, end;
            if (parse_link(s, n, img ? i + 1 : i, &ts, &tl, &us, &ul, &end)) {
                if (!url_is_safe(s + us, ul)) {
                    inline_md(o, s + ts, tl, base);
                } else if (img) {
                    buf_puts(o, "<img src=\"");
                    put_url(o, s + us, ul, base);
                    buf_puts(o, "\" alt=\"");
                    esc_html(o, s + ts, tl);
                    buf_puts(o, "\" loading=\"lazy\" decoding=\"async\">");
                } else {
                    buf_puts(o, "<a href=\"");
                    put_url(o, s + us, ul, base);
                    buf_putc(o, '"');
                    if (ul > 4 && !strncmp(s + us, "http", 4)) buf_puts(o, " rel=\"noopener\"");
                    buf_putc(o, '>');
                    inline_md(o, s + ts, tl, base);
                    buf_puts(o, "</a>");
                }
                i = end;
                continue;
            }
        }
        if (c == '*' && i + 1 < n && s[i + 1] == '*') {
            const char *e = find_str(s, n, i + 2, "**");
            if (e && e > s + i + 2) {
                buf_puts(o, "<strong>");
                inline_md(o, s + i + 2, (size_t)(e - s) - i - 2, base);
                buf_puts(o, "</strong>");
                i = (size_t)(e - s) + 2;
                continue;
            }
        }
        if ((c == '*' || c == '_') && i + 1 < n && !isspace((unsigned char)s[i + 1]) &&
            !(c == '_' && i > 0 && isalnum((unsigned char)s[i - 1]))) {
            size_t j = i + 1;
            while (j < n && !(s[j] == c && !isspace((unsigned char)s[j - 1]))) j++;
            if (j < n && j > i + 1) {
                buf_puts(o, "<em>");
                inline_md(o, s + i + 1, j - i - 1, base);
                buf_puts(o, "</em>");
                i = j + 1;
                continue;
            }
        }
        esc_html(o, &c, 1);
        i++;
    }
}

typedef struct {
    buf *o;
    buf para;
    int list;  /* 0 none, 1 ul, 2 ol */
    int quote;
    const char *base;
} state;

static void flush_para(state *st) {
    if (!st->para.len) return;
    buf_puts(st->o, "<p>");
    inline_md(st->o, st->para.s, st->para.len, st->base);
    buf_puts(st->o, "</p>\n");
    st->para.len = 0;
    st->para.s[0] = '\0';
}

static void close_list(state *st) {
    if (st->list == 1) buf_puts(st->o, "</ul>\n");
    if (st->list == 2) buf_puts(st->o, "</ol>\n");
    st->list = 0;
}

static void close_quote(state *st) {
    if (!st->quote) return;
    flush_para(st);
    buf_puts(st->o, "</blockquote>\n");
    st->quote = 0;
}

static void flush_all(state *st) {
    flush_para(st);
    close_list(st);
    close_quote(st);
}

static int is_hr(const char *t) {
    char ch = 0;
    int count = 0;
    for (; *t; t++) {
        if (*t == ' ') continue;
        if (*t != '-' && *t != '*' && *t != '_') return 0;
        if (ch && *t != ch) return 0;
        ch = *t;
        count++;
    }
    return count >= 3;
}

void md_to_html(const char *src, const char *base, buf *o) {
    state st = {0};
    st.o = o;
    st.base = base;
    int in_code = 0;
    const char *p = src;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char *line = xstrndup(p, len);
        if (len && line[len - 1] == '\r') line[--len] = '\0';
        p = nl ? nl + 1 : p + len;

        if (in_code) {
            char *t = line;
            while (*t == ' ') t++;
            if (!strncmp(t, "```", 3)) {
                buf_puts(o, "</code></pre>\n");
                in_code = 0;
            } else {
                esc_html_s(o, line);
                buf_putc(o, '\n');
            }
            free(line);
            continue;
        }

        char *t = line;
        for (int k = 0; k < 3 && *t == ' '; k++) t++;

        if (st.quote && *t != '>') close_quote(&st);

        if (!strncmp(t, "```", 3)) {
            flush_all(&st);
            char *lang = trim(t + 3);
            buf_puts(o, "<pre><code");
            if (*lang) {
                buf_puts(o, " class=\"language-");
                for (char *q = lang; *q; q++)
                    if (isalnum((unsigned char)*q) || *q == '-' || *q == '+') buf_putc(o, *q);
                buf_putc(o, '"');
            }
            buf_putc(o, '>');
            in_code = 1;
        } else if (!*trim(t)) {
            flush_para(&st);
            close_list(&st);
        } else if (*t == '#') {
            int lvl = 0;
            while (t[lvl] == '#') lvl++;
            if (lvl <= 6 && (t[lvl] == ' ' || t[lvl] == '\0')) {
                flush_all(&st);
                char *h = trim(t + lvl);
                size_t hl = strlen(h);
                while (hl && h[hl - 1] == '#') h[--hl] = '\0';
                h = trim(h);
                char *id = slugify(h);
                buf_printf(o, "<h%d id=\"%s\">", lvl, id);
                inline_md(o, h, strlen(h), base);
                buf_printf(o, "</h%d>\n", lvl);
                free(id);
            } else {
                if (st.list) close_list(&st);
                if (st.para.len) buf_putc(&st.para, ' ');
                buf_puts(&st.para, trim(t));
            }
        } else if (is_hr(t)) {
            flush_all(&st);
            buf_puts(o, "<hr>\n");
        } else if (*t == '>') {
            flush_para(&st);
            close_list(&st);
            if (!st.quote) {
                buf_puts(o, "<blockquote>\n");
                st.quote = 1;
            }
            char *q = t + 1;
            if (*q == ' ') q++;
            if (!*trim(q)) flush_para(&st);
            else {
                if (st.para.len) buf_putc(&st.para, ' ');
                buf_puts(&st.para, trim(q));
            }
        } else if ((*t == '-' || *t == '*' || *t == '+') && t[1] == ' ') {
            flush_para(&st);
            if (st.list != 1) {
                close_list(&st);
                buf_puts(o, "<ul>\n");
                st.list = 1;
            }
            char *item = trim(t + 2);
            buf_puts(o, "<li>");
            inline_md(o, item, strlen(item), base);
            buf_puts(o, "</li>\n");
        } else if (isdigit((unsigned char)*t)) {
            char *d = t;
            while (isdigit((unsigned char)*d)) d++;
            if ((*d == '.' || *d == ')') && d[1] == ' ') {
                flush_para(&st);
                if (st.list != 2) {
                    close_list(&st);
                    buf_puts(o, "<ol>\n");
                    st.list = 2;
                }
                char *item = trim(d + 2);
                buf_puts(o, "<li>");
                inline_md(o, item, strlen(item), base);
                buf_puts(o, "</li>\n");
            } else {
                if (st.list) close_list(&st);
                if (st.para.len) buf_putc(&st.para, ' ');
                buf_puts(&st.para, trim(t));
            }
        } else {
            if (st.list) close_list(&st);
            if (st.para.len) buf_putc(&st.para, ' ');
            buf_puts(&st.para, trim(t));
        }
        free(line);
    }
    if (in_code) buf_puts(o, "</code></pre>\n");
    flush_all(&st);
    buf_free(&st.para);
}
