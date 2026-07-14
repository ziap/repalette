use alloc::boxed::Box;
use alloc::vec::Vec;
use core::ffi::{CStr, c_char};

use crate::palette::Palette;

#[derive(Clone)]
pub struct Image {
	pub width: u32,
	pub height: u32,
	pub pixels: Vec<u8>,
}

mod c {
	use super::*;
	use core::mem::MaybeUninit;

	unsafe extern "C" {
		pub fn repalette_process(
			pixels: *mut u8,
			width: u32,
			height: u32,
			colors: *const u8,
			count: usize,
			ditherer: usize,
		);

		pub fn repalette_process_index(
			pixels: *mut u8,
			width: u32,
			height: u32,
			colors: *const u8,
			count: usize,
			ditherer: usize,
			out: *mut MaybeUninit<u8>,
		);

		pub fn ditherer_count() -> usize;
		pub fn ditherer_name(index: usize) -> *const c_char;
		pub fn ditherer_display(index: usize) -> *const c_char;

		pub fn repalette_extract(
			pixels: *mut u8,
			width: u32,
			height: u32,
			k: usize,
			threshold: u32,
			soa: *mut MaybeUninit<f32>,
			bins: *mut MaybeUninit<u64>,
			out: *mut u8,
		) -> usize;
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

fn dither_index(name: &str) -> usize {
	(0..unsafe { c::ditherer_count() })
		.find(|&i| unsafe { CStr::from_ptr(c::ditherer_name(i)) }.to_str() == Ok(name))
		.expect("clap validated the ditherer name")
}

pub fn process(img: &mut Image, palette: &Palette, ditherer: &str) {
	let colors = palette.as_slice();
	unsafe {
		c::repalette_process(
			img.pixels.as_mut_ptr(),
			img.width,
			img.height,
			colors.as_flattened().as_ptr(),
			colors.len(),
			dither_index(ditherer),
		)
	}
}

pub fn process_index(img: &mut Image, palette: &Palette, ditherer: &str) -> Vec<u8> {
	let colors = palette.as_slice();
	let w = unsafe { usize::try_from(img.width).unwrap_unchecked() };
	let h = unsafe { usize::try_from(img.height).unwrap_unchecked() };
	let mut out = Box::<[u8]>::new_uninit_slice(w * h);

	unsafe {
		c::repalette_process_index(
			img.pixels.as_mut_ptr(),
			img.width,
			img.height,
			colors.as_flattened().as_ptr(),
			colors.len(),
			dither_index(ditherer),
			out.as_mut_ptr(),
		)
	};

	unsafe { out.assume_init().into() }
}

pub fn extract_palette(mut img: Image, count: u16, threshold: u32) -> Palette {
	let k = usize::from(count.clamp(1, 256));
	let thr = threshold as usize;

	let mut soa = Box::<[f32]>::new_uninit_slice(thr * 4);
	let mut bins = Box::<[u64]>::new_uninit_slice((thr + 1) * 2);

	let mut out = [[0u8; 3]; 256];

	let n = unsafe {
		c::repalette_extract(
			img.pixels.as_mut_ptr(),
			img.width,
			img.height,
			k,
			threshold,
			soa.as_mut_ptr(),
			bins.as_mut_ptr(),
			out[..k].as_flattened_mut().as_mut_ptr(),
		)
	};

	Palette::from_colors(&out[..n])
}
