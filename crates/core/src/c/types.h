#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint32_t Hex;

// GCC/Clang vector extensions used by the recolor/dither SIMD in repalette.c.
typedef int32_t i32x4 __attribute__((vector_size(16)));
typedef uint8_t u8x4 __attribute__((vector_size(4)));

// Images are fixed to 4 channels (RGBA) so each pixel is one 32-bit unit.
#define CHANNELS 4

typedef struct {
	i32 r;
	i32 g;
	i32 b;
} Color;

typedef struct {
	size_t size;

	const i32 *rs;
	const i32 *gs;
	const i32 *bs;
} Palette;

typedef struct {
	u8 *pixels;
	u32 width;
	u32 height;
} Image;

#endif
