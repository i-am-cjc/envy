asdenvy: envy.c
	$(CC) terminal.c row.c buffer.c envy.c -s -Os -o envy -Wall -Wextra -pedantic -std=c99
debug:
	$(CC) terminal.c row.c buffer.c envy.c -Os -o envy -Wall -Wextra -pedantic -std=c99 -g
clean:
	rm envy
.PHONY: install
install: envy
	cp envy /usr/local/bin/envy
