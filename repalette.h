#ifndef REPALETTE_H
#define REPALETTE_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint32_t Hex;

typedef struct {
  int r;
  int g;
  int b;
} Color;

// TODO: Force image to have 4 channels
typedef struct {
  u8* pixels;
  int width;
  int height;
  int channels;
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

extern void recolor(Image, Color*, size_t, Ditherer);

#endif
