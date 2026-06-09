fn main() {
  cc::Build::new()
    .file("main.c")
    .file("repalette.c")
    .std("c99")
    .opt_level(3)
    .flag("-march=native")
    .flag("-mtune=native")
    .compile("repalette_core");
}
