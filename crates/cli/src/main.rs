use std::io::Write;
use std::path::Path;

use clap::builder::{PossibleValue, PossibleValuesParser};
use clap::{Parser, Subcommand};

use crate::imgio::{ImageIo, IndexedImage};
use repalette_core::{self as repalette, Ditherer, Image};
use repalette_palettes as palettes;

mod imgio;

fn dither_values() -> PossibleValuesParser {
	let ditherers = repalette::get_ditherers();

	ditherers
		.iter()
		.map(|ditherer| {
			let Ditherer { name, display_name } = ditherer;
			PossibleValue::new(name).help(display_name)
		})
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
				Err(err) => {
					eprintln!("{err}");
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

	let format = image::ImageFormat::from_path(&args.output).unwrap_or_else(|err| {
		eprintln!("Failed to detect output format: {err}");
		std::process::exit(1);
	});

	let mut img = Image::read(&args.input).unwrap_or_else(|err| {
		eprintln!("{err}");
		std::process::exit(1);
	});

	if format == image::ImageFormat::Png && colors.len() <= 256 {
		let indices = repalette::process_index(&mut img, colors, &args.dither)
			.unwrap_or_else(|err| std::process::exit(err.status));

		let indexed = IndexedImage {
			width: img.width,
			height: img.height,
			indices,
			colors,
		};

		indexed.write_png(&args.output).unwrap_or_else(|err| {
			eprintln!("{err}");
			std::process::exit(1);
		});
	} else {
		repalette::process(&mut img, colors, &args.dither)
			.unwrap_or_else(|err| std::process::exit(err.status));

		img.write(&args.output, format).unwrap_or_else(|err| {
			eprintln!("{err}");
			std::process::exit(1);
		});
	}
}
