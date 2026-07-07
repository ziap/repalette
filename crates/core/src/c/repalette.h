#ifndef REPALETTE_H
#define REPALETTE_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t Hex;

// Images are fixed to 4 channels (RGBA) so each pixel is one 32-bit unit.
#define CHANNELS 4

typedef struct {
	int r;
	int g;
	int b;
} Color;

typedef struct {
	size_t size;

	const int *rs;
	const int *gs;
	const int *bs;
} Palette;

typedef struct {
	u8 *pixels;
	int width;
	int height;
} Image;

#define DITHERERS(X) \
	X(NONE)            \
	X(FLOYD_STEINBERG) \
	X(ATKINSON)        \
	X(JJN)             \
	X(BURKES)          \
	X(SIERRA32)        \
	X(SIERRA4)

typedef enum {
	DITHER_INVALID = -1,
#define X(e) e,
	DITHERERS(X)
#undef X
		DITHER_COUNT
} Ditherer;

extern void recolor(Image, Palette, Ditherer);
extern void recolor_index(Image, Palette, Ditherer, u8 *out);

extern const char **dither_names;
extern const char **dither_display_names;

#endif
