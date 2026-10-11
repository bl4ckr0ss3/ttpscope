#include "ttpscope/json.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *source = "{\"name\":\"PowerShell\",\"active\":true,\"items\":[\"one\",\"two\"],\"unicode\":\"A\\u0026B\"}";
    char *owned = malloc(strlen(source) + 1);
    assert(owned != NULL);
    strcpy(owned, source);
    JsonDoc doc;
    char error[128];
    assert(json_parse_owned(owned, strlen(owned), &doc, error, sizeof(error)));
    int name = json_object_get(&doc, 0, "name");
    assert(json_token_equals(&doc, name, "PowerShell"));
    assert(json_token_bool(&doc, json_object_get(&doc, 0, "active"), false));
    int items = json_object_get(&doc, 0, "items");
    assert(json_array_size(&doc, items) == 2);
    assert(json_token_equals(&doc, json_array_at(&doc, items, 1), "two"));
    char *unicode = json_token_strdup(&doc, json_object_get(&doc, 0, "unicode"));
    assert(unicode != NULL && strcmp(unicode, "A&B") == 0);
    free(unicode);
    json_free(&doc);

    char *bad = malloc(8);
    assert(bad != NULL);
    strcpy(bad, "{\"x\":]");
    assert(!json_parse_owned(bad, strlen(bad), &doc, error, sizeof(error)));
    return 0;
}
