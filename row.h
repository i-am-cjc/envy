//#include "editorconfig.h"

int erowRxToCx(erow *row, int rx);
void eUpdateRow(erow *row);
void eInsertRow(int at, char *s, size_t len, struct editorConfig *E);
void eFreeRow(erow *row);
void eDelRow(int at);
void eRowInsertChar(erow *row, int at, int c, struct editorConfig *E);
void eRowAppendString(erow *row, char *s, size_t len);
void eRowDelChar(erow *row, int at, struct editorConfig *E);
