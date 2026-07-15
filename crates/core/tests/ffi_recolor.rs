use std::collections::HashSet;

use repalette_core::{Image, Palette, process, process_index};

const DITHERERS: &[&str] = &[
	"none", "fs", "atkinson", "jjn", "burkes", "sierra32", "sierra4",
];

// TODO: Make an RNG crate
fn random_colors(seed: usize, n: usize) -> Vec<[u8; 3]> {
	const MUL: u64 = 6364136223846793005;
	const INC: u64 = 1442695040888963407;

	const fn jump_params(n: u64) -> (u64, u64) {
		let mut result: (u64, u64) = (1, 0);
		let mut a: (u64, u64) = (MUL, 1);

		let mut b = n;
		while b > 0 {
			if b & 1 != 0 {
				result = (
					result.0.wrapping_mul(a.0),
					result.0.wrapping_mul(a.1).wrapping_add(result.1),
				)
			}

			a = (
				a.0.wrapping_mul(a.0),
				a.0.wrapping_mul(a.1).wrapping_add(a.1),
			);

			b >>= 1;
		}

		result
	}

	let mut s = INC;
	let (mul, inc) = const { jump_params(0x9e3779b97f4a7c15) };
	for _ in 0..seed {
		s = s.wrapping_mul(mul).wrapping_add(inc)
	}

	let mut next = || {
		s = s.wrapping_mul(MUL).wrapping_add(INC);
		(s >> 56) as u8
	};

	(0..n).map(|_| [next(), next(), next()]).collect()
}

fn random_image(seed: usize, w: u32, h: u32) -> Image {
	let mut pixels = Vec::with_capacity((w * h * 4) as usize);
	for [r, g, b] in random_colors(seed, (w * h) as usize) {
		pixels.extend_from_slice(&[r, g, b, 255]);
	}
	Image {
		width: w,
		height: h,
		pixels,
	}
}

/// Reference nearest-colour search: squared-Euclidean RGB, lowest index on ties
/// (matching `find_nearest`'s strict-`<` reduction).
fn nearest(colors: &[[u8; 3]], px: [u8; 3]) -> usize {
	let mut best = 0;
	let mut best_dist = i64::MAX;
	for (i, &[r, g, b]) in colors.iter().enumerate() {
		let dr = px[0] as i64 - r as i64;
		let dg = px[1] as i64 - g as i64;
		let db = px[2] as i64 - b as i64;
		let dist = dr * dr + dg * dg + db * db;
		if dist < best_dist {
			best_dist = dist;
			best = i;
		}
	}
	best
}

#[test]
fn output_is_always_a_palette_color() {
	let colors = random_colors(0, 17); // deliberately not a multiple of 4
	let palette = Palette::from_colors(&colors);
	let set: HashSet<[u8; 3]> = colors.iter().copied().collect();

	for (i, &d) in DITHERERS.iter().enumerate() {
		let mut img = random_image(1 + i, 23, 11);
		process(&mut img, &palette, d);

		for px in img.pixels.chunks_exact(4) {
			let c = [px[0], px[1], px[2]];
			assert!(
				set.contains(&c),
				"ditherer {d}: {c:?} is not a palette color"
			);
		}
	}
}

#[test]
fn nearest_matches_brute_force() {
	let mut seed = 0;

	for &n in &[1usize, 2, 3, 5, 8, 13, 31, 200] {
		let colors = random_colors(seed, n);
		let palette = Palette::from_colors(&colors);
		seed += 1;

		let src = random_image(seed, 29, 17);
		seed += 1;

		let mut got = src.clone();
		process(&mut got, &palette, "none");

		for (i, (input, output)) in src
			.pixels
			.chunks_exact(4)
			.zip(got.pixels.chunks_exact(4))
			.enumerate()
		{
			let idx = nearest(&colors, [input[0], input[1], input[2]]);
			assert_eq!(
				[output[0], output[1], output[2]],
				colors[idx],
				"n={n} pixel {i}: recolor disagrees with brute-force nearest",
			);
		}
	}
}

#[test]
fn process_and_index_agree() {
	let colors = random_colors(0, 19);
	let palette = Palette::from_colors(&colors);
	let (w, h) = (21, 13);

	for (i, &d) in DITHERERS.iter().enumerate() {
		let src = random_image(1 + i, w, h);

		let mut colored = src.clone();
		process(&mut colored, &palette, d);

		let mut indexed = src.clone();
		let indices = process_index(&mut indexed, &palette, d);

		assert_eq!(
			indices.len(),
			(w * h) as usize,
			"ditherer {d}: wrong index count"
		);
		for (i, px) in colored.pixels.chunks_exact(4).enumerate() {
			assert_eq!(
				[px[0], px[1], px[2]],
				colors[indices[i] as usize],
				"ditherer {d} pixel {i}: process/process_index disagree",
			);
		}
	}
}

#[test]
fn indices_are_in_range() {
	let mut seed = 0;

	for &n in &[1usize, 2, 3, 5, 7, 15, 17, 100] {
		let colors = random_colors(seed, n);
		let palette = Palette::from_colors(&colors);
		seed += 1;

		for &d in DITHERERS {
			let mut img = random_image(seed, 19, 7);
			seed += 1;

			let indices = process_index(&mut img, &palette, d);
			for &ix in &indices {
				assert!(
					(ix as usize) < n,
					"ditherer {d}: index {ix} out of range for n={n}"
				);
			}
		}
	}
}

#[test]
fn recoloring_is_idempotent() {
	let colors = random_colors(0, 12);
	let palette = Palette::from_colors(&colors);

	for (i, &d) in DITHERERS.iter().enumerate() {
		let mut once = random_image(1 + i, 24, 18);
		process(&mut once, &palette, d);

		let mut twice = once.clone();
		process(&mut twice, &palette, d);

		assert_eq!(
			once.pixels, twice.pixels,
			"ditherer {d}: recolor is not idempotent"
		);
	}
}

/// Degenerate sizes (1x1, single row/column, odd) don't crash and preserve the
/// buffer dimensions, for both entry points and every ditherer.
#[test]
fn degenerate_sizes() {
	const SIZES: &[(u32, u32)] = &[
		(1, 1),
		(2, 1),
		(1, 2),
		(1, 5),
		(5, 1),
		(3, 3),
		(7, 5),
		(16, 3),
	];

	let palette = Palette::from_colors(&[[0, 0, 0], [128, 64, 200], [255, 255, 255]]);
	let mut seed = 0;

	for &(w, h) in SIZES {
		for &d in DITHERERS {
			let mut colored = random_image(seed, w, h);
			seed += 1;
			process(&mut colored, &palette, d);
			assert_eq!(colored.width, w, "{w}x{h} d={d}: width changed");
			assert_eq!(colored.height, h, "{w}x{h} d={d}: height changed");
			assert_eq!(
				colored.pixels.len(),
				(w * h * 4) as usize,
				"{w}x{h} d={d}: pixel buffer length changed",
			);

			let mut indexed = random_image(seed, w, h);
			seed += 1;
			let indices = process_index(&mut indexed, &palette, d);
			assert_eq!(
				indices.len(),
				(w * h) as usize,
				"{w}x{h} d={d}: wrong index count"
			);
		}
	}
}
