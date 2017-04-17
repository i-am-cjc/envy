envy: envy.c
	$(CC) envy.c -s -Os -o envy -Wall -Wextra -pedantic -std=c99
clean:
	rm envy
