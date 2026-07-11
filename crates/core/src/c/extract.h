#ifndef EXTRACT_H
#define EXTRACT_H

#include <stddef.h>
#include <stdint.h>

#include "repalette.h"

typedef struct {
	int threshold;
	uint32_t *bins0, *bins1;
	u8 *work, *aux;
} HistogramScratch;

typedef struct {
	float *l, *a, *b, *w;
	size_t len;
} OklabHist;

extern int extract_palette(
	Image img, int k, HistogramScratch hist, OklabHist reps, u8 *out
);

#endif
