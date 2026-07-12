#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint32_t Hex;

// GCC/Clang vector extensions used by the recolor/dither SIMD in repalette.c.
typedef int32_t i32x4 __attribute__((vector_size(16)));
typedef uint8_t u8x4 __attribute__((vector_size(4)));

#endif
