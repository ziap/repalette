#include "oklab.h"

#include <math.h>

// OKLab transform (Ottosson). Forward for bin centers, inverse for centroids.
// Only used on the <= THRESHOLD bin reps / K centroids, never per pixel, so the
// cbrt/pow cost is negligible.

static inline float srgb_to_linear(float c) {
	return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static inline float linear_to_srgb(float c) {
	return c <= 0.0031308f ? 12.92f * c : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static inline uint8_t quant(float linear) {
	float v = linear_to_srgb(linear) * 255.0f;
	if (v <= 0.0f) return 0;
	if (v >= 255.0f) return 255;
	return (uint8_t)(v + 0.5f);
}

// RGB (each channel in [0, 255]) -> OKLab.
Oklab rgb_to_oklab(Frgb c) {
	float lr = srgb_to_linear(c.r / 255.0f);
	float lg = srgb_to_linear(c.g / 255.0f);
	float lb = srgb_to_linear(c.b / 255.0f);

	float l = 0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb;
	float m = 0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb;
	float s = 0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb;

	float l_ = cbrtf(l), m_ = cbrtf(m), s_ = cbrtf(s);

	return (Oklab){
		.l = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
		.a = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
		.b = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_,
	};
}

// OKLab -> sRGB triple in out[0..2].
void oklab_to_rgb(Oklab c, uint8_t out[3]) {
	float l_ = c.l + 0.3963377774f * c.a + 0.2158037573f * c.b;
	float m_ = c.l - 0.1055613458f * c.a - 0.0638541728f * c.b;
	float s_ = c.l - 0.0894841775f * c.a - 1.2914855480f * c.b;

	float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;

	out[0] = quant(+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s);
	out[1] = quant(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s);
	out[2] = quant(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s);
}
