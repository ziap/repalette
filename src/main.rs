use std::io::Write;
use std::path::Path;

use clap::builder::{PossibleValue, PossibleValuesParser};
use clap::{Parser, Subcommand};
use image::{ImageReader, RgbImage};

mod palettes;
mod repalette;

fn dither_values() -> PossibleValuesParser {
	let ditherers = repalette::get_ditherers();

	ditherers
		.iter()
		.map(|ditherer| PossibleValue::new(ditherer.name).help(ditherer.display_name))
		.into()
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

#[derive(clap::Args, Debug)]
struct ApplyArgs {
	/// Path to the source image
	input: Box<Path>,
	/// Path to write the recolored image
	output: Box<Path>,

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
		default_value = repalette::default_ditherer(),
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
	Custom(Box<[u32]>),
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

	let img = ImageReader::open(&args.input)
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to open image '{}': {}",
				args.input.display(),
				error
			);
			std::process::exit(1);
		})
		.with_guessed_format()
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to detect image format of '{}': {}",
				args.input.display(),
				error
			);
			std::process::exit(1);
		})
		.decode()
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to decode image '{}': {}",
				args.input.display(),
				error
			);
			std::process::exit(1);
		})
		.into_rgba8();

	let width = img.width();
	let height = img.height();
	let mut pixels = img.into_raw();

	repalette::process(&mut pixels, width, height, colors, &args.dither).unwrap_or_else(|err| {
		std::process::exit(err.status);
	});

	// Convert to RGB
	for idx in 0..(width * height) as usize {
		let src = idx * 4..idx * 4 + 4;
		let dst = idx * 3;
		pixels.copy_within(src, dst);
	}

	RgbImage::from_raw(width, height, pixels)
		.unwrap()
		.save(&args.output)
		.unwrap_or_else(|error| {
			eprintln!(
				"ERROR: Failed to save image '{}': {}",
				args.output.display(),
				error
			);
			std::process::exit(1);
		});
}
