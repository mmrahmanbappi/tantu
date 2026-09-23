/* tantu seo: meta tags, Open Graph, schema and site-wide SEO files. */
#ifndef TT_SEO_H
#define TT_SEO_H

#include "util.h"

void seo_head(const map *site, const map *page, buf *out);
void seo_sitemap(const map *site, map **pages, int n, buf *out);
void seo_robots(const map *site, buf *out);
void seo_feed(const map *site, map **posts, int n, buf *out);
void seo_htaccess(const map *site, buf *out);
void seo_headers_file(buf *out);

#endif
