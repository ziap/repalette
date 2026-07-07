fn main() {
	for path in ["src/c/cli.c", "src/c/repalette.c", "src/c/repalette.h"] {
		println!("cargo:rerun-if-changed={path}");
	}

	let args = ["-O3", "-std=c99", "-Wall", "-Wextra", "-pedantic"];

	let native_args = ["-march=native", "-mtune=native"];

	cc::Build::new()
		.files(["src/c/cli.c", "src/c/repalette.c"])
		.std("c99")
		.opt_level(3)
		.flags(args.iter())
		.flags(native_args.iter())
		.compile("repalette_core");
}
