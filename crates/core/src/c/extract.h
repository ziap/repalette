#ifndef EXTRACT_H
#define EXTRACT_H

#include "histogram.h"

extern size_t extract_palette(
	Image img, size_t k, HistogramScratch hist, Histogram reps, u8 *out
);

#endif
