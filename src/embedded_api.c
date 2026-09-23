#define _XOPEN_SOURCE 700
#include "embedded.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

const unsigned char *embedded_get(const char *path, size_t *len) {
    for (size_t i = 0; i < tt_nfiles; i++) {
        if (!strcmp(tt_files[i].path, path)) {
            if (len) *len = tt_files[i].len;
            return tt_files[i].data;
        }
    }
    return NULL;
}

int extract_prefix(const char *prefix, const char *dst) {
    size_t pl = strlen(prefix);
    int n = 0;
    for (size_t i = 0; i < tt_nfiles; i++) {
        if (strncmp(tt_files[i].path, prefix, pl) != 0) continue;
        char *out = path_join(dst, tt_files[i].path + pl);
        int rc = write_file(out, (const char *)tt_files[i].data, tt_files[i].len);
        free(out);
        if (rc != 0) return -1;
        n++;
    }
    return n;
}

int theme_exists(const char *name) {
    if (!*name || strchr(name, '/') || strchr(name, '.')) return 0;
    char path[256];
    snprintf(path, sizeof path, "themes/%s/theme.conf", name);
    return embedded_get(path, NULL) != NULL;
}

void list_themes(FILE *out) {
    for (size_t i = 0; i < tt_nfiles; i++) {
        const char *p = tt_files[i].path;
        size_t l = strlen(p);
        if (strncmp(p, "themes/", 7) != 0 || l < 18 || strcmp(p + l - 11, "/theme.conf") != 0) continue;
        char *text = xstrndup((const char *)tt_files[i].data, tt_files[i].len);
        map m = {0};
        char *save = NULL;
        for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            map_set(&m, trim(line), trim(eq + 1));
        }
        const char *id = map_get(&m, "id"), *desc = map_get(&m, "description");
        fprintf(out, "  %-12s %s\n", id ? id : "?", desc ? desc : "");
        map_free(&m);
        free(text);
    }
}
