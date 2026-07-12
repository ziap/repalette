#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "types.h"

typedef struct {
	size_t threshold;
	size_t *bins0, *bins1;
	u8 *work, *aux;
} HistogramScratch;

typedef struct {
	float *l, *a, *b, *w;
	size_t len;
} Histogram;

extern void build_hist(Image img, HistogramScratch scratch, Histogram *out);

#endif
