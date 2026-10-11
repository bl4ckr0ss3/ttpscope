#include "ttpscope/json.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *message) {
    if (error != NULL && size > 0) snprintf(error, size, "%s", message);
}

static int add_token(JsonDoc *doc, JsonType type, size_t start, int parent) {
    if (doc->count == doc->capacity) {
        size_t next = doc->capacity == 0 ? 256 : doc->capacity * 2;
        if (next < doc->capacity || next > SIZE_MAX / sizeof(*doc->tokens)) return -1;
        JsonToken *tokens = realloc(doc->tokens, next * sizeof(*tokens));
        if (tokens == NULL) return -1;
        doc->tokens = tokens;
        doc->capacity = next;
    }
    JsonToken *token = &doc->tokens[doc->count];
    *token = (JsonToken){.type = type, .start = start, .end = 0, .parent = parent, .children = 0};
    if (parent >= 0) doc->tokens[parent].children++;
    return (int)doc->count++;
}

static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool parse_string(JsonDoc *doc, size_t *position, int parent, char *error, size_t error_size) {
    size_t start = ++(*position);
    int token = add_token(doc, JSON_STRING, start, parent);
    if (token < 0) { set_error(error, error_size, "out of memory"); return false; }
    while (*position < doc->length) {
        unsigned char c = (unsigned char)doc->text[*position];
        if (c == '"') {
            doc->tokens[token].end = *position;
            (*position)++;
            return true;
        }
        if (c < 0x20) { set_error(error, error_size, "control character in JSON string"); return false; }
        if (c == '\\') {
            (*position)++;
            if (*position >= doc->length) break;
            c = (unsigned char)doc->text[*position];
            if (strchr("\"\\/bfnrt", (int)c) == NULL && c != 'u') {
                set_error(error, error_size, "invalid JSON escape"); return false;
            }
            if (c == 'u') {
                for (int i = 0; i < 4; i++) {
                    (*position)++;
                    if (*position >= doc->length || !((doc->text[*position] >= '0' && doc->text[*position] <= '9') ||
                        (doc->text[*position] >= 'a' && doc->text[*position] <= 'f') ||
                        (doc->text[*position] >= 'A' && doc->text[*position] <= 'F'))) {
                        set_error(error, error_size, "invalid unicode escape"); return false;
                    }
                }
            }
        }
        (*position)++;
    }
    set_error(error, error_size, "unterminated JSON string");
    return false;
}

static bool parse_primitive(JsonDoc *doc, size_t *position, int parent, char *error, size_t error_size) {
    size_t start = *position;
    while (*position < doc->length) {
        char c = doc->text[*position];
        if (is_ws(c) || c == ',' || c == ']' || c == '}') break;
        if ((unsigned char)c < 0x20 || c == ':' || c == '"' || c == '[' || c == '{') {
            set_error(error, error_size, "invalid JSON primitive"); return false;
        }
        (*position)++;
    }
    if (*position == start) { set_error(error, error_size, "empty JSON primitive"); return false; }
    int token = add_token(doc, JSON_PRIMITIVE, start, parent);
    if (token < 0) { set_error(error, error_size, "out of memory"); return false; }
    doc->tokens[token].end = *position;
    return true;
}

bool json_parse_owned(char *text, size_t length, JsonDoc *doc, char *error, size_t error_size) {
    *doc = (JsonDoc){.text = text, .length = length};
    int parent = -1;
    size_t position = 0;
    while (position < length) {
        char c = text[position];
        if (is_ws(c) || c == ',' || c == ':') { position++; continue; }
        if (c == '{' || c == '[') {
            JsonType type = c == '{' ? JSON_OBJECT : JSON_ARRAY;
            int token = add_token(doc, type, position++, parent);
            if (token < 0) { set_error(error, error_size, "out of memory"); json_free(doc); return false; }
            parent = token;
            continue;
        }
        if (c == '}' || c == ']') {
            JsonType expected = c == '}' ? JSON_OBJECT : JSON_ARRAY;
            if (parent < 0 || doc->tokens[parent].type != expected) {
                set_error(error, error_size, "mismatched JSON delimiter"); json_free(doc); return false;
            }
            doc->tokens[parent].end = ++position;
            parent = doc->tokens[parent].parent;
            continue;
        }
        if (c == '"') {
            if (!parse_string(doc, &position, parent, error, error_size)) { json_free(doc); return false; }
            continue;
        }
        if (!parse_primitive(doc, &position, parent, error, error_size)) { json_free(doc); return false; }
    }
    if (parent != -1 || doc->count == 0) {
        set_error(error, error_size, parent == -1 ? "empty JSON document" : "unterminated JSON container");
        json_free(doc);
        return false;
    }
    return true;
}

bool json_load_file(const char *path, JsonDoc *doc, char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) { snprintf(error, error_size, "%s: %s", path, strerror(errno)); return false; }
    if (fseek(file, 0, SEEK_END) != 0) { set_error(error, error_size, "cannot seek JSON file"); fclose(file); return false; }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) { set_error(error, error_size, "cannot measure JSON file"); fclose(file); return false; }
    char *text = malloc((size_t)length + 1);
    if (text == NULL) { set_error(error, error_size, "out of memory"); fclose(file); return false; }
    size_t got = fread(text, 1, (size_t)length, file);
    if (got != (size_t)length || ferror(file)) { set_error(error, error_size, "cannot read JSON file"); free(text); fclose(file); return false; }
    fclose(file);
    text[got] = '\0';
    return json_parse_owned(text, got, doc, error, error_size);
}

void json_free(JsonDoc *doc) {
    free(doc->tokens);
    free(doc->text);
    *doc = (JsonDoc){0};
}

bool json_token_equals(const JsonDoc *doc, int token, const char *text) {
    if (token < 0 || (size_t)token >= doc->count) return false;
    size_t length = doc->tokens[token].end - doc->tokens[token].start;
    return strlen(text) == length && memcmp(doc->text + doc->tokens[token].start, text, length) == 0;
}

int json_object_get(const JsonDoc *doc, int object, const char *key) {
    if (object < 0 || (size_t)object >= doc->count || doc->tokens[object].type != JSON_OBJECT) return -1;
    for (size_t i = (size_t)object + 1; i + 1 < doc->count && doc->tokens[i].start < doc->tokens[object].end; i++) {
        if (doc->tokens[i].parent == object && doc->tokens[i].type == JSON_STRING && json_token_equals(doc, (int)i, key)) {
            if (doc->tokens[i + 1].parent == object) return (int)i + 1;
            return -1;
        }
    }
    return -1;
}

size_t json_array_size(const JsonDoc *doc, int array) {
    if (array < 0 || (size_t)array >= doc->count || doc->tokens[array].type != JSON_ARRAY) return 0;
    size_t count = 0;
    for (size_t i = (size_t)array + 1; i < doc->count && doc->tokens[i].start < doc->tokens[array].end; i++)
        if (doc->tokens[i].parent == array) count++;
    return count;
}

int json_array_at(const JsonDoc *doc, int array, size_t index) {
    if (array < 0 || (size_t)array >= doc->count || doc->tokens[array].type != JSON_ARRAY) return -1;
    size_t count = 0;
    for (size_t i = (size_t)array + 1; i < doc->count && doc->tokens[i].start < doc->tokens[array].end; i++) {
        if (doc->tokens[i].parent == array && count++ == index) return (int)i;
    }
    return -1;
}

bool json_token_bool(const JsonDoc *doc, int token, bool fallback) {
    if (token < 0 || (size_t)token >= doc->count || doc->tokens[token].type != JSON_PRIMITIVE) return fallback;
    if (json_token_equals(doc, token, "true")) return true;
    if (json_token_equals(doc, token, "false")) return false;
    return fallback;
}

static unsigned hex_value(char c) {
    if (c >= '0' && c <= '9') return (unsigned)(c - '0');
    if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a' + 10);
    return (unsigned)(c - 'A' + 10);
}

static size_t utf8_write(char *out, uint32_t cp) {
    if (cp <= 0x7f) { out[0] = (char)cp; return 1; }
    if (cp <= 0x7ff) { out[0] = (char)(0xc0 | cp >> 6); out[1] = (char)(0x80 | (cp & 0x3f)); return 2; }
    if (cp <= 0xffff) { out[0] = (char)(0xe0 | cp >> 12); out[1] = (char)(0x80 | ((cp >> 6) & 0x3f)); out[2] = (char)(0x80 | (cp & 0x3f)); return 3; }
    out[0] = (char)(0xf0 | cp >> 18); out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f)); out[3] = (char)(0x80 | (cp & 0x3f)); return 4;
}

char *json_token_strdup(const JsonDoc *doc, int token) {
    if (token < 0 || (size_t)token >= doc->count) return NULL;
    const JsonToken *t = &doc->tokens[token];
    size_t length = t->end - t->start;
    char *result = malloc(length + 1);
    if (result == NULL) return NULL;
    size_t output = 0;
    for (size_t i = t->start; i < t->end; i++) {
        char c = doc->text[i];
        if (t->type == JSON_STRING && c == '\\' && i + 1 < t->end) {
            c = doc->text[++i];
            if (c == 'n') result[output++] = '\n';
            else if (c == 'r') result[output++] = '\r';
            else if (c == 't') result[output++] = '\t';
            else if (c == 'b') result[output++] = '\b';
            else if (c == 'f') result[output++] = '\f';
            else if (c == 'u' && i + 4 < t->end) {
                uint32_t cp = 0;
                for (int n = 0; n < 4; n++) cp = cp * 16 + hex_value(doc->text[++i]);
                output += utf8_write(result + output, cp);
            } else result[output++] = c;
        } else result[output++] = c;
    }
    for (size_t i = 0; i < output; i++)
        if ((unsigned char)result[i] < 0x20 && result[i] != '\n' && result[i] != '\t') result[i] = ' ';
    result[output] = '\0';
    return result;
}
