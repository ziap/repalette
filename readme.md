# Repalette

Repalette is an image colorizer written in C and Rust. It is available as both
a CLI utility and a web application.

![](img/preview.png)

## Features

- Recolor images using nearest neighbor search.
- Optional dithering for smoother results.
- SIMD accelerated palette search and dithering.
 

**Web-version:**

- WASM-powered web application (mostly for mobile usage)
- Fetch color schemes from [Gosh](https://gogh-co.github.io/Gogh/)
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

```
cargo build --release
```

Build the WASM version

```
cargo build --release --features wasm
```

## Usage

### CLI version

```yaml
USAGE:
  repalette -h | --help
  repalette <input file> <output file> [options]

OPTIONS:
  -p, --palette COLOR[,COLOR...]
  -d, --dither <ditherer>

DITHERER: none | fs | atkinson | jjn | burkes | sierra32 | sierra4
  none     - No dithering
  fs       - Floyd-Steinberg
  atkinson - Atkinson
  jjn      - Jarvis, Judice, and Ninke
  burkes   - Burkes
  sierra32 - Sierra
  sierra4  - Sierra Lite
```

- The default color palette is `000000,ffffff` (black and white)
- The default dithering algorithm is `fs` (Floyd-Steinberg)

Example:

```
repalette input.jpg output-no-dither.png -p 000000,ff0000,ffffff --dither none
repalette input.jpg output-burkes.png -p 000000,ff0000,ffffff --dither burkes
```

| input.jpg          | output-no-dither.png          | output-burkes.png          |
| ------------------ | ----------------------------- | -------------------------- |
| ![](img/input.jpg) | ![](img/output-no-dither.png) | ![](img/output-burkes.png) |

### Web version

Host the website with any server or go to <https://ziap.github.io/repalette>.

The user interface:

![](img/web-ui.png)

## License

This project is licensed under the [AGPL-3.0 license](LICENSE).
