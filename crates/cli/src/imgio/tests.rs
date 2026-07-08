use std::io::{Cursor, Seek, SeekFrom};

use crate::*;

const PALETTE: [[u8; 3]; 256] = {
	let mul = 6364136223846793005;
	let inc = 1442695040888963407;

	let mut buf = [[0u8; 3]; 256];
	let mut s: u64 = inc;
	let mut i = 0;
	while i < buf.len() {
		s = s.wrapping_mul(mul).wrapping_add(inc);
		buf[i][0] = (s >> 56) as u8;
		s = s.wrapping_mul(mul).wrapping_add(inc);
		buf[i][1] = (s >> 56) as u8;
		s = s.wrapping_mul(mul).wrapping_add(inc);
		buf[i][2] = (s >> 56) as u8;
		i += 1;
	}

	buf
};

fn test_image(width: u32, height: u32, palette_count: usize) -> IndexedImage<'static> {
	IndexedImage {
		width,
		height,
		indices: (0..(width * height) as usize)
			.map(|v| (v % palette_count) as u8)
			.collect(),
		colors: &PALETTE[0..palette_count],
	}
}

fn to_image(img: &IndexedImage) -> Image {
	Image {
		width: img.width,
		height: img.height,
		pixels: img
			.indices
			.iter()
			.flat_map(|&i| img.colors[i as usize].iter().chain(&[255]))
			.copied()
			.collect(),
	}
}

fn check_cursor(img: &Image, cursor: &mut Cursor<Vec<u8>>) {
	let loaded = image::load_from_memory(cursor.get_ref())
		.unwrap()
		.to_rgba8();

	assert_eq!(img.width, loaded.width());
	assert_eq!(img.height, loaded.height());
	assert_eq!(img.pixels, loaded.into_raw());

	cursor.seek(SeekFrom::Start(0)).unwrap();
	let roundtrip = Image::read(cursor).unwrap();
	assert_eq!(img.width, roundtrip.width);
	assert_eq!(img.height, roundtrip.height);
	assert_eq!(img.pixels, roundtrip.pixels);
}

fn test_rgb(sizes: &[(u32, u32)]) {
	for &(width, height) in sizes {
		let img = to_image(&test_image(width, height, 256));
		let mut cursor = Cursor::new(Vec::new());
		img.clone().write(&mut cursor, ImageFormat::Png).unwrap();
		check_cursor(&img, &mut cursor);
	}
}

fn test_indexed(sizes: &[(u32, u32)]) {
	for &(width, height) in sizes {
		for palette_count in 1..=256 {
			let indexed = test_image(width, height, palette_count);
			let img = to_image(&indexed);

			let mut cursor = Cursor::new(Vec::new());
			indexed.write_png(&mut cursor).unwrap();
			check_cursor(&img, &mut cursor);
		}
	}
}

const SIZES_EVEN: &[(u32, u32)] = &[(32, 32), (32, 16), (16, 32)];
const SIZES_ODD: &[(u32, u32)] = &[(32, 32), (32, 16), (16, 32)];

#[test]
fn even_rgb() {
	test_rgb(SIZES_EVEN);
}

#[test]
fn even_indexed() {
	test_indexed(SIZES_EVEN);
}

#[test]
fn odd_rgb() {
	test_rgb(SIZES_ODD);
}

#[test]
fn odd_indexed() {
	test_indexed(SIZES_ODD);
}

#[test]
fn edge_cases_rgb() {
	test_rgb(&[(1, 1)]);
}

#[test]
fn edge_cases_indexed() {
	test_indexed(&[(1, 1)]);
}
