use std::ffi::{CString, c_char};
use std::path::Path;

use clap::{Parser, Subcommand};
use image::error::UnsupportedErrorKind;
use image::{DynamicImage, ImageError, ImageReader};

mod palettes;

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
#[command(disable_help_flag = true, args_conflicts_with_subcommands = true)]
struct Args {
	#[command(subcommand)]
	command: Option<Command>,

	input: Option<Box<Path>>,
	output: Option<Box<Path>>,

	/// Built-in palette preset (see `repalette palette list`)
	#[arg(short, long)]
	palette: Option<String>,

	/// Manual palette: comma-separated hex colors, e.g. 000000,ffffff
	#[arg(short = 'c', long, conflicts_with = "palette")]
	colors: Option<CString>,

	#[arg(short, long, default_value = "")]
	dither: CString,

	#[arg(short = 'h', long = "help")]
	help: bool,
}

#[derive(Subcommand, Debug)]
enum Command {
	/// Inspect the built-in palette presets
	Palette {
		#[command(subcommand)]
		action: PaletteAction,
	},
}

#[derive(Subcommand, Debug)]
enum PaletteAction {
	/// List every preset name
	List,
	/// Print a preset's colors
	Show { name: String },
}

fn unknown_preset(name: &str) -> ! {
	eprintln!("ERROR: unknown palette preset '{name}' (try `repalette palette list`)");
	std::process::exit(1);
}

fn run_palette(action: PaletteAction) {
	match action {
		PaletteAction::List => {
			for name in palettes::NAMES {
				println!("{name}");
			}
		}
		PaletteAction::Show { name } => match palettes::get(&name) {
			Some(colors) => println!("{colors}"),
			None => unknown_preset(&name),
		},
	}
}

fn resolve_palette(preset: Option<&str>, colors: Option<CString>) -> CString {
	match (preset, colors) {
		(Some(name), _) => match palettes::get(name) {
			Some(hex) => CString::new(hex).unwrap(),
			None => unknown_preset(name),
		},
		(None, Some(colors)) => colors,
		(None, None) => CString::default(),
	}
}

fn main() {
	let args = Args::parse();

	if args.help {
		unsafe { repalette_help() };
		return;
	}

	if let Some(Command::Palette { action }) = args.command {
		run_palette(action);
		return;
	}

	let palette = resolve_palette(args.palette.as_deref(), args.colors);

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
			palette.as_ptr(),
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
