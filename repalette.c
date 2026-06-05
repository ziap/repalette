#include <limits.h>

#include "repalette.h"

typedef int32_t i32x4 __attribute__((vector_size(16)));
typedef uint8_t u8x4 __attribute__((vector_size(4)));

static inline void update_pixel(Image img, int x, int y, Color err, int mul, int div) {
  if (x < 0 || x >= img.width) return;
  size_t idx = CHANNELS * (y * img.width + x);

  // One pixel is a 4-byte RGBA unit: load + widen the channels into a vector.
  u8x4 bytes;
  __builtin_memcpy(&bytes, img.pixels + idx, sizeof bytes);
  i32x4 pix = __builtin_convertvector(bytes, i32x4);

  i32x4 e = {err.r, err.g, err.b, 0};  // alpha error is 0, so alpha is unchanged
  i32x4 out = pix + e * mul / div;

  // Clamp each lane to [0, 255] branchlessly (mask is -1 / 0 per lane).
  i32x4 under = out < 0;
  out = out & ~under;
  i32x4 over = out > 255;
  out = (255 & over) | (out & ~over);

  bytes = __builtin_convertvector(out, u8x4);
  __builtin_memcpy(img.pixels + idx, &bytes, sizeof bytes);
}

static void dither_floyd_steinberg(Image img, int x, int y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 7, 16);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 1, y + 1, err, 3, 16);
  update_pixel(img, x + 0, y + 1, err, 5, 16);
  update_pixel(img, x + 1, y + 1, err, 1, 16);
}

static void dither_atkinson(Image img, int x, int y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 1, 8);
  update_pixel(img, x + 2, y + 0, err, 1, 8);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 1, y + 1, err, 1, 8);
  update_pixel(img, x + 0, y + 1, err, 1, 8);
  update_pixel(img, x + 1, y + 1, err, 1, 8);
  if (y + 2 >= img.height) return;
  update_pixel(img, x + 1, y + 2, err, 1, 8);
}

static void dither_jjn(Image img, int x, int y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 7, 48);
  update_pixel(img, x + 2, y + 0, err, 5, 48);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 2, y + 1, err, 3, 48);
  update_pixel(img, x - 1, y + 1, err, 5, 48);
  update_pixel(img, x + 0, y + 1, err, 7, 48);
  update_pixel(img, x + 1, y + 1, err, 5, 48);
  update_pixel(img, x + 2, y + 1, err, 3, 48);
  if (y + 2 >= img.height) return;
  update_pixel(img, x - 2, y + 2, err, 1, 48);
  update_pixel(img, x - 1, y + 2, err, 3, 48);
  update_pixel(img, x + 0, y + 2, err, 5, 48);
  update_pixel(img, x + 1, y + 2, err, 3, 48);
  update_pixel(img, x + 2, y + 2, err, 1, 48);
}

static void dither_burkes(Image img, int x, int y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 8, 32);
  update_pixel(img, x + 2, y + 0, err, 4, 32);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 2, y + 1, err, 2, 32);
  update_pixel(img, x - 1, y + 1, err, 4, 32);
  update_pixel(img, x + 0, y + 1, err, 8, 32);
  update_pixel(img, x + 1, y + 1, err, 4, 32);
  update_pixel(img, x + 2, y + 1, err, 2, 32);
}

static void dither_sierra(Image img, int x, int y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 5, 32);
  update_pixel(img, x + 2, y + 0, err, 3, 32);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 2, y + 1, err, 2, 32);
  update_pixel(img, x - 1, y + 1, err, 4, 32);
  update_pixel(img, x + 0, y + 1, err, 5, 32);
  update_pixel(img, x + 1, y + 1, err, 4, 32);
  update_pixel(img, x + 2, y + 1, err, 2, 32);
  if (y + 2 >= img.height) return;
  update_pixel(img, x - 1, y + 2, err, 2, 32);
  update_pixel(img, x + 0, y + 2, err, 3, 32);
  update_pixel(img, x + 1, y + 2, err, 2, 32);
}

static void dither_sierra_lite(Image img, int x, int y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 2, 4);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 1, y + 1, err, 1, 4);
  update_pixel(img, x + 0, y + 1, err, 1, 4);
}

static void dither_none(Image img, int x, int y, Color err) {
  (void)img;
  (void)x;
  (void)y;
  (void)err;
}

static int find_nearest(Palette palette, Color c) {
  i32x4 vmin = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
  i32x4 vlane = {0, 1, 2, 3};
  i32x4 vbest = vlane;

  for (size_t i = 0; i < palette.size; i += 4) {
    i32x4 pr, pg, pb;
    __builtin_memcpy(&pr, &palette.rs[i], sizeof pr);
    __builtin_memcpy(&pg, &palette.gs[i], sizeof pg);
    __builtin_memcpy(&pb, &palette.bs[i], sizeof pb);

    i32x4 dr = c.r - pr;
    i32x4 dg = c.g - pg;
    i32x4 db = c.b - pb;
    i32x4 diff = dr * dr + dg * dg + db * db;

    // Lanes where this group beats the running min (mask is -1 / 0 per lane).
    i32x4 m = diff < vmin;
    vmin = (diff & m) | (vmin & ~m);
    vbest = (vlane & m) | (vbest & ~m);
    vlane += 4;
  }

  int min_diff = vmin[0];
  int best = vbest[0];
  for (int k = 1; k < 4; ++k) {
    if (vmin[k] < min_diff || (vmin[k] == min_diff && vbest[k] < best)) {
      min_diff = vmin[k];
      best = vbest[k];
    }
  }

  return best;
}

#define DEFINE_RECOLOR(name, dither)                                          \
  static void name(Image img, Palette palette) {                              \
    for (int y = 0; y < img.height; ++y) {                                    \
      for (int x = 0; x < img.width; ++x) {                                   \
        const size_t idx = (CHANNELS * (y * img.width + x));                  \
                                                                              \
        Color old_color = {                                                   \
          img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2]};         \
                                                                              \
        int best = find_nearest(palette, old_color);                          \
                                                                              \
        Color error = {                                                       \
          old_color.r - palette.rs[best],                                     \
          old_color.g - palette.gs[best],                                     \
          old_color.b - palette.bs[best],                                     \
        };                                                                    \
                                                                              \
        img.pixels[idx + 0] = palette.rs[best];                               \
        img.pixels[idx + 1] = palette.gs[best];                               \
        img.pixels[idx + 2] = palette.bs[best];                               \
                                                                              \
        dither(img, x, y, error);                                             \
      }                                                                       \
    }                                                                         \
  }

DEFINE_RECOLOR(recolor_none, dither_none)
DEFINE_RECOLOR(recolor_floyd_steinberg, dither_floyd_steinberg)
DEFINE_RECOLOR(recolor_atkinson, dither_atkinson)
DEFINE_RECOLOR(recolor_jjn, dither_jjn)
DEFINE_RECOLOR(recolor_burkes, dither_burkes)
DEFINE_RECOLOR(recolor_sierra, dither_sierra)
DEFINE_RECOLOR(recolor_sierra_lite, dither_sierra_lite)

void recolor(Image img, Palette palette, Ditherer dither) {
  switch (dither) {
    case NONE: recolor_none(img, palette); break;
    case FLOYD_STEINBERG: recolor_floyd_steinberg(img, palette); break;
    case ATKINSON: recolor_atkinson(img, palette); break;
    case JJN: recolor_jjn(img, palette); break;
    case BURKES: recolor_burkes(img, palette); break;
    case SIERRA: recolor_sierra(img, palette); break;
    case SIERRA_LITE: recolor_sierra_lite(img, palette); break;
  }
}
