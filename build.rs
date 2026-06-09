use std::env;
use std::path::Path;
use std::process::Command;

fn main() {
  let manifest = env::var("CARGO_MANIFEST_DIR").unwrap();
  let out = Path::new(&manifest).join("repalette.wasm");
  let cc = env::var("CLANG").unwrap_or_else(|_| "clang".into());

  let args = [
    "-O3",
    "-Wall",
    "-Wextra",
    "-pedantic",
    "-std=c99",
  ];

  let wasm_args = [
    "--target=wasm32",
    "-flto",
    "-nostdlib",
    "-fvisibility=hidden",
    "-mbulk-memory",
    "-msimd128",
    "-Wl,--no-entry",
    "-Wl,--strip-all",
    "-Wl,--lto-O3",
    "-Wl,--allow-undefined",
    "-Wl,--export-dynamic",
  ];

  let status = Command::new(&cc)
    .args(args.iter().copied().chain(wasm_args))
    .arg("-o")
    .arg(&out)
    .args(["wasm_main.c", "repalette.c"])
    .status()
    .unwrap_or_else(|e| panic!("failed to invoke `{cc}` for the wasm build: {e}"));

  assert!(status.success(), "wasm build failed: `{cc}` exited with {status}");
}
