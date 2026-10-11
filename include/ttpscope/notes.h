#ifndef TTPSCOPE_NOTES_H
#define TTPSCOPE_NOTES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

bool note_append(const char *attack_id, const char *text, char *error, size_t error_size);
bool note_print(const char *attack_id, FILE *out, char *error, size_t error_size);

#endif
