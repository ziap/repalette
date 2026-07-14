#include "dither.h"

#define export extern __attribute__((visibility("default")))
#define PAGE 65536

static size_t memory_capacity = 0;
static u8 *memory = 0;

static void resize(size_t new_size) {
	if (new_size > memory_capacity) {
		if (memory_capacity == 0)
			memory = (u8 *)(__builtin_wasm_memory_size(0) * PAGE);

		size_t pages = (new_size - memory_capacity + PAGE - 1) / PAGE;
		__builtin_wasm_memory_grow(0, pages);
		memory_capacity += pages * PAGE;
	}
}

struct {
	u32 size;
	u32 capacity;

	i32 *rs;
	i32 *gs;
	i32 *bs;
} state;

export u32 ditherer_count(void) { return DITHER_COUNT; }

export const char *ditherer_display(Ditherer ditherer) {
	return dither_display_names[ditherer];
}

export void palette_init(u32 capacity) {
	u32 padded_capacity = (capacity + 3) / 4 * 4;
	state.size = 0;
	state.capacity = padded_capacity;

	resize(padded_capacity * 3 * sizeof(i32));
	state.rs = (i32 *)memory + 0 * padded_capacity;
	state.gs = (i32 *)memory + 1 * padded_capacity;
	state.bs = (i32 *)memory + 2 * padded_capacity;
}

export void palette_add(u8 red, u8 green, u8 blue) {
	state.rs[state.size] = red;
	state.gs[state.size] = green;
	state.bs[state.size] = blue;

	state.size += 1;
}

export u8 *get_pixels(u32 width, u32 height) {
	size_t offset = state.capacity * 3 * sizeof(i32);
	resize((size_t)4 * width * height + offset);
	return memory + offset;
}

export void update_canvas(u32 width, u32 height, Ditherer ditherer) {
	if (state.size == 0) return;
	for (u32 i = state.size; i < state.capacity; ++i) {
		state.rs[i] = state.rs[state.size - 1];
		state.gs[i] = state.gs[state.size - 1];
		state.bs[i] = state.bs[state.size - 1];
	}

	u8 *pixels = memory + state.capacity * 3 * sizeof(i32);
	Palette palette = {
		.size = state.capacity,
		.rs = state.rs,
		.gs = state.gs,
		.bs = state.bs,
	};

	Image img = {pixels, width, height};

	recolor(img, palette, ditherer);
}
