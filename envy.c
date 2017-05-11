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
#include <fcntl.h>

#include "terminal.h"
#include "config.h"
#include "editorconfig.h"
#include "buffer.h"
#include "row.h"
#include "stack.h"

// acts as a constructor for an empty buffer
#define ABUF_INIT {NULL, 0}

// CTRL Key basically strips the 6 and 7th bit from the key that has been
// pressed with ctrl, nice!
#define CTRL_KEY(k) ((k) & 0x1F)

struct editorConfig E;

/*** prototypes ***/
char *ePrompt(char *prompt, void (*callback)(char *, int));
int eRowCxToRx(erow *row, int cx);

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
    
	int len = snprintf(status, sizeof(status), "%.20s %s",
            E.filename ? E.filename : "[No Name]",
            E.dirty ? "[modified]" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d %s",
            E.cy + 1, E.numrows,
            E.mode ? "I" : "N");

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

/*** Editor Ops ***/
void eInsertChar(int c) {
    if (E.cy == E.numrows)
        eInsertRow(E.numrows, "", 0, &E);

    eRowInsertChar(&E.row[E.cy], E.cx, c, &E);
    E.cx++;
}

void eInsertNewLine() {
    if (E.cx == 0) {
        eInsertRow(E.cy, "", 0, &E);
    } else {
        erow *row = &E.row[E.cy];
        eInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx, &E);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        eUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

void eDelChar() {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
        eRowDelChar(row, E.cx - 1, &E);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        eRowAppendString(&E.row[E.cy - 1], row->chars, row->size, &E);
        eDelRow(E.cy, &E);
        E.cy--;
    }
}

/*** file i/o ***/
char *eRowsToString(int *buflen) {
    int totlen = 0;
    int i;
    for (i = 0; i < E.numrows; i++) 
        totlen += E.row[i].size + 1;
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;

    for (i = 0; i < E.numrows; i++) {
        memcpy(p, E.row[i].chars, E.row[i].size);
        p += E.row[i].size;
        *p = '\n';
        p++;
    }

    return buf;
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
        eInsertRow(E.numrows, line, linelen, &E);
    }
    free(line);
    fclose(fp);

    // reset the "dirtiness" of the file
    E.dirty = 0;
}

void eSave() {
    if (E.filename == NULL) 
        E.filename = ePrompt("Filename: %s", NULL);
    // If the prompt was aborted we are NULL again
    if (E.filename == NULL) {
        eSetStatusMessage("Aborted");
        return;
    }

    int len;
    char *buf = eRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                eSetStatusMessage("%d bytes written to disk", len);
                // reset the "dirtiness" of the file
                E.dirty = 0;
                return;
            }
        }
        close(fd);
    }

    free(buf);
    eSetStatusMessage("Error writing to disk: %s", strerror(errno));
}

/*** search and find ***/
void eFindCallback(char *query, int key) {
	static int last_match = -1;
	static int direction = 1;

    if (key == '\r' || key == '\x1b') {
		last_match = -1;
		direction = 1;
        return;
	} else if (key == DOWN) {
		direction = 1;
	} else if (key == UP) {
		direction = -1;
	} else {
		last_match = -1;
		direction = 1;
	}

	if (last_match == -1) direction = 1;
	int current = last_match;

    int i;
    for (i = 0; i < E.numrows; i++) {
		current += direction;
		if (current == -1) current = E.numrows - 1;
		else if (current == E.numrows) current = 0;

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if (match) {
			last_match = current;
            E.cy = current;
            E.cx = erowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;
            break;
        }
    }
}

void eFind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
	int saved_rowoff = E.rowoff;

    char *query = ePrompt("Find: %s", eFindCallback);

    if (query) {
		free(query);
	} else {
		E.cx = saved_cx;
		E.cy = saved_cy;
		E.coloff = saved_coloff;
		E.rowoff = saved_rowoff;
	}
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

/** input **/
char *ePrompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        eSetStatusMessage(prompt, buf);
        eRefreshScreen();

        int c = eReadKey();
        if (c == BACKSPACE || c == CTRL_KEY('h')) {
            if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
            eSetStatusMessage("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                eSetStatusMessage("");
                if (callback) callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if (callback) callback(buf, c);
    }
}

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

    if (E.mode) { // insert mode
        switch(c) {
            case '\r':
                eInsertNewLine();
                break;

            case BACKSPACE:
            case CTRL_KEY('h'):
                eDelChar();
                break;

            case RIGHT:
            case LEFT:
            case DOWN:
            case UP:
                eMoveCursor(c);
                break;

            case '\x1b':
                E.mode = 0;
                break;

            default:
                eInsertChar(c);
                break;
        }
    } else { // normal mode 
        switch(c) {
            case 'd':
				// TODO yank row
                eDelRow(E.cy, &E);
                break;

			case 'y':
				// TODO yank row
				stackPush(&E.row[E.cy]);
				break;

			case 'p':
				// TODO put row
                if (stackEmpty()) break;
                eInsertNewLine();
                E.row[E.cy + 1] = stackPop();
				break;

			case 'O':
				E.cy--;
			case 'o':
				if (E.cy > E.numrows) E.cy--;
				E.cx = E.row[E.cy].size;
				eInsertNewLine();
				E.mode = 1;
				break;
 
            case '/':
                eFind();
                break;

			case 'g':
				E.cy = 0;
				break;
			case 'G':
				E.cy = E.numrows - 1;
				break;

            case 'i':
                E.mode = 1;
                break;

            case RIGHT:
            case LEFT:
            case DOWN:
            case UP:
                eMoveCursor(c);
                break;

            case '\x1b':
                E.mode = 0;
                break;

            case 'w':
                eSave();
                break;

            case 'x':
				// find current row, move right, remove char
                // TODO
				break;

            case 'z':
				eSave();
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
				exit(0);
				break;

            case 'q':
                if (E.dirty) {
                    eSetStatusMessage("File changed. "
                      "Press Q to quit without saving.");
                    return;
                }
            case 'Q':
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
                break;

        }
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
    E.dirty = 0;

    E.filename = NULL;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");

    // status line and commadn line
    E.screenrows -= 2;

    // status message
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    E.mode = 0;
}

int main(int argc, char *argv[]) {
    initEditor();
    enableRawMode(&E);
    if (argc >= 2)
        eOpen(argv[1]);

    while (1) {
        eRefreshScreen();
        eProcessKeypress();
    }

    return 0;
}
