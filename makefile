CC=clang
FLAGS=-march=native -mtune=native -Wall -Wextra -s
NATIVE_FLAGS=-O3 -lm
WASM_FLAGS=--target=wasm32 -O3 -flto -fno-builtin -nostdlib \
	-fvisibility=hidden -Wl,--no-entry, -Wl,--strip-all, -Wl,-lto-O3 \
	-Wl,--allow-undefined -Wl,--export-dynamic

all: repalette repalette.wasm

repalette: main.c repalette.c stb_image.h stb_image_write.h
	$(CC) -o $@ main.c repalette.c $(FLAGS) $(NATIVE_FLAGS)

repalette.wasm: wasm_main.c repalette.c
	$(CC) -o $@ $^ $(WASM_FLAGS) $(FLAGS)

stb_image.h:
	wget "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"

stb_image_write.h:
	wget "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"
