#include "ttpscope/stix.h"
#include "ttpscope/json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AttackKind kind_from_type(const char *type) {
    if (type == NULL) return ATTACK_UNKNOWN;
    if (strcmp(type, "attack-pattern") == 0) return ATTACK_TECHNIQUE;
    if (strcmp(type, "x-mitre-tactic") == 0) return ATTACK_TACTIC;
    if (strcmp(type, "intrusion-set") == 0) return ATTACK_GROUP;
    if (strcmp(type, "malware") == 0 || strcmp(type, "tool") == 0) return ATTACK_SOFTWARE;
    if (strcmp(type, "course-of-action") == 0) return ATTACK_MITIGATION;
    if (strcmp(type, "campaign") == 0) return ATTACK_CAMPAIGN;
    if (strcmp(type, "x-mitre-data-source") == 0) return ATTACK_DATA_SOURCE;
    if (strcmp(type, "x-mitre-data-component") == 0) return ATTACK_DATA_COMPONENT;
    return ATTACK_UNKNOWN;
}

static bool reserve_objects(AttackDb *db) {
    if (db->object_count < db->object_capacity) return true;
    size_t capacity = db->object_capacity == 0 ? 1024 : db->object_capacity * 2;
    if (capacity > SIZE_MAX / sizeof(*db->objects)) return false;
    AttackObject *objects = realloc(db->objects, capacity * sizeof(*objects));
    if (objects == NULL) return false;
    db->objects = objects;
    db->object_capacity = capacity;
    return true;
}

static bool reserve_relationships(AttackDb *db) {
    if (db->relationship_count < db->relationship_capacity) return true;
    size_t capacity = db->relationship_capacity == 0 ? 2048 : db->relationship_capacity * 2;
    if (capacity > SIZE_MAX / sizeof(*db->relationships)) return false;
    AttackRelationship *relationships = realloc(db->relationships, capacity * sizeof(*relationships));
    if (relationships == NULL) return false;
    db->relationships = relationships;
    db->relationship_capacity = capacity;
    return true;
}

static char *field(const JsonDoc *doc, int object, const char *name) {
    return json_token_strdup(doc, json_object_get(doc, object, name));
}

static char *join_array(const JsonDoc *doc, int array, const char *nested_key) {
    size_t count = json_array_size(doc, array);
    if (count == 0) return NULL;
    size_t length = 1;
    for (size_t i = 0; i < count; i++) {
        int token = json_array_at(doc, array, i);
        if (nested_key != NULL) token = json_object_get(doc, token, nested_key);
        if (token >= 0) length += doc->tokens[token].end - doc->tokens[token].start + 2;
    }
    char *joined = calloc(length, 1);
    if (joined == NULL) return NULL;
    bool first = true;
    for (size_t i = 0; i < count; i++) {
        int token = json_array_at(doc, array, i);
        if (nested_key != NULL) token = json_object_get(doc, token, nested_key);
        char *value = json_token_strdup(doc, token);
        if (value == NULL) continue;
        if (!first) strcat(joined, ", ");
        strcat(joined, value);
        first = false;
        free(value);
    }
    if (*joined == '\0') { free(joined); return NULL; }
    return joined;
}

static void parse_external_reference(const JsonDoc *doc, int object, AttackObject *out) {
    int references = json_object_get(doc, object, "external_references");
    size_t count = json_array_size(doc, references);
    for (size_t i = 0; i < count; i++) {
        int reference = json_array_at(doc, references, i);
        int external_id = json_object_get(doc, reference, "external_id");
        if (external_id < 0) continue;
        char *candidate = json_token_strdup(doc, external_id);
        if (candidate == NULL) continue;
        if (candidate[0] == 'T' || candidate[0] == 'G' || candidate[0] == 'S' || candidate[0] == 'M' || candidate[0] == 'C' || candidate[0] == 'D') {
            out->attack_id = candidate;
            out->url = field(doc, reference, "url");
            return;
        }
        free(candidate);
    }
}

static bool parse_object(const JsonDoc *doc, int object, AttackDb *db) {
    char *type = field(doc, object, "type");
    AttackKind kind = kind_from_type(type);
    free(type);
    if (kind == ATTACK_UNKNOWN) return true;
    if (!reserve_objects(db)) return false;
    AttackObject *out = &db->objects[db->object_count];
    *out = (AttackObject){0};
    out->kind = kind;
    out->stix_id = field(doc, object, "id");
    out->name = field(doc, object, "name");
    out->description = field(doc, object, "description");
    out->created = field(doc, object, "created");
    out->modified = field(doc, object, "modified");
    out->shortname = field(doc, object, "x_mitre_shortname");
    out->platforms = join_array(doc, json_object_get(doc, object, "x_mitre_platforms"), NULL);
    out->aliases = join_array(doc, json_object_get(doc, object, "aliases"), NULL);
    out->tactics = join_array(doc, json_object_get(doc, object, "kill_chain_phases"), "phase_name");
    out->revoked = json_token_bool(doc, json_object_get(doc, object, "revoked"), false);
    out->deprecated = json_token_bool(doc, json_object_get(doc, object, "x_mitre_deprecated"), false);
    parse_external_reference(doc, object, out);
    if (out->kind == ATTACK_TACTIC && out->attack_id == NULL && out->shortname != NULL) out->attack_id = strdup(out->shortname);
    db->object_count++;
    return true;
}

static bool parse_relationship(const JsonDoc *doc, int object, AttackDb *db) {
    if (!reserve_relationships(db)) return false;
    AttackRelationship *out = &db->relationships[db->relationship_count];
    *out = (AttackRelationship){0};
    out->source_ref = field(doc, object, "source_ref");
    out->target_ref = field(doc, object, "target_ref");
    out->type = field(doc, object, "relationship_type");
    out->description = field(doc, object, "description");
    db->relationship_count++;
    return true;
}

bool stix_load(const char *path, AttackDb *db, char *error, size_t error_size) {
    JsonDoc doc;
    if (!json_load_file(path, &doc, error, error_size)) return false;
    int objects = json_object_get(&doc, 0, "objects");
    if (objects < 0 || doc.tokens[objects].type != JSON_ARRAY) {
        snprintf(error, error_size, "STIX bundle does not contain an objects array");
        json_free(&doc);
        return false;
    }
    AttackDb loaded;
    attack_db_init(&loaded);
    loaded.spec_version = field(&doc, 0, "spec_version");
    for (size_t i = (size_t)objects + 1; i < doc.count && doc.tokens[i].start < doc.tokens[objects].end; i++) {
        if (doc.tokens[i].parent != objects) continue;
        int object = (int)i;
        int type_token = json_object_get(&doc, object, "type");
        bool ok = json_token_equals(&doc, type_token, "relationship")
            ? parse_relationship(&doc, object, &loaded)
            : parse_object(&doc, object, &loaded);
        if (!ok) {
            snprintf(error, error_size, "out of memory while loading STIX data");
            attack_db_free(&loaded);
            json_free(&doc);
            return false;
        }
    }
    json_free(&doc);
    attack_db_free(db);
    *db = loaded;
    return true;
}
