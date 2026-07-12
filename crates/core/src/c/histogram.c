#include "histogram.h"
#include "oklab.h"

static inline size_t digit(const u8 *px, int shift) {
	return (((px[0] >> shift) & 1) << 2) | (((px[1] >> shift) & 1) << 1) |
				 ((px[2] >> shift) & 1);
}

static Oklab collect_bin(const u8 *buf, size_t start, size_t end) {
	size_t count = end - start;
	if (count == 0) return rgb_to_oklab((Frgb){.r = 0.0f, .g = 0.0f, .b = 0.0f});

	uint64_t sr = 0, sg = 0, sb = 0;
	for (size_t p = start; p < end; ++p) {
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
	size_t P = (size_t)img.width * img.height;

	__builtin_memcpy(scratch.work, img.pixels, P * CHANNELS);
	u8 *cur = scratch.work, *other = scratch.aux;
	uint32_t *bcur = scratch.bins0, *bother = scratch.bins1;

	bcur[0] = 0;
	bcur[1] = (uint32_t)P;
	size_t nb = 1;

	for (int level = 1; level <= 8; ++level) {
		int shift = 8 - level;

		size_t nonempty = 0;
		for (size_t j = 0; j < nb; ++j) {
			int h[8] = {0};
			for (size_t p = bcur[j]; p < bcur[j + 1]; ++p)
				h[digit(cur + p * CHANNELS, shift)]++;
			for (int d = 0; d < 8; ++d)
				if (h[d]) nonempty++;
		}
		if (nonempty > (size_t)scratch.threshold) break;

		size_t new_nb = 0;
		for (size_t j = 0; j < nb; ++j) {
			size_t start = bcur[j], end = bcur[j + 1];
			int h[8] = {0};
			for (size_t p = start; p < end; ++p)
				h[digit(cur + p * CHANNELS, shift)]++;

			uint32_t off[8];
			uint32_t acc = (uint32_t)start;
			for (int d = 0; d < 8; ++d) {
				off[d] = acc;
				acc += (uint32_t)h[d];
			}
			for (int d = 0; d < 8; ++d)
				if (h[d]) bother[new_nb++] = off[d];

			uint32_t cur_off[8];
			for (int d = 0; d < 8; ++d) cur_off[d] = off[d];
			for (size_t p = start; p < end; ++p) {
				int d = digit(cur + p * CHANNELS, shift);
				__builtin_memcpy(
					other + (size_t)cur_off[d] * CHANNELS, cur + p * CHANNELS, CHANNELS
				);
				cur_off[d]++;
			}
		}
		bother[new_nb] = (uint32_t)P;
		nb = new_nb;

		u8 *tp = cur;
		cur = other;
		other = tp;
		uint32_t *tb = bcur;
		bcur = bother;
		bother = tb;
	}

	for (size_t j = 0; j < nb; ++j) {
		Oklab lab = collect_bin(cur, bcur[j], bcur[j + 1]);
		out->l[j] = lab.l;
		out->a[j] = lab.a;
		out->b[j] = lab.b;
		out->w[j] = (float)(bcur[j + 1] - bcur[j]);
	}
	out->len = nb;
}
