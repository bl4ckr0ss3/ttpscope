#ifndef TTPSCOPE_PATHS_H
#define TTPSCOPE_PATHS_H

#include <stdbool.h>
#include <stddef.h>

bool ttpscope_data_dir(char *buffer, size_t size);
bool ttpscope_cache_dir(char *buffer, size_t size);
bool ttpscope_dataset_path(char *buffer, size_t size);
bool ttpscope_ensure_dir(const char *path);

#endif
