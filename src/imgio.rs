use image::{ImageError, ImageReader, RgbImage};
use std::fmt::{self, Display, Formatter};
use std::io;
use std::path::Path;

pub struct Image {
	pub width: u32,
	pub height: u32,
	pub pixels: Vec<u8>,
}

pub enum ReadError {
	OpenError(io::Error),
	DetectFormatError(io::Error),
	DecodeError(ImageError),
}

pub struct WriteError(ImageError);

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
		let Self(err) = self;
		write!(f, "Failed to save image: {err}")
	}
}

impl Image {
	pub fn read(path: &Path) -> Result<Self, ReadError> {
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

	pub fn write(self, path: &Path) -> Result<(), WriteError> {
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
			.save(path)
			.map_err(WriteError)
	}
}
