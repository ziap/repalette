#![no_std]

extern crate alloc;

mod ffi;
mod palette;

#[cfg(test)]
mod tests;

pub use ffi::*;
pub use palette::*;
