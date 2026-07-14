use std::collections::HashSet;

use repalette_core::{Image, extract_palette};

const THRESHOLD: usize = 1 << 18;

fn image(pixels: &[[u8; 3]]) -> Image {
	let mut buf = Vec::with_capacity(pixels.len() * 4);
	for &[r, g, b] in pixels {
		buf.extend_from_slice(&[r, g, b, 255]);
	}
	Image {
		width: pixels.len() as u32,
		height: 1,
		pixels: buf,
	}
}

fn palette(pixels: &[[u8; 3]], k: u16) -> Vec<[u8; 3]> {
	extract_palette(&mut image(pixels), k, THRESHOLD as u32)
		.as_slice()
		.to_vec()
}

fn distinct(pixels: &[[u8; 3]]) -> HashSet<[u8; 3]> {
	pixels.iter().copied().collect()
}

#[test]
fn fewer_colors_than_k() {
	let cases: &[&[[u8; 3]]] = &[
		&[[0x80, 0x80, 0x80]],
		&[[0, 0, 0], [255, 255, 255]],
		&[[0x1b, 0x1c, 0x18], [0x6a, 0x85, 0xbb], [0xff, 0x33, 0x33]],
		&[
			[10, 20, 30],
			[200, 100, 50],
			[0, 128, 255],
			[255, 255, 0],
			[90, 45, 67],
		],
	];

	for colors in cases {
		// Tile the colors across many pixels so each is well represented.
		let pixels: Vec<[u8; 3]> = colors.iter().copied().cycle().take(4096).collect();
		let out = palette(&pixels, 16);

		assert_eq!(
			out.len(),
			colors.len(),
			"want {} colors, got {out:?}",
			colors.len()
		);

		// Return exactly input colors, also test accuracy of Oklab roundtrip
		assert_eq!(
			distinct(&out),
			distinct(colors),
			"palette must equal the input colors"
		);
	}
}

#[test]
fn duplicating_pixels_keeps_palette() {
	// 65536 distinct colors in 65536 pixels: both below THRESHOLD.
	let base: Vec<[u8; 3]> = (0..65536u32)
		.map(|i| [i as u8, (i >> 8) as u8, 0])
		.collect();
	assert!(base.len() < THRESHOLD);

	let once = palette(&base, 64);

	// Duplicate until the pixel count exceeds THRESHOLD; colors are unchanged.
	let dup = base.repeat(8);
	assert!(dup.len() > THRESHOLD);
	assert_eq!(distinct(&dup).len(), base.len());

	let duplicated = palette(&dup, 64);

	assert_eq!(once, duplicated);
}

#[test]
fn same_histogram_same_palette() {
	let cap = 2 * 64 * 64 * 64;
	let mut a = Vec::with_capacity(cap);
	let mut b = Vec::with_capacity(cap);

	for jr in 0..64u8 {
		for jg in 0..64u8 {
			for jb in 0..64u8 {
				let (r, g, bl) = (4 * jr, 4 * jg, 4 * jb);
				a.push([r, g, bl]);
				a.push([r + 3, g + 3, bl + 3]);
				b.push([r + 1, g + 1, bl + 1]);
				b.push([r + 2, g + 2, bl + 2]);
			}
		}
	}

	assert_ne!(a, b, "the two images must actually differ");

	// Both exceed THRESHOLD (2 * 64^3 distinct colors), forcing quantization
	assert!(distinct(&a).len() > THRESHOLD);
	assert!(distinct(&b).len() > THRESHOLD);

	assert_eq!(palette(&a, 256), palette(&b, 256));
}

#[test]
fn extreme_case() {
	let img: Vec<[u8; 3]> = (0..4096 * 4096)
		.map(|i| [(i >> 16) as u8, (i >> 8) as u8, (i >> 0) as u8])
		.collect();

	let out = palette(&img, 256);
	assert_eq!(out.len(), 256);
}

/// `count` is clamped to `1..=256`, so `k = 0` and `k = 1` both yield a single
/// color and `k = 512` yields the 256-color maximum.
#[test]
fn k_is_clamped() {
	// 256 distinct colors (grayscale ramp) so k = 256 can return a full palette.
	let colors: Vec<[u8; 3]> = (0..256u32).map(|i| [i as u8, i as u8, i as u8]).collect();

	assert_eq!(palette(&colors, 0).len(), 1);
	assert_eq!(palette(&colors, 1).len(), 1);
	assert_eq!(
		palette(&colors, 0),
		palette(&colors, 1),
		"k=0 and k=1 both clamp to 1"
	);

	assert_eq!(palette(&colors, 512).len(), 256, "k=512 clamps to 256");
}
