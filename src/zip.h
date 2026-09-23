/* tantu zip: writes a folder into a .zip file (stored, no compression). */
#ifndef TT_ZIP_H
#define TT_ZIP_H

#include "util.h"

/* Appends a zip archive of every file under dir to out. Returns the number
 * of files, or -1 on error. Hidden files are included (such as .htaccess). */
int zip_dir(const char *dir, buf *out);

#endif
