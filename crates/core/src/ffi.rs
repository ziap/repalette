use alloc::boxed::Box;
use alloc::{vec, vec::Vec};
use core::ffi::{CStr, c_char, c_int};

use crate::palette::Palette;

#[derive(Clone)]
pub struct Image {
	pub width: u32,
	pub height: u32,
	pub pixels: Vec<u8>,
}

mod c {
	use super::*;

	unsafe extern "C" {
		pub fn repalette_process(
			pixels: *mut u8,
			width: c_int,
			height: c_int,
			colors: *const u8,
			count: usize,
			ditherer: c_int,
		);

		pub fn repalette_process_index(
			pixels: *mut u8,
			width: c_int,
			height: c_int,
			colors: *const u8,
			count: usize,
			ditherer: c_int,
			out: *mut u8,
		);

		pub fn ditherer_count() -> c_int;
		pub fn ditherer_name(index: c_int) -> *const c_char;
		pub fn ditherer_display(index: c_int) -> *const c_char;

		pub fn repalette_extract(
			pixels: *mut u8,
			width: c_int,
			height: c_int,
			k: c_int,
			threshold: c_int,
			soa: *mut f32,
			bins: *mut u32,
			pixbuf: *mut u8,
			out: *mut u8,
		) -> c_int;
	}
}

pub struct Ditherer {
	pub name: &'static str,
	pub display_name: &'static str,
}

fn get_str(ptr: *const c_char) -> &'static str {
	unsafe { CStr::from_ptr(ptr).to_str().unwrap() }
}

pub fn default_ditherer() -> &'static str {
	get_str(unsafe { c::ditherer_name(1) })
}

pub fn get_ditherers() -> Box<[Ditherer]> {
	(0..unsafe { c::ditherer_count() })
		.map(|i| Ditherer {
			name: get_str(unsafe { c::ditherer_name(i) }),
			display_name: get_str(unsafe { c::ditherer_display(i) }),
		})
		.collect()
}

fn dither_index(name: &str) -> c_int {
	(0..unsafe { c::ditherer_count() })
		.find(|&i| unsafe { CStr::from_ptr(c::ditherer_name(i)) }.to_str() == Ok(name))
		.expect("clap validated the ditherer name")
}

pub fn process(img: &mut Image, palette: &Palette, ditherer: &str) {
	let colors = palette.as_slice();
	unsafe {
		c::repalette_process(
			img.pixels.as_mut_ptr(),
			img.width as c_int,
			img.height as c_int,
			colors.as_flattened().as_ptr(),
			colors.len(),
			dither_index(ditherer),
		)
	}
}

pub fn process_index(img: &mut Image, palette: &Palette, ditherer: &str) -> Vec<u8> {
	let colors = palette.as_slice();
	let mut out = vec![0u8; (img.width * img.height) as usize];

	unsafe {
		c::repalette_process_index(
			img.pixels.as_mut_ptr(),
			img.width as c_int,
			img.height as c_int,
			colors.as_flattened().as_ptr(),
			colors.len(),
			dither_index(ditherer),
			out.as_mut_ptr(),
		)
	};

	out
}

const EXTRACT_THRESHOLD: usize = 1 << 19; // 524288, multiple of 4

pub fn extract_palette(img: &mut Image, count: u16) -> Palette {
	let k = count.clamp(1, 256) as usize;
	let p = (img.width * img.height) as usize;
	let thr = EXTRACT_THRESHOLD;

	let mut soa = Box::<[f32]>::new_uninit_slice(thr * 4);
	let mut bins = Box::<[u32]>::new_uninit_slice((thr + 1) * 2);
	let mut pix = Box::<[u8]>::new_uninit_slice(p * 4 * 2);

	let mut out = [[0u8; 3]; 256];

	let n = unsafe {
		c::repalette_extract(
			img.pixels.as_mut_ptr(),
			img.width as c_int,
			img.height as c_int,
			k as c_int,
			thr as c_int,
			soa.as_mut_ptr() as *mut f32,
			bins.as_mut_ptr() as *mut u32,
			pix.as_mut_ptr() as *mut u8,
			out[..k].as_flattened_mut().as_mut_ptr(),
		)
	} as usize;

	Palette::from_colors(&out[..n])
}
