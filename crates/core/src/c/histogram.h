#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "types.h"

typedef struct {
	u32 threshold;
	u64 *bins0, *bins1;
} HistogramScratch;

typedef struct {
	float *l, *a, *b, *w;
	u32 len;
} Histogram;

extern void build_hist(Image img, HistogramScratch scratch, Histogram *out);

#endif
