#include "dither.h"

#include <stddef.h>

#include "types.h"

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

#define DEFINE_RING_DITHER(name)                                              \
	static void dither_ring_##name(                                             \
		i16 *dither_ring, u32 width, u32 scale, u32 height, u32 row,              \
		Palette palette, const u8 *source_row                                     \
	) {                                                                         \
		u32 hi_width = width * scale;                                             \
		u32 hi_height = height * scale;                                           \
		i16 *slot = dither_ring + (u64)((row + 1) & 3) * hi_width * CHANNELS;     \
		(void)hi_height;                                                          \
		for (u32 sx = 0; sx < width; ++sx) {                                      \
			const u8 *source_pixel = source_row + (u64)sx * CHANNELS;               \
			u8x4 source_bytes;                                                      \
			__builtin_memcpy(&source_bytes, source_pixel, sizeof source_bytes);     \
			i16x4 source = __builtin_convertvector(source_bytes, i16x4);            \
			for (u32 k = 0; k < scale; ++k) {                                       \
				u32 x = sx * scale + k;                                               \
				u64 offset = (u64)x * CHANNELS;                                       \
				i16x4 accumulator;                                                    \
				__builtin_memcpy(&accumulator, slot + offset, sizeof accumulator);    \
				i16x4 old = accumulator + source;                                     \
				i16x4 under = old < 0;                                                \
				old = old & ~under;                                                   \
				i16x4 over = old > 255;                                               \
				old = (255 & over) | (old & ~over);                                   \
				i32 best = find_nearest(palette, (Color){old[0], old[1], old[2]});    \
				i16x4 quantized = {                                                   \
					palette.rs[best], palette.gs[best], palette.bs[best], old[3]        \
				};                                                                    \
				__builtin_memcpy(slot + offset, &quantized, sizeof quantized);        \
				i16x4 error = old - quantized; /* alpha lane is old[3]-old[3] == 0 */ \
				(void)error;                                                          \
				TAPS_##name(APPLY_RING, GUARD_RING);                                  \
			}                                                                       \
		}                                                                         \
	}

#define X(name) DEFINE_RING_DITHER(name)
DITHERERS(X)
#undef X

typedef void (*RowDitherFn)(i16 *, u32, u32, u32, u32, Palette, const u8 *);

static RowDitherFn row_ditherer(Ditherer dither) {
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
	for (u32 c = 0; c < CHANNELS; ++c) {
		sums[c] = (u16)(*(p0++) + 3 * *(p1++) + 3 * *(p2++) + *(p3++));
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

static void conv_vertical(u16 *rows[4], u32 row, u8 *out, u32 width) {
	u16 w[4];
	w[(row + 2) & 3] = 1;
	w[(row + 3) & 3] = 3;
	w[(row + 0) & 3] = 3;
	w[(row + 1) & 3] = 1;

	const u16 *r0 = rows[0];
	const u16 *r1 = rows[1];
	const u16 *r2 = rows[2];
	const u16 *r3 = rows[3];

	for (u64 i = 0; i < (u64)width * CHANNELS; ++i) {
		u16 s = w[0] * *(r0++) + w[1] * *(r1++) + w[2] * *(r2++) + w[3] * *(r3++);
		*(out++) = (s + 32) / 64;
	}
}

static void conv_vertical_mid(u16 *rows[4], u32 row, i16 *out, u32 width) {
	u16 w[4];
	w[(row + 2) & 3] = 1;
	w[(row + 3) & 3] = 3;
	w[(row + 0) & 3] = 3;
	w[(row + 1) & 3] = 1;

	const u16 *r0 = rows[0];
	const u16 *r1 = rows[1];
	const u16 *r2 = rows[2];
	const u16 *r3 = rows[3];

	for (u64 i = 0; i < (u64)width * CHANNELS; ++i) {
		u16 s = w[0] * *(r0++) + w[1] * *(r1++) + w[2] * *(r2++) + w[3] * *(r3++);
		*(out++) = (i16)((s + 32) / 64);
	}
}

typedef struct {
	RowDitherFn fn;
	i16 *dither_ring;
	u16 *conv_ring;
	Palette palette;
	u32 width;
	u32 height;
	u32 scale;
} RowDitherer;

static inline void dither_row(const RowDitherer *s, u32 row, const u8 *source) {
	u32 hi_width = s->width * s->scale;
	u32 conv_width = hi_width / 2;	// conv_horizontal decimates 2x
	u64 dither_stride = (u64)hi_width * CHANNELS;
	u64 conv_stride = (u64)conv_width * CHANNELS;
	u64 slot = (row + 1) & 3;

	i16 *row_ptr = s->dither_ring + slot * dither_stride;
	s->fn(s->dither_ring, s->width, s->scale, s->height, row, s->palette, source);
	conv_horizontal(row_ptr, s->conv_ring + slot * conv_stride, conv_width);
	__builtin_memset(row_ptr, 0, dither_stride * sizeof(i16));
}

static inline void pad_conv_row(u16 *dst, const u16 *src, u32 width) {
	__builtin_memcpy(dst, src, (u64)width * CHANNELS * sizeof(u16));
}

void recolor_multisample_2x(
	Image img, Palette palette, Ditherer dither, i16 *dither_ring, u16 *conv_ring
) {
	u32 w = img.width;
	u32 h = img.height;
	if (w == 0 || h == 0) return;

	u32 w2 = 2 * w;
	u32 h2 = 2 * h;
	u64 dither_stride = (u64)w2 * CHANNELS;
	u64 conv_stride = (u64)w * CHANNELS;
	u8 *pixels = img.pixels;

	u16 *conv_rows[4];
	for (u32 i = 0; i < 4; ++i) { conv_rows[i] = conv_ring + i * conv_stride; }

	RowDitherer ss = {
		.fn = row_ditherer(dither),
		.dither_ring = dither_ring,
		.conv_ring = conv_ring,
		.palette = palette,
		.width = w,
		.height = h,
		.scale = 2,
	};

	__builtin_memset(dither_ring, 0, 4 * dither_stride * sizeof(i16));

	dither_row(&ss, 0, pixels);
	pad_conv_row(conv_rows[0], conv_rows[1], w);
	dither_row(&ss, 1, pixels);

	for (u32 s = 1; s < h; ++s) {
		const u8 *source = pixels + s * conv_stride;

		dither_row(&ss, 2 * s, source);
		conv_vertical(conv_rows, 2 * s, pixels + (s - 1) * conv_stride, w);
		dither_row(&ss, 2 * s + 1, source);
	}

	pad_conv_row(conv_rows[(h2 + 1) & 3], conv_rows[h2 & 3], w);
	conv_vertical(conv_rows, h2, pixels + (h - 1) * conv_stride, w);
}

void recolor_multisample_4x(
	Image img, Palette palette, Ditherer dither, i16 *dither_ring,
	u16 *conv_ring_1, i16 *mid_row, u16 *conv_ring_2
) {
	u32 w = img.width;
	u32 h = img.height;
	if (w == 0 || h == 0) return;

	u32 w4 = 4 * w;
	u32 h4 = 4 * h;

	u32 w2 = 2 * w;
	u32 h2 = 2 * h;

	u64 out_stride = (u64)w * CHANNELS;
	u64 mid_stride = (u64)w2 * CHANNELS;
	u64 dither_stride = (u64)w4 * CHANNELS;
	u8 *pixels = img.pixels;

	u16 *conv1_rows[4];
	u16 *conv2_rows[4];
	for (u32 i = 0; i < 4; ++i) {
		conv1_rows[i] = conv_ring_1 + i * mid_stride;
		conv2_rows[i] = conv_ring_2 + i * out_stride;
	}

	RowDitherer ss = {
		.fn = row_ditherer(dither),
		.dither_ring = dither_ring,
		.conv_ring = conv_ring_1,
		.palette = palette,
		.width = w,
		.height = h,
		.scale = 4,
	};
	__builtin_memset(dither_ring, 0, 4 * dither_stride * sizeof(i16));

	dither_row(&ss, 0, pixels);

	pad_conv_row(conv1_rows[0], conv1_rows[1], w2);
	dither_row(&ss, 1, pixels);
	dither_row(&ss, 2, pixels);
	conv_vertical_mid(conv1_rows, 2, mid_row, w2);
	conv_horizontal(mid_row, conv2_rows[1], w);
	pad_conv_row(conv2_rows[0], conv2_rows[1], w);
	dither_row(&ss, 3, pixels);

	for (u32 s = 1; s < h; ++s) {
		const u8 *source = pixels + (u64)s * out_stride;

		dither_row(&ss, 4 * s, source);
		conv_vertical_mid(conv1_rows, 4 * s, mid_row, w2);
		conv_horizontal(mid_row, conv2_rows[(2 * s) & 3], w);
		dither_row(&ss, 4 * s + 1, source);
		dither_row(&ss, 4 * s + 2, source);
		conv_vertical_mid(conv1_rows, 4 * s + 2, mid_row, w2);
		conv_horizontal(mid_row, conv2_rows[(2 * s + 1) & 3], w);
		conv_vertical(conv2_rows, 2 * s, pixels + (u64)(s - 1) * out_stride, w);
		dither_row(&ss, 4 * s + 3, source);
	}

	pad_conv_row(conv1_rows[(h4 + 1) & 3], conv1_rows[h4 & 3], w2);
	conv_vertical_mid(conv1_rows, h4, mid_row, w2);
	conv_horizontal(mid_row, conv2_rows[h2 & 3], w);
	pad_conv_row(conv2_rows[(h2 + 1) & 3], conv2_rows[h2 & 3], w);
	conv_vertical(conv2_rows, h2, pixels + (u64)(h - 1) * out_stride, w);
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
