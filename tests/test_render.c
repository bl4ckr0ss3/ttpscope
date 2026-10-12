#include "ttpscope/rich.h"

#include <assert.h>
#include <string.h>

static void prepare(const char *src, char *out, unsigned char *attrs, size_t cap) {
    memset(attrs, 0xAA, cap);
    rich_prepare(src, out, attrs, cap);
}

int main(void) {
    char out[256];
    unsigned char attrs[256];

    /* Plain text passes through untouched. */
    prepare("hello world", out, attrs, sizeof(out));
    assert(strcmp(out, "hello world") == 0);
    for (size_t i = 0; i < strlen(out); i++) assert(attrs[i] == RICH_ATTR_PLAIN);

    /* Labelled link renders as its label, all LINK-attributed. */
    prepare("see [RedPenguin](https://attack.mitre.org/campaigns/C0056) now", out, attrs, sizeof(out));
    assert(strcmp(out, "see RedPenguin now") == 0);
    assert(attrs[4] == RICH_ATTR_LINK);
    assert(attrs[13] == RICH_ATTR_LINK);
    assert(attrs[14] == RICH_ATTR_PLAIN);
    assert(attrs[17] == RICH_ATTR_PLAIN);

    /* Bare URL (label equals the URL) shortens by dropping scheme/host. */
    prepare("[https://attack.mitre.org/campaigns/C0056](https://attack.mitre.org/campaigns/C0056)",
            out, attrs, sizeof(out));
    assert(strcmp(out, "campaigns/C0056") == 0);
    assert(attrs[0] == RICH_ATTR_LINK);
    assert(attrs[14] == RICH_ATTR_LINK);

    /* Empty label behaves like a bare URL. */
    prepare("[](https://attack.mitre.org/software/S0357)", out, attrs, sizeof(out));
    assert(strcmp(out, "software/S0357") == 0);

    /* Citations condense to a dim [source] marker. */
    prepare("launched by Juniper.(Citation: Mandiant 2025) later", out, attrs, sizeof(out));
    assert(strcmp(out, "launched by Juniper.[Mandiant 2025] later") == 0);
    size_t bracket = strchr(out, '[') - out;
    assert(attrs[bracket] == RICH_ATTR_DIM);
    assert(attrs[strlen(out) - 1] == RICH_ATTR_PLAIN);

    /* Adjacent citations get one separating space. */
    prepare("(Citation: A)(Citation: B)", out, attrs, sizeof(out));
    assert(strcmp(out, "[A] [B]") == 0);
    assert(attrs[3] == RICH_ATTR_DIM);
    assert(attrs[4] == RICH_ATTR_PLAIN);

    /* Paragraph breaks survive. */
    prepare("one\n\ntwo", out, attrs, sizeof(out));
    assert(strcmp(out, "one\n\ntwo") == 0);

    /* Non-markdown brackets pass through. */
    prepare("array[0] stays", out, attrs, sizeof(out));
    assert(strcmp(out, "array[0] stays") == 0);
    assert(attrs[6] == RICH_ATTR_PLAIN);

    /* Truncation at the cap is safe and NUL-terminated. */
    prepare("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", out, attrs, 8);
    assert(strlen(out) == 7);
    assert(out[7] == '\0');

    return 0;
}