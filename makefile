CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -static

.PHONY: all ports clean clean-dir

all: slateport ports

slateport: slateport.c
	$(CC) $(CFLAGS) slateport.c -o slateport

ports: slateport
	./slateport slate.slateport .
	./slateport slate-x11.slateport .

clean:
	rm -f slateport
	find . -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +
clean-dir:
	find . -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +
