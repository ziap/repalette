#include <stdlib.h>

#include "repalette.h"

static Color from_hex(Hex hex) {
	Color c;

	c.r = (hex & 0xff0000) >> 16;
	c.g = (hex & 0x00ff00) >> 8;
	c.b = (hex & 0x0000ff) >> 0;

	return c;
}

int ditherer_count(void) { return DITHER_COUNT; }

const char* ditherer_name(int i) { return dither_names[i]; }

const char* ditherer_display(int i) { return dither_display_names[i]; }

static int* build_palette(const uint32_t* colors, size_t count, Palette* pal) {
	size_t size = (count + 3) / 4 * 4;
	int* buffer = malloc(3 * size * sizeof(int));
	if (!buffer) return NULL;

	int* rs = buffer + 0 * size;
	int* gs = buffer + 1 * size;
	int* bs = buffer + 2 * size;

	for (size_t i = 0; i < count; ++i) {
		Color color = from_hex(colors[i]);
		rs[i] = color.r;
		gs[i] = color.g;
		bs[i] = color.b;
	}

	Color last = from_hex(count > 0 ? colors[count - 1] : 0);
	for (size_t i = count; i < size; ++i) {
		rs[i] = last.r;
		gs[i] = last.g;
		bs[i] = last.b;
	}

	pal->size = size;
	pal->rs = rs;
	pal->gs = gs;
	pal->bs = bs;

	return buffer;
}

int repalette_process(
	u8* pixels, int width, int height, const uint32_t* colors, size_t count,
	int ditherer
) {
	Palette pal;
	int* buffer = build_palette(colors, count, &pal);
	if (!buffer) return 1;

	Image img = {pixels, width, height};
	recolor(img, pal, (Ditherer)ditherer);

	free(buffer);
	return 0;
}

int repalette_process_index(
	u8* pixels, int width, int height, const uint32_t* colors, size_t count,
	int ditherer, u8* out
) {
	Palette pal;
	int* buffer = build_palette(colors, count, &pal);
	if (!buffer) return 1;

	Image img = {pixels, width, height};
	recolor_index(img, pal, (Ditherer)ditherer, out);

	free(buffer);
	return 0;
}
