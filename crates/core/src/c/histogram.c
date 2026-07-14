#include "histogram.h"

#include "oklab.h"

static inline u32 digit(const u8 *px, int shift) {
	u32 r = (px[0] >> shift) & 1;
	u32 g = (px[1] >> shift) & 1;
	u32 b = (px[2] >> shift) & 1;
	return (r << 2) | (g << 1) | b;
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

	__builtin_memcpy(scratch.work, img.pixels, P * CHANNELS);
	u8 *curr = scratch.work, *next = scratch.aux;
	u64 *bcurr = scratch.bins0;
	u64 *bnext = scratch.bins1;

	bcurr[0] = 0;
	bcurr[1] = P;
	u32 nb = 1;

	for (int level = 1; level <= 8; ++level) {
		int shift = 8 - level;

		u32 new_nb = 0;
		for (u32 j = 0; j < nb; ++j) {
			u64 start = bcurr[j], end = bcurr[j + 1];
			u64 count[8] = {0};
			for (u64 p = start; p < end; ++p) {
				count[digit(curr + p * CHANNELS, shift)]++;
			}

			u64 off[8];
			u64 acc = start;
			for (int d = 0; d < 8; ++d) {
				off[d] = acc;
				acc += count[d];
			}
			for (int d = 0; d < 8; ++d) {
				if (count[d] > 0) {
					if (new_nb >= scratch.threshold) goto done;
					bnext[new_nb++] = off[d];
				}
			}

			u64 cur_off[8];
			for (int d = 0; d < 8; ++d) cur_off[d] = off[d];
			for (u64 p = start; p < end; ++p) {
				u32 d = digit(curr + p * CHANNELS, shift);
				__builtin_memcpy(
					next + cur_off[d] * CHANNELS, curr + p * CHANNELS, CHANNELS
				);
				cur_off[d]++;
			}
		}
		bnext[new_nb] = P;
		nb = new_nb;

		u8 *tp = curr;
		curr = next;
		next = tp;
		u64 *tb = bcurr;
		bcurr = bnext;
		bnext = tb;
	}
done:

	for (u32 j = 0; j < nb; ++j) {
		Oklab lab = collect_bin(curr, bcurr[j], bcurr[j + 1]);
		out->l[j] = lab.l;
		out->a[j] = lab.a;
		out->b[j] = lab.b;
		out->w[j] = (float)(bcurr[j + 1] - bcurr[j]);
	}
	out->len = nb;
}
