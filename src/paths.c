#include "ttpscope/paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static bool base_path(char *buffer, size_t size, const char *xdg_name, const char *fallback) {
    const char *xdg = getenv(xdg_name);
    int written;
    if (xdg != NULL && *xdg != '\0') written = snprintf(buffer, size, "%s/ttpscope", xdg);
    else {
        const char *home = getenv("HOME");
        if (home == NULL || *home == '\0') return false;
        written = snprintf(buffer, size, "%s/%s/ttpscope", home, fallback);
    }
    return written >= 0 && (size_t)written < size;
}

bool ttpscope_data_dir(char *buffer, size_t size) { return base_path(buffer, size, "XDG_DATA_HOME", ".local/share"); }
bool ttpscope_cache_dir(char *buffer, size_t size) { return base_path(buffer, size, "XDG_CACHE_HOME", ".cache"); }

bool ttpscope_dataset_path(char *buffer, size_t size) {
    char dir[4096];
    if (!ttpscope_cache_dir(dir, sizeof(dir))) return false;
    int written = snprintf(buffer, size, "%s/enterprise-attack.json", dir);
    return written >= 0 && (size_t)written < size;
}

bool ttpscope_ensure_dir(const char *path) {
    char buffer[4096];
    if (strlen(path) >= sizeof(buffer)) return false;
    strcpy(buffer, path);
    for (char *p = buffer + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) return false;
        *p = '/';
    }
    return mkdir(buffer, 0755) == 0 || errno == EEXIST;
}
