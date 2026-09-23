/* tantu image: responsive image sizes and social preview images. */
#ifndef TT_IMAGE_H
#define TT_IMAGE_H

/* Reads the size of a PNG or JPEG file. Returns 0 on success. */
int img_info(const char *path, int *w, int *h);

/* Writes a copy of src no wider than max_w (same format). Returns 0 on success. */
int img_resize(const char *src, const char *dst, int max_w);

/* Draws a 1200x630 PNG with the site name and page title.
 * Colors are "#rrggbb". Returns 0 on success, 1 if the title uses
 * characters the built-in font cannot draw, -1 on error. */
int og_render(const char *out_png, const char *site, const char *title, const char *footer,
              const char *bg, const char *fg, const char *accent);

#endif
