#ifndef DITHER_H
#define DITHER_H

#include "types.h"

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
