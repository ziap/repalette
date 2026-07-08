use std::fmt::Display;
use std::fs::File;
use std::io::{self, BufReader, BufWriter, Write};
use std::path::Path;
use std::process;

use clap::builder::{PossibleValue, PossibleValuesParser};
use clap::{Parser, Subcommand};
use image::ImageFormat;

use crate::imgio::{ImageIo, IndexedImage};
use repalette_core::{self as repalette, Ditherer, HexError, Image, ParseError};
use repalette_palettes::{self as palettes, ResolveError};

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

fn parse_failed(err: ParseError) -> ! {
	use ParseError::*;

	let mut out = io::stderr().lock();

	match err {
		Hex(HexError::BadChar { color, pos }) => {
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
		Hex(HexError::WrongDigits(color)) => {
			let _ = out.write_all(b"Error: Palette must be 6 hex digits, '");
			let _ = out.write_all(color);
			let _ = write!(out, "' has {}", color.len());
			let _ = out.write_all(b"\n");
			process::exit(1);
		}
		TooLarge => {
			eprintln!("Error: Palette too large, please enter only up to 256 colors");
			process::exit(1);
		}
	}
}

fn invalid_palette<T>(err: ResolveError) -> T {
	use ResolveError::*;

	match err {
		NotFound(name) => unknown_preset(name),
		ParseFailed(err) => parse_failed(err),
	}
}

fn print_and_exit<T>(err: impl Display) -> T {
	eprintln!("{err}");
	process::exit(1);
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

	let palette = palettes::resolve(preset, custom).unwrap_or_else(invalid_palette);

	let colors = palette.as_slice();

	let format = ImageFormat::from_path(&args.output)
		.map_err(|err| format!("Failed to detect output format: {err}"))
		.unwrap_or_else(print_and_exit);

	let in_file = File::open(&args.input)
		.map_err(|err| format!("Failed to open the source image: {err}"))
		.unwrap_or_else(print_and_exit);
	let mut reader = BufReader::new(in_file);

	let mut img = Image::read(&mut reader).unwrap_or_else(print_and_exit);

	let out_file = File::create(args.output)
		.map_err(|err| format!("Failed to open the target image: {err}"))
		.unwrap_or_else(print_and_exit);

	let mut writer = BufWriter::new(out_file);

	if format == ImageFormat::Png {
		let indices = repalette::process_index(&mut img, &palette, &args.dither);

		let indexed = IndexedImage {
			width: img.width,
			height: img.height,
			indices,
			colors,
		};

		indexed
			.write_png(&mut writer)
			.unwrap_or_else(print_and_exit);
	} else {
		repalette::process(&mut img, &palette, &args.dither);

		img
			.write(&mut writer, format)
			.unwrap_or_else(print_and_exit);
	}
}
