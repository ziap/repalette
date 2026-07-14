# Repalette

Repalette is an image colorizer written in C and Rust. It is available as both
a CLI utility and a web application.

![](.github/img/preview.png)

## Features

- Recolor images using nearest neighbor search.
- Optional dithering for smoother results.
- Palette extraction from any image
- SIMD accelerated palette search and dithering.
- Built-in palette presets bundled into the binary at compile time.
- Optimized encoding for small-palette PNG export.

**Web-version:**

- WASM-powered web application (mostly for mobile usage)
- Support for both text-based and color-picker-based palette editing
- Responsive layout and adaptive light/dark theme

## Benchmark

(TBA)

To run the benchmark yourself:

- Install [hyperfine](https://github.com/sharkdp/hyperfine)
- Build the CLI version
- Put an image with the name `test.jpg` in the same directory as `benchmark.sh`
- Run `./benchmark.sh`

## Building

**You need:**

- Both: Rust
- For native build: Any C compiler that supports C99.
- For WASM build: Clang, LLVM, and LLD.

Build the native version

```sh
cargo build --release
```

Build the WASM version

```sh
build.sh
```

## Usage

### CLI version

Examples:

```sh
# a built-in preset
repalette apply input.jpg output.png -p nord

# a manual palette (6-digit hex, no '#')
repalette apply input.jpg output-no-dither.png -c 000000,ff0000,ffffff --dither none
repalette apply input.jpg output-burkes.png -c 000000,ff0000,ffffff --dither burkes

# recolor using a palette extracted from the image itself (compression)
repalette apply input.jpg output-extracted.png -k 32

# recolor using a palette extracted from a different reference image
repalette apply input.jpg output-transfer.png --palette-from reference.jpg

# browse the presets
repalette palette list
repalette palette show gruvbox-dark

# extract an N-color palette from an image
repalette extract input.jpg -k 16

```

| input.jpg                  | output-no-dither.png                  | output-burkes.png                  |
| -------------------------- | ------------------------------------- | ---------------------------------- |
| ![](.github/img/input.jpg) | ![](.github/img/output-no-dither.png) | ![](.github/img/output-burkes.png) |

Run `repalette --help` to learn more 

### Web version

Host the website with any server or go to <https://ziap.github.io/repalette>.

The user interface:

![](img/web-ui.png)

## License

This project is licensed under the [AGPL-3.0 license](LICENSE).
