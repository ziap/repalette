CC=clang
CFLAGS=-O3 -Wall -Wextra -pedantic -std=c99
LDLIBS=-lm

NATIVE_FLAGS=-s -march=native -mtune=native

.PHONY: all
all: repalette

repalette: main.c repalette.c stb_image.h stb_image_write.h
	$(CC) -o $@ main.c repalette.c $(CFLAGS) $(LDLIBS) $(NATIVE_FLAGS)


stb_image.h:
	curl -o stb_image.h "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"

stb_image_write.h:
	curl -o stb_image_write.h "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"
