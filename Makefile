envy: envy.c
	$(CC) buffer.c envy.c -s -Os -o envy -Wall -Wextra -pedantic -std=c99
clean:
	rm envy
.PHONY: install
install: envy
	cp envy /usr/local/bin/envy
