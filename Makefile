envy: envy.c
	$(CC) terminal.c stack.c row.c buffer.c envy.c -Os -o envy -Wall -Wextra -pedantic -std=c99 -s
debug:
	$(CC) terminal.c stack.c row.c buffer.c envy.c -Os -o envy -Wall -Wextra -pedantic -std=c99 -g
clean:
	rm envy
.PHONY: install
install: envy
	cp envy /usr/local/bin/envy
