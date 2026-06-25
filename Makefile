CC = gcc

CFLAGS = -g -fsanitize=address -Wall -Iinclude
SRC = $(wildcard src/*.c)

helix: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o helix

clean:
	rm -f helix