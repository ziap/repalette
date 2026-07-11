use image::{ImageError, ImageFormat, ImageReader, RgbImage};
use repalette_core::Image;
use std::fmt::{self, Display, Formatter};
use std::io::{self, BufRead, Seek, Write};

#[cfg(test)]
mod tests;

#[derive(Debug)]
pub enum ReadError {
	DetectFormatError(io::Error),
	DecodeError(ImageError),
}

#[derive(Debug)]
pub enum WriteError {
	Encode(ImageError),
	Png(png::EncodingError),
}

impl Display for ReadError {
	fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
		match self {
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
		}
	}
}

pub trait ImageIo: Sized {
	fn read<R>(r: &mut R) -> Result<Self, ReadError>
	where
		R: BufRead + Seek;

	fn write<W>(self, w: &mut W, format: ImageFormat) -> Result<(), WriteError>
	where
		W: Write + Seek;
}

impl ImageIo for Image {
	fn read<R>(r: &mut R) -> Result<Self, ReadError>
	where
		R: BufRead + Seek,
	{
		let img = ImageReader::new(r)
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

	fn write<W>(self, w: &mut W, format: ImageFormat) -> Result<(), WriteError>
	where
		W: Write + Seek,
	{
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
			.write_to(w, format)
			.map_err(WriteError::Encode)
	}
}

pub struct IndexedImage<'a> {
	pub width: u32,
	pub height: u32,
	pub indices: Vec<u8>,
	pub colors: &'a [[u8; 3]],
}

impl<'a> IndexedImage<'a> {
	pub fn write_png(mut self, w: &mut impl Write) -> Result<(), WriteError> {
		use WriteError::*;

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

		let mut enc = png::Encoder::new(w, self.width, self.height);
		enc.set_color(png::ColorType::Indexed);
		enc.set_depth(depth);
		enc.set_palette(self.colors.as_flattened());
		enc.set_compression(png::Compression::Balanced);
		enc.set_filter(png::Filter::NoFilter);

		let mut writer = enc.write_header().map_err(Png)?;
		writer.write_image_data(&self.indices).map_err(Png)
	}
}
