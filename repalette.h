#ifndef REPALETTE_H
#define REPALETTE_H

typedef __INT32_TYPE__ i32;
typedef __UINT8_TYPE__ u8;
typedef __UINT32_TYPE__ u32;
typedef __SIZE_TYPE__ usize;

typedef struct {
  i32 r;
  i32 g;
  i32 b;
} Color;

typedef struct {
  u8* pixels;
  i32 width;
  i32 height;
  i32 channels;
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

extern void recolor(Image, Color*, usize, Ditherer);

#endif
