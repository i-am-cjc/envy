#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>

#define ENVY_VERSION "0.0.1"
#define ENVY_TAB_STOP 4

// CTRL Key basically strips the 6 and 7th bit from the key that has been
// pressed with ctrl, nice!
#define CTRL_KEY(k) ((k) & 0x1F)
enum eKey {
    UP = 1000,
    DOWN,
    LEFT,
    RIGHT
};

typedef struct erow {
    int size;
    int rsize;
    char *chars;
    char *render;
} erow;

struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    char *filename;
    struct termios origTermios;
    char statusmsg[80];
    time_t statusmsg_time;
};

struct editorConfig E;

// APPEND BUFFER
struct abuf {
    char *b;
    int len;
};

// acts as a constructor for an empty buffer
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

// OUTPUT
void eScroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = eRowCxToRx(&E.row[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.rowoff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void eDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                        "Envy Editor -- version %s", ENVY_VERSION);
                if(welcomelen > E.screencols) welcomelen = E.screencols;

                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                } 

                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void eDrawStatusBar(struct abuf *ab) {
    // render the status bar
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines",
            E.filename ? E.filename : "[No Name]", E.numrows);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d ",
            E.cy + 1);
    if(len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void eDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void eRefreshScreen() {
    eScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    eDrawRows(&ab);
    eDrawStatusBar(&ab);
    eDrawMessageBar(&ab);

    // position the cursor 
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, 
                                              (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    //abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void eSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}
// TERMINAL

void die(const char *s) {
    //eBlankScreen();
    perror(s);
    exit(1);
}

void disableRawMode() {
    // set the original flags back to the terminal
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.origTermios) == -1)
        die("tcsetattr");
}

void enableRawMode() {

    // take copies of the current terminal setup
    if (tcgetattr(STDIN_FILENO, &E.origTermios) == -1)
        die("tcsetattr");

    struct termios raw = E.origTermios;

    // reset when we exit
    atexit(disableRawMode);

    // bit wise ANDing lflag against A NOTted ECHO flag set 
    // this allows us to unset the ECHO flag set whilst retaining the rest
    // of the lflags (local flags from termios)
    // ICANON flag allows us to turn off canonical mode, ie we will be reading
    // in input byte by byte rather than a line at a time
    // ISIG stops C-z and C-c
    // IEXTEN stops C-v and C-o
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // turn off the IXON flag, which stops C-s and C-q from doing anything
    // ICRNL stops terminal translating things for us like C-m 
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);

    // Turn off all output processing
    raw.c_oflag &= ~(OPOST);

    // Set the character size to 8 just incase
    raw.c_cflag |= (CS8);

    // Set some control characters to allow read to timeout and not block
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int eReadKey() {
    int nread;
    char c;
    char esc = '\x1b';

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    if (c == '\x1b') {
        // escape sequence?
        char seq[3];

        // check if it is, otherwise it might just be escape...
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return esc;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return esc;

        if (seq[0] == '[') {
            // parse escape sequence
            switch (seq[1]) {
                case 'A': return UP;
                case 'B': return DOWN;
                case 'C': return RIGHT;
                case 'D': return LEFT;
            }
        }

        return esc;
    } else {
        return c;
    }
}

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

void eAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    eUpdateRow(&E.row[at]);

    E.numrows++;
}

void eOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        if (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        eAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return -1;
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

// INPUT
void eMoveCursor(int key) {
    // get the current row
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key) {
        case LEFT:
            if (E.cx != 0)
                E.cx--;
            break;
        case DOWN:
            if (E.cy < E.numrows)
                E.cy++;
            break;
        case UP:
            if (E.cy != 0)
                E.cy--;
            break;
        case RIGHT:
            if (row && E.cx < row->size)
                E.cx++;
            break;
    }

    // move the cursor if we are beyond the line we end up on
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
        E.cx = rowlen;
}

void eProcessKeypress() {
    int c = eReadKey();
    switch(c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
        case RIGHT:
        case LEFT:
        case DOWN:
        case UP:
            eMoveCursor(c);
            break;
    }
}

// init

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;

    E.numrows = 0;
    E.row = NULL;
    E.filename = NULL;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");

    // status line and commadn line
    E.screenrows -= 2;

    // status message
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2)
        eOpen(argv[1]);

    eSetStatusMessage("C-q to quit");

    while (1) {
        eRefreshScreen();
        eProcessKeypress();
    }

    return 0;
}
