#include "ttpscope/notes.h"
#include "ttpscope/paths.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static bool note_path(const char *id, char *path, size_t size, char *error, size_t error_size) {
    if (id == NULL || *id == '\0') { snprintf(error, error_size, "an ATT&CK ID is required"); return false; }
    for (const char *p = id; *p; p++) if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-') {
        snprintf(error, error_size, "invalid ATT&CK ID: %s", id); return false;
    }
    char dir[4096];
    if (!ttpscope_data_dir(dir, sizeof(dir))) { snprintf(error, error_size, "cannot determine data directory"); return false; }
    char notes[4096];
    int n = snprintf(notes, sizeof(notes), "%s/notes", dir);
    if (n < 0 || (size_t)n >= sizeof(notes) || !ttpscope_ensure_dir(notes)) { snprintf(error, error_size, "cannot create notes directory"); return false; }
    n = snprintf(path, size, "%s/%s.md", notes, id);
    if (n < 0 || (size_t)n >= size) { snprintf(error, error_size, "note path is too long"); return false; }
    return true;
}

bool note_append(const char *id, const char *text, char *error, size_t error_size) {
    char path[4096];
    if (!note_path(id, path, sizeof(path), error, error_size)) return false;
    FILE *file = fopen(path, "a");
    if (file == NULL) { snprintf(error, error_size, "%s: %s", path, strerror(errno)); return false; }
    time_t now = time(NULL);
    struct tm utc;
    gmtime_r(&now, &utc);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    long at = ftell(file);
    if (at == 0) fprintf(file, "# %s research notes\n\n", id);
    fprintf(file, "## %s\n\n%s\n\n", timestamp, text);
    bool ok = fclose(file) == 0;
    if (!ok) snprintf(error, error_size, "cannot save note: %s", strerror(errno));
    return ok;
}

bool note_print(const char *id, FILE *out, char *error, size_t error_size) {
    char path[4096];
    if (!note_path(id, path, sizeof(path), error, error_size)) return false;
    FILE *file = fopen(path, "r");
    if (file == NULL) { snprintf(error, error_size, "%s", errno == ENOENT ? "no notes for this object" : strerror(errno)); return false; }
    char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) fwrite(buffer, 1, count, out);
    bool ok = !ferror(file);
    fclose(file);
    if (!ok) snprintf(error, error_size, "cannot read note");
    return ok;
}
