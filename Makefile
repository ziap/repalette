CC=clang
CFLAGS=-O3 -Wall -Wextra -pedantic -std=c99
LDLIBS=-lm

NATIVE_FLAGS=-s -march=native -mtune=native

WASM_CFLAGS=--target=wasm32 -flto -nostdlib -fvisibility=hidden -mbulk-memory -msimd128
WASM_LDFLAGS=--no-entry --strip-all --lto-O3 --allow-undefined --export-dynamic
WASM_FLAGS=$(WASM_CFLAGS) $(foreach flag,$(WASM_LDFLAGS),-Wl,$(flag))

.PHONY: all
all: repalette repalette.wasm

repalette: main.c repalette.c stb_image.h stb_image_write.h
	$(CC) -o $@ main.c repalette.c $(CFLAGS) $(LDLIBS) $(NATIVE_FLAGS)

repalette.wasm: wasm_main.c repalette.c
	$(CC) $(WASM_FLAGS) $(CFLAGS) -o $@ $^


stb_image.h:
	curl -o stb_image.h "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"

stb_image_write.h:
	curl -o stb_image_write.h "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"
