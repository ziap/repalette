#include "repalette.h"

static void update_pixel(Image img, int x, int y, Color err, int mul, int div) {
  if (x < 0 || x >= img.width) return;
  size_t idx = img.channels * (y * img.width + x);

  // TODO: Vectorize pixel update
  int r = img.pixels[idx + 0] + err.r * mul / div;
  int g = img.pixels[idx + 1] + err.g * mul / div;
  int b = img.pixels[idx + 2] + err.b * mul / div;

  r = r < 0 ? 0 : (r > 255 ? 255 : r);
  g = g < 0 ? 0 : (g > 255 ? 255 : g);
  b = b < 0 ? 0 : (b > 255 ? 255 : b);

  img.pixels[idx + 0] = r;
  img.pixels[idx + 1] = g;
  img.pixels[idx + 2] = b;
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

void recolor(Image img, Color *palette, size_t palette_size, Ditherer dither) {
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const size_t idx = (img.channels * (y * img.width + x));

      Color old_color = {
        img.pixels[idx], img.pixels[idx + 1], img.pixels[idx + 2]};

      Color error;

      int min_diff = -1;

      // TODO: Vectorize best color selection
      for (Color *color = palette; color != palette + palette_size; ++color) {
        int dr = old_color.r - color->r;
        int dg = old_color.g - color->g;
        int db = old_color.b - color->b;

        int diff = dr * dr + dg * dg + db * db;
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

      // TODO: Move this switch outside the for loop
      switch (dither) {
        case NONE: break;
        case FLOYD_STEINBERG: dither_floyd_steinberg(img, x, y, error); break;
        case ATKINSON: dither_atkinson(img, x, y, error); break;
        case JJN: dither_jjn(img, x, y, error); break;
        case BURKES: dither_burkes(img, x, y, error); break;
        case SIERRA: dither_sierra(img, x, y, error); break;
        case SIERRA_LITE: dither_sierra_lite(img, x, y, error); break;
      }
    }
  }
}
