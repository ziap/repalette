#ifndef EXTRACT_H
#define EXTRACT_H

#include "histogram.h"

extern u32 extract_palette(
	Image img, u32 k, HistogramScratch hist, Histogram reps, u8 *out
);

#endif
