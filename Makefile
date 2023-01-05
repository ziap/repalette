WASM_FLAGS      ?= --target=wasm32 -O3 -flto -fno-builtin -nostdlib -fvisibility=hidden -msimd128
WASM_LINK_FLAGS ?= --no-entry --strip-all --lto-O3 --allow-undefined --export-dynamic
WASM_OPT_FLAGS  ?= -O3 --enable-simd

.PHONY: all
all: repalette repalette.wasm

repalette: main.c repalette.c stb_image.h stb_image_write.h
	$(CC) -o $@ main.c repalette.c -lm $(CFLAGS)

repalette.wasm: wasm_main.c repalette.c
	$(CC) wasm_main.c -o wasm_main.o $(WASM_FLAGS) $(FLAGS) -c
	$(CC) repalette.c -o repalette.o $(WASM_FLAGS) $(FLAGS) -c
	wasm-ld -o $@ $(WASM_LINK_FLAGS) repalette.o wasm_main.o
	wasm-opt repalette.wasm -o repalette.wasm $(WASM_OPT_FLAGS)
	rm repalette.o wasm_main.o
