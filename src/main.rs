use std::ffi::CString;
use std::path::Path;

use clap::{ArgAction, Parser};

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


  dbg!(args.palette);
  dbg!(args.dither);
}
