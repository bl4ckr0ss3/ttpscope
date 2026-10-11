#include "ttpscope/tui.h"

#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

static void line_text(int row, int column, int width, const char *text, int attributes) {
    if (text == NULL || width <= 0) return;
    attron(attributes);
    mvaddnstr(row, column, text, width);
    attroff(attributes);
}

static void wrap_text(int row, int column, int height, int width, const char *text) {
    if (text == NULL) return;
    int y = row, x = 0;
    const char *p = text;
    while (*p && y < row + height) {
        if (*p == '\n') { y++; x = 0; p++; continue; }
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != ' ' && *end != '\n') end++;
        int length = (int)(end - p);
        if (x > 0 && x + 1 + length > width) { y++; x = 0; }
        if (y >= row + height) break;
        if (x > 0) { mvaddch(y, column + x, ' '); x++; }
        int available = width - x;
        if (available > 0) mvaddnstr(y, column + x, p, length < available ? length : available);
        x += length;
        p = end;
    }
}

static void draw(const AttackDb *db, const AttackObject **results, size_t count, size_t selected, const char *query, bool editing) {
    erase();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int left = cols < 90 ? cols : cols / 3;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvhline(0, 0, ' ', cols);
    mvaddnstr(0, 1, "TTPscope · MITRE ATT&CK research", cols - 2);
    attroff(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 1, "%zu objects  / search  j/k move  PgUp/PgDn jump  q quit", count);
    mvhline(2, 0, ACS_HLINE, cols);
    size_t visible = rows > 5 ? (size_t)(rows - 5) : 0;
    size_t top = selected >= visible && visible > 0 ? selected - visible + 1 : 0;
    for (int y = 3; y < rows - 2 && top + (size_t)(y - 3) < count; y++) {
        size_t index = top + (size_t)(y - 3);
        const AttackObject *o = results[index];
        char label[1024];
        snprintf(label, sizeof(label), "%-12s %s", o->attack_id ? o->attack_id : attack_kind_name(o->kind), o->name ? o->name : "");
        line_text(y, 1, left - 2, label, index == selected ? A_REVERSE : A_NORMAL);
    }
    if (cols >= 90) {
        mvvline(3, left, ACS_VLINE, rows - 5);
        if (selected < count) {
            const AttackObject *o = results[selected];
            char title[1024];
            snprintf(title, sizeof(title), "%s%s%s", o->attack_id ? o->attack_id : "", o->attack_id ? " — " : "", o->name ? o->name : "");
            line_text(3, left + 2, cols - left - 3, title, A_BOLD);
            mvprintw(5, left + 2, "Type: %s", attack_kind_name(o->kind));
            if (o->platforms) mvaddnstr(6, left + 2, o->platforms, cols - left - 3);
            if (o->tactics) mvaddnstr(7, left + 2, o->tactics, cols - left - 3);
            wrap_text(9, left + 2, rows - 12, cols - left - 3, o->description);
        }
    }
    mvhline(rows - 2, 0, ACS_HLINE, cols);
    mvprintw(rows - 1, 1, "%s%s", editing ? "Search: " : "Filter: ", query);
    if (editing) { curs_set(1); move(rows - 1, (int)strlen(query) + 9); } else curs_set(0);
    refresh();
    (void)db;
}

int tui_run(const AttackDb *db) {
    setlocale(LC_ALL, "");
    if (initscr() == NULL) return 1;
    cbreak(); noecho(); keypad(stdscr, TRUE);
    if (has_colors()) { start_color(); use_default_colors(); init_pair(1, COLOR_CYAN, -1); }
    char query[256] = "";
    const AttackObject **results = NULL;
    size_t count = attack_search(db, query, &results), selected = 0;
    bool editing = false, running = true;
    while (running) {
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
