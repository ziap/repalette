use std::io::{self, Write};
use std::path::Path;
use std::process;

use clap::builder::{PossibleValue, PossibleValuesParser};
use clap::{Parser, Subcommand};

use crate::imgio::{ImageIo, IndexedImage};
use repalette_core::{self as repalette, Ditherer, Image};
use repalette_palettes::{self as palettes, ColorError, Palette, PaletteError};

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
	eprintln!("Error: unknown palette preset '{name}' (try `repalette palette list`)");
	process::exit(1);
}

fn invalid_palette(err: PaletteError) -> ! {
	// Colors are raw `&[u8]`, so write them straight through instead of going
	// via a UTF-8 string.
	let mut out = io::stderr().lock();
	match err {
		PaletteError::NotFound(name) => unknown_preset(name),
		PaletteError::TooLarge => {
			eprintln!("Error: Palette too large, please enter only up to 256 colors");
			process::exit(1);
		}
		PaletteError::ParseError(ColorError::BadChar { color, pos }) => {
			let (prefix, mid) = (b"Error: Invalid char '", b"' in color '");
			_ = out.write_all(prefix);
			_ = out.write_all(&[color[pos]]);
			_ = out.write_all(mid);
			_ = out.write_all(color);
			_ = out.write_all(b"'\n");

			// Caret line, aligned under the color token.
			let indent = prefix.len() + 1 + mid.len();
			let mut caret = vec![b' '; indent];
			caret.extend((0..color.len()).map(|i| if i == pos { b'^' } else { b'~' }));
			caret.push(b'\n');
			_ = out.write_all(&caret);

			process::exit(1);
		}
		PaletteError::ParseError(ColorError::WrongDigits(color)) => {
			let _ = out.write_all(b"Error: Palette must be 6 hex digits, '");
			let _ = out.write_all(color);
			let _ = write!(out, "' has {}", color.len());
			let _ = out.write_all(b"\n");
			process::exit(1);
		}
	}
}

fn run_palette(action: PaletteArgs) {
	let mut out = std::io::stdout().lock();
	match action {
		PaletteArgs::List => {
			for &name in &palettes::NAMES {
				_ = out.write_all(name);
				_ = out.write_all(b"\n");
			}
		}
		PaletteArgs::Show { name } => match palettes::get(&name) {
			Some(colors) => {
				for (i, color) in colors.iter().enumerate() {
					if i > 0 {
						_ = out.write_all(b",");
					}
					let [r, g, b] = color;
					_ = write!(out, "{r:02x}{g:02x}{b:02x}");
				}
				_ = out.write_all(b"\n");
			}
			None => unknown_preset(&name),
		},
	};
}

fn main() {
	match Args::parse().command {
		Command::Apply(args) => run_apply(args),
		Command::Palette { action } => run_palette(action),
	}
}

fn run_apply(args: ApplyArgs) {
	let preset = args.palette.as_deref();
	let custom = args.colors.as_deref();

	let palette = Palette::resolve(preset, custom).unwrap_or_else(|err| invalid_palette(err));

	let colors = palette.as_slice();

	let format = image::ImageFormat::from_path(&args.output).unwrap_or_else(|err| {
		eprintln!("Failed to detect output format: {err}");
		process::exit(1);
	});

	let mut img = Image::read(&args.input).unwrap_or_else(|err| {
		eprintln!("{err}");
		process::exit(1);
	});

	if format == image::ImageFormat::Png && colors.len() <= 256 {
		let indices = repalette::process_index(&mut img, colors, &args.dither);

		let indexed = IndexedImage {
			width: img.width,
			height: img.height,
			indices,
			colors,
		};

		indexed.write_png(&args.output).unwrap_or_else(|err| {
			eprintln!("{err}");
			process::exit(1);
		});
	} else {
		repalette::process(&mut img, colors, &args.dither);

		img.write(&args.output, format).unwrap_or_else(|err| {
			eprintln!("{err}");
			process::exit(1);
		});
	}
}
