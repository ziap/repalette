#include "histogram.h"

#include "oklab.h"

static inline u32 digit(const u8 *px, u32 shift) {
	u32 r = (px[0] >> shift) & 1;
	u32 g = (px[1] >> shift) & 1;
	u32 b = (px[2] >> shift) & 1;
	return (r << 2) | (g << 1) | b;
}

static inline void swap_px(u8 *pixels, u64 i, u64 j) {
	u8 tmp[CHANNELS];
	u8 *a = pixels + i * CHANNELS, *b = pixels + j * CHANNELS;
	__builtin_memcpy(tmp, a, CHANNELS);
	__builtin_memcpy(a, b, CHANNELS);
	__builtin_memcpy(b, tmp, CHANNELS);
}

static Oklab collect_bin(const u8 *buf, u64 start, u64 end) {
	u64 count = end - start;

	uint64_t sr = 0, sg = 0, sb = 0;
	for (u64 p = start; p < end; ++p) {
		const u8 *px = buf + p * CHANNELS;
		sr += px[0];
		sg += px[1];
		sb += px[2];
	}
	return rgb_to_oklab((Frgb){
		.r = (float)sr / (float)count,
		.g = (float)sg / (float)count,
		.b = (float)sb / (float)count,
	});
}

void build_hist(Image img, HistogramScratch scratch, Histogram *out) {
	u64 P = (u64)img.width * img.height;

	u8 *pixels = img.pixels;
	u64 *bcurr = scratch.bins0;
	u64 *bnext = scratch.bins1;

	bcurr[0] = 0;
	bcurr[1] = P;
	u32 nb = 1;

	for (u32 level = 1; level <= 8; ++level) {
		u32 shift = 8 - level;

		u32 new_nb = 0;
		for (u32 j = 0; j < nb; ++j) {
			u64 start = bcurr[j], end = bcurr[j + 1];
			u64 count[8] = {0};
			for (u64 p = start; p < end; ++p) {
				count[digit(pixels + p * CHANNELS, shift)] += 1;
			}

			u64 off[9];
			u64 acc = start;
			for (u32 d = 0; d < 8; ++d) {
				off[d] = acc;
				acc += count[d];
			}
			off[8] = end;
			for (u32 d = 0; d < 8; ++d) {
				if (count[d] > 0) {
					if (new_nb >= scratch.threshold) goto done;
					bnext[new_nb++] = off[d];
				}
			}

			// In-place American-flag permutation of this slice into the 8 buckets.
			u64 next[8];
			__builtin_memcpy(next, off, sizeof(next));
			for (u32 b = 0; b < 8; ++b) {
				while (next[b] < off[b + 1]) {
					u32 d = digit(pixels + next[b] * CHANNELS, shift);
					if (d == b) {
						next[b] += 1;
					} else {
						swap_px(pixels, next[b], next[d]);
						next[d] += 1;
					}
				}
			}
		}
		bnext[new_nb] = P;
		nb = new_nb;

		u64 *tb = bcurr;
		bcurr = bnext;
		bnext = tb;
	}
done:

	for (u32 j = 0; j < nb; ++j) {
		Oklab lab = collect_bin(pixels, bcurr[j], bcurr[j + 1]);
		out->l[j] = lab.l;
		out->a[j] = lab.a;
		out->b[j] = lab.b;
		out->w[j] = (float)(bcurr[j + 1] - bcurr[j]);
	}
	out->len = nb;
}
