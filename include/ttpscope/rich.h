#ifndef TTPSCOPE_RICH_H
#define TTPSCOPE_RICH_H

#include <stddef.h>

/* Attribute classes the detail renderer assigns per character. */
#define RICH_ATTR_PLAIN 0
#define RICH_ATTR_LINK 1
#define RICH_ATTR_DIM 2

/* Rewrite inline ATT&CK description markup into plain text plus a
 * per-character attribute map for the TUI detail pane:
 *
 *   [label](url)       -> label      (RICH_ATTR_LINK; a bare URL shortens
 *                                    by dropping its scheme/host)
 *   (Citation: source) -> [source]   (RICH_ATTR_DIM; consecutive citations
 *                                    get a separating space)
 *
 * `out` and `attrs` must hold at least `cap` bytes; output is always
 * NUL-terminated and truncated safely at the cap. Returns the written
 * length (excluding the terminator). */
size_t rich_prepare(const char *src, char *out, unsigned char *attrs, size_t cap);

#endif