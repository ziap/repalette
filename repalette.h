#ifndef REPALETTE_H
#define REPALETTE_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint32_t Hex;

// Images are fixed to 4 channels (RGBA) so each pixel is one 32-bit unit.
#define CHANNELS 4

typedef struct {
  int r;
  int g;
  int b;
} Color;

typedef struct {
  int *buffer;
  size_t size;

  const int *rs;
  const int *gs;
  const int *bs;
} Palette;

typedef struct {
  u8* pixels;
  int width;
  int height;
} Image;

typedef enum {
  NONE,
  FLOYD_STEINBERG,
  ATKINSON,
  JJN,
  BURKES,
  SIERRA,
  SIERRA_LITE,
} Ditherer;

extern void recolor(Image, Palette, Ditherer);

#endif
