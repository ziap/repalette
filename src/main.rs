use std::ffi::{CString, c_char, c_int};
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
		dither: *const c_char,
	) -> c_int;

	fn repalette_help();
}

#[derive(Parser, Debug)]
#[command(
  styles = clap::builder::Styles::plain(),
  disable_help_subcommand = true,
)]
struct Args {
	#[command(subcommand)]
	command: Command,
}

#[derive(Subcommand, Debug)]
enum Command {
	/// Recolor an image to a palette
	#[command(disable_help_flag = true)]
	Apply(ApplyArgs),
	/// Inspect the built-in palette presets
	Palette {
		#[command(subcommand)]
		action: PaletteArgs,
	},
}

#[derive(clap::Args, Debug)]
struct ApplyArgs {
	input: Option<Box<Path>>,
	output: Option<Box<Path>>,

	/// Built-in preset (see `repalette palette list`)
	#[arg(short, long)]
	palette: Option<Box<str>>,

	/// Manual palette, e.g. 000000,ffffff
	#[arg(short = 'c', long, conflicts_with = "palette")]
	colors: Option<Box<str>>,

	#[arg(short, long, default_value = "")]
	dither: CString,

	#[arg(short = 'h', long = "help")]
	help: bool,
}

#[derive(Subcommand, Debug)]
enum PaletteArgs {
	/// List every palette name
	List,
	/// Print a palette's colors
	Show { name: Box<str> },
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
	if args.help {
		unsafe { repalette_help() };
		return;
	}

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
