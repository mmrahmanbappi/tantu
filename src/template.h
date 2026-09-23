/* tantu template: a tiny, logic-light template engine.
 *
 *   {{ name }}            value, HTML escaped
 *   {{{ name }}}          value, raw (only for trusted HTML such as content)
 *   {{#if name}}..{{else}}..{{/if}}
 *   {{#each list}}..{{/each}}
 *   {{> partial}}
 *
 * Names are looked up in the current scope first, then in parent scopes.
 */
#ifndef TT_TEMPLATE_H
#define TT_TEMPLATE_H

#include "util.h"

typedef struct {
    const char *name;
    map **items;
    int n;
} tlist;

typedef struct tctx {
    const map *vars;
    const struct tctx *parent;
    const tlist *lists;
    int nlists;
    const map *partials;
} tctx;

void tpl_render(const char *tpl, size_t n, const tctx *ctx, buf *out);

#endif
