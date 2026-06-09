use std::ffi::CString;
use std::path::Path;

use clap::Parser;
use image::{DynamicImage, ImageError, ImageReader};
use image::error::UnsupportedErrorKind;

#[derive(Parser, Debug)]
#[command(disable_help_flag = true)]
struct Args {
  input: Option<Box<Path>>,
  output: Option<Box<Path>>,

  #[arg(short, long, default_value="")]
  palette: CString,

  #[arg(short, long, default_value="")]
  dither: CString,

  #[arg(short = 'h', long = "help")]
  help: bool,
}

fn help() {
  unimplemented!("TODO: print help from C");
}

fn main() {
  let args = Args::parse();

  if args.help {
    help();
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
      eprintln!("ERROR: Failed to open image '{}': {}", input.display(), error);
      std::process::exit(1);
    })
    .with_guessed_format()
    .unwrap_or_else(|error| {
      eprintln!("ERROR: Failed to detect image format of '{}': {}", input.display(), error);
      std::process::exit(1);
    })
    .decode()
    .unwrap_or_else(|error| {
      eprintln!("ERROR: Failed to decode image '{}': {}", input.display(), error);
      std::process::exit(1);
    })
    .into_rgba8();

  let width = image.width();
  let height = image.height();
  let pixels = image.into_raw();

  // TODO: recolor `pixels` in place via the C core, e.g.
  //   repalette_process(pixels.as_mut_ptr(), width, height,
  //                     args.palette.as_ptr(), args.dither.as_ptr())
  let _ = (&args.palette, &args.dither);

  let image = image::RgbaImage::from_raw(width, height, pixels).unwrap();

let result = match image.save(output) {
    Err(ImageError::Unsupported(e))
      if matches!(e.kind(), UnsupportedErrorKind::Color(_)) =>
    {
      // Encoder rejected RGBA (e.g. JPEG) — drop alpha and retry as RGB.
      DynamicImage::ImageRgba8(image).into_rgb8().save(output)
    }
    other => other,
  };

  result.unwrap_or_else(|error| {
    eprintln!("ERROR: Failed to save image '{}': {}", output.display(), error);
    std::process::exit(1);
  });
}
