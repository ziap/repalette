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

static int parse_palette(const char* param, Palette* palette) {
  size_t size = 1;
  for (const char* c = param; *c != '\0'; ++c) {
    if (*c == ',') ++size;
  }

  palette->size = (size + 3) / 4 * 4;
  palette->buffer = malloc(3 * palette->size * sizeof(int));
  if (!palette->buffer) return 1;

  int* rs = palette->buffer + 0 * palette->size;
  int* gs = palette->buffer + 1 * palette->size;
  int* bs = palette->buffer + 2 * palette->size;

  size = 0;
  Hex hex = 0;
  for (const char* c = param; *c != '\0'; ++c) {
    if (*c == ',') {
      Color color = from_hex(hex);
      rs[size] = color.r;
      gs[size] = color.g;
      bs[size] = color.b;

      size += 1;
      hex = 0;
      continue;
    }
    hex *= 16;
    if (*c >= '0' && *c <= '9') hex += *c - '0';
    else if (*c >= 'a' && *c <= 'f')
      hex += *c - 'a' + 10;
    else if (*c >= 'A' && *c <= 'F')
      hex += *c - 'A' + 10;
    else {
      fprintf(stderr, "ERROR: Unsupported character '%c' in color\n", *c);
      return 1;
    }
  }

  Color color = from_hex(hex);
  for (size_t i = size; i < palette->size; ++i) {
    rs[i] = color.r;
    gs[i] = color.g;
    bs[i] = color.b;
  }

  palette->rs = rs;
  palette->gs = gs;
  palette->bs = bs;
  return 0;
}

static int default_palette(Palette* palette) {
  palette->size = 4;
  palette->buffer = malloc(3 * palette->size * sizeof(int));
  if (!palette->buffer) return 1;

  int* rs = palette->buffer + 0 * palette->size;
  int* gs = palette->buffer + 1 * palette->size;
  int* bs = palette->buffer + 2 * palette->size;

  rs[0] = gs[0] = bs[0] = 0x00;
  for (size_t i = 1; i < palette->size; ++i) {
    rs[i] = gs[i] = bs[i] = 0xff;
  }

  palette->rs = rs;
  palette->gs = gs;
  palette->bs = bs;
  return 0;
}

int repalette_process(u8* pixels, int width, int height, const char* palette, const char* dither) {
  Ditherer ditherer = FLOYD_STEINBERG;
  if (dither && *dither) {
    if (parse_dither(dither, &ditherer)) return 1;
  }

  Palette pal = {0};
  if (palette && *palette) {
    if (parse_palette(palette, &pal)) return 1;
  } else if (default_palette(&pal)) {
    return 1;
  }

  Image img = {pixels, width, height};
  recolor(img, pal, ditherer);

  free(pal.buffer);
  return 0;
}

void repalette_help(void) {
  printf("USAGE:\n");
  printf("  repalette -h | --help\n");
  printf("  repalette <input file> <output file> [options]\n");
  printf("\n");
  printf("OPTIONS:\n");
  printf("  -p, --palette COLOR[,COLOR...]\n");
  printf("  -d, --dither <ditherer>\n");
  printf("\n");

  printf("DITHERER: %s", dither_names[0]);
  for (int i = 1; i < DITHER_COUNT; ++i) {
    printf(" | %s", dither_names[i]);
  }
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
