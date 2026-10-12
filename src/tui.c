#include "ttpscope/tui.h"
#include "ttpscope/rich.h"

#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RICH_CAP 16384

static void line_text(int row, int column, int width, const char *text, int attributes) {
    if (text == NULL || width <= 0) return;
    attron(attributes);
    mvaddnstr(row, column, text, width);
    attroff(attributes);
}

static int attr_for(unsigned char a) {
    if (a == RICH_ATTR_LINK) return COLOR_PAIR(2) | A_UNDERLINE;
    if (a == RICH_ATTR_DIM) return A_DIM;
    return A_NORMAL;
}

/* Word-wrap rich text: blank line on paragraph breaks, per-character
 * attributes preserved across wrapping (links keep their style mid-line). */
static void wrap_rich(int row, int column, int height, int width,
                      const char *text, const unsigned char *attrs) {
    if (text == NULL || width <= 0) return;
    size_t n = strlen(text);
    size_t i = 0;
    int y = row, x = 0;
    while (i < n && y < row + height) {
        if (text[i] == '\n') {
            y++; x = 0; i++;
            if (i < n && text[i] == '\n') { y++; i++; } /* paragraph break */
            continue;
        }
        while (i < n && text[i] == ' ') i++;
        if (i >= n || text[i] == '\n') continue;
        size_t start = i;
        while (i < n && text[i] != ' ' && text[i] != '\n') i++;
        size_t wlen = i - start;
        if (x > 0 && x + 1 + (int)wlen > width) { y++; x = 0; }
        if (y >= row + height) break;
        if (x > 0) { mvaddch(y, column + x, ' '); x++; }
        size_t j = start;
        while (j < i && y < row + height) {
            unsigned char a = attrs[j];
            size_t k = j;
            while (k < i && attrs[k] == a) k++;
            int avail = width - x;
            if (avail <= 0) { y++; x = 0; continue; }
            size_t len = k - j;
            if (len > (size_t)avail) len = (size_t)avail;
            attron(attr_for(a));
            mvaddnstr(y, column + x, text + j, (int)len);
            attroff(attr_for(a));
            x += (int)len;
            j += len;
            if (j < i) { y++; x = 0; } /* run truncated by the edge */
        }
    }
}

static void draw(const AttackDb *db, const AttackObject **results, size_t count, size_t selected, const char *query, bool editing) {
    static char rich[RICH_CAP];
    static unsigned char rich_attrs[RICH_CAP];
    (void)db;
    erase();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int left = cols < 90 ? cols : cols / 3;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvhline(0, 0, ' ', cols);
    mvaddnstr(0, 1, "TTPscope · MITRE ATT&CK research", cols - 2);
    attroff(COLOR_PAIR(1) | A_BOLD);
    char objects[32];
    int nobjects = snprintf(objects, sizeof(objects), "%zu objects", count);
    mvaddnstr(1, 1, objects, nobjects);
    attron(A_DIM);
    mvaddstr(1, 1 + nobjects, "   / search   j/k move   PgUp/PgDn jump   g/G ends   q quit");
    attroff(A_DIM);
    mvhline(2, 0, ACS_HLINE, cols);
    size_t visible = rows > 5 ? (size_t)(rows - 5) : 0;
    size_t top = selected >= visible && visible > 0 ? selected - visible + 1 : 0;
    for (int y = 3; y < rows - 2 && top + (size_t)(y - 3) < count; y++) {
        size_t index = top + (size_t)(y - 3);
        const AttackObject *o = results[index];
        char idbuf[16];
        snprintf(idbuf, sizeof(idbuf), "%-12.12s", o->attack_id ? o->attack_id : attack_kind_name(o->kind));
        int id_attr = index == selected ? A_REVERSE : (COLOR_PAIR(1) | A_DIM);
        int name_attr = index == selected ? A_REVERSE : A_NORMAL;
        line_text(y, 1, 12, idbuf, id_attr);
        line_text(y, 13, left - 14, o->name ? o->name : "", name_attr);
    }
    if (cols >= 90) {
        mvvline(3, left, ACS_VLINE, rows - 5);
        if (selected < count) {
            const AttackObject *o = results[selected];
            char title[1024];
            snprintf(title, sizeof(title), "%s%s%s", o->attack_id ? o->attack_id : "", o->attack_id ? " — " : "", o->name ? o->name : "");
            line_text(3, left + 2, cols - left - 3, title, A_BOLD);
            char label[16];
            attron(A_DIM);
            snprintf(label, sizeof(label), "%-11.11s", "Type:");
            mvaddstr(5, left + 2, label);
            snprintf(label, sizeof(label), "%-11.11s", "Platforms:");
            mvaddstr(6, left + 2, label);
            snprintf(label, sizeof(label), "%-11.11s", "Tactics:");
            mvaddstr(7, left + 2, label);
            attroff(A_DIM);
            mvaddstr(5, left + 13, attack_kind_name(o->kind));
            if (o->platforms) mvaddnstr(6, left + 13, o->platforms, cols - left - 14);
            if (o->tactics) mvaddnstr(7, left + 13, o->tactics, cols - left - 14);
            rich_prepare(o->description ? o->description : "", rich, rich_attrs, RICH_CAP);
            wrap_rich(9, left + 2, rows - 12, cols - left - 3, rich, rich_attrs);
        }
    }
    mvhline(rows - 2, 0, ACS_HLINE, cols);
    attron(A_DIM);
    mvaddstr(rows - 1, 1, editing ? "Search: " : "Filter: ");
    attroff(A_DIM);
    mvaddstr(rows - 1, 9, query);
    if (editing) { curs_set(1); move(rows - 1, (int)strlen(query) + 9); } else curs_set(0);
    refresh();
}

int tui_run(const AttackDb *db) {
    setlocale(LC_ALL, "");
    if (initscr() == NULL) return 1;
    cbreak(); noecho(); keypad(stdscr, TRUE);
    if (has_colors()) {
        start_color(); use_default_colors();
        init_pair(1, COLOR_CYAN, -1);
        init_pair(2, COLOR_BLUE, -1);
        init_pair(3, COLOR_YELLOW, -1);
    }
    char query[256] = "";
    const char *initial = getenv("TTPSCOPE_TUI_SELECT");
    if (initial != NULL && initial[0] != '\0') {
        snprintf(query, sizeof(query), "%.*s", (int)(sizeof(query) - 1), initial);
    }
    /* TTPSCOPE_TUI_QUIT_AFTER=N exits N seconds after startup: lets scripts
     * capture screenshots or demos of the browser. */
    const char *quit_after = getenv("TTPSCOPE_TUI_QUIT_AFTER");
    time_t deadline = 0;
    if (quit_after != NULL && quit_after[0] != '\0') {
        deadline = time(NULL) + (time_t)atoi(quit_after);
        halfdelay(2);
    }
    const AttackObject **results = NULL;
    size_t count = attack_search(db, query, &results), selected = 0;
    bool editing = false, running = true;
    while (running) {
        if (deadline != 0 && time(NULL) >= deadline) running = false;
        if (!running) break;
        draw(db, results, count, selected, query, editing);
        int key = getch();
        if (editing) {
            size_t length = strlen(query);
            if (key == 27 || key == '\n' || key == KEY_ENTER) editing = false;
            else if ((key == KEY_BACKSPACE || key == 127 || key == 8) && length > 0) query[length - 1] = '\0';
            else if (key >= 32 && key <= 126 && length + 1 < sizeof(query)) { query[length] = (char)key; query[length + 1] = '\0'; }
            free((void *)results); results = NULL; count = attack_search(db, query, &results); selected = 0;
        } else if (key == 'q') running = false;
        else if (key == '/' || key == 's') editing = true;
        else if ((key == 'j' || key == KEY_DOWN) && selected + 1 < count) selected++;
        else if ((key == 'k' || key == KEY_UP) && selected > 0) selected--;
        else if (key == KEY_NPAGE && selected + 10 < count) selected += 10;
        else if (key == KEY_PPAGE) selected = selected > 10 ? selected - 10 : 0;
        else if (key == 'g') selected = 0;
        else if (key == 'G' && count > 0) selected = count - 1;
    }
    free((void *)results);
    endwin();
    return 0;
}