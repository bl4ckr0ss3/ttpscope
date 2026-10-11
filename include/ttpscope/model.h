#ifndef TTPSCOPE_MODEL_H
#define TTPSCOPE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    ATTACK_UNKNOWN = 0,
    ATTACK_TECHNIQUE,
    ATTACK_TACTIC,
    ATTACK_GROUP,
    ATTACK_SOFTWARE,
    ATTACK_MITIGATION,
    ATTACK_CAMPAIGN,
    ATTACK_DATA_SOURCE,
    ATTACK_DATA_COMPONENT
} AttackKind;

typedef struct {
    char *stix_id;
    char *attack_id;
    char *name;
    char *description;
    char *url;
    char *created;
    char *modified;
    char *platforms;
    char *tactics;
    char *aliases;
    char *shortname;
    AttackKind kind;
    bool revoked;
    bool deprecated;
} AttackObject;

typedef struct {
    char *source_ref;
    char *target_ref;
    char *type;
    char *description;
} AttackRelationship;

typedef struct {
    AttackObject *objects;
    size_t object_count;
    size_t object_capacity;
    AttackRelationship *relationships;
    size_t relationship_count;
    size_t relationship_capacity;
    char *spec_version;
} AttackDb;

void attack_db_init(AttackDb *db);
void attack_db_free(AttackDb *db);
const AttackObject *attack_find(const AttackDb *db, const char *id);
const char *attack_kind_name(AttackKind kind);
void attack_print_object(const AttackDb *db, const AttackObject *object, FILE *out);
size_t attack_search(const AttackDb *db, const char *query, const AttackObject ***results);

#endif
