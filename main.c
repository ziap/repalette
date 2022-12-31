#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "repalette.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Color16 from_hex(u32 hex) {
  Color16 c;

  c.r = (hex & 0xff0000) >> 16;
  c.g = (hex & 0x00ff00) >> 8;
  c.b = (hex & 0x0000ff);

  return c;
}

typedef struct {
  const char* input_file;
  const char* output_file;

  Color16* palette;
  usize palette_size;

  Ditherer dither;
} Options;

int parse_dither(const char* param, Ditherer* dither) {
  if (strcmp(param, "none") == 0) {
    *dither = none;
    return 0;
  }
  if (strcmp(param, "floyd-steinberg") == 0) {
    *dither = floyd_steinberg;
    return 0;
  }
  if (strcmp(param, "atkinson") == 0) {
    *dither = atkinson;
    return 0;
  }
  if (strcmp(param, "jjn") == 0) {
    *dither = jjn;
    return 0;
  }
  if (strcmp(param, "burkes") == 0) {
    *dither = burkes;
    return 0;
  }
  fprintf(stderr, "ERROR: Unknown dithering strategy \"%s\"\n", param);
  return 1;
}

int parse_palette(const char* param, Color16** palette, usize* palette_size) {
  *palette_size = 1;
  for (const char* c = param; *c != '\0'; ++c) {
    if (*c == ',') ++*palette_size;
  }
  if (*palette) free(*palette);
  *palette = malloc(*palette_size * sizeof(Color16));
  *palette_size = 0;
  u32 hex = 0;
  for (const char* c = param; *c != '\0'; ++c) {
    if (*c == ',') {
      (*palette)[(*palette_size)++] = from_hex(hex);
      hex = 0;
      continue;
    }
    hex *= 16;
    if (*c >= '0' && *c <= '9') hex += *c - '0';
    else if (*c >= 'a' && *c <= 'f')
      hex += *c - 'a' + 10;
    else if (*c >= 'A' && *c <= 'F')
      hex += *c - 'A' + 10;
    else {
      fprintf(stderr, "ERROR: Unsuppored character '%c' in color\n", *c);
      return 1;
    }
  }
  (*palette)[(*palette_size)++] = from_hex(hex);
  return 0;
}

int parse_arguments(int argc, char** argv, Options* opt) {
  if (argc < 3) {
    fprintf(stderr, "ERROR: Not enough arguments\n");
    return 1;
  }

  opt->input_file = argv[1];
  opt->output_file = argv[2];
  opt->dither = floyd_steinberg;
  opt->palette_size = 0;

  for (int i = 3; i < argc; i += 2) {
    if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dither") == 0) {
      if (i == argc - 1) {
        fprintf(
          stderr, "ERROR: parameter required for argument \"%s\"\n", argv[i]
        );
        return 1;
      }

      if (parse_dither(argv[i + 1], &opt->dither)) return 1;

      continue;
    }
    if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--palette")) {
      if (i == argc - 1) {
        fprintf(
          stderr, "ERROR: parameter required for argument \"%s\"\n", argv[i]
        );
        return 1;
      }

      if (parse_palette(argv[i + 1], &opt->palette, &opt->palette_size))
        return 1;
      continue;
    }
    fprintf(stderr, "ERROR: Unknown argument \"%s\"\n", argv[i]);
    return 1;
  }

  if (opt->palette_size == 0) {
    opt->palette_size = 2;

    if (opt->palette) free(opt->palette);

    opt->palette = malloc(opt->palette_size * sizeof(Color16));
    opt->palette[0] = from_hex(0x000000);
    opt->palette[1] = from_hex(0xffffff);
  }
  return 0;
}

int main(int argc, char** argv) {
  Options opt;

  if (parse_arguments(argc, argv, &opt)) {
    printf("\nUSAGE:  %s <input file> <output file> [options]\n\n", argv[0]);
    printf("OPTIONS:\n");
    printf("  -p, --palette [,COLOR,COLOR...]\n");
    printf("  -d, --dither  {floyd-steinberg|atkinson|jjn|burkes}\n");
    if (opt.palette) free(opt.palette);
    return 1;
  }

  Image img;

  img.pixels =
    stbi_load(opt.input_file, &img.width, &img.height, &img.channels, 3);

  if (img.pixels == NULL) {
    fprintf(
      stderr, "ERROR: Can't load image %s: %s\n", opt.input_file,
      strerror(errno)
    );
    free(opt.palette);
    return errno;
  }

  recolor(img, opt.palette, opt.palette_size, opt.dither);

  stbi_write_png(
    opt.output_file, img.width, img.height, img.channels, img.pixels,
    img.width * img.channels
  );
  if (errno) {
    fprintf(
      stderr, "ERROR: Failed to write image %s: %s\n", opt.output_file,
      strerror(errno)
    );
    free(opt.palette);
    free(img.pixels);
    return errno;
  }

  free(opt.palette);
  free(img.pixels);
  return 0;
}
