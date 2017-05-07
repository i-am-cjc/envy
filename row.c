#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "editorconfig.h"

/*** Row Ops ***/
int eRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int i;
    for (i = 0; i < cx; i++) {
        if (row->chars[i] == '\t')
            rx += (ENVY_TAB_STOP - 1) - (rx % ENVY_TAB_STOP);
        rx++;
    }
    return rx;
}

int erowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->chars[cx] == '\t')
            cur_rx += (ENVY_TAB_STOP - 1) - (cur_rx % ENVY_TAB_STOP);
        cur_rx++;

        if (cur_rx > rx) return cx;
    }

    return cx;
}

void eUpdateRow(erow *row) {
    int tabs = 0;
    int i;
    for (i = 0; i < row->size; i++)
        if (row->chars[i] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(ENVY_TAB_STOP - 1) + 1);

    int idx = 0;
    for (i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            row->render[idx++] = ' ';
            while (idx % ENVY_TAB_STOP  != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void eInsertRow(int at, char *s, size_t len, struct editorConfig *E) {
    if (at < 0 || at > E->numrows) return;

    E->row = realloc(E->row, sizeof(erow) * (E->numrows + 1));
    memmove(&E->row[at + 1], &E->row[at], sizeof(erow) * (E->numrows - at));

    E->row[at].size = len;
    E->row[at].chars = malloc(len + 1);
    memcpy(E->row[at].chars, s, len);
    E->row[at].chars[len] = '\0';

    E->row[at].rsize = 0;
    E->row[at].render = NULL;
    eUpdateRow(&E->row[at]);

    E->numrows++;
    E->dirty++;
}

void eFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

void eDelRow(int at, struct editorConfig *E) {
    if (at < 0 || at >= E->numrows) return;
    eFreeRow(&E->row[at]);
    memmove(&E->row[at], &E->row[at + 1], sizeof(erow) * (E->numrows - at - 1));
    E->numrows--;
    E->dirty++;
}

void eRowInsertChar(erow *row, int at, int c, struct editorConfig *E) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    eUpdateRow(row);
    E->dirty++;
}

void eRowAppendString(erow *row, char *s, size_t len, struct editorConfig *E) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    eUpdateRow(row);
    E->dirty++;
}

void eRowDelChar(erow *row, int at, struct editorConfig *E) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    eUpdateRow(row);
    E->dirty++;
}

