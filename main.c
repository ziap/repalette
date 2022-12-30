#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repalette.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Color16 from_hex(u32 hex) {
  Color16 c;

  c.red = (hex & 0xff0000) >> 16;
  c.green = (hex & 0x00ff00) >> 8;
  c.blue = (hex & 0x0000ff);

  return c;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "ERROR: Not enough arguments\n");
    printf("USAGE: %s <input> <output>\n", argv[0]);
    return 1;
  }

  i32 width, height, channels;
  u8* img = stbi_load(argv[1], &width, &height, &channels, 3);

  if (img == NULL) {
    fprintf(
      stderr, "ERROR: Can't load image %s: %s\n", argv[1], strerror(errno)
    );
    return errno;
  }

  Color16* palette = malloc(16 * sizeof(Color16));

  palette[0] = from_hex(0x2E3440);
  palette[1] = from_hex(0x3B4252);
  palette[2] = from_hex(0x434C5E);
  palette[3] = from_hex(0x4C566A);
  palette[4] = from_hex(0xD8DEE9);
  palette[5] = from_hex(0xE5E9F0);
  palette[6] = from_hex(0xECEFF4);
  palette[7] = from_hex(0x8FBCBB);
  palette[8] = from_hex(0x88C0D0);
  palette[9] = from_hex(0x81A1C1);
  palette[10] = from_hex(0x5E81AC);
  palette[11] = from_hex(0xBF616A);
  palette[12] = from_hex(0xD08770);
  palette[13] = from_hex(0xEBCB8B);
  palette[14] = from_hex(0xA3BE8C);
  palette[15] = from_hex(0xB48EAD);

  recolor(img, width, height, palette, 16, channels);

  stbi_write_png(argv[2], width, height, channels, img, width * channels);
  if (errno) {
    fprintf(
      stderr, "ERROR: Failed to write image %s: %s\n", argv[2], strerror(errno)
    );
    return errno;
  }

  free(palette);
  free(img);
  return 0;
}
