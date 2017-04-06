#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

// CTRL Key basically strips the 6 and 7th bit from the key that has been
// pressed with ctrl, nice!
#define CTRL_KEY(k) ((k) & 0x1F)
#define ENVY_VERSION "0.0.1"

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios origTermios;
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
void eDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        if (y == E.screenrows / 3) {
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

        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void eRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    eDrawRows(&ab);

    // position the cursor 
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    //abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
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

char eReadKey() {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    return c;
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
void eMoveCursor(char key) {
    switch(key) {
        case 'h':
            E.cx--;
            break;
        case 'j':
            E.cy++;
            break;
        case 'k':
            E.cy--;
            break;
        case 'l':
            E.cx++;
            break;
    }
}

void eProcessKeypress() {
    char c = eReadKey();
    switch(c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            eMoveCursor(c);
            break;
    }
}

// init

void initEditor() {
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        eRefreshScreen();
        eProcessKeypress();
    }

    return 0;
}
