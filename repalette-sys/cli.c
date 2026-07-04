#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repalette.h"

static Color from_hex(Hex hex) {
	Color c;

	c.r = (hex & 0xff0000) >> 16;
	c.g = (hex & 0x00ff00) >> 8;
	c.b = (hex & 0x0000ff) >> 0;

	return c;
}

static int parse_dither(const char* param, Ditherer* dither) {
#define X(e)                                 \
	if (strcmp(param, dither_names[e]) == 0) { \
		*dither = e;                             \
		return 0;                                \
	}

	DITHERERS(X)
#undef X

	fprintf(stderr, "ERROR: Unknown dithering strategy \"%s\"\n", param);
	return 1;
}

int repalette_process(
	u8* pixels, int width, int height, const uint32_t* colors, size_t count,
	const char* dither
) {
	Ditherer ditherer = FLOYD_STEINBERG;
	if (dither && *dither) {
		if (parse_dither(dither, &ditherer)) return 1;
	}

	size_t size = (count + 3) / 4 * 4;
	int* buffer = malloc(3 * size * sizeof(int));
	if (!buffer) return 1;

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

	Palette pal = {
		.size = size,
		.rs = rs,
		.gs = gs,
		.bs = bs,
	};

	Image img = {pixels, width, height};
	recolor(img, pal, ditherer);

	free(buffer);
	return 0;
}

void repalette_help(void) {
	printf("USAGE:\n");
	printf("  repalette -h | --help\n");
	printf("  repalette <input file> <output file> [options]\n");
	printf("  repalette palette list\n");
	printf("  repalette palette show <name>\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf(
		"  -p, --palette <name>          Built-in preset (see 'palette list')\n"
	);
	printf(
		"  -c, --colors COLOR[,COLOR...] Manual palette, e.g. 000000,ffffff\n"
	);
	printf("  -d, --dither <ditherer>\n");
	printf("\n");

	printf("DITHERER: %s", dither_names[0]);
	for (int i = 1; i < DITHER_COUNT; ++i) { printf(" | %s", dither_names[i]); }
	printf("\n");

	int maxlen = strlen(dither_names[0]);
	for (int i = 1; i < DITHER_COUNT; ++i) {
		int len = strlen(dither_names[i]);
		if (len > maxlen) maxlen = len;
	}
	for (int i = 0; i < DITHER_COUNT; ++i) {
		printf("  %-*s - %s\n", maxlen, dither_names[i], dither_display_names[i]);
	}
	printf("\n");
}
