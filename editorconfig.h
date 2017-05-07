#include <termios.h>
#include <time.h>

#include "erow.h"

struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty;
    char *filename;
    struct termios origTermios;
    char statusmsg[80];
    time_t statusmsg_time;
	int mode; // 0 for N, 1 for I
};
