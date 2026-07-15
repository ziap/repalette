use repalette_core::{Image, Palette, process_multisample_2x, process_multisample_4x};

type Multisampler = fn(&mut Image, &Palette, &str);

const FACTORS: &[(&str, Multisampler)] = &[
	("2x", process_multisample_2x),
	("4x", process_multisample_4x),
];

const DITHERERS: &[&str] = &[
	"none", "fs", "atkinson", "jjn", "burkes", "sierra32", "sierra4",
];

/// Solid `w x h` RGBA image of a single opaque colour.
fn solid(color: [u8; 3], w: u32, h: u32) -> Image {
	let [r, g, b] = color;
	let mut pixels = Vec::with_capacity((w * h * 4) as usize);
	for _ in 0..w * h {
		pixels.extend_from_slice(&[r, g, b, 255]);
	}
	Image {
		width: w,
		height: h,
		pixels,
	}
}

#[test]
fn size_preserved() {
	// Includes 1x1, single row/column, and odd dimensions to test
	// every edge/padding branch of the ring-buffer scheduling.
	const SIZES: &[(u32, u32)] = &[
		(1, 1),
		(2, 1),
		(1, 2),
		(1, 5),
		(5, 1),
		(2, 2),
		(3, 3),
		(7, 5),
		(8, 8),
		(16, 3),
	];

	let palette = Palette::from_colors(&[[0, 0, 0], [128, 64, 200], [255, 255, 255]]);

	for &(w, h) in SIZES {
		for &(fname, run) in FACTORS {
			for &d in DITHERERS {
				let mut img = solid([90, 140, 210], w, h);
				run(&mut img, &palette, d);

				assert_eq!(img.width, w, "{fname} {w}x{h} d={d}: width changed");
				assert_eq!(img.height, h, "{fname} {w}x{h} d={d}: height changed");
				assert_eq!(
					img.pixels.len(),
					(w * h * 4) as usize,
					"{fname} {w}x{h} d={d}: pixel buffer length changed",
				);
			}
		}
	}
}

#[test]
fn produces_intermediate_colors() {
	// Pure black/white palette: the indexed path could only ever emit 0 or 255.
	let palette = Palette::from_colors(&[[0, 0, 0], [255, 255, 255]]);

	// A solid mid-grey large enough that downsampling averages many independent
	// dither decisions into intermediate tones.
	for &(fname, run) in FACTORS {
		let mut img = solid([128, 128, 128], 32, 32);
		run(&mut img, &palette, "fs");

		let has_intermediate = img
			.pixels
			.chunks_exact(4)
			.flat_map(|px| &px[..3])
			.any(|&c| c != 0 && c != 255);

		assert!(
			has_intermediate,
			"{fname}: expected tones between the two palette colours, \
        got only pure black/white",
		);
	}
}
