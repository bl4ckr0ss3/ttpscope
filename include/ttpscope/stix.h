#ifndef TTPSCOPE_STIX_H
#define TTPSCOPE_STIX_H

#include <stdbool.h>
#include <stddef.h>
#include "ttpscope/model.h"

bool stix_load(const char *path, AttackDb *db, char *error, size_t error_size);

#endif
