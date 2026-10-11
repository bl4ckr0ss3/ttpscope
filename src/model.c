#include "ttpscope/model.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void attack_db_init(AttackDb *db) { *db = (AttackDb){0}; }

void attack_db_free(AttackDb *db) {
    for (size_t i = 0; i < db->object_count; i++) {
        AttackObject *o = &db->objects[i];
        free(o->stix_id); free(o->attack_id); free(o->name); free(o->description);
        free(o->url); free(o->created); free(o->modified); free(o->platforms);
        free(o->tactics); free(o->aliases); free(o->shortname);
    }
    for (size_t i = 0; i < db->relationship_count; i++) {
        AttackRelationship *r = &db->relationships[i];
        free(r->source_ref); free(r->target_ref); free(r->type); free(r->description);
    }
    free(db->objects); free(db->relationships); free(db->spec_version);
    *db = (AttackDb){0};
}

const char *attack_kind_name(AttackKind kind) {
    switch (kind) {
        case ATTACK_TECHNIQUE: return "technique";
        case ATTACK_TACTIC: return "tactic";
        case ATTACK_GROUP: return "group";
        case ATTACK_SOFTWARE: return "software";
        case ATTACK_MITIGATION: return "mitigation";
        case ATTACK_CAMPAIGN: return "campaign";
        case ATTACK_DATA_SOURCE: return "data-source";
        case ATTACK_DATA_COMPONENT: return "data-component";
        default: return "unknown";
    }
}

static bool equal_fold(const char *a, const char *b) {
    if (a == NULL || b == NULL) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
    }
    return *a == *b;
}

const AttackObject *attack_find(const AttackDb *db, const char *id) {
    for (size_t i = 0; i < db->object_count; i++) {
        const AttackObject *object = &db->objects[i];
        if (equal_fold(object->attack_id, id) || equal_fold(object->stix_id, id)) return object;
    }
    return NULL;
}

static bool contains_fold(const char *haystack, const char *needle) {
    if (needle != NULL && *needle == '\0') return true;
    if (haystack == NULL || needle == NULL) return false;
    size_t length = strlen(needle);
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (i < length && haystack[i] && tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == length) return true;
    }
    return false;
}

size_t attack_search(const AttackDb *db, const char *query, const AttackObject ***results) {
    typedef struct { const AttackObject *object; int score; } Ranked;
    Ranked *ranked = NULL;
    size_t count = 0;
    size_t capacity = 0;
    for (size_t i = 0; i < db->object_count; i++) {
        const AttackObject *o = &db->objects[i];
        if (o->revoked || o->deprecated) continue;
        int score = 0;
        if (query != NULL && *query == '\0') score = 1;
        else if (equal_fold(o->attack_id, query)) score = 1000;
        else {
            if (equal_fold(o->name, query)) score = 800;
            else if (o->name != NULL && strncasecmp(o->name, query, strlen(query)) == 0) score = 600;
            else if (contains_fold(o->name, query)) score = 400;
            else if (contains_fold(o->aliases, query)) score = 300;
            else if (contains_fold(o->attack_id, query)) score = 250;
            else if (contains_fold(o->description, query)) score = 100;
        }
        if (score == 0) continue;
        if (count == capacity) {
            size_t next = capacity == 0 ? 32 : capacity * 2;
            Ranked *grown = realloc(ranked, next * sizeof(*ranked));
            if (grown == NULL) { free(ranked); *results = NULL; return 0; }
            ranked = grown;
            capacity = next;
        }
        ranked[count++] = (Ranked){.object = o, .score = score};
    }
    for (size_t i = 1; i < count; i++) {
        Ranked value = ranked[i];
        size_t j = i;
        while (j > 0 && ranked[j - 1].score < value.score) { ranked[j] = ranked[j - 1]; j--; }
        ranked[j] = value;
    }
    const AttackObject **ordered = malloc(count * sizeof(*ordered));
    if (count > 0 && ordered == NULL) { free(ranked); *results = NULL; return 0; }
    for (size_t i = 0; i < count; i++) ordered[i] = ranked[i].object;
    free(ranked);
    *results = ordered;
    return count;
}

static const AttackObject *find_stix(const AttackDb *db, const char *id) {
    for (size_t i = 0; i < db->object_count; i++)
        if (db->objects[i].stix_id != NULL && id != NULL && strcmp(db->objects[i].stix_id, id) == 0) return &db->objects[i];
    return NULL;
}

static void print_field(FILE *out, const char *label, const char *value) {
    if (value != NULL && *value != '\0') fprintf(out, "%-12s %s\n", label, value);
}

void attack_print_object(const AttackDb *db, const AttackObject *o, FILE *out) {
    fprintf(out, "%s%s%s\n", o->attack_id ? o->attack_id : "", o->attack_id ? " — " : "", o->name ? o->name : "(unnamed)");
    fprintf(out, "%-12s %s\n", "Type", attack_kind_name(o->kind));
    print_field(out, "Platforms", o->platforms);
    print_field(out, "Tactics", o->tactics);
    print_field(out, "Aliases", o->aliases);
    print_field(out, "Created", o->created);
    print_field(out, "Modified", o->modified);
    print_field(out, "URL", o->url);
    if (o->revoked) fprintf(out, "Status       revoked\n");
    else if (o->deprecated) fprintf(out, "Status       deprecated\n");
    if (o->description) fprintf(out, "\n%s\n", o->description);

    bool heading = false;
    for (size_t i = 0; i < db->relationship_count; i++) {
        const AttackRelationship *r = &db->relationships[i];
        const char *other_id = NULL;
        const char *direction = NULL;
        if (o->stix_id && r->source_ref && strcmp(o->stix_id, r->source_ref) == 0) { other_id = r->target_ref; direction = "→"; }
        else if (o->stix_id && r->target_ref && strcmp(o->stix_id, r->target_ref) == 0) { other_id = r->source_ref; direction = "←"; }
        if (other_id == NULL) continue;
        const AttackObject *other = find_stix(db, other_id);
        if (other == NULL) continue;
        if (!heading) { fprintf(out, "\nRelationships\n"); heading = true; }
        fprintf(out, "  %s %-12s %-13s %s\n", direction, r->type ? r->type : "related", other->attack_id ? other->attack_id : "", other->name ? other->name : "");
    }
}
