#include "repalette.h"

int ditherer_count(void) { return DITHER_COUNT; }

const char* ditherer_name(int i) { return dither_names[i]; }

const char* ditherer_display(int i) { return dither_display_names[i]; }

static Palette build_palette(const u8* colors, size_t count, int* buffer) {
	size_t size = (count + 3) / 4 * 4;

	int* rs = buffer + 0 * size;
	int* gs = buffer + 1 * size;
	int* bs = buffer + 2 * size;

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
	u8* pixels, int width, int height, const u8* colors, size_t count,
	int ditherer
) {
	int buffer[3 * 256];
	Palette pal = build_palette(colors, count, buffer);

	Image img = {pixels, width, height};
	recolor(img, pal, (Ditherer)ditherer);
}

void repalette_process_index(
	u8* pixels, int width, int height, const u8* colors, size_t count,
	int ditherer, u8* out
) {
	int buffer[3 * 256];
	Palette pal = build_palette(colors, count, buffer);

	Image img = {pixels, width, height};
	recolor_index(img, pal, (Ditherer)ditherer, out);
}
