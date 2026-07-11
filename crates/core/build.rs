fn main() {
	for path in [
		"src/c/cli.c",
		"src/c/repalette.c",
		"src/c/extract.c",
		"src/c/oklab.c",
		"src/c/repalette.h",
		"src/c/extract.h",
		"src/c/oklab.h",
	] {
		println!("cargo:rerun-if-changed={path}");
	}

	let args = ["-O3", "-std=c99", "-Wall", "-Wextra", "-pedantic"];

	let native_args = ["-march=native", "-mtune=native"];

	cc::Build::new()
		.files([
			"src/c/cli.c",
			"src/c/repalette.c",
			"src/c/extract.c",
			"src/c/oklab.c",
		])
		.std("c99")
		.opt_level(3)
		.flags(args.iter())
		.flags(native_args.iter())
		.compile("repalette_core");

	// extract.c pulls in libm (cbrtf / powf) for the OKLab transform.
	println!("cargo:rustc-link-lib=m");
}
