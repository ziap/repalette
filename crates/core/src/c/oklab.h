#ifndef OKLAB_H
#define OKLAB_H

#include <stdint.h>

typedef struct {
	float r, g, b;
} Frgb;

typedef struct {
	float l, a, b;
} Oklab;

extern Oklab rgb_to_oklab(Frgb c);
extern void oklab_to_rgb(Oklab c, uint8_t out[3]);

#endif
