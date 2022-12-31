#include "repalette.h"

static void update_pixel(
  Image img, i32 x, i32 y, i32 *error, i32 mul, i32 div
) {
  if (x < 0 || x >= img.width || y < 0 || y >= img.height) return;

  usize idx = img.channels * (y * img.width + x);

  int r = img.pixels[idx + 0] + error[0] * mul / div;
  int g = img.pixels[idx + 1] + error[1] * mul / div;
  int b = img.pixels[idx + 2] + error[2] * mul / div;

  r = r < 0 ? 0 : (r > 255 ? 255 : r);
  g = g < 0 ? 0 : (g > 255 ? 255 : g);
  b = b < 0 ? 0 : (b > 255 ? 255 : b);

  img.pixels[idx + 0] = r;
  img.pixels[idx + 1] = g;
  img.pixels[idx + 2] = b;
}

static void dither_floyd_steinberg(Image img, i32 x, i32 y, i32 *error) {
  update_pixel(img, x + 1, y + 0, error, 7, 16);
  update_pixel(img, x - 1, y + 1, error, 3, 16);
  update_pixel(img, x + 0, y + 1, error, 5, 16);
  update_pixel(img, x + 1, y + 1, error, 1, 16);
}

static void dither_atkinson(Image img, i32 x, i32 y, i32 *error) {
  update_pixel(img, x + 1, y + 0, error, 1, 8);
  update_pixel(img, x + 2, y + 0, error, 1, 8);
  update_pixel(img, x - 1, y + 1, error, 1, 8);
  update_pixel(img, x + 0, y + 1, error, 1, 8);
  update_pixel(img, x + 1, y + 1, error, 1, 8);
  update_pixel(img, x + 1, y + 2, error, 1, 8);
}

static void dither_jjn(Image img, i32 x, i32 y, i32 *error) {
  update_pixel(img, x + 1, y + 0, error, 7, 48);
  update_pixel(img, x + 2, y + 0, error, 5, 48);
  update_pixel(img, x - 2, y + 1, error, 3, 48);
  update_pixel(img, x - 1, y + 1, error, 5, 48);
  update_pixel(img, x + 0, y + 1, error, 7, 48);
  update_pixel(img, x + 1, y + 1, error, 5, 48);
  update_pixel(img, x + 2, y + 1, error, 3, 48);
  update_pixel(img, x - 2, y + 2, error, 1, 48);
  update_pixel(img, x - 1, y + 2, error, 3, 48);
  update_pixel(img, x + 0, y + 2, error, 5, 48);
  update_pixel(img, x + 1, y + 2, error, 3, 48);
  update_pixel(img, x + 2, y + 2, error, 1, 48);
}

static void dither_burkes(Image img, i32 x, i32 y, i32 *error) {
  update_pixel(img, x + 1, y + 0, error, 8, 32);
  update_pixel(img, x + 2, y + 0, error, 4, 32);
  update_pixel(img, x - 2, y + 1, error, 2, 32);
  update_pixel(img, x - 1, y + 1, error, 4, 32);
  update_pixel(img, x + 0, y + 1, error, 8, 32);
  update_pixel(img, x + 1, y + 1, error, 4, 32);
  update_pixel(img, x + 2, y + 1, error, 2, 32);
}

void recolor(Image img, Color16 *palette, usize palette_size, Ditherer dither) {
  for (i32 y = 0; y < img.height; ++y) {
    for (i32 x = 0; x < img.width; ++x) {
      const usize idx = (img.channels * (y * img.width + x));

      Color16 old_color = {
        img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2]};
      i32 error[3];

      i32 min_diff = -1;

      for (Color16 *color = palette; color != palette + palette_size; ++color) {
        i32 dr = old_color.r - color->r;
        i32 dg = old_color.g - color->g;
        i32 db = old_color.b - color->b;

        i32 diff = dr * dr + dg * dg + db * db;
        if (min_diff == -1 || diff < min_diff) {
          min_diff = diff;

          img.pixels[idx + 0] = color->r;
          img.pixels[idx + 1] = color->g;
          img.pixels[idx + 2] = color->b;

          error[0] = dr;
          error[1] = dg;
          error[2] = db;
        }
      }
      switch (dither) {
        case none: break;
        case floyd_steinberg: dither_floyd_steinberg(img, x, y, error); break;
        case atkinson: dither_atkinson(img, x, y, error); break;
        case jjn: dither_jjn(img, x, y, error); break;
        case burkes: dither_burkes(img, x, y, error); break;
      }
    }
  }
}
