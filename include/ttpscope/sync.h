#ifndef TTPSCOPE_SYNC_H
#define TTPSCOPE_SYNC_H

#include <stdbool.h>
#include <stddef.h>

#define TTPSCOPE_DATA_URL "https://raw.githubusercontent.com/mitre-attack/attack-stix-data/master/enterprise-attack/enterprise-attack.json"

bool ttpscope_sync(char *error, size_t error_size);
bool ttpscope_import(const char *source, char *error, size_t error_size);

#endif
