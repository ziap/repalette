use std::fmt::Display;
use std::fs::File;
use std::io::{self, BufReader, BufWriter, Write};
use std::path::Path;
use std::process;

use clap::builder::{PossibleValue, PossibleValuesParser};
use clap::{Parser, Subcommand};
use image::ImageFormat;

use crate::imgio::{ImageIo, IndexedImage};
use repalette_core::{self as repalette, Ditherer, HexError, Image, Palette, ParseError};
use repalette_palettes::{self as palettes};

mod imgio;

const DEFAULT_EXTRACT: u16 = 16;
const EXTRACT_THRESHOLD: u32 = 1 << 18;

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

	/// Extract an optimal color palette from an image
	Extract(ExtractArgs),

	/// Inspect the built-in palette presets
	Palette {
		#[command(subcommand)]
		action: PaletteArgs,
	},
}

#[derive(clap::Args, Debug)]
struct ExtractArgs {
	/// Path to the image
	image: Box<Path>,

	/// Number of colors to extract (1-256)
	#[arg(
		short = 'k',
		long,
		value_name = "N",
		default_value_t = DEFAULT_EXTRACT,
		value_parser = clap::value_parser!(u16).range(1..=256),
	)]
	extract: u16,
}

#[derive(clap::Args, Debug)]
struct ApplyArgs {
	/// Path to the source image
	input: Box<Path>,
	/// Path to write the recolored image
	output: Box<Path>,

	/// Built-in palette (see `repalette palette list`)
	#[arg(short, long, group = "source")]
	palette: Option<Box<str>>,

	/// Manual palette, e.g. 000000,ffffff
	#[arg(short = 'c', long, group = "source")]
	colors: Option<Box<str>>,

	/// Extract an optimal N-color palette from the image (1-256)
	#[arg(
		short = 'k',
		long,
		value_name = "N",
		conflicts_with = "source",
		value_parser = clap::value_parser!(u16).range(1..=256),
	)]
	extract: Option<u16>,

	/// Extract the palette from a reference image instead of the input
	#[arg(long, value_name = "PATH", conflicts_with = "source")]
	palette_from: Option<Box<Path>>,

	/// Dithering algorithm
	#[arg(
		short,
		long,
		default_value = repalette::default_ditherer(),
		value_parser = dither_values(),
	)]
	dither: String,

	/// Multisample: dither at 2x then downsample (higher quality, truecolor output)
	#[arg(short, long)]
	multisample: bool,
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

fn print_and_exit<T>(err: impl Display) -> T {
	eprintln!("{err}");
	process::exit(1);
}

fn print_colors(out: &mut impl Write, colors: &[[u8; 3]]) {
	for (i, color) in colors.iter().enumerate() {
		if i > 0 {
			_ = out.write_all(b",");
		}
		let [r, g, b] = color;
		_ = write!(out, "{r:02x}{g:02x}{b:02x}");
	}
	_ = out.write_all(b"\n");
}

fn read_image(path: &Path) -> Image {
	let file = File::open(path)
		.map_err(|err| format!("Failed to open the source image: {err}"))
		.unwrap_or_else(print_and_exit);
	Image::read(&mut BufReader::new(file)).unwrap_or_else(print_and_exit)
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
			Some(colors) => print_colors(&mut out, colors),
			None => unknown_preset(&name),
		},
	};
}

fn run_extract(args: ExtractArgs) {
	let mut out = std::io::stdout().lock();
	let img = read_image(&args.image);
	let palette = repalette::extract_palette(img, args.extract, EXTRACT_THRESHOLD);
	print_colors(&mut out, palette.as_slice());
}

fn main() {
	match Args::parse().command {
		Command::Apply(args) => run_apply(args),
		Command::Extract(args) => run_extract(args),
		Command::Palette { action } => run_palette(action),
	}
}

fn resolve_palette(args: &ApplyArgs, input: &Image) -> Palette {
	if let Some(path) = &args.palette_from {
		let count = args.extract.unwrap_or(DEFAULT_EXTRACT);
		let source = read_image(path);
		repalette::extract_palette(source, count, EXTRACT_THRESHOLD)
	} else if let Some(count) = args.extract {
		repalette::extract_palette(input.clone(), count, EXTRACT_THRESHOLD)
	} else if let Some(name) = args.palette.as_deref() {
		let colors = palettes::get(name).unwrap_or_else(|| unknown_preset(name));
		Palette::from_colors(colors)
	} else if let Some(custom) = args.colors.as_deref() {
		Palette::parse(custom).unwrap_or_else(|err| parse_failed(err))
	} else {
		Palette::default()
	}
}

fn run_apply(args: ApplyArgs) {
	let format = ImageFormat::from_path(&args.output)
		.map_err(|err| format!("Failed to detect output format: {err}"))
		.unwrap_or_else(print_and_exit);

	let in_file = File::open(&args.input)
		.map_err(|err| format!("Failed to open the source image: {err}"))
		.unwrap_or_else(print_and_exit);
	let mut reader = BufReader::new(in_file);

	let mut img = Image::read(&mut reader).unwrap_or_else(print_and_exit);

	let out_file = File::create(&args.output)
		.map_err(|err| format!("Failed to open the target image: {err}"))
		.unwrap_or_else(print_and_exit);

	let mut writer = BufWriter::new(out_file);

	let palette = resolve_palette(&args, &img);
	let colors = palette.as_slice();

	// Multisample output is truecolor (averaged pixels aren't palette-restricted),
	// so it always bypasses the indexed-PNG path.
	if format == ImageFormat::Png && !args.multisample {
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
		if args.multisample {
			repalette::process_multisample(&mut img, &palette, &args.dither);
		} else {
			repalette::process(&mut img, &palette, &args.dither);
		}

		img
			.write(&mut writer, format)
			.unwrap_or_else(print_and_exit);
	}
}
