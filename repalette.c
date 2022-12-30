#include "repalette.h"

void recolor(
  u8 *pixels, usize width, usize height, Color16 *colors, usize colors_size,
  usize channels
) {
  for (usize y = 0; y < height; ++y) {
    for (usize x = 0; x < width; ++x) {
      const usize idx = (channels * (y * width + x));

      Color16 color = {pixels[idx], pixels[idx + 1], pixels[idx + 2]};
      i32 error[3];

      i32 min_diff = -1;

      for (Color16 *c = colors; c != colors + colors_size; ++c) {
        i32 dr = c->red - color.red;
        i32 dg = c->green - color.green;
        i32 db = c->blue - color.blue;

        i32 diff = dr * dr + dg * dg + db * db;
        if (min_diff == -1 || diff < min_diff) {
          min_diff = diff;

          pixels[idx + 0] = c->red;
          pixels[idx + 1] = c->green;
          pixels[idx + 2] = c->blue;

          error[0] = dr;
          error[1] = dg;
          error[2] = db;
        }
      }
    }
  }
}
