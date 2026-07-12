#include "oklab.h"
#include "extract.h"

typedef struct {
	size_t offset, length;
} Range;

static Oklab weighted_mean(Histogram r, Range range) {
	size_t off = range.offset, len = range.length;
	double sw = 0, sl = 0, sa = 0, sb = 0;
	for (size_t i = off; i < off + len; ++i) {
		double w = r.w[i];
		sw += w;
		sl += w * r.l[i];
		sa += w * r.a[i];
		sb += w * r.b[i];
	}
	if (sw <= 0.0) return (Oklab){.l = 0.0f, .a = 0.0f, .b = 0.0f};
	return (Oklab){
		.l = (float)(sl / sw), .a = (float)(sa / sw), .b = (float)(sb / sw)
	};
}

static inline float linf6(const float *m) {
	float r = 0.0f;
	for (int i = 0; i < 6; ++i) {
		float v = m[i] < 0.0f ? -m[i] : m[i];
		if (v > r) r = v;
	}
	return r;
}

static void covariance(Histogram r, Range range, Oklab mu, float *m) {
	size_t off = range.offset, len = range.length;
	float ll = 0, la = 0, lb = 0, aa = 0, ab = 0, bb = 0;
	for (size_t i = off; i < off + len; ++i) {
		float w = r.w[i];
		float dl = r.l[i] - mu.l, da = r.a[i] - mu.a, db = r.b[i] - mu.b;
		ll += w * dl * dl;
		la += w * dl * da;
		lb += w * dl * db;
		aa += w * da * da;
		ab += w * da * db;
		bb += w * db * db;
	}
	m[0] = ll;
	m[1] = la;
	m[2] = lb;
	m[3] = aa;
	m[4] = ab;
	m[5] = bb;
}

typedef struct {
	float eval;
	Oklab evec;
} Eigen;

static Eigen pca(Histogram r, Range range) {
	Oklab mu = weighted_mean(r, range);

	float m[6];
	covariance(r, range, mu, m);

	float A[6] = {m[0], m[1], m[2], m[3], m[4], m[5]};
	float scale = linf6(A);
	if (scale <= 0)
		return (Eigen){
			.eval = 0.0f,
			.evec = {.l = 1.0f, .a = 0.0f, .b = 0.0f},
		};

	// 16 steps power iteration
	for (int it = 0; it < 16; ++it) {
		for (int d = 0; d < 6; ++d) A[d] /= scale;
		float a = A[0], b = A[1], c = A[2], e = A[3], f = A[4], g = A[5];
		A[0] = a * a + b * b + c * c;
		A[1] = a * b + b * e + c * f;
		A[2] = a * c + b * f + c * g;
		A[3] = b * b + e * e + f * f;
		A[4] = b * c + e * f + f * g;
		A[5] = c * c + f * f + g * g;
		scale = linf6(A);
		if (scale <= 0) break;
	}

	// Eigenvector = column with the greatest diagonal
	float best = A[0];
	Oklab c = {.l = A[0], .a = A[1], .b = A[2]};
	if (A[3] > best) {
		best = A[3];
		c = (Oklab){.l = A[1], .a = A[3], .b = A[4]};
	}
	if (A[5] > best) c = (Oklab){.l = A[2], .a = A[4], .b = A[5]};

	// Rayleigh quotient on the original covariance m.
	float mc0 = m[0] * c.l + m[1] * c.a + m[2] * c.b;
	float mc1 = m[1] * c.l + m[3] * c.a + m[4] * c.b;
	float mc2 = m[2] * c.l + m[4] * c.a + m[5] * c.b;
	float num = c.l * mc0 + c.a * mc1 + c.b * mc2;
	float den = c.l * c.l + c.a * c.a + c.b * c.b;

	return (Eigen){.eval = den > 0.0f ? num / den : 0.0f, .evec = c};
}

static inline void swap_reps(Histogram r, size_t i, size_t j) {
	float tl = r.l[i];
	r.l[i] = r.l[j];
	r.l[j] = tl;
	float ta = r.a[i];
	r.a[i] = r.a[j];
	r.a[j] = ta;
	float tb = r.b[i];
	r.b[i] = r.b[j];
	r.b[j] = tb;
	float tw = r.w[i];
	r.w[i] = r.w[j];
	r.w[j] = tw;
}

typedef struct {
	float val[256];
	Oklab vec[256];
} EigenArray;

static inline void eigen_store(EigenArray *arr, size_t i, Eigen e) {
	arr->val[i] = e.eval;
	arr->vec[i] = e.evec;
}

size_t extract_palette(
	Image img, size_t k, HistogramScratch hist, Histogram reps, u8 *out
) {
	size_t P = (size_t)img.width * img.height;
	if (P == 0) return 0;

	build_hist(img, hist, &reps);
	size_t nbins = reps.len;
	if (nbins == 0) return 0;

	Range ranges[256];
	EigenArray arr;

	ranges[0] = (Range){.offset = 0, .length = nbins};
	eigen_store(&arr, 0, pca(reps, ranges[0]));
	size_t count = 1;
	float eps = arr.val[0] * 1e-6f;	 // relative to the root spread

	while (count < k) {
		int bi = 0;
		float bv = arr.val[0];
		for (size_t i = 1; i < count; ++i)
			if (arr.val[i] > bv) {
				bv = arr.val[i];
				bi = i;
			}
		if (!(arr.val[bi] > eps)) break;	// nothing worth splitting

		size_t o = ranges[bi].offset;
		size_t ln = ranges[bi].length;
		Oklab mu = weighted_mean(reps, ranges[bi]);
		Oklab c = arr.vec[bi];

		// Partition around the centroid plane: project onto the principal axis,
		// negatives left, non-negatives right (Lomuto, deterministic).
		size_t store = o;
		for (size_t i = o; i < o + ln; ++i) {
			float t = (reps.l[i] - mu.l) * c.l + (reps.a[i] - mu.a) * c.a +
								(reps.b[i] - mu.b) * c.b;
			if (t < 0.0f) {
				swap_reps(reps, store, i);
				store++;
			}
		}
		size_t nleft = store - o;
		if (nleft == 0 || nleft == ln) {
			arr.val[bi] = 0.0f;	 // could not separate; mark unsplittable
			continue;
		}

		ranges[bi] = (Range){.offset = o, .length = nleft};
		ranges[count] = (Range){.offset = o + nleft, .length = ln - nleft};
		eigen_store(&arr, bi, pca(reps, ranges[bi]));
		eigen_store(&arr, count, pca(reps, ranges[count]));
		count++;
	}

	for (size_t i = 0; i < count; ++i) {
		oklab_to_rgb(weighted_mean(reps, ranges[i]), out + i * 3);
	}

	return count;
}
