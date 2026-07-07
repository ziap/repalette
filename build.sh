#!/bin/sh -xe

CC="${CC:-clang}"
CC_WASM="${CC_WASM:-$CC}"

CFLAGS="-O3 -std=c99 -Wall -Wextra -pedantic -s"

WASM_CFLAGS="--target=wasm32 -flto -nostdlib -fvisibility=hidden"
WASM_CFLAGS+=" -mbulk-memory -msimd128"
WASM_LDFLAGS="--no-entry --strip-all --lto-O3 --allow-undefined --export-dynamic"

WASM_FLAGS="$WASM_CFLAGS"
for FLAG in $WASM_LDFLAGS
do
  WASM_FLAGS+=" -Wl,$FLAG"
done

$CC_WASM $CFLAGS $WASM_FLAGS -o web/repalette.wasm \
  crates/core/src/c/wasm.c crates/core/src/c/repalette.c
