#include "repalette.h"

static inline void update_pixel(
	Image img, u32 x, u32 y, i32x4 err, i32 mul, i32 div
) {
	// A negative neighbour offset underflows u32, so a single unsigned check
	// catches both the left (wrapped-around) and right edges.
	if (x >= img.width) return;
	size_t idx = CHANNELS * ((size_t)y * img.width + x);

	// One pixel is a 4-byte RGBA unit: load + widen the channels into a vector.
	u8x4 bytes;
	__builtin_memcpy(&bytes, img.pixels + idx, sizeof bytes);
	i32x4 pix = __builtin_convertvector(bytes, i32x4);

	i32x4 out = pix + err * mul / div;

	// Clamp each lane to [0, 255] branchlessly (mask is -1 / 0 per lane).
	i32x4 under = out < 0;
	out = out & ~under;
	i32x4 over = out > 255;
	out = (255 & over) | (out & ~over);

	bytes = __builtin_convertvector(out, u8x4);
	__builtin_memcpy(img.pixels + idx, &bytes, sizeof bytes);
}

#define DEFINE_DITHER(name, body) \
	static void dither_##name(Image img, u32 x, u32 y, i32x4 err) body

DEFINE_DITHER(FLOYD_STEINBERG, {
	update_pixel(img, x + 1, y + 0, err, 7, 16);
	if (y + 1 >= img.height) return;
	update_pixel(img, x - 1, y + 1, err, 3, 16);
	update_pixel(img, x + 0, y + 1, err, 5, 16);
	update_pixel(img, x + 1, y + 1, err, 1, 16);
})

DEFINE_DITHER(ATKINSON, {
	update_pixel(img, x + 1, y + 0, err, 1, 8);
	update_pixel(img, x + 2, y + 0, err, 1, 8);
	if (y + 1 >= img.height) return;
	update_pixel(img, x - 1, y + 1, err, 1, 8);
	update_pixel(img, x + 0, y + 1, err, 1, 8);
	update_pixel(img, x + 1, y + 1, err, 1, 8);
	if (y + 2 >= img.height) return;
	update_pixel(img, x + 1, y + 2, err, 1, 8);
})

DEFINE_DITHER(JJN, {
	update_pixel(img, x + 1, y + 0, err, 7, 48);
	update_pixel(img, x + 2, y + 0, err, 5, 48);
	if (y + 1 >= img.height) return;
	update_pixel(img, x - 2, y + 1, err, 3, 48);
	update_pixel(img, x - 1, y + 1, err, 5, 48);
	update_pixel(img, x + 0, y + 1, err, 7, 48);
	update_pixel(img, x + 1, y + 1, err, 5, 48);
	update_pixel(img, x + 2, y + 1, err, 3, 48);
	if (y + 2 >= img.height) return;
	update_pixel(img, x - 2, y + 2, err, 1, 48);
	update_pixel(img, x - 1, y + 2, err, 3, 48);
	update_pixel(img, x + 0, y + 2, err, 5, 48);
	update_pixel(img, x + 1, y + 2, err, 3, 48);
	update_pixel(img, x + 2, y + 2, err, 1, 48);
})

DEFINE_DITHER(BURKES, {
	update_pixel(img, x + 1, y + 0, err, 8, 32);
	update_pixel(img, x + 2, y + 0, err, 4, 32);
	if (y + 1 >= img.height) return;
	update_pixel(img, x - 2, y + 1, err, 2, 32);
	update_pixel(img, x - 1, y + 1, err, 4, 32);
	update_pixel(img, x + 0, y + 1, err, 8, 32);
	update_pixel(img, x + 1, y + 1, err, 4, 32);
	update_pixel(img, x + 2, y + 1, err, 2, 32);
})

DEFINE_DITHER(SIERRA32, {
	update_pixel(img, x + 1, y + 0, err, 5, 32);
	update_pixel(img, x + 2, y + 0, err, 3, 32);
	if (y + 1 >= img.height) return;
	update_pixel(img, x - 2, y + 1, err, 2, 32);
	update_pixel(img, x - 1, y + 1, err, 4, 32);
	update_pixel(img, x + 0, y + 1, err, 5, 32);
	update_pixel(img, x + 1, y + 1, err, 4, 32);
	update_pixel(img, x + 2, y + 1, err, 2, 32);
	if (y + 2 >= img.height) return;
	update_pixel(img, x - 1, y + 2, err, 2, 32);
	update_pixel(img, x + 0, y + 2, err, 3, 32);
	update_pixel(img, x + 1, y + 2, err, 2, 32);
})

DEFINE_DITHER(SIERRA4, {
	update_pixel(img, x + 1, y + 0, err, 2, 4);
	if (y + 1 >= img.height) return;
	update_pixel(img, x - 1, y + 1, err, 1, 4);
	update_pixel(img, x + 0, y + 1, err, 1, 4);
})

DEFINE_DITHER(NONE, {
	(void)img;
	(void)x;
	(void)y;
	(void)err;
})

static i32 find_nearest(Palette palette, Color c) {
	i32x4 vmin = {INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX};
	i32x4 vlane = {0, 1, 2, 3};
	i32x4 vbest = vlane;

	for (size_t i = 0; i < palette.size; i += 4) {
		i32x4 pr, pg, pb;
		__builtin_memcpy(&pr, &palette.rs[i], sizeof pr);
		__builtin_memcpy(&pg, &palette.gs[i], sizeof pg);
		__builtin_memcpy(&pb, &palette.bs[i], sizeof pb);

		i32x4 dr = c.r - pr;
		i32x4 dg = c.g - pg;
		i32x4 db = c.b - pb;
		i32x4 diff = dr * dr + dg * dg + db * db;

		// Lanes where this group beats the running min (mask is -1 / 0 per lane).
		i32x4 m = diff < vmin;
		vmin = (diff & m) | (vmin & ~m);
		vbest = (vlane & m) | (vbest & ~m);
		vlane += 4;
	}

	i32 min_diff = vmin[0];
	i32 best = vbest[0];
	for (i32 k = 1; k < 4; ++k) {
		if (vmin[k] < min_diff || (vmin[k] == min_diff && vbest[k] < best)) {
			min_diff = vmin[k];
			best = vbest[k];
		}
	}

	return best;
}

#define RECOLOR_BODY(name, WRITE)                  \
	for (u32 y = 0; y < img.height; ++y) {           \
		for (u32 x = 0; x < img.width; ++x) {          \
			size_t pos = (size_t)y * img.width + x;      \
			size_t idx = CHANNELS * pos;                 \
                                                   \
			Color old_color = {                          \
				img.pixels[idx + 0],                       \
				img.pixels[idx + 1],                       \
				img.pixels[idx + 2],                       \
			};                                           \
                                                   \
			i32 best = find_nearest(palette, old_color); \
                                                   \
			i32x4 error = {                              \
				old_color.r - palette.rs[best],            \
				old_color.g - palette.gs[best],            \
				old_color.b - palette.bs[best],            \
				0,                                         \
			};                                           \
                                                   \
			WRITE;                                       \
                                                   \
			dither_##name(img, x, y, error);             \
		}                                              \
	}

#define X(name)                                                           \
	static void recolor_##name(Image img, Palette palette) {                \
		RECOLOR_BODY(                                                         \
			name, do {                                                          \
				img.pixels[idx + 0] = palette.rs[best];                           \
				img.pixels[idx + 1] = palette.gs[best];                           \
				img.pixels[idx + 2] = palette.bs[best];                           \
			} while (0)                                                         \
		)                                                                     \
	}                                                                       \
	static void recolor_index_##name(Image img, Palette palette, u8 *out) { \
		RECOLOR_BODY(name, out[pos] = (u8)best)                               \
	}

DITHERERS(X)
#undef X

void recolor(Image img, Palette palette, Ditherer dither) {
	switch (dither) {
#define X(e) \
	case e: recolor_##e(img, palette); break;
		DITHERERS(X)
#undef X
		case DITHER_INVALID: break;
		case DITHER_COUNT: __builtin_unreachable(); break;
	}
}

void recolor_index(Image img, Palette palette, Ditherer dither, u8 *out) {
	switch (dither) {
#define X(e) \
	case e: recolor_index_##e(img, palette, out); break;
		DITHERERS(X)
#undef X
		case DITHER_INVALID: break;
		case DITHER_COUNT: __builtin_unreachable(); break;
	}
}

const char **dither_names = (const char *[]){
	[NONE] = "none",			 [FLOYD_STEINBERG] = "fs", [ATKINSON] = "atkinson",
	[JJN] = "jjn",				 [BURKES] = "burkes",			 [SIERRA32] = "sierra32",
	[SIERRA4] = "sierra4",
};

const char **dither_display_names = (const char *[]){
	[NONE] = "No dithering",	 [FLOYD_STEINBERG] = "Floyd-Steinberg",
	[ATKINSON] = "Atkinson",	 [JJN] = "Jarvis, Judice, and Ninke",
	[BURKES] = "Burkes",			 [SIERRA32] = "Sierra",
	[SIERRA4] = "Sierra Lite",
};
