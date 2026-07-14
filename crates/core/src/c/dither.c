#include "dither.h"

#include <stddef.h>

static inline void update_pixel(
	Image img, u32 x, u32 y, i32x4 err, i32 mul, i32 div
) {
	// A negative neighbour offset underflows u32, so a single unsigned check
	// catches both the left (wrapped-around) and right edges.
	if (x >= img.width) return;
	u64 idx = CHANNELS * ((u64)y * img.width + x);

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

#define TAPS_FLOYD_STEINBERG(A, G) \
	do {                             \
		A(1, 0, 7, 16);                \
		G(1);                          \
		A(-1, 1, 3, 16);               \
		A(0, 1, 5, 16);                \
		A(1, 1, 1, 16);                \
	} while (0)

#define TAPS_ATKINSON(A, G) \
	do {                      \
		A(1, 0, 1, 8);          \
		A(2, 0, 1, 8);          \
		G(1);                   \
		A(-1, 1, 1, 8);         \
		A(0, 1, 1, 8);          \
		A(1, 1, 1, 8);          \
		G(2);                   \
		A(1, 2, 1, 8);          \
	} while (0)

#define TAPS_JJN(A, G) \
	do {                 \
		A(1, 0, 7, 48);    \
		A(2, 0, 5, 48);    \
		G(1);              \
		A(-2, 1, 3, 48);   \
		A(-1, 1, 5, 48);   \
		A(0, 1, 7, 48);    \
		A(1, 1, 5, 48);    \
		A(2, 1, 3, 48);    \
		G(2);              \
		A(-2, 2, 1, 48);   \
		A(-1, 2, 3, 48);   \
		A(0, 2, 5, 48);    \
		A(1, 2, 3, 48);    \
		A(2, 2, 1, 48);    \
	} while (0)

#define TAPS_BURKES(A, G) \
	do {                    \
		A(1, 0, 8, 32);       \
		A(2, 0, 4, 32);       \
		G(1);                 \
		A(-2, 1, 2, 32);      \
		A(-1, 1, 4, 32);      \
		A(0, 1, 8, 32);       \
		A(1, 1, 4, 32);       \
		A(2, 1, 2, 32);       \
	} while (0)

#define TAPS_SIERRA32(A, G) \
	do {                      \
		A(1, 0, 5, 32);         \
		A(2, 0, 3, 32);         \
		G(1);                   \
		A(-2, 1, 2, 32);        \
		A(-1, 1, 4, 32);        \
		A(0, 1, 5, 32);         \
		A(1, 1, 4, 32);         \
		A(2, 1, 2, 32);         \
		G(2);                   \
		A(-1, 2, 2, 32);        \
		A(0, 2, 3, 32);         \
		A(1, 2, 2, 32);         \
	} while (0)

#define TAPS_SIERRA4(A, G) \
	do {                     \
		A(1, 0, 2, 4);         \
		G(1);                  \
		A(-1, 1, 1, 4);        \
		A(0, 1, 1, 4);         \
	} while (0)

#define TAPS_NONE(A, G) \
	do {                  \
	} while (0)

#define APPLY_INPLACE(dx, dy, num, den) \
	update_pixel(img, x + (dx), y + (dy), err, (num), (den))
#define GUARD_INPLACE(dy) \
	if (y + (dy) >= img.height) return

#define DEFINE_DITHER(name)                                       \
	static void dither_##name(Image img, u32 x, u32 y, i32x4 err) { \
		(void)img;                                                    \
		(void)x;                                                      \
		(void)y;                                                      \
		(void)err;                                                    \
		TAPS_##name(APPLY_INPLACE, GUARD_INPLACE);                    \
	}

#define X(name) DEFINE_DITHER(name)
DITHERERS(X)
#undef X

static i32 find_nearest(Palette palette, Color c) {
	i32x4 vmin = {INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX};
	i32x4 vlane = {0, 1, 2, 3};
	i32x4 vbest = vlane;

	for (u32 i = 0; i < palette.size; i += 4) {
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
			u64 pos = (u64)y * img.width + x;            \
			u64 idx = CHANNELS * pos;                    \
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

static inline void update_ring(
	i16 *ring, u32 hi_width, u32 x, u32 row, i16x4 err, i16 mul, i16 div
) {
	if (x >= hi_width) return;
	u64 idx = (u64)((row + 1) & 3) * hi_width * CHANNELS + x * CHANNELS;

	i16x4 pixel;
	__builtin_memcpy(&pixel, ring + idx, sizeof pixel);
	pixel += err * mul / div;
	__builtin_memcpy(ring + idx, &pixel, sizeof pixel);
}

#define APPLY_RING(dx, dy, num, den) \
	update_ring(dither_ring, hi_width, x + (dx), row + (dy), error, (num), (den))
#define GUARD_RING(dy) \
	if (row + (dy) >= hi_height) continue

#define DEFINE_RING_DITHER(name)                                             \
	static void dither_ring_##name(                                            \
		i16 *dither_ring, u32 hi_width, u32 hi_height, u32 row, Palette palette, \
		const u8 *source_row                                                     \
	) {                                                                        \
		i16 *slot = dither_ring + (u64)((row + 1) & 3) * hi_width * CHANNELS;    \
		(void)hi_height;                                                         \
		for (u32 x = 0; x < hi_width; ++x) {                                     \
			u64 offset = (u64)x * CHANNELS;                                        \
			const u8 *source_pixel = source_row + (u64)(x >> 1) * CHANNELS;        \
			i16x4 accumulator, source;                                             \
			u8x4 source_bytes;                                                     \
			__builtin_memcpy(&accumulator, slot + offset, sizeof accumulator);     \
			__builtin_memcpy(&source_bytes, source_pixel, sizeof source_bytes);    \
			source = __builtin_convertvector(source_bytes, i16x4);                 \
			i16x4 old = accumulator + source;                                      \
			i16x4 under = old < 0;                                                 \
			old = old & ~under;                                                    \
			i16x4 over = old > 255;                                                \
			old = (255 & over) | (old & ~over);                                    \
			i32 best = find_nearest(                                               \
				palette, (Color){                                                    \
									 .r = old[0],                                              \
									 .g = old[1],                                              \
									 .b = old[2],                                              \
								 }                                                           \
			);                                                                     \
			i16x4 quantized = {                                                    \
				palette.rs[best],                                                    \
				palette.gs[best],                                                    \
				palette.bs[best],                                                    \
				old[3],                                                              \
			};                                                                     \
			__builtin_memcpy(slot + offset, &quantized, sizeof quantized);         \
			i16x4 error = old - quantized; /* alpha lane is old[3]-old[3] == 0 */  \
			(void)error;                                                           \
			TAPS_##name(APPLY_RING, GUARD_RING);                                   \
		}                                                                        \
	}

#define X(name) DEFINE_RING_DITHER(name)
DITHERERS(X)
#undef X

typedef void (*DitherRow)(i16 *, u32, u32, u32, Palette, const u8 *);

static DitherRow dither_row_for(Ditherer dither) {
	switch (dither) {
#define X(e) \
	case e: return dither_ring_##e;
		DITHERERS(X)
#undef X
		case DITHER_INVALID:
		case DITHER_COUNT: break;
	}
	__builtin_unreachable();
}

static void conv_h_edge(const i16 *hi_row, u16 *sums, u32 x, u32 width) {
	ptrdiff_t l = x == 0 ? 0 : -1;
	ptrdiff_t r = x == width - 1 ? 1 : 2;

	const i16 *base = hi_row + (u64)(2 * x) * CHANNELS;
	const i16 *p0 = base + l * CHANNELS;
	const i16 *p1 = base + 0 * CHANNELS;
	const i16 *p2 = base + 1 * CHANNELS;
	const i16 *p3 = base + r * CHANNELS;

	sums += (u64)x * CHANNELS;
	for (u32 channel = 0; channel < CHANNELS; ++channel) {
		sums[channel] = (u16)(*p0 + 3 * *p1 + 3 * *p2 + *p3);
		p0 += 1;
		p1 += 1;
		p2 += 1;
		p3 += 1;
	}
}

static void conv_horizontal(const i16 *hi_row, u16 *sums, u32 width) {
	conv_h_edge(hi_row, sums, 0, width);

	for (u32 x = 1; x + 1 < width; ++x) {
		const i16 *base = hi_row + (u64)(2 * x - 1) * CHANNELS;
		i16x4 p0, p1, p2, p3;
		__builtin_memcpy(&p0, base + 0 * CHANNELS, sizeof p0);
		__builtin_memcpy(&p1, base + 1 * CHANNELS, sizeof p1);
		__builtin_memcpy(&p2, base + 2 * CHANNELS, sizeof p2);
		__builtin_memcpy(&p3, base + 3 * CHANNELS, sizeof p3);
		u16x4 sum = __builtin_convertvector(p0 + 3 * p1 + 3 * p2 + p3, u16x4);
		__builtin_memcpy(sums + (u64)x * CHANNELS, &sum, sizeof sum);
	}

	if (width > 1) { conv_h_edge(hi_row, sums, width - 1, width); }
}

static void conv_vertical(
	u16 *const rows[4], u32 last_row, u8 *out, u32 width
) {
	u16 weight[4];
	weight[(last_row + 0) & 3] = 3;
	weight[(last_row + 1) & 3] = 1;
	weight[(last_row + 2) & 3] = 1;
	weight[(last_row + 3) & 3] = 3;

	for (u64 i = 0; i < (u64)width * CHANNELS; i += CHANNELS) {
		u16x4 r0, r1, r2, r3;
		__builtin_memcpy(&r0, rows[0] + i, sizeof r0);
		__builtin_memcpy(&r1, rows[1] + i, sizeof r1);
		__builtin_memcpy(&r2, rows[2] + i, sizeof r2);
		__builtin_memcpy(&r3, rows[3] + i, sizeof r3);
		u16x4 sum =
			weight[0] * r0 + weight[1] * r1 + weight[2] * r2 + weight[3] * r3;
		u8x4 pixel = __builtin_convertvector((sum + 32) >> 6, u8x4);
		__builtin_memcpy(out + i, &pixel, sizeof pixel);
	}
}

void recolor_multisample(
	Image img, Palette palette, Ditherer dither, i16 *dither_ring, u16 *conv_ring
) {
	u32 w = img.width;
	u32 h = img.height;
	if (w == 0 || h == 0) return;

	u32 hi_width = 2 * w;
	u32 hi_height = 2 * h;
	u64 dither_stride = (u64)hi_width * CHANNELS;
	u64 conv_stride = (u64)w * CHANNELS;
	u8 *pixels = img.pixels;

	i16 *dither_rows[4];
	u16 *conv_rows[4];
	for (u32 i = 0; i < 4; ++i) {
		dither_rows[i] = dither_ring + i * dither_stride;
		conv_rows[i] = conv_ring + i * conv_stride;
	}

	DitherRow dither_row = dither_row_for(dither);

	__builtin_memset(dither_ring, 0, 4 * dither_stride * sizeof(i16));

  // Handle top edge padding
	dither_row(dither_ring, hi_width, hi_height, 0, palette, pixels);
	dither_row(dither_ring, hi_width, hi_height, 1, palette, pixels);
	conv_horizontal(dither_rows[1], conv_rows[1], w);
	__builtin_memcpy(conv_rows[0], conv_rows[1], conv_stride * sizeof(u16));
	__builtin_memset(dither_rows[1], 0, dither_stride * sizeof(i16));
	conv_horizontal(dither_rows[2], conv_rows[2], w);
	__builtin_memset(dither_rows[2], 0, dither_stride * sizeof(i16));

	for (u32 s = 1; s < h; ++s) {
		u32 row1 = 2 * s;
		u32 row2 = row1 + 1;
		u32 slot1 = (row1 + 1) & 3;
		u32 slot2 = (row2 + 1) & 3;
		u8 *source = pixels + s * conv_stride;

		dither_row(dither_ring, hi_width, hi_height, row1, palette, source);
		conv_horizontal(dither_rows[slot1], conv_rows[slot1], w);
		__builtin_memset(dither_rows[slot1], 0, dither_stride * sizeof(i16));

		// Each iteration have enough information to output the previous row
		conv_vertical(conv_rows, row1, pixels + (s - 1) * conv_stride, w);

		dither_row(dither_ring, hi_width, hi_height, row2, palette, source);
		conv_horizontal(dither_rows[slot2], conv_rows[slot2], w);
		__builtin_memset(dither_rows[slot2], 0, dither_stride * sizeof(i16));
	}

  // Mirror the last row for the final convolution
	u16 *prev_row = conv_rows[(2 * h + 0) & 3];
	u16 *last_row = conv_rows[(2 * h + 1) & 3];

	__builtin_memcpy(last_row, prev_row, conv_stride * sizeof(u16));
	conv_vertical(conv_rows, 2 * h, pixels + (h - 1) * conv_stride, w);
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
