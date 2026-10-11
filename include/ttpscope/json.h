#ifndef TTPSCOPE_JSON_H
#define TTPSCOPE_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    JSON_UNDEFINED = 0,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_PRIMITIVE
} JsonType;

typedef struct {
    JsonType type;
    size_t start;
    size_t end;
    int parent;
    size_t children;
} JsonToken;

typedef struct {
    char *text;
    size_t length;
    JsonToken *tokens;
    size_t count;
    size_t capacity;
} JsonDoc;

bool json_load_file(const char *path, JsonDoc *doc, char *error, size_t error_size);
bool json_parse_owned(char *text, size_t length, JsonDoc *doc, char *error, size_t error_size);
void json_free(JsonDoc *doc);
int json_object_get(const JsonDoc *doc, int object, const char *key);
int json_array_at(const JsonDoc *doc, int array, size_t index);
size_t json_array_size(const JsonDoc *doc, int array);
bool json_token_equals(const JsonDoc *doc, int token, const char *text);
bool json_token_bool(const JsonDoc *doc, int token, bool fallback);
char *json_token_strdup(const JsonDoc *doc, int token);

#endif
