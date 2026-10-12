#include "ttpscope/rich.h"

#include <string.h>

size_t rich_prepare(const char *src, char *out, unsigned char *attrs, size_t cap) {
    size_t o = 0;
    const char *p = src;
    while (*p != '\0' && o + 1 < cap) {
        if (p[0] == '[') {
            const char *close = strchr(p, ']');
            if (close != NULL && close[1] == '(') {
                const char *urlend = strchr(close + 2, ')');
                if (urlend != NULL) {
                    const char *label = p + 1;
                    size_t llen = (size_t)(close - p - 1);
                    const char *url = close + 2;
                    size_t ulen = (size_t)(urlend - url);
                    if (llen == 0 || (llen == ulen && strncmp(label, url, ulen) == 0)) {
                        /* bare URL: show it without the scheme/host */
                        label = url;
                        llen = ulen;
                        if (ulen > 26 && strncmp(url, "https://attack.mitre.org/", 26) == 0) {
                            label = url + 26;
                            llen = ulen - 26;
                        } else if (ulen > 8 && strncmp(url, "https://", 8) == 0) {
                            label = url + 8;
                            llen = ulen - 8;
                        }
                    }
                    for (size_t i = 0; i < llen && o + 1 < cap; i++) {
                        out[o] = label[i];
                        attrs[o] = RICH_ATTR_LINK;
                        o++;
                    }
                    p = urlend + 1;
                    continue;
                }
            }
        }
        if (strncmp(p, "(Citation: ", 11) == 0) {
            const char *end = strchr(p, ')');
            if (end != NULL) {
                const char *source = p + 11;
                size_t slen = (size_t)(end - source);
                if (o + 1 < cap) {
                    out[o] = '[';
                    attrs[o] = RICH_ATTR_DIM;
                    o++;
                }
                for (size_t i = 0; i < slen && o + 1 < cap; i++) {
                    out[o] = source[i];
                    attrs[o] = RICH_ATTR_DIM;
                    o++;
                }
                if (o + 1 < cap) {
                    out[o] = ']';
                    attrs[o] = RICH_ATTR_DIM;
                    o++;
                }
                p = end + 1;
                if (p[0] == '(' && o + 1 < cap) {
                    out[o] = ' ';
                    attrs[o] = RICH_ATTR_DIM;
                    o++;
                }
                continue;
            }
        }
        out[o] = *p;
        attrs[o] = RICH_ATTR_PLAIN;
        o++;
        p++;
    }
    out[o] = '\0';
    return o;
}