#include <stddef.h>

#include "dither.h"
#include "extract.h"

size_t ditherer_count(void) { return DITHER_COUNT; }

const char* ditherer_name(size_t i) { return dither_names[i]; }

const char* ditherer_display(size_t i) { return dither_display_names[i]; }

static Palette build_palette(const u8* colors, size_t count, i32* buffer) {
	size_t size = (count + 3) / 4 * 4;

	i32* rs = buffer + 0 * size;
	i32* gs = buffer + 1 * size;
	i32* bs = buffer + 2 * size;

	for (size_t i = 0; i < count; ++i) {
		rs[i] = colors[i * 3 + 0];
		gs[i] = colors[i * 3 + 1];
		bs[i] = colors[i * 3 + 2];
	}

	for (size_t i = count; i < size; ++i) {
		rs[i] = rs[count - 1];
		gs[i] = gs[count - 1];
		bs[i] = bs[count - 1];
	}

	return (Palette){
		.size = size,
		.rs = rs,
		.gs = gs,
		.bs = bs,
	};
}

void repalette_process(
	u8* pixels, u32 width, u32 height, const u8* colors, size_t count,
	size_t ditherer
) {
	i32 buffer[3 * 256];
	Palette pal = build_palette(colors, count, buffer);

	Image img = {.pixels = pixels, .width = width, .height = height};
	recolor(img, pal, (Ditherer)ditherer);
}

void repalette_process_index(
	u8* pixels, u32 width, u32 height, const u8* colors, size_t count,
	size_t ditherer, u8* out
) {
	i32 buffer[3 * 256];
	Palette pal = build_palette(colors, count, buffer);

	Image img = {.pixels = pixels, .width = width, .height = height};
	recolor_index(img, pal, (Ditherer)ditherer, out);
}

size_t repalette_extract(
	u8* pixels, u32 width, u32 height, size_t k, size_t threshold, float* soa,
	size_t* bins, u8* pixbuf, u8* out
) {
	size_t P = (size_t)width * height;

	HistogramScratch hist = {
		.threshold = threshold,
		.bins0 = bins,
		.bins1 = bins + (threshold + 1),
		.work = pixbuf,
		.aux = pixbuf + P * CHANNELS,
	};

	Histogram reps = {
		.l = soa,
		.a = soa + threshold,
		.b = soa + 2 * threshold,
		.w = soa + 3 * threshold,
	};

	Image img = {.pixels = pixels, .width = width, .height = height};
	return extract_palette(img, k, hist, reps, out);
}
