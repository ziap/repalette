#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "repalette.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Color from_hex(Hex hex) {
  Color c;

  c.r = (hex & 0xff0000) >> 16;
  c.g = (hex & 0x00ff00) >> 8;
  c.b = (hex & 0x0000ff) >> 0;

  return c;
}

typedef struct {
  const char* input_file;
  const char* output_file;

  Palette palette;
  Ditherer dither;
} Options;

int parse_dither(const char* param, Ditherer* dither) {
  if (strcmp(param, "none") == 0) {
    *dither = NONE;
    return 0;
  }
  if (strcmp(param, "floyd-steinberg") == 0) {
    *dither = FLOYD_STEINBERG;
    return 0;
  }
  if (strcmp(param, "atkinson") == 0) {
    *dither = ATKINSON;
    return 0;
  }
  if (strcmp(param, "jjn") == 0) {
    *dither = JJN;
    return 0;
  }
  if (strcmp(param, "burkes") == 0) {
    *dither = BURKES;
    return 0;
  }
  if (strcmp(param, "sierra") == 0) {
    *dither = SIERRA;
    return 0;
  }
  if (strcmp(param, "sierra-lite") == 0) {
    *dither = SIERRA_LITE;
    return 0;
  }
  fprintf(stderr, "ERROR: Unknown dithering strategy \"%s\"\n", param);
  return 1;
}

int parse_palette(const char *param, Palette *palette) {
  size_t size = 1;
  for (const char* c = param; *c != '\0'; ++c) {
    if (*c == ',') ++size;
  }

  palette->size = (size + 3) / 4 * 4;

  if (palette->buffer) free(palette->buffer);
  palette->buffer = malloc(3 * palette->size * sizeof(int));

  int *rs = palette->buffer + 0 * palette->size;
  int *gs = palette->buffer + 1 * palette->size;
  int *bs = palette->buffer + 2 * palette->size;

  size = 0;
  Hex hex = 0;
  for (const char* c = param; *c != '\0'; ++c) {
    if (*c == ',') {
      Color color = from_hex(hex);
      rs[size] = color.r;
      gs[size] = color.g;
      bs[size] = color.b;

      size += 1;
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

  Color color = from_hex(hex);
  for (size_t i = size; i < palette->size; ++i) {
    rs[i] = color.r;
    gs[i] = color.g;
    bs[i] = color.b;
  }

  palette->rs = rs;
  palette->gs = gs;
  palette->bs = bs;
  return 0;
}

int parse_arguments(int argc, char** argv, Options* opt) {
  if (argc > 1) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      printf("USAGE:\n");
      printf("  %s -h | --help\n", argv[0]);
      printf("  %s <input file> <output file> [options]\n", argv[0]);
      printf("\n");
      printf("OPTIONS:\n");
      printf("  -p, --palette COLOR[,COLOR...]\n");
      printf(
        "  -d, --dither  none | floyd-steinberg | atkinson | jjn | burkes | "
        "sierra | sierra-lite\n"
      );
      return 1;
    }
  } else {
    fprintf(stderr, "ERROR: input file not provided\n");
    return -1;
  }
  if (argc <= 2) {
    fprintf(stderr, "ERROR: output file not provided\n");
    return -1;
  }

  *opt = (Options) {
    .input_file = argv[1],
    .output_file = argv[2],
    .dither = FLOYD_STEINBERG,

    .palette = (Palette) {
      .size = 0,
      .buffer = NULL,
      .rs = NULL,
      .gs = NULL,
      .bs = NULL,
    },
  };

  for (int i = 3; i < argc; i += 2) {
    if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dither") == 0) {
      if (i == argc - 1) {
        fprintf(
          stderr, "ERROR: parameter required for argument \"%s\"\n", argv[i]
        );
        return -1;
      }

      if (parse_dither(argv[i + 1], &opt->dither)) return -1;

      continue;
    }
    if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--palette") == 0) {
      if (i == argc - 1) {
        fprintf(
          stderr, "ERROR: parameter required for argument \"%s\"\n", argv[i]
        );
        return -1;
      }

      if (parse_palette(argv[i + 1], &opt->palette))
        return -1;
      continue;
    }
    fprintf(stderr, "ERROR: Unknown argument \"%s\"\n", argv[i]);
    return -1;
  }

  if (opt->palette.size == 0) {
    opt->palette.size = 4;

    if (opt->palette.buffer) free(opt->palette.buffer);
    opt->palette.buffer = malloc(12 * sizeof(int));

    int *rs = opt->palette.buffer + 0;
    int *gs = opt->palette.buffer + 4;
    int *bs = opt->palette.buffer + 8;

    rs[0] = 0x00;
    gs[0] = 0x00;
    bs[0] = 0x00;

    for (size_t i = 1; i < opt->palette.size; ++i) {
      rs[i] = 0xff;
      gs[i] = 0xff;
      bs[i] = 0xff;
    }

    opt->palette.rs = rs;
    opt->palette.gs = gs;
    opt->palette.bs = bs;
  }
  return 0;
}

int main(int argc, char** argv) {
  Options opt = {0};

  int status = parse_arguments(argc, argv, &opt);
  if (status != 0) {
    if (opt.palette.buffer) free(opt.palette.buffer);
    return status < 0;
  }

  Image img;

  int n;
  img.pixels = stbi_load(opt.input_file, &img.width, &img.height, &n, CHANNELS);

  if (img.pixels == NULL) {
    fprintf(
      stderr, "ERROR: Can't load image %s: %s\n", opt.input_file,
      strerror(errno)
    );
    free(opt.palette.buffer);
    return errno;
  }

  recolor(img, opt.palette, opt.dither);

  stbi_write_png(
    opt.output_file, img.width, img.height, CHANNELS, img.pixels,
    img.width * CHANNELS
  );
  if (errno) {
    fprintf(
      stderr, "ERROR: Failed to write image %s: %s\n", opt.output_file,
      strerror(errno)
    );
    free(opt.palette.buffer);
    free(img.pixels);
    return errno;
  }

  free(opt.palette.buffer);
  free(img.pixels);
  return 0;
}
