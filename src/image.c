#include "image.h"
#include "embedded.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#include "vendor/stb_image.h"
#include "vendor/stb_image_resize2.h"
#include "vendor/stb_image_write.h"
#include "vendor/stb_truetype.h"

static int ends_with(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    if (a < b) return 0;
    for (size_t i = 0; i < b; i++) {
        char c = s[a - b + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != suf[i]) return 0;
    }
    return 1;
}

int img_info(const char *path, int *w, int *h) {
    int comp;
    return stbi_info(path, w, h, &comp) ? 0 : -1;
}

int img_resize(const char *src, const char *dst, int max_w) {
    int w, h, comp;
    unsigned char *in = stbi_load(src, &w, &h, &comp, 0);
    if (!in) return -1;
    int nw = w, nh = h;
    if (w > max_w) {
        nw = max_w;
        nh = (int)((double)h * max_w / w + 0.5);
        if (nh < 1) nh = 1;
    }
    unsigned char *out = in;
    if (nw != w) {
        stbir_pixel_layout layout = comp == 4 ? STBIR_RGBA : comp == 3 ? STBIR_RGB : comp == 2 ? STBIR_RA : STBIR_1CHANNEL;
        out = stbir_resize_uint8_srgb(in, w, h, 0, NULL, nw, nh, 0, layout);
        if (!out) { stbi_image_free(in); return -1; }
    }
    int ok;
    /* make sure the folder exists */
    char *dir = xstrdup(dst);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = '\0'; mkdir_p(dir); }
    free(dir);
    if (ends_with(dst, ".png")) {
        stbi_write_png_compression_level = 9;
        ok = stbi_write_png(dst, nw, nh, comp, out, nw * comp);
    } else {
        ok = stbi_write_jpg(dst, nw, nh, comp, out, 82);
    }
    if (out != in) free(out);
    stbi_image_free(in);
    return ok ? 0 : -1;
}

/* ---------- social images ---------- */

typedef struct {
    unsigned char *px;
    int w, h;
} canvas;

static void parse_hex(const char *s, unsigned char rgb[3]) {
    rgb[0] = rgb[1] = rgb[2] = 0;
    if (!s || *s != '#' || strlen(s) < 7) return;
    for (int i = 0; i < 3; i++) {
        char hx[3] = {s[1 + i * 2], s[2 + i * 2], 0};
        rgb[i] = (unsigned char)strtol(hx, NULL, 16);
    }
}

static void fill(canvas *c, int x0, int y0, int x1, int y1, const unsigned char rgb[3]) {
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < c->h; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < c->w; x++) {
            unsigned char *p = c->px + (y * c->w + x) * 3;
            p[0] = rgb[0]; p[1] = rgb[1]; p[2] = rgb[2];
        }
}

static int utf8_next(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    int cp;
    if (!*p) return 0;
    if (p[0] < 0x80) { cp = p[0]; *s += 1; }
    else if ((p[0] & 0xE0) == 0xC0 && p[1]) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *s += 2; }
    else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *s += 3; }
    else if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) { cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); *s += 4; }
    else { cp = 0xFFFD; *s += 1; }
    return cp;
}

static int font_covers(const stbtt_fontinfo *f, const char *s) {
    int cp;
    while ((cp = utf8_next(&s)))
        if (cp > 32 && !stbtt_FindGlyphIndex(f, cp)) return 0;
    return 1;
}

static float text_width(const stbtt_fontinfo *f, float scale, const char *s, size_t n) {
    const char *end = s + n;
    float w = 0;
    int prev = 0, cp;
    while (s < end && (cp = utf8_next(&s))) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(f, cp, &adv, &lsb);
        if (prev) w += (float)stbtt_GetCodepointKernAdvance(f, prev, cp) * scale;
        w += (float)adv * scale;
        prev = cp;
    }
    return w;
}

static void draw_text(canvas *c, const stbtt_fontinfo *f, float scale, int x, int baseline, const char *s,
                      size_t n, const unsigned char rgb[3]) {
    const char *end = s + n;
    float pen = (float)x;
    int prev = 0, cp;
    while (s < end && (cp = utf8_next(&s))) {
        if (prev) pen += (float)stbtt_GetCodepointKernAdvance(f, prev, cp) * scale;
        int adv, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(f, cp, &adv, &lsb);
        stbtt_GetCodepointBitmapBox(f, cp, scale, scale, &x0, &y0, &x1, &y1);
        int gw = x1 - x0, gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            unsigned char *g = xmalloc((size_t)gw * (size_t)gh);
            stbtt_MakeCodepointBitmap(f, g, gw, gh, gw, scale, scale, cp);
            int ox = (int)(pen + 0.5f) + x0, oy = baseline + y0;
            for (int yy = 0; yy < gh; yy++)
                for (int xx = 0; xx < gw; xx++) {
                    int px = ox + xx, py = oy + yy;
                    if (px < 0 || py < 0 || px >= c->w || py >= c->h) continue;
                    unsigned a = g[yy * gw + xx];
                    if (!a) continue;
                    unsigned char *p = c->px + (py * c->w + px) * 3;
                    for (int k = 0; k < 3; k++) p[k] = (unsigned char)((rgb[k] * a + p[k] * (255 - a)) / 255);
                }
            free(g);
        }
        pen += (float)adv * scale;
        prev = cp;
    }
}

/* Splits text into at most max_lines lines that fit width. Returns the
 * number of lines, or 0 if it does not fit. starts/lens receive byte ranges. */
static int wrap(const stbtt_fontinfo *f, float scale, const char *s, float width, int max_lines,
                size_t *starts, size_t *lens) {
    size_t n = strlen(s), i = 0;
    int lines = 0;
    while (i < n) {
        while (i < n && s[i] == ' ') i++;
        if (i >= n) break;
        if (lines == max_lines) return 0;
        size_t best = 0, j = i;
        while (j <= n) {
            if (j == n || s[j] == ' ') {
                if (text_width(f, scale, s + i, j - i) <= width) best = j;
                else break;
            }
            j++;
        }
        if (!best) return 0; /* a single word is too long */
        starts[lines] = i;
        lens[lines] = best - i;
        lines++;
        i = best;
    }
    return lines;
}

int og_render(const char *out_png, const char *site, const char *title, const char *footer,
              const char *bg, const char *fg, const char *accent) {
    size_t bn = 0, rn = 0;
    const unsigned char *bold = embedded_get("assets/fonts/DejaVuSans-Bold-subset.ttf", &bn);
    const unsigned char *reg = embedded_get("assets/fonts/DejaVuSans-subset.ttf", &rn);
    if (!bold || !reg) return -1;
    stbtt_fontinfo fb, fr;
    if (!stbtt_InitFont(&fb, bold, 0) || !stbtt_InitFont(&fr, reg, 0)) return -1;
    if (!font_covers(&fb, title) || !font_covers(&fr, site) || !font_covers(&fr, footer)) return 1;

    canvas c = {NULL, 1200, 630};
    c.px = xmalloc((size_t)c.w * (size_t)c.h * 3);
    unsigned char cb[3], cf[3], ca[3];
    parse_hex(bg, cb);
    parse_hex(fg, cf);
    parse_hex(accent, ca);
    fill(&c, 0, 0, c.w, c.h, cb);
    fill(&c, 0, 0, c.w, 16, ca);

    int margin = 80;
    float small = stbtt_ScaleForPixelHeight(&fr, 36);
    draw_text(&c, &fr, small, margin, 120, site, strlen(site), ca);

    size_t st[4], ln[4];
    int lines = 0, px = 84;
    float sc = 0;
    for (px = 84; px >= 44; px -= 4) {
        sc = stbtt_ScaleForPixelHeight(&fb, (float)px);
        lines = wrap(&fb, sc, title, (float)(c.w - margin * 2), 3, st, ln);
        if (lines) break;
    }
    if (!lines) { /* very long title: cut it to what fits */
        px = 44;
        sc = stbtt_ScaleForPixelHeight(&fb, (float)px);
        char *cut = xstrdup(title);
        size_t L = strlen(cut);
        while (L > 10) {
            L--;
            while (L && (cut[L] & 0xC0) == 0x80) L--;
            cut[L] = '\0';
            char *tmp = xmalloc(L + 4);
            snprintf(tmp, L + 4, "%s...", cut);
            lines = wrap(&fb, sc, tmp, (float)(c.w - margin * 2), 3, st, ln);
            if (lines) {
                int lh = (int)(px * 1.2);
                int y = 200 + px;
                for (int i = 0; i < lines; i++, y += lh) draw_text(&c, &fb, sc, margin, y, tmp + st[i], ln[i], cf);
                free(tmp);
                break;
            }
            free(tmp);
        }
        free(cut);
    } else {
        int lh = (int)(px * 1.2);
        int y = 200 + px;
        for (int i = 0; i < lines; i++, y += lh) draw_text(&c, &fb, sc, margin, y, title + st[i], ln[i], cf);
    }

    if (*footer) {
        unsigned char muted[3];
        for (int k = 0; k < 3; k++) muted[k] = (unsigned char)((cf[k] * 3 + cb[k] * 2) / 5);
        float fs = stbtt_ScaleForPixelHeight(&fr, 30);
        draw_text(&c, &fr, fs, margin, c.h - 70, footer, strlen(footer), muted);
    }

    char *dir = xstrdup(out_png);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = '\0'; mkdir_p(dir); }
    free(dir);
    stbi_write_png_compression_level = 9;
    int ok = stbi_write_png(out_png, c.w, c.h, 3, c.px, c.w * 3);
    free(c.px);
    return ok ? 0 : -1;
}
