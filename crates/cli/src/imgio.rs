use image::{ImageError, ImageFormat, ImageReader, RgbImage};
use repalette_core::Image;
use std::fmt::{self, Display, Formatter};
use std::fs::File;
use std::io::{self, BufWriter};
use std::path::Path;

pub enum ReadError {
	OpenError(io::Error),
	DetectFormatError(io::Error),
	DecodeError(ImageError),
}

pub enum WriteError {
	Encode(ImageError),
	Png(png::EncodingError),
	Create(io::Error),
}

impl Display for ReadError {
	fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
		match self {
			Self::OpenError(err) => {
				write!(f, "Failed to open image: {err}")
			}
			Self::DetectFormatError(err) => {
				write!(f, "Failed to detect image format: {err}")
			}
			Self::DecodeError(err) => {
				write!(f, "Failed to decode image: {err}")
			}
		}
	}
}

impl Display for WriteError {
	fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
		match self {
			Self::Encode(err) => write!(f, "Failed to save image: {err}"),
			Self::Png(err) => write!(f, "Failed to save image: {err}"),
			Self::Create(err) => write!(f, "Failed to create output file: {err}"),
		}
	}
}

pub trait ImageIo: Sized {
	fn read(path: &Path) -> Result<Self, ReadError>;
	fn write(self, path: &Path, format: ImageFormat) -> Result<(), WriteError>;
}

impl ImageIo for Image {
	fn read(path: &Path) -> Result<Self, ReadError> {
		let img = ImageReader::open(path)
			.map_err(ReadError::OpenError)?
			.with_guessed_format()
			.map_err(ReadError::DetectFormatError)?
			.decode()
			.map_err(ReadError::DecodeError)?
			.into_rgba8();

		Ok(Self {
			width: img.width(),
			height: img.height(),
			pixels: img.into_raw(),
		})
	}

	fn write(self, path: &Path, format: ImageFormat) -> Result<(), WriteError> {
		let size = (self.width * self.height) as usize;

		// Convert to RGB
		let pixels = {
			let mut pixels = self.pixels;
			for idx in 0..size {
				let src = idx * 4..idx * 4 + 3;
				let dst = idx * 3;
				pixels.copy_within(src, dst);
			}
			pixels.resize(size * 3, 0);
			pixels
		};

		RgbImage::from_raw(self.width, self.height, pixels)
			.unwrap()
			.save_with_format(path, format)
			.map_err(WriteError::Encode)
	}
}

pub struct IndexedImage<'a> {
	pub width: u32,
	pub height: u32,
	pub indices: Vec<u8>,
	pub colors: &'a [u32],
}

impl<'a> IndexedImage<'a> {
	pub fn write_png(mut self, path: &Path) -> Result<(), WriteError> {
		let mut plte = Vec::with_capacity(self.colors.len() * 3);
		for &c in self.colors {
			plte.extend_from_slice(&[(c >> 16) as u8, (c >> 8) as u8, c as u8]);
		}

		let depth = if self.colors.len() <= 16 {
			let width = self.width as usize;
			let row_bytes = (width + 1) / 2;
			for r in 0..self.height as usize {
				let (src, dst) = (r * width, r * row_bytes);
				let end = width / 2;
				for j in 0..end {
					let lo = self.indices[src + 2 * j + 0];
					let hi = self.indices[src + 2 * j + 1];
					self.indices[dst + j] = (lo << 4) | hi;
				}

				if width % 2 == 1 {
					self.indices[dst + end] = self.indices[src + width - 1] << 4;
				}
			}
			self.indices.truncate(row_bytes * self.height as usize);
			png::BitDepth::Four
		} else {
			png::BitDepth::Eight
		};

		let file = File::create(path).map_err(WriteError::Create)?;
		let mut enc = png::Encoder::new(BufWriter::new(file), self.width, self.height);
		enc.set_color(png::ColorType::Indexed);
		enc.set_depth(depth);
		enc.set_palette(plte);
		enc.set_compression(png::Compression::Balanced);

		let mut writer = enc.write_header().map_err(WriteError::Png)?;
		writer
			.write_image_data(&self.indices)
			.map_err(WriteError::Png)
	}
}
