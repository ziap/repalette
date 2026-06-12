use std::ffi::{CString, c_char};
use std::path::Path;

use clap::Parser;
use image::error::UnsupportedErrorKind;
use image::{DynamicImage, ImageError, ImageReader};

unsafe extern "C" {
	fn repalette_process(
		pixels: *mut u8,
		width: i32,
		height: i32,
		palette: *const c_char,
		dither: *const c_char,
	) -> i32;

	fn repalette_help();
}

#[derive(Parser, Debug)]
#[command(disable_help_flag = true)]
struct Args {
	input: Option<Box<Path>>,
	output: Option<Box<Path>>,

	#[arg(short, long, default_value = "")]
	palette: CString,

	#[arg(short, long, default_value = "")]
	dither: CString,

	#[arg(short = 'h', long = "help")]
	help: bool,
}

fn main() {
	let args = Args::parse();

	if args.help {
		unsafe { repalette_help() };
		return;
	}

	let (input, output) = match (args.input.as_deref(), args.output.as_deref()) {
		(None, _) => {
			eprintln!("ERROR: input file not provided");
			std::process::exit(1);
		}
		(_, None) => {
			eprintln!("ERROR: output file not provided");
			std::process::exit(1);
		}
		(Some(input), Some(output)) => (input, output),
	};

	let image = ImageReader::open(input)
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to open image '{}': {}",
				input.display(),
				error
			);
			std::process::exit(1);
		})
		.with_guessed_format()
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to detect image format of '{}': {}",
				input.display(),
				error
			);
			std::process::exit(1);
		})
		.decode()
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to decode image '{}': {}",
				input.display(),
				error
			);
			std::process::exit(1);
		})
		.into_rgba8();

	let width = image.width();
	let height = image.height();
	let mut pixels = image.into_raw();

	let status = unsafe {
		repalette_process(
			pixels.as_mut_ptr(),
			width as i32,
			height as i32,
			args.palette.as_ptr(),
			args.dither.as_ptr(),
		)
	};

	if status != 0 {
		std::process::exit(status);
	}

	let image = image::RgbaImage::from_raw(width, height, pixels).unwrap();

	let result = match image.save(output) {
		Err(ImageError::Unsupported(e)) if let UnsupportedErrorKind::Color(_) = e.kind() => {
			// Encoder rejected RGBA (e.g. JPEG) — drop alpha and retry as RGB.
			DynamicImage::ImageRgba8(image).into_rgb8().save(output)
		}
		other => other,
	};

	result.unwrap_or_else(|error| {
		eprintln!(
			"ERROR: Failed to save image '{}': {}",
			output.display(),
			error
		);
		std::process::exit(1);
	});
}
