#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "repalette_simd.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define INPUT_FILE "test.jpg"
#define WARMUP 3
#define RUNS 20

// Scalar AoS reference: `recolor` from repalette.c, linked into this binary.
// Its Image keeps a `channels` field, so declare a layout-matching struct for
// the call. `Color`/`Ditherer` are identical to repalette.h's, so they're
// reused from repalette_simd.h.
typedef struct {
  u8* pixels;
  int width;
  int height;
  int channels;
} RefImage;

extern void recolor(RefImage, Color*, size_t, Ditherer);

static const char* dither_names[] = {
  [NONE] = "none",
  [FLOYD_STEINBERG] = "floyd-steinberg",
  [ATKINSON] = "atkinson",
  [JJN] = "jjn",
  [BURKES] = "burkes",
  [SIERRA] = "sierra",
  [SIERRA_LITE] = "sierra-lite",
};

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

int main(void) {
  /* 13 Nord colors, padded to 16 (a multiple of 4) by repeating the last
     color so the SIMD search needs no scalar tail. Duplicate entries are
     harmless: they yield the same distance and the same output color. */
  Palette palette = {
    .rs = (int[]){0x3b, 0xbf, 0xa3, 0xeb, 0x81, 0xb4, 0x88, 0xe5, 0x4c, 0x8f, 0xec, 0xd8, 0x2e, 0x2e, 0x2e, 0x2e},
    .gs = (int[]){0x42, 0x61, 0xbe, 0xcb, 0xa1, 0x8e, 0xc0, 0xe9, 0x56, 0xbc, 0xef, 0xde, 0x34, 0x34, 0x34, 0x34},
    .bs = (int[]){0x52, 0x6a, 0x8c, 0x8b, 0xc1, 0xad, 0xd0, 0xf0, 0x6a, 0xbb, 0xf4, 0xe9, 0x40, 0x40, 0x40, 0x40},
  };
  size_t palette_size = 16;

  Image img;
  int n;
  u8* original =
    stbi_load(INPUT_FILE, &img.width, &img.height, &n, CHANNELS);

  if (original == NULL) {
    fprintf(
      stderr, "ERROR: Can't load image %s: %s\n", INPUT_FILE, strerror(errno)
    );
    return errno;
  }

  size_t size = (size_t)img.width * img.height * CHANNELS;
  img.pixels = malloc(size);

  printf(
    "%s: %dx%d, palette %zu colors, %d runs each\n\n", INPUT_FILE, img.width,
    img.height, palette_size, RUNS
  );
  printf("%-16s %12s %12s\n", "DITHER", "MEAN (ms)", "STD (ms)");

  size_t num_dithers = sizeof(dither_names) / sizeof(dither_names[0]);
  for (size_t dither = 0; dither < num_dithers; ++dither) {
    for (int w = 0; w < WARMUP; ++w) {
      memcpy(img.pixels, original, size);
      recolor_simd(img, palette, palette_size, dither);
    }

    double times[RUNS];
    double sum = 0;
    for (int r = 0; r < RUNS; ++r) {
      memcpy(img.pixels, original, size);

      double start = now_ms();
      recolor_simd(img, palette, palette_size, dither);
      times[r] = now_ms() - start;

      sum += times[r];
    }

    double mean = sum / RUNS;
    double var = 0;
    for (int r = 0; r < RUNS; ++r) var += (times[r] - mean) * (times[r] - mean);
    double std = sqrt(var / (RUNS - 1));

    printf("%-16s %12.3f %12.3f\n", dither_names[dither], mean, std);
  }

  // Correctness: diff each dither's output against the scalar AoS reference.
  Color ref_palette[16];
  for (size_t i = 0; i < palette_size; ++i)
    ref_palette[i] = (Color){palette.rs[i], palette.gs[i], palette.bs[i]};

  u8* ref = malloc(size);
  size_t total_px = (size_t)img.width * img.height;

  printf("\n%-16s %14s %10s\n", "DITHER", "WRONG PX", "%");
  for (size_t dither = 0; dither < num_dithers; ++dither) {
    memcpy(img.pixels, original, size);
    recolor_simd(img, palette, palette_size, dither);

    memcpy(ref, original, size);
    RefImage rimg = {ref, img.width, img.height, CHANNELS};
    recolor(rimg, ref_palette, palette_size, dither);

    size_t wrong = 0;
    for (size_t p = 0; p < total_px; ++p) {
      size_t b = p * CHANNELS;
      if (img.pixels[b + 0] != ref[b + 0] || img.pixels[b + 1] != ref[b + 1] ||
          img.pixels[b + 2] != ref[b + 2])
        ++wrong;
    }
    printf(
      "%-16s %14zu %9.4f%%\n", dither_names[dither], wrong,
      100.0 * wrong / total_px
    );
  }

  free(ref);
  free(img.pixels);
  free(original);
  return 0;
}
