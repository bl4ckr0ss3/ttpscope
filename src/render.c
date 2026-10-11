#include "ttpscope/render.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct {
    const char *shortname;
    const char *label;
} Tactic;

static const Tactic tactics[] = {
    {"reconnaissance", "Reconnaissance"}, {"resource-development", "Resource Development"},
    {"initial-access", "Initial Access"}, {"execution", "Execution"},
    {"persistence", "Persistence"}, {"privilege-escalation", "Privilege Escalation"},
    {"defense-evasion", "Defense Evasion"}, {"credential-access", "Credential Access"},
    {"discovery", "Discovery"}, {"lateral-movement", "Lateral Movement"},
    {"collection", "Collection"}, {"command-and-control", "Command and Control"},
    {"exfiltration", "Exfiltration"}, {"impact", "Impact"}
};

static bool contains_fold(const char *haystack, const char *needle) {
    if (needle == NULL || *needle == '\0') return true;
    if (haystack == NULL) return false;
    size_t length = strlen(needle);
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (i < length && haystack[i] && tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == length) return true;
    }
    return false;
}

static int by_id(const void *a, const void *b) {
    const AttackObject *left = *(const AttackObject *const *)a;
    const AttackObject *right = *(const AttackObject *const *)b;
    return strcmp(left->attack_id ? left->attack_id : "", right->attack_id ? right->attack_id : "");
}

static size_t techniques_for(const AttackDb *db, const char *tactic, const char *platform, const AttackObject ***result) {
    size_t count = 0, capacity = 0;
    *result = NULL;
    for (size_t i = 0; i < db->object_count; i++) {
        const AttackObject *o = &db->objects[i];
        if (o->kind != ATTACK_TECHNIQUE || o->revoked || o->deprecated || !contains_fold(o->tactics, tactic) || !contains_fold(o->platforms, platform)) continue;
        if (count == capacity) {
            size_t next = capacity == 0 ? 32 : capacity * 2;
            const AttackObject **grown = realloc((void *)*result, next * sizeof(**result));
            if (grown == NULL) { free((void *)*result); *result = NULL; return 0; }
            *result = grown; capacity = next;
        }
        (*result)[count++] = o;
    }
    qsort((void *)*result, count, sizeof(**result), by_id);
    return count;
}

static void cell(FILE *out, const char *text, int width) {
    int max = width - 1;
    int used = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p && used < max; p++) {
        if (*p < 0x80) { fputc(*p, out); used++; }
        else { fputc(*p, out); while ((p[1] & 0xc0) == 0x80) fputc(*++p, out); used++; }
    }
    while (used++ < width) fputc(' ', out);
}

void attack_render_matrix(const AttackDb *db, const char *platform, FILE *out) {
    struct winsize terminal = {0};
    int width = 100;
    if (ioctl(fileno(out), TIOCGWINSZ, &terminal) == 0 && terminal.ws_col > 20) width = terminal.ws_col;
    int column_width = width >= 120 ? 28 : 24;
    int columns = width / (column_width + 1);
    if (columns < 1) columns = 1;
    fprintf(out, "Enterprise matrix%s%s\n\n", platform && *platform ? " · " : "", platform && *platform ? platform : "");
    size_t tactic_count = sizeof(tactics) / sizeof(tactics[0]);
    for (size_t base = 0; base < tactic_count; base += (size_t)columns) {
        size_t group = tactic_count - base < (size_t)columns ? tactic_count - base : (size_t)columns;
        const AttackObject **lists[8] = {0};
        size_t counts[8] = {0};
        size_t rows = 0;
        for (size_t c = 0; c < group; c++) {
            counts[c] = techniques_for(db, tactics[base + c].shortname, platform, &lists[c]);
            if (counts[c] > rows) rows = counts[c];
            fputc('|', out); cell(out, tactics[base + c].label, column_width);
        }
        fprintf(out, "|\n");
        for (size_t c = 0; c < group; c++) { fputc('+', out); for (int n = 0; n < column_width; n++) fputc('-', out); }
        fprintf(out, "+\n");
        for (size_t row = 0; row < rows; row++) {
            for (size_t c = 0; c < group; c++) {
                fputc('|', out);
                if (row < counts[c]) {
                    char label[512];
                    snprintf(label, sizeof(label), "%s %s", lists[c][row]->attack_id ? lists[c][row]->attack_id : "", lists[c][row]->name ? lists[c][row]->name : "");
                    cell(out, label, column_width);
                } else cell(out, "", column_width);
            }
            fprintf(out, "|\n");
        }
        fprintf(out, "\n");
        for (size_t c = 0; c < group; c++) free((void *)lists[c]);
    }
}
