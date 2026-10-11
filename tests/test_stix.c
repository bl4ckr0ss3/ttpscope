#include "ttpscope/model.h"
#include "ttpscope/stix.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    AttackDb db;
    attack_db_init(&db);
    char error[256];
    assert(stix_load(FIXTURE_PATH, &db, error, sizeof(error)));
    assert(db.object_count == 2);
    assert(db.relationship_count == 1);
    const AttackObject *technique = attack_find(&db, "T1059.001");
    assert(technique != NULL);
    assert(strcmp(technique->name, "PowerShell") == 0);
    assert(strcmp(technique->platforms, "Windows") == 0);
    const AttackObject **results = NULL;
    assert(attack_search(&db, "power", &results) == 1);
    free((void *)results);
    attack_db_free(&db);
    return 0;
}
