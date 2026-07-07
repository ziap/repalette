#![no_std]

extern crate alloc;

use alloc::boxed::Box;
use alloc::{vec, vec::Vec};
use core::ffi::{CStr, c_char, c_int};

pub struct Image {
	pub width: u32,
	pub height: u32,
	pub pixels: Vec<u8>,
}

mod c {
	use core::ffi::{c_char, c_int};
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

pub fn process(img: &mut Image, colors: &[[u8; 3]], ditherer: &str) {
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

pub fn process_index(img: &mut Image, colors: &[[u8; 3]], ditherer: &str) -> Vec<u8> {
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
