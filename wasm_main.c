#include "repalette.h"

#define export extern __attribute__((visibility("default")))
#define PAGE 65536

static usize memory_capacity = 0;
static u8 *memory = 0;

static void resize(usize new_size) {
  if (new_size > memory_capacity) {
    if (memory_capacity == 0)
      memory = (u8 *)(__builtin_wasm_memory_size(0) * PAGE);

    usize pages = (new_size - memory_capacity + PAGE - 1) / PAGE;
    __builtin_wasm_memory_grow(0, pages);
    memory_capacity += pages * PAGE;
  }
}

usize palette_size;

export void palette_clear() { palette_size = 0; }

export void palette_add(u8 red, u8 green, u8 blue) {
  resize((palette_size + 1) * sizeof(Color));

  Color c = {red, green, blue};
  ((Color *)memory)[palette_size++] = c;
}

export u8 *get_pixels(i32 width, i32 height) {
  resize(4 * width * height + palette_size * sizeof(Color));
  return memory + palette_size * sizeof(Color);
}

export void update_canvas(i32 width, i32 height) {
  if (palette_size == 0) return;

  u8 *pixels = memory + palette_size * sizeof(Color);
  Color *palette = (Color *)memory;

  Image img = {pixels, width, height, 4};

  recolor(img, palette, palette_size, burkes);
}
