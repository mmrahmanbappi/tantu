/* tantu seo: everything search engines and social networks read.
 *
 * Generates the <head> tags for each page (title, description, canonical,
 * robots, Open Graph, Twitter card, JSON-LD schema) and the site-wide files
 * sitemap.xml, robots.txt, feed.xml, .htaccess and _headers.
 */
#include "seo.h"

#include <stdlib.h>
#include <string.h>

#define GENERATOR "tantu 0.5.0"

static const char *get(const map *m, const char *k) {
    const char *v = map_get(m, k);
    return v ? v : "";
}

/* origin + path, where path already includes the base path */
static char *abs_url(const map *site, const char *path) {
    buf o = {0};
    if (!strncmp(path, "http://", 7) || !strncmp(path, "https://", 8)) {
        buf_puts(&o, path);
    } else {
        buf_puts(&o, get(site, "origin"));
        buf_puts(&o, path);
    }
    return buf_take(&o);
}

/* Image paths in front matter are relative to the site root. */
static char *abs_image(const map *site, const char *img) {
    if (!*img) return xstrdup("");
    if (!strncmp(img, "http://", 7) || !strncmp(img, "https://", 8)) return xstrdup(img);
    buf p = {0};
    buf_puts(&p, get(site, "base_path"));
    if (img[0] != '/') buf_putc(&p, '/');
    buf_puts(&p, img);
    char *path = buf_take(&p);
    char *u = abs_url(site, path);
    free(path);
    return u;
}

static void meta_name(buf *o, const char *name, const char *content) {
    if (!*content) return;
    buf_printf(o, "<meta name=\"%s\" content=\"", name);
    esc_html_s(o, content);
    buf_puts(o, "\">\n");
}

static void meta_prop(buf *o, const char *prop, const char *content) {
    if (!*content) return;
    buf_printf(o, "<meta property=\"%s\" content=\"", prop);
    esc_html_s(o, content);
    buf_puts(o, "\">\n");
}

/* JSON helpers */
static void jstr(buf *o, const char *key, const char *val, int *first) {
    if (!*val) return;
    if (!*first) buf_putc(o, ',');
    *first = 0;
    buf_printf(o, "\"%s\":\"", key);
    esc_json(o, val);
    buf_putc(o, '"');
}

static void jraw(buf *o, const char *key, const char *raw, int *first) {
    if (!*first) buf_putc(o, ',');
    *first = 0;
    buf_printf(o, "\"%s\":%s", key, raw);
}

static void jref(buf *o, const char *key, const char *id, int *first) {
    if (!*first) buf_putc(o, ',');
    *first = 0;
    buf_printf(o, "\"%s\":{\"@id\":\"", key);
    esc_json(o, id);
    buf_puts(o, "\"}");
}

static void jlist_csv(buf *o, const char *key, const char *csv, int *first) {
    if (!*csv) return;
    if (!*first) buf_putc(o, ',');
    *first = 0;
    buf_printf(o, "\"%s\":[", key);
    char *copy = xstrdup(csv);
    int f = 1;
    for (char *tok = strtok(copy, ","); tok; tok = strtok(NULL, ",")) {
        char *v = trim(tok);
        if (!*v) continue;
        if (!f) buf_putc(o, ',');
        f = 0;
        buf_putc(o, '"');
        esc_json(o, v);
        buf_putc(o, '"');
    }
    free(copy);
    buf_putc(o, ']');
}

static void crumb(buf *o, int pos, const char *name, const char *url) {
    if (pos > 1) buf_putc(o, ',');
    buf_printf(o, "{\"@type\":\"ListItem\",\"position\":%d,\"name\":\"", pos);
    esc_json(o, name);
    buf_puts(o, "\",\"item\":\"");
    esc_json(o, url);
    buf_puts(o, "\"}");
}

void seo_head(const map *site, const map *page, buf *o) {
    const char *kind = get(page, "kind");
    const char *site_title = get(site, "title");
    const char *title = get(page, "title");
    const char *desc = get(page, "description");
    if (!*desc) desc = get(site, "description");
    int is_home = !strcmp(kind, "home");
    int is_post = !strcmp(kind, "post");
    int noindex = !strcmp(kind, "404") || !strcmp(get(page, "robots"), "noindex");

    /* Title: "Page title | Site" or "Site | Tagline" on the home page */
    buf full = {0};
    if (is_home) {
        buf_puts(&full, site_title);
        if (*get(site, "tagline")) {
            buf_puts(&full, " | ");
            buf_puts(&full, get(site, "tagline"));
        }
    } else {
        buf_puts(&full, title);
        buf_puts(&full, " | ");
        buf_puts(&full, site_title);
    }

    char *canon = abs_url(site, get(page, "url"));
    const char *img_src = get(page, "image");
    if (!*img_src) img_src = get(site, "og_image");
    char *img = abs_image(site, img_src);
    char *home = abs_url(site, *get(site, "base_path") ? get(site, "base_path") : "/");
    size_t hl = strlen(home);
    if (!hl || home[hl - 1] != '/') {
        char *h2 = xmalloc(hl + 2);
        memcpy(h2, home, hl);
        h2[hl] = '/';
        h2[hl + 1] = '\0';
        free(home);
        home = h2;
    }

    buf_puts(o, "<title>");
    esc_html_s(o, full.s);
    buf_puts(o, "</title>\n");
    meta_name(o, "description", desc);
    if (!noindex) {
        buf_puts(o, "<link rel=\"canonical\" href=\"");
        esc_html_s(o, canon);
        buf_puts(o, "\">\n");
    }
    meta_name(o, "robots", noindex ? "noindex, follow" : "index, follow, max-image-preview:large");
    meta_name(o, "author", get(site, "author"));
    meta_name(o, "generator", GENERATOR);
    meta_name(o, "google-site-verification", get(site, "google_verification"));
    meta_name(o, "msvalidate.01", get(site, "bing_verification"));

    /* Open Graph */
    meta_prop(o, "og:type", is_post ? "article" : "website");
    meta_prop(o, "og:site_name", site_title);
    meta_prop(o, "og:title", is_home ? full.s : title);
    meta_prop(o, "og:description", desc);
    meta_prop(o, "og:url", canon);
    meta_prop(o, "og:locale", get(site, "locale"));
    if (*img) {
        meta_prop(o, "og:image", img);
        meta_prop(o, "og:image:alt", is_home ? full.s : title);
    }
    if (is_post) {
        meta_prop(o, "article:published_time", get(page, "date"));
        meta_prop(o, "article:modified_time",
                  *get(page, "updated") ? get(page, "updated") : get(page, "date"));
        char *tags = xstrdup(get(page, "tags"));
        for (char *tok = strtok(tags, ","); tok; tok = strtok(NULL, ","))
            meta_prop(o, "article:tag", trim(tok));
        free(tags);
    }

    /* Twitter / X */
    meta_name(o, "twitter:card", *img ? "summary_large_image" : "summary");
    meta_name(o, "twitter:site", get(site, "twitter"));
    meta_name(o, "twitter:title", is_home ? full.s : title);
    meta_name(o, "twitter:description", desc);
    if (*img) meta_name(o, "twitter:image", img);

    /* Feed and icon */
    buf_printf(o, "<link rel=\"alternate\" type=\"application/atom+xml\" title=\"");
    esc_html_s(o, site_title);
    buf_puts(o, "\" href=\"");
    esc_html_s(o, get(site, "base_path"));
    buf_puts(o, "/feed.xml\">\n");
    if (*get(site, "favicon")) {
        buf_puts(o, "<link rel=\"icon\" href=\"");
        esc_html_s(o, get(site, "base_path"));
        buf_puts(o, "/");
        esc_html_s(o, get(site, "favicon"));
        buf_puts(o, "\">\n");
    }

    /* JSON-LD */
    if (!noindex) {
        char *site_id = NULL, *owner_id = NULL;
        buf t = {0};
        buf_printf(&t, "%s#website", home);
        site_id = buf_take(&t);
        buf_printf(&t, "%s#owner", home);
        owner_id = buf_take(&t);
        const char *ot = get(site, "owner_type");
        const char *owner_type = "Person";
        if (!strcmp(ot, "Organization") || !strcmp(ot, "LocalBusiness") ||
            !strcmp(ot, "EducationalOrganization") || !strcmp(ot, "Store") ||
            !strcmp(ot, "Restaurant") || !strcmp(ot, "ProfessionalService"))
            owner_type = ot;
        int is_person = !strcmp(owner_type, "Person");

        buf_puts(o, "<script type=\"application/ld+json\">\n{\"@context\":\"https://schema.org\",\"@graph\":[");

        /* WebSite */
        int f = 1;
        buf_putc(o, '{');
        jstr(o, "@type", "WebSite", &f);
        jstr(o, "@id", site_id, &f);
        jstr(o, "url", home, &f);
        jstr(o, "name", site_title, &f);
        jstr(o, "description", get(site, "description"), &f);
        jstr(o, "inLanguage", get(site, "language"), &f);
        jref(o, "publisher", owner_id, &f);
        buf_putc(o, '}');

        /* Owner: Person or Organization */
        f = 1;
        buf_puts(o, ",{");
        jstr(o, "@type", owner_type, &f);
        jstr(o, "@id", owner_id, &f);
        jstr(o, "name", get(site, "author"), &f);
        jstr(o, "url", home, &f);
        jstr(o, is_person ? "jobTitle" : "description",
             get(site, is_person ? "job_title" : "description"), &f);
        if (*get(site, "logo")) {
            char *logo = abs_image(site, get(site, "logo"));
            jstr(o, is_person ? "image" : "logo", logo, &f);
            if (!is_person) jstr(o, "image", logo, &f);
            free(logo);
        }
        jstr(o, "email", get(site, "email"), &f);
        jstr(o, "telephone", get(site, "phone"), &f);
        if (!is_person && *get(site, "address_city")) {
            if (!f) buf_putc(o, ',');
            f = 0;
            int g = 1;
            buf_puts(o, "\"address\":{");
            jstr(o, "@type", "PostalAddress", &g);
            jstr(o, "streetAddress", get(site, "address_street"), &g);
            jstr(o, "addressLocality", get(site, "address_city"), &g);
            jstr(o, "addressRegion", get(site, "address_region"), &g);
            jstr(o, "postalCode", get(site, "address_postal"), &g);
            jstr(o, "addressCountry", get(site, "address_country"), &g);
            buf_putc(o, '}');
        }
        jlist_csv(o, "openingHours", get(site, "opening_hours"), &f);
        jstr(o, "priceRange", get(site, "price_range"), &f);
        jstr(o, "areaServed", get(site, "area_served"), &f);
        jlist_csv(o, "sameAs", get(site, "social"), &f);
        buf_putc(o, '}');

        /* The page itself */
        f = 1;
        buf_puts(o, ",{");
        const char *ptype = "WebPage";
        if (is_home) ptype = *get(site, "home_schema") ? get(site, "home_schema") : "WebPage";
        else if (!strcmp(kind, "list")) ptype = "CollectionPage";
        else if (is_post) ptype = *get(site, "post_schema") ? get(site, "post_schema") : "BlogPosting";
        else if (*get(page, "schema")) ptype = get(page, "schema");
        jstr(o, "@type", ptype, &f);
        buf t2 = {0};
        buf_printf(&t2, "%s#main", canon);
        char *pid = buf_take(&t2);
        jstr(o, "@id", pid, &f);
        jstr(o, "url", canon, &f);
        const map *ev = is_home ? site : page; /* event fields live in site.conf on home pages */
        if (!strcmp(ptype, "Event")) {
            jstr(o, "name", is_home ? site_title : title, &f);
            const char *start = is_home ? get(site, "event_start") : get(page, "start");
            if (!*start) start = get(page, "date");
            jstr(o, "startDate", start, &f);
            jstr(o, "endDate", is_home ? get(site, "event_end") : get(page, "end"), &f);
            jstr(o, "eventStatus", "https://schema.org/EventScheduled", &f);
            int online = !strcmp(get(ev, is_home ? "event_attendance" : "attendance"), "online");
            jstr(o, "eventAttendanceMode", online ? "https://schema.org/OnlineEventAttendanceMode"
                                                  : "https://schema.org/OfflineEventAttendanceMode", &f);
            const char *venue = is_home ? get(site, "event_venue") : get(page, "location");
            if (!*venue) venue = get(site, "event_venue");
            const char *addr = get(site, "event_address");
            if (*venue || online) {
                if (!f) buf_putc(o, ',');
                f = 0;
                int g = 1;
                buf_puts(o, "\"location\":{");
                if (online) {
                    jstr(o, "@type", "VirtualLocation", &g);
                    jstr(o, "url", canon, &g);
                } else {
                    jstr(o, "@type", "Place", &g);
                    jstr(o, "name", venue, &g);
                    jstr(o, "address", addr, &g);
                }
                buf_putc(o, '}');
            }
            jref(o, "organizer", owner_id, &f);
            if (*get(page, "speaker")) {
                if (!f) buf_putc(o, ',');
                f = 0;
                buf_puts(o, "\"performer\":{\"@type\":\"Person\",\"name\":\"");
                esc_json(o, get(page, "speaker"));
                buf_puts(o, "\"}");
            }
        } else if (!strcmp(ptype, "Course")) {
            jstr(o, "name", is_home ? site_title : title, &f);
            jref(o, "provider", owner_id, &f);
            jref(o, "isPartOf", site_id, &f);
        } else if (!strcmp(ptype, "Service")) {
            jstr(o, "name", title, &f);
            jstr(o, "serviceType", title, &f);
            jref(o, "provider", owner_id, &f);
            jstr(o, "areaServed", get(site, "area_served"), &f);
            if (*get(page, "price")) {
                if (!f) buf_putc(o, ',');
                f = 0;
                buf_puts(o, "\"offers\":{\"@type\":\"Offer\",\"price\":\"");
                esc_json(o, get(page, "price"));
                buf_puts(o, "\",\"priceCurrency\":\"");
                esc_json(o, *get(site, "currency") ? get(site, "currency") : "USD");
                buf_puts(o, "\"}");
            }
        } else if (is_post) {
            jstr(o, !strcmp(ptype, "VisualArtwork") || !strcmp(ptype, "LearningResource") ? "name" : "headline",
                 title, &f);
            jstr(o, "datePublished", get(page, "date"), &f);
            jstr(o, "dateModified", *get(page, "updated") ? get(page, "updated") : get(page, "date"), &f);
            if (*get(page, "authors")) {
                if (!f) buf_putc(o, ',');
                f = 0;
                buf_puts(o, "\"author\":[");
                char *au = xstrdup(get(page, "authors"));
                int first_a = 1;
                for (char *tok = strtok(au, ","); tok; tok = strtok(NULL, ",")) {
                    char *nm = trim(tok);
                    if (!*nm) continue;
                    if (!first_a) buf_putc(o, ',');
                    first_a = 0;
                    buf_puts(o, "{\"@type\":\"Person\",\"name\":\"");
                    esc_json(o, nm);
                    buf_puts(o, "\"}");
                }
                free(au);
                buf_putc(o, ']');
            } else {
                jref(o, !strcmp(ptype, "VisualArtwork") ? "creator" : "author", owner_id, &f);
            }
            if (strcmp(ptype, "VisualArtwork")) jref(o, "publisher", owner_id, &f);
            jstr(o, "mainEntityOfPage", canon, &f);
            jlist_csv(o, "keywords", get(page, "tags"), &f);
            if (*get(page, "venue")) {
                if (!f) buf_putc(o, ',');
                f = 0;
                buf_puts(o, "\"isPartOf\":{\"@type\":\"Periodical\",\"name\":\"");
                esc_json(o, get(page, "venue"));
                buf_puts(o, "\"}");
            }
            if (*get(page, "doi")) {
                buf dl = {0};
                buf_printf(&dl, "https://doi.org/%s", get(page, "doi"));
                jstr(o, "sameAs", dl.s, &f);
                buf_free(&dl);
            }
            jstr(o, "educationalLevel", get(page, "level"), &f);
            jstr(o, "timeRequired", get(page, "time_required"), &f);
            if (*get(page, "word_count") && strcmp(ptype, "VisualArtwork")) jraw(o, "wordCount", get(page, "word_count"), &f);
        } else {
            jstr(o, "name", is_home ? full.s : title, &f);
            jref(o, "isPartOf", site_id, &f);
            if (is_home && !strcmp(ptype, "ProfilePage")) jref(o, "mainEntity", owner_id, &f);
            else if (is_home) jref(o, "about", owner_id, &f);
        }
        jstr(o, "description", desc, &f);
        jstr(o, "inLanguage", get(site, "language"), &f);
        if (*img) jstr(o, "image", img, &f);
        buf_putc(o, '}');

        /* Breadcrumbs for everything except the home page */
        if (!is_home) {
            buf_puts(o, ",{\"@type\":\"BreadcrumbList\",\"itemListElement\":[");
            crumb(o, 1, "Home", home);
            int pos = 2;
            if (is_post) {
                char *lurl = abs_url(site, get(page, "list_url"));
                crumb(o, pos++, get(site, "section_title"), lurl);
                free(lurl);
            }
            crumb(o, pos, is_post || strcmp(kind, "list") ? title : get(site, "section_title"), canon);
            buf_puts(o, "]}");
        }

        buf_puts(o, "]}\n</script>\n");
        free(site_id);
        free(owner_id);
        free(pid);
    }

    buf_free(&full);
    free(canon);
    free(img);
    free(home);
}

void seo_sitemap(const map *site, map **pages, int n, buf *o) {
    buf_puts(o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
    for (int i = 0; i < n; i++) {
        if (!strcmp(get(pages[i], "robots"), "noindex")) continue;
        char *u = abs_url(site, get(pages[i], "url"));
        buf_puts(o, "  <url>\n    <loc>");
        esc_html_s(o, u);
        buf_puts(o, "</loc>\n");
        const char *lm = *get(pages[i], "updated") ? get(pages[i], "updated") : get(pages[i], "date");
        if (*lm) {
            buf_puts(o, "    <lastmod>");
            esc_html_s(o, lm);
            buf_puts(o, "</lastmod>\n");
        }
        buf_puts(o, "  </url>\n");
        free(u);
    }
    buf_puts(o, "</urlset>\n");
}

void seo_robots(const map *site, buf *o) {
    char *sm = abs_url(site, get(site, "base_path"));
    buf_printf(o, "User-agent: *\nAllow: /\n\nSitemap: %s/sitemap.xml\n", sm);
    free(sm);
}

void seo_feed(const map *site, map **posts, int n, buf *o) {
    char *home = abs_url(site, get(site, "base_path"));
    buf_puts(o, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<feed xmlns=\"http://www.w3.org/2005/Atom\">\n  <title>");
    esc_html_s(o, get(site, "title"));
    buf_puts(o, "</title>\n  <subtitle>");
    esc_html_s(o, get(site, "description"));
    buf_printf(o, "</subtitle>\n  <link href=\"%s/feed.xml\" rel=\"self\"/>\n  <link href=\"%s/\"/>\n  <id>%s/</id>\n",
               home, home, home);
    const char *updated = n ? get(posts[0], "date") : "2026-01-01";
    buf_printf(o, "  <updated>%sT00:00:00Z</updated>\n  <author><name>", *updated ? updated : "2026-01-01");
    esc_html_s(o, get(site, "author"));
    buf_puts(o, "</name></author>\n  <generator>" GENERATOR "</generator>\n");
    for (int i = 0; i < n && i < 20; i++) {
        char *u = abs_url(site, get(posts[i], "url"));
        const char *d = *get(posts[i], "date") ? get(posts[i], "date") : "2026-01-01";
        buf_puts(o, "  <entry>\n    <title>");
        esc_html_s(o, get(posts[i], "title"));
        buf_printf(o, "</title>\n    <link href=\"%s\"/>\n    <id>%s</id>\n    <updated>%sT00:00:00Z</updated>\n    <summary>",
                   u, u, d);
        esc_html_s(o, get(posts[i], "description"));
        buf_puts(o, "</summary>\n  </entry>\n");
        free(u);
    }
    buf_puts(o, "</feed>\n");
    free(home);
}

#define CSP "default-src 'self'; img-src 'self' data: https:; style-src 'self'; script-src 'self'; " \
            "font-src 'self'; frame-ancestors 'self'; base-uri 'self'; form-action 'self' https:"

void seo_htaccess(const map *site, buf *o) {
    buf_printf(o,
        "# Generated by " GENERATOR " for Apache and LiteSpeed shared hosting.\n"
        "ErrorDocument 404 %s/404.html\n"
        "Options -Indexes\n\n"
        "<IfModule mod_headers.c>\n"
        "  Header always set X-Content-Type-Options \"nosniff\"\n"
        "  Header always set X-Frame-Options \"SAMEORIGIN\"\n"
        "  Header always set Referrer-Policy \"strict-origin-when-cross-origin\"\n"
        "  Header always set Permissions-Policy \"camera=(), microphone=(), geolocation=()\"\n"
        "  Header always set Content-Security-Policy \"" CSP "\"\n"
        "</IfModule>\n\n"
        "<IfModule mod_deflate.c>\n"
        "  AddOutputFilterByType DEFLATE text/html text/css text/plain application/xml application/atom+xml image/svg+xml\n"
        "</IfModule>\n\n"
        "<IfModule mod_expires.c>\n"
        "  ExpiresActive On\n"
        "  ExpiresByType text/css \"access plus 1 month\"\n"
        "  ExpiresByType image/webp \"access plus 1 year\"\n"
        "  ExpiresByType image/png \"access plus 1 year\"\n"
        "  ExpiresByType image/jpeg \"access plus 1 year\"\n"
        "  ExpiresByType image/svg+xml \"access plus 1 year\"\n"
        "</IfModule>\n",
        get(site, "base_path"));
}

void seo_headers_file(buf *o) {
    buf_puts(o,
        "# Security headers for Cloudflare Pages and Netlify. Generated by " GENERATOR ".\n"
        "/*\n"
        "  X-Content-Type-Options: nosniff\n"
        "  X-Frame-Options: SAMEORIGIN\n"
        "  Referrer-Policy: strict-origin-when-cross-origin\n"
        "  Permissions-Policy: camera=(), microphone=(), geolocation=()\n"
        "  Content-Security-Policy: " CSP "\n");
}
