#include "oklab.h"

// Source:
// https://sepwww.stanford.edu/data/media/public/oldsep/stew/ForDaveHale/sep148stew1.pdf
static inline float fast_cbrt(float x) {
	const float onethird = 1.0f / 3.0f;
	const float fourthirds = 4.0f / 3.0f;

	float thirdx = x * onethird;
	union {
		uint32_t ix;
		float fx;
	} z;

	z.fx = x;
	z.ix = 0x54a21d2a - z.ix / 3;
	float y = z.fx;

	y *= fourthirds - thirdx * y * y * y;
	y *= fourthirds - thirdx * y * y * y;
	y *= fourthirds - thirdx * y * y * y;
	return y * y * x;
}

// srgb_to_linear: exact linear segment below the sRGB threshold, 5/5 Remez
// minimax rational (absolute, ~2.7e-9 over [0.04045, 1]) above.
static inline float srgb_to_linear(float fc) {
	if (fc <= 0.04045f) return fc / 12.92f;
	double u = (double)fc;
	double p = 6.83538057954819896;
	p = p * u + 12.2085894395117097;
	p = p * u + 5.25367207907485761;
	p = p * u + 0.762688391405956183;
	p = p * u + 0.0429712638191277381;
	p = p * u + 0.000833998108215813823;
	double q = 0.0205513280263164009;
	q = q * u + -0.242152119179972029;
	q = q * u + 3.36551634066526532;
	q = q * u + 13.0407745784694425;
	q = q * u + 7.91944563316124088;
	q = q * u + 1.0;
	return (float)(p / q);
}

// linear_to_srgb: exact linear segment below the sRGB threshold, 5/5 Remez
// minimax rational (absolute, ~3.6e-6 over [0.0031308, 1]) above. The branch
// also covers 0/negatives, so the rational only runs where it is pole-free.
static inline float linear_to_srgb(float fc) {
	if (fc <= 0.0031308f) return 12.92f * fc;
	double u = (double)fc;
	double p = 241262.022993526948;
	p = p * u + 479970.948495260698;
	p = p * u + 122530.069429288926;
	p = p * u + 5100.36843802263403;
	p = p * u + 25.2985242058552079;
	p = p * u + -0.0221751769541056964;
	double q = 70548.2611011445298;
	q = q * u + 471560.259452460904;
	q = q * u + 278914.196370766501;
	q = q * u + 27400.9231571624867;
	q = q * u + 467.126443353403846;
	q = q * u + 1.0;
	return (float)(p / q);
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

	float l_ = fast_cbrt(l), m_ = fast_cbrt(m), s_ = fast_cbrt(s);

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
