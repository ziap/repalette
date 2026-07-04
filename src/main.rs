use std::ffi::{CStr, c_char, c_int};
use std::io::Write;
use std::path::Path;

use clap::{Parser, Subcommand};
use image::error::UnsupportedErrorKind;
use image::{DynamicImage, ImageError, ImageReader};

mod palettes;

unsafe extern "C" {
	fn repalette_process(
		pixels: *mut u8,
		width: c_int,
		height: c_int,
		colors: *const u32,
		count: usize,
		ditherer: c_int,
	) -> c_int;

	fn ditherer_count() -> c_int;
	fn ditherer_name(index: c_int) -> *const c_char;
	fn ditherer_display(index: c_int) -> *const c_char;
}

fn dither_values() -> clap::builder::PossibleValuesParser {
	let values = (0..unsafe { ditherer_count() }).map(|i| {
		let name = get_str(unsafe { ditherer_name(i) });
		let display = get_str(unsafe { ditherer_display(i) });
		clap::builder::PossibleValue::new(name).help(display)
	});
	clap::builder::PossibleValuesParser::new(values)
}

fn dither_index(name: &str) -> c_int {
	(0..unsafe { ditherer_count() })
		.find(|&i| unsafe { CStr::from_ptr(ditherer_name(i)) }.to_str() == Ok(name))
		.expect("clap validated the ditherer name")
}

#[derive(Parser, Debug)]
struct Args {
	#[command(subcommand)]
	command: Command,
}

#[derive(Subcommand, Debug)]
enum Command {
	/// Recolor an image to a palette
	Apply(ApplyArgs),
	/// Inspect the built-in palette presets
	Palette {
		#[command(subcommand)]
		action: PaletteArgs,
	},
}

fn get_str(ptr: *const c_char) -> &'static str {
	unsafe { CStr::from_ptr(ptr).to_str().unwrap() }
}

#[derive(clap::Args, Debug)]
struct ApplyArgs {
	/// Path to the source image
	input: Option<Box<Path>>,
	/// Path to write the recolored image
	output: Option<Box<Path>>,

	/// Built-in palette (see `repalette palette list`)
	#[arg(short, long)]
	palette: Option<Box<str>>,

	/// Manual palette, e.g. 000000,ffffff
	#[arg(short = 'c', long, conflicts_with = "palette")]
	colors: Option<Box<str>>,

	/// Dithering algorithm
	#[arg(
		short,
		long,
		default_value = get_str(unsafe { ditherer_name(1) }),
		value_parser = dither_values(),
	)]
	dither: String,
}

#[derive(Subcommand, Debug)]
enum PaletteArgs {
	/// List all available color palettes
	List,
	/// Print a palette's colors
	Show {
		/// Palette name
		name: Box<str>,
	},
}

fn unknown_preset(name: &str) -> ! {
	eprintln!("ERROR: unknown palette preset '{name}' (try `repalette palette list`)");
	std::process::exit(1);
}

fn run_palette(action: PaletteArgs) {
	let mut out = std::io::stdout().lock();
	match action {
		PaletteArgs::List => palettes::write_names(&mut out).unwrap(),
		PaletteArgs::Show { name } => match palettes::get(&name) {
			Some(colors) => {
				for (i, color) in colors.iter().enumerate() {
					if i > 0 {
						out.write_all(b",").unwrap();
					}
					write!(out, "{color:06x}").unwrap();
				}
				out.write_all(b"\n").unwrap();
			}
			None => unknown_preset(&name),
		},
	}
}

enum Palette {
	Preset(&'static [u32]),
	Custom(Vec<u32>),
}

impl Palette {
	const DEFAULT: Self = Self::Preset(&[0x000000, 0xffffff]);

	fn as_slice(&self) -> &[u32] {
		match self {
			Palette::Preset(s) => s,
			Palette::Custom(v) => v,
		}
	}

	fn resolve(preset: Option<&str>, colors: Option<Box<str>>) -> Self {
		match (preset, colors) {
			(Some(name), _) => match palettes::get(name) {
				Some(c) => Self::Preset(c),
				None => unknown_preset(name),
			},
			(None, Some(s)) => match palettes::parse_palette(&s) {
				Ok(v) => Palette::Custom(v),
				Err(e) => {
					eprintln!("{}", e.message());
					std::process::exit(1);
				}
			},
			(None, None) => Self::DEFAULT,
		}
	}
}

fn main() {
	match Args::parse().command {
		Command::Apply(args) => run_apply(args),
		Command::Palette { action } => run_palette(action),
	}
}

fn run_apply(args: ApplyArgs) {
	let palette = Palette::resolve(args.palette.as_deref(), args.colors);
	let colors = palette.as_slice();

	let (input, output) = match (args.input.as_deref(), args.output.as_deref()) {
		(None, _) => {
			eprintln!("ERROR: input file not provided");
			eprintln!();
			eprintln!("For more information, try '--help'.");
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
			colors.as_ptr(),
			colors.len(),
			dither_index(&args.dither),
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
