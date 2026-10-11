#include "ttpscope/model.h"
#include "ttpscope/notes.h"
#include "ttpscope/paths.h"
#include "ttpscope/render.h"
#include "ttpscope/stix.h"
#include "ttpscope/sync.h"
#include "ttpscope/tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef TTPSCOPE_VERSION
#define TTPSCOPE_VERSION "development"
#endif

static void usage(FILE *out) {
    fprintf(out,
        "TTPscope %s — terminal research for MITRE ATT&CK\n\n"
        "Usage:\n"
        "  ttpscope sync                       Download current Enterprise ATT&CK data\n"
        "  ttpscope import FILE                Install a local STIX bundle\n"
        "  ttpscope search QUERY               Search IDs, names, aliases and descriptions\n"
        "  ttpscope show ID                    Show an object and its relationships\n"
        "  ttpscope matrix [--platform NAME]   Render the Enterprise matrix\n"
        "  ttpscope note ID TEXT...            Append a timestamped research note\n"
        "  ttpscope notes ID                   Read research notes\n"
        "  ttpscope tui                        Open the interactive interface\n"
        "  ttpscope version                    Print version information\n",
        TTPSCOPE_VERSION);
}

static bool load_database(AttackDb *db) {
    char path[4096], error[512];
    if (!ttpscope_dataset_path(path, sizeof(path))) { fprintf(stderr, "ttpscope: cannot determine dataset path\n"); return false; }
    if (!stix_load(path, db, error, sizeof(error))) {
        fprintf(stderr, "ttpscope: %s\nRun 'ttpscope sync' or 'ttpscope import FILE' first.\n", error);
        return false;
    }
    return true;
}

static char *join_arguments(int argc, char **argv, int start) {
    size_t length = 1;
    for (int i = start; i < argc; i++) length += strlen(argv[i]) + 1;
    char *text = malloc(length);
    if (text == NULL) return NULL;
    *text = '\0';
    for (int i = start; i < argc; i++) { if (i > start) strcat(text, " "); strcat(text, argv[i]); }
    return text;
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) { usage(stdout); return 0; }
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0) { printf("ttpscope %s\n", TTPSCOPE_VERSION); return 0; }
    char error[512] = {0};
    if (strcmp(argv[1], "sync") == 0) {
        fprintf(stderr, "Syncing Enterprise ATT&CK data...\n");
        if (!ttpscope_sync(error, sizeof(error))) { fprintf(stderr, "ttpscope: %s\n", error); return 1; }
        AttackDb check; attack_db_init(&check);
        if (!load_database(&check)) { attack_db_free(&check); return 1; }
        printf("Loaded %zu objects and %zu relationships.\n", check.object_count, check.relationship_count);
        attack_db_free(&check); return 0;
    }
    if (strcmp(argv[1], "import") == 0) {
        if (argc != 3) { fprintf(stderr, "usage: ttpscope import FILE\n"); return 2; }
        AttackDb check; attack_db_init(&check);
        if (!stix_load(argv[2], &check, error, sizeof(error))) { fprintf(stderr, "ttpscope: invalid STIX bundle: %s\n", error); return 1; }
        attack_db_free(&check);
        if (!ttpscope_import(argv[2], error, sizeof(error))) { fprintf(stderr, "ttpscope: %s\n", error); return 1; }
        puts("Dataset imported."); return 0;
    }
    if (strcmp(argv[1], "note") == 0) {
        if (argc < 4) { fprintf(stderr, "usage: ttpscope note ID TEXT...\n"); return 2; }
        char *text = join_arguments(argc, argv, 3);
        if (text == NULL || !note_append(argv[2], text, error, sizeof(error))) { fprintf(stderr, "ttpscope: %s\n", text ? error : "out of memory"); free(text); return 1; }
        free(text); printf("Note added to %s.\n", argv[2]); return 0;
    }
    if (strcmp(argv[1], "notes") == 0) {
        if (argc != 3) { fprintf(stderr, "usage: ttpscope notes ID\n"); return 2; }
        if (!note_print(argv[2], stdout, error, sizeof(error))) { fprintf(stderr, "ttpscope: %s\n", error); return 1; }
        return 0;
    }
    AttackDb db; attack_db_init(&db);
    if (!load_database(&db)) return 1;
    int status = 0;
    if (strcmp(argv[1], "search") == 0) {
        if (argc < 3) { fprintf(stderr, "usage: ttpscope search QUERY\n"); status = 2; }
        else {
            char *query = join_arguments(argc, argv, 2);
            const AttackObject **results = NULL;
            size_t count = attack_search(&db, query, &results);
            for (size_t i = 0; i < count; i++) printf("%-13s %-14s %s\n", results[i]->attack_id ? results[i]->attack_id : "-", attack_kind_name(results[i]->kind), results[i]->name ? results[i]->name : "");
            free((void *)results); free(query);
            if (count == 0) status = 3;
        }
    } else if (strcmp(argv[1], "show") == 0) {
        if (argc != 3) { fprintf(stderr, "usage: ttpscope show ID\n"); status = 2; }
        else {
            const AttackObject *object = attack_find(&db, argv[2]);
            if (object == NULL) { fprintf(stderr, "ttpscope: object not found: %s\n", argv[2]); status = 3; }
            else attack_print_object(&db, object, stdout);
        }
    } else if (strcmp(argv[1], "matrix") == 0) {
        const char *platform = NULL;
        if (argc == 4 && strcmp(argv[2], "--platform") == 0) platform = argv[3];
        else if (argc != 2) { fprintf(stderr, "usage: ttpscope matrix [--platform NAME]\n"); status = 2; }
        if (status == 0) attack_render_matrix(&db, platform, stdout);
    } else if (strcmp(argv[1], "tui") == 0) {
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) { fprintf(stderr, "ttpscope: tui requires an interactive terminal\n"); status = 1; }
        else status = tui_run(&db);
    } else { fprintf(stderr, "ttpscope: unknown command: %s\n\n", argv[1]); usage(stderr); status = 2; }
    attack_db_free(&db);
    return status;
}
