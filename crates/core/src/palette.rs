#[derive(Debug)]
pub enum HexError<'a> {
	BadChar { color: &'a [u8], pos: usize },
	WrongDigits(&'a [u8]),
}

#[derive(Debug)]
pub enum ParseError<'a> {
	Hex(HexError<'a>),
	TooLarge,
}

const fn subbytes(bytes: &[u8], start: usize, end: usize) -> &[u8] {
	bytes.split_at(end).0.split_at(start).1
}

pub struct HexResult {
	pub color: [u8; 3],
	pub next: usize,
}

pub const fn parse_hex<'a>(b: &'a [u8], pos: usize, term: u8) -> Result<HexResult, HexError<'a>> {
	use HexError::*;

	let mut next = pos;
	let mut color = 0u32;
	let mut digits = 0u32;
	while next < b.len() && b[next] != term {
		let c = b[next];
		let d = match c {
			b'0'..=b'9' => c - b'0',
			b'a'..=b'f' => c - b'a' + 10,
			b'A'..=b'F' => c - b'A' + 10,
			_ => {
				let mut end = next;
				while end < b.len() && b[end] != term {
					end += 1;
				}
				return Err(BadChar {
					color: subbytes(b, pos, end),
					pos: next - pos,
				});
			}
		} as u32;
		digits += 1;
		if digits > 6 {
			let mut end = next;
			while end < b.len() && b[end] != b',' {
				end += 1;
			}
			return Err(WrongDigits(subbytes(b, pos, end)));
		}
		color = (color << 4) | d;
		next += 1;
	}

	if digits != 6 {
		Err(WrongDigits(subbytes(b, pos, next)))
	} else {
		Ok(HexResult {
			color: [(color >> 16) as u8, (color >> 8) as u8, color as u8],
			next,
		})
	}
}

#[derive(Debug)]
pub struct Palette {
	size: usize,
	data: [[u8; 3]; 256],
}

impl Default for Palette {
	fn default() -> Self {
		let mut out = Self {
			size: 2,
			data: [[0; 3]; 256],
		};
		out.data[1] = [0xff, 0xff, 0xff];
		out
	}
}

impl Palette {
	pub fn parse<'a>(s: &'a str) -> Result<Self, ParseError<'a>> {
		use ParseError::*;

		let b = s.as_bytes();
		let mut out = Self {
			size: 0,
			data: [[0; 3]; 256],
		};
		let mut pos = 0;
		loop {
			let HexResult { color, next } = parse_hex(b, pos, b',').map_err(Hex)?;

			if out.size < 256 {
				out.data[out.size] = color;
				out.size += 1;
			} else {
				return Err(TooLarge);
			}

			if next >= b.len() {
				return Ok(out);
			}
			pos = next + 1; // skip the comma
		}
	}

	pub fn from_colors(colors: &[[u8; 3]]) -> Self {
		let size = colors.len();
		let mut data = [[0; 3]; 256];
		data[..size].copy_from_slice(colors);
		Self { data, size }
	}

	pub fn as_slice(&self) -> &[[u8; 3]] {
		&self.data[..self.size]
	}
}
