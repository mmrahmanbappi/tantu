/* tantu site: functions shared by the build and the dashboard. */
#ifndef TT_SITE_H
#define TT_SITE_H

#include "util.h"

void parse_conf(const char *text, map *m);
const char *front_matter(const char *src, map *m);
int valid_date(const char *d);
int cmd_build(const char *dir, int quiet);

#endif
