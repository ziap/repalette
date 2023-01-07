#include "repalette.h"

static void update_pixel(Image img, i32 x, i32 y, Color err, i32 mul, i32 div) {
  if (x < 0 || x >= img.width) return;
  usize idx = img.channels * (y * img.width + x);

  i32 r = img.pixels[idx + 0] + err.r * mul / div;
  i32 g = img.pixels[idx + 1] + err.g * mul / div;
  i32 b = img.pixels[idx + 2] + err.b * mul / div;

  r = r < 0 ? 0 : (r > 255 ? 255 : r);
  g = g < 0 ? 0 : (g > 255 ? 255 : g);
  b = b < 0 ? 0 : (b > 255 ? 255 : b);

  img.pixels[idx + 0] = r;
  img.pixels[idx + 1] = g;
  img.pixels[idx + 2] = b;
}

static void dither_floyd_steinberg(Image img, i32 x, i32 y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 7, 16);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 1, y + 1, err, 3, 16);
  update_pixel(img, x + 0, y + 1, err, 5, 16);
  update_pixel(img, x + 1, y + 1, err, 1, 16);
}

static void dither_atkinson(Image img, i32 x, i32 y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 1, 8);
  update_pixel(img, x + 2, y + 0, err, 1, 8);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 1, y + 1, err, 1, 8);
  update_pixel(img, x + 0, y + 1, err, 1, 8);
  update_pixel(img, x + 1, y + 1, err, 1, 8);
  if (y + 2 >= img.height) return;
  update_pixel(img, x + 1, y + 2, err, 1, 8);
}

static void dither_jjn(Image img, i32 x, i32 y, Color err) {
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

static void dither_burkes(Image img, i32 x, i32 y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 8, 32);
  update_pixel(img, x + 2, y + 0, err, 4, 32);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 2, y + 1, err, 2, 32);
  update_pixel(img, x - 1, y + 1, err, 4, 32);
  update_pixel(img, x + 0, y + 1, err, 8, 32);
  update_pixel(img, x + 1, y + 1, err, 4, 32);
  update_pixel(img, x + 2, y + 1, err, 2, 32);
}

static void dither_sierra(Image img, i32 x, i32 y, Color err) {
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

static void dither_sierra_lite(Image img, i32 x, i32 y, Color err) {
  update_pixel(img, x + 1, y + 0, err, 2, 4);
  if (y + 1 >= img.height) return;
  update_pixel(img, x - 1, y + 1, err, 1, 4);
  update_pixel(img, x + 0, y + 1, err, 1, 4);
}

void recolor(Image img, Color *palette, usize palette_size, Ditherer dither) {
  for (i32 y = 0; y < img.height; ++y) {
    for (i32 x = 0; x < img.width; ++x) {
      const usize idx = (img.channels * (y * img.width + x));

      Color old_color = {
        img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2]};

      Color error;

      i32 min_diff = -1;

      for (Color *color = palette; color != palette + palette_size; ++color) {
        i32 dr = old_color.r - color->r;
        i32 dg = old_color.g - color->g;
        i32 db = old_color.b - color->b;

        i32 diff = dr * dr + dg * dg + db * db;
        if (min_diff == -1 || diff < min_diff) {
          min_diff = diff;

          img.pixels[idx + 0] = color->r;
          img.pixels[idx + 1] = color->g;
          img.pixels[idx + 2] = color->b;

          error.r = dr;
          error.g = dg;
          error.b = db;
        }
      }

      switch (dither) {
        case none: break;
        case floyd_steinberg: dither_floyd_steinberg(img, x, y, error); break;
        case atkinson: dither_atkinson(img, x, y, error); break;
        case jjn: dither_jjn(img, x, y, error); break;
        case burkes: dither_burkes(img, x, y, error); break;
        case sierra: dither_sierra(img, x, y, error); break;
        case sierra_lite: dither_sierra_lite(img, x, y, error); break;
      }
    }
  }
}
