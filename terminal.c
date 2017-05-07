#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "editorconfig.h"
#include "config.h"

// TERMINAL
void die(const char *s) {
    //eBlankScreen();
    perror(s);
    exit(1);
}

void disableRawMode(struct editorConfig *E) {
    // set the original flags back to the terminal
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E->origTermios) == -1)
        die("tcsetattr");
}

void enableRawMode(struct editorConfig *E) {

    // take copies of the current terminal setup
    if (tcgetattr(STDIN_FILENO, &E->origTermios) == -1)
        die("tcsetattr");

    struct termios raw = E->origTermios;

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

