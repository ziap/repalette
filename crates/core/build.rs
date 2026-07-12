use std::io::Write;
use std::process::Command;

const SOURCES: [&str; 5] = [
	"src/c/dither.c",
	"src/c/oklab.c",
	"src/c/histogram.c",
	"src/c/extract.c",
	"src/c/cli.c",
];

fn main() {
	let args = ["-O3", "-std=c99", "-Wall", "-Wextra", "-pedantic"];
	let native_args = ["-march=native", "-mtune=native"];

	let mut build = cc::Build::new();
	build
		.files(SOURCES)
		.std("c99")
		.opt_level(3)
		.flags(args.iter())
		.flags(native_args.iter());

	// Automatically generate dependencies using -MM
	let compiler = build.get_compiler();
	let mut stdout = std::io::stdout().lock();
	for src in SOURCES {
		let out = Command::new(compiler.path())
			.arg("-MM")
			.arg(src)
			.output()
			.expect("running the C compiler for the header dependency scan");

		// Output is a Make rule: `foo.o: foo.c bar.h baz.h`. Skip the `foo.o:`
		// target and line-continuation backslashes; the rest are dependencies.
		for token in out.stdout.split(|b| b.is_ascii_whitespace()) {
			if token.is_empty() || token.ends_with(b":") || token == b"\\" {
				continue;
			}
			_ = stdout.write_all(b"cargo:rerun-if-changed=");
			_ = stdout.write_all(token);
			_ = stdout.write_all(b"\n");
		}
	}
	drop(stdout);

	build.compile("repalette_core");

	// TODO: remove libm
	println!("cargo:rustc-link-lib=m");
}
