/* tantu markdown: safe Markdown to HTML. */
#ifndef TT_MARKDOWN_H
#define TT_MARKDOWN_H

#include "util.h"

/* Converts Markdown to HTML. base is the site's base path (for example
 * "/mysite" or ""), added to root-relative links and images. */
void md_to_html(const char *src, const char *base, buf *out);

#endif
