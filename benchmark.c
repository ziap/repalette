#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "repalette.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define INPUT_FILE "test.jpg"
#define CHANNELS 3
#define WARMUP 3
#define RUNS 20

Color from_hex(Hex hex) {
  Color c;

  c.r = (hex & 0xff0000) >> 16;
  c.g = (hex & 0x00ff00) >> 8;
  c.b = (hex & 0x0000ff);

  return c;
}

static const char* dither_names[] = {
  [NONE] = "none",
  [FLOYD_STEINBERG] = "floyd-steinberg",
  [ATKINSON] = "atkinson",
  [JJN] = "jjn",
  [BURKES] = "burkes",
  [SIERRA] = "sierra",
  [SIERRA_LITE] = "sierra-lite",
};

static const Hex palette_hex[] = {
  0x3b4252, 0xbf616a, 0xa3be8c, 0xebcb8b, 0x81a1c1, 0xb48ead, 0x88c0d0,
  0xe5e9f0, 0x4c566a, 0x8fbcbb, 0xeceff4, 0xd8dee9, 0x2e3440,
};

static Color palette[sizeof(palette_hex) / sizeof(palette_hex[0])];

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

int main(void) {
  size_t palette_size = sizeof(palette) / sizeof(palette[0]);
  for (size_t i = 0; i < palette_size; ++i) palette[i] = from_hex(palette_hex[i]);

  Image img;
  u8* original =
    stbi_load(INPUT_FILE, &img.width, &img.height, &img.channels, CHANNELS);

  if (original == NULL) {
    fprintf(
      stderr, "ERROR: Can't load image %s: %s\n", INPUT_FILE, strerror(errno)
    );
    return errno;
  }

  img.channels = CHANNELS;
  size_t size = (size_t)img.width * img.height * img.channels;
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
      recolor(img, palette, palette_size, dither);
    }

    double times[RUNS];
    double sum = 0;
    for (int r = 0; r < RUNS; ++r) {
      memcpy(img.pixels, original, size);

      double start = now_ms();
      recolor(img, palette, palette_size, dither);
      times[r] = now_ms() - start;

      sum += times[r];
    }

    double mean = sum / RUNS;
    double var = 0;
    for (int r = 0; r < RUNS; ++r) var += (times[r] - mean) * (times[r] - mean);
    double std = sqrt(var / (RUNS - 1));

    printf("%-16s %12.3f %12.3f\n", dither_names[dither], mean, std);
  }

  free(img.pixels);
  free(original);
  return 0;
}
