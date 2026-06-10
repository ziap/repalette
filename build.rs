use std::process::Command;

fn main() {
  let args = [
    "-O3",
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-pedantic",
    "-s",
  ];

  let native_args = [
    "-march=native",
    "-mtune=native",
  ];

  cc::Build::new()
    .files([
      "main.c",
      "repalette.c",
    ])
    .std("c99")
    .opt_level(3)
    .flags(args.iter())
    .flags(native_args.iter())
    .compile("repalette_core");

  if std::env::var_os("CARGO_FEATURE_WASM").is_some() {
    build_wasm(&args);
  }
}

fn build_wasm(args: &[&str]) {
  let wasm_flags = [
    "--target=wasm32",
    "-flto",
    "-nostdlib",
    "-fvisibility=hidden",
    "-mbulk-memory",
    "-msimd128",
  ];

  let wasm_ldflags = [
    "--no-entry",
    "--strip-all",
    "--lto-O3",
    "--allow-undefined",
    "--export-dynamic",
  ];

  let clang = std::env::var("CC_WASM").unwrap_or_else(|_| "clang".into());

  let status = Command::new(&clang)
    .args(args)
    .args(wasm_flags)
    .args(wasm_ldflags.iter().map(|flag| format!("-Wl,{flag}")))
    .args(["-o", "repalette.wasm"])
    .args(["wasm_main.c", "repalette.c"])
    .status()
    .unwrap_or_else(|e| panic!("failed to invoke `{clang}` for the wasm build: {e}"));

  assert!(status.success(), "wasm build failed: `{clang}` exited with {status}");
}
