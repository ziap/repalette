#![no_std]

use repalette_core::{HexError, HexResult, Palette, ParseError, parse_hex};

const RAW: &[u8] = include_bytes!("data/palettes.txt");

const fn subbytes(bytes: &[u8], start: usize, end: usize) -> &[u8] {
	bytes.split_at(end).0.split_at(start).1
}

struct Sizes {
	n: usize,
	min_len: usize,
	max_len: usize,
	key_bytes: usize,
	value_count: usize,
}

// Pass 1: validate structure + the 6-digit-per-color condition and tally the
// sizes needed to size the const pools.
const SIZES: Sizes = {
	let b = RAW;
	let len = b.len();
	if len == 0 {
		panic!("palettes.txt is empty")
	}
	if b[len - 1] != b'\n' {
		panic!("palettes.txt must end with a newline")
	}

	let mut n = 0;
	let mut min_len = usize::MAX;
	let mut max_len = 0;
	let mut key_bytes = 0;
	let mut value_count = 0;
	let mut i = 0;
	while i < len {
		// name: non-empty, lowercase alphanumeric or '-', up to the ':'
		let name_start = i;
		while i < len && b[i] != b':' && b[i] != b'\n' {
			let c = b[i];
			if !((c >= b'a' && c <= b'z') || (c >= b'0' && c <= b'9') || c == b'-') {
				panic!("palettes.txt: name must be lowercase alphanumeric or '-'")
			}
			i += 1;
		}
		if i >= len || b[i] != b':' {
			panic!("palettes.txt: expected ':' after the preset name")
		}
		let name_len = i - name_start;
		if name_len == 0 {
			panic!("palettes.txt: empty preset name")
		}
		if name_len < min_len {
			min_len = name_len;
		}
		if name_len > max_len {
			max_len = name_len;
		}
		key_bytes += name_len;
		i += 1; // ':'

		// separator: zero or more spaces
		while i < len && b[i] == b' ' {
			i += 1;
		}

		// values: space-separated `key=RRGGBB` pairs. Validate the key then drop it.
		let mut colors = 0;
		while i < len && b[i] != b'\n' {
			// key: non-empty, lowercase alphanumeric or '-', up to '=' (no space before)
			let key_start = i;
			while i < len && b[i] != b'=' && b[i] != b' ' && b[i] != b'\n' {
				let c = b[i];
				if !((c >= b'a' && c <= b'z') || (c >= b'0' && c <= b'9') || c == b'-') {
					panic!("palettes.txt: key must be lowercase alphanumeric or '-'")
				}
				i += 1;
			}
			if i == key_start {
				panic!("palettes.txt: empty key")
			}
			if i >= len || b[i] != b'=' {
				panic!("palettes.txt: expected '=' immediately after key (no space around '=')")
			}
			i += 1; // '='

			// value: exactly 6 digits, no space after '='
			let mut digits = 0;
			while i < len && b[i] != b' ' && b[i] != b'\n' {
				digits += 1;
				if digits > 6 {
					panic!("palettes.txt: each color must be 6 hex digits")
				}
				i += 1;
			}
			if digits != 6 {
				panic!("palettes.txt: each color must be 6 hex digits")
			}
			colors += 1;

			// one or more spaces between pairs
			while i < len && b[i] == b' ' {
				i += 1;
			}
		}

		if colors > 256 {
			panic!("palettes.txt: too many colors")
		}

		value_count += colors;
		i += 1; // '\n'
		n += 1;
	}

	Sizes {
		n,
		min_len,
		max_len,
		key_bytes,
		value_count,
	}
};

const N: usize = SIZES.n;
const MIN_LEN: usize = SIZES.min_len;
const MAX_LEN: usize = SIZES.max_len;
const NBINS: usize = MAX_LEN + 1 - MIN_LEN;
const KEY_POOL_SIZE: usize = SIZES.key_bytes;
const VALUE_POOL_SIZE: usize = SIZES.value_count;

struct StringMap {
	key_pool: [u8; KEY_POOL_SIZE],
	value_pool: [[u8; 3]; VALUE_POOL_SIZE],
	key_off: [usize; NBINS + 1],
	starts: [usize; NBINS + 1],
	val_off: [usize; N + 1],
	name_at: [(usize, usize); N], // per entry (file order): (offset in key_pool, len)
}

// Pass 2: fill the pools. Entries are counting-sorted by key length so each bin
// is a contiguous run of same-length keys.
const MAP: StringMap = {
	let b = RAW;
	let len = b.len();

	// per-entry spans in file order
	let mut name_off = [0usize; N];
	let mut name_len = [0usize; N];
	let mut col_off = [0usize; N];
	let mut col_end = [0usize; N];
	let mut i = 0;
	let mut e = 0;
	while i < len {
		let ns = i;
		while b[i] != b':' {
			i += 1;
		}
		name_off[e] = ns;
		name_len[e] = i - ns;
		i += 1; // ':'
		while i < len && b[i] == b' ' {
			i += 1;
		}
		col_off[e] = i;
		while b[i] != b'\n' {
			i += 1;
		}
		col_end[e] = i;
		i += 1; // '\n'
		e += 1;
	}

	// counting sort by key length
	let mut cnt = [0usize; NBINS];
	let mut k = 0;
	while k < N {
		cnt[name_len[k] - MIN_LEN] += 1;
		k += 1;
	}
	let mut starts = [0usize; NBINS + 1];
	let mut key_off = [0usize; NBINS + 1];
	let mut acc = 0;
	let mut koff = 0;
	let mut bin = 0;
	while bin < NBINS {
		let stride = MIN_LEN + bin;
		starts[bin] = acc;
		key_off[bin] = koff;
		acc += cnt[bin];
		koff += cnt[bin] * stride;
		bin += 1;
	}
	starts[NBINS] = acc;
	key_off[NBINS] = koff;

	// stable placement: order[sorted position] = original entry index
	let mut cursor = starts;
	let mut order = [0usize; N];
	let mut j = 0;
	while j < N {
		let bin = name_len[j] - MIN_LEN;
		order[cursor[bin]] = j;
		cursor[bin] += 1;
		j += 1;
	}

	// fill pools in sorted order
	let mut key_pool = [0u8; KEY_POOL_SIZE];
	let mut value_pool = [[0u8; 3]; VALUE_POOL_SIZE];
	let mut val_off = [0usize; N + 1];
	let mut name_at = [(0usize, 0usize); N];
	let mut kp = 0;
	let mut vp = 0;
	let mut s = 0;
	while s < N {
		let ent = order[s];

		let key = subbytes(RAW, name_off[ent], name_off[ent] + name_len[ent]);
		name_at[ent] = (kp, key.len()); // this key's span in key_pool, scattered by order
		let mut t = 0;
		while t < key.len() {
			key_pool[kp] = key[t];
			kp += 1;
			t += 1;
		}

		val_off[s] = vp;
		let colors = subbytes(RAW, col_off[ent], col_end[ent]);
		let mut p = 0;
		while p < colors.len() {
			// skip the key and '=' — keep only the value
			while p < colors.len() && colors[p] != b'=' {
				p += 1;
			}
			p += 1; // '='
			match parse_hex(colors, p, b' ') {
				Ok(HexResult { color, next }) => {
					value_pool[vp] = color;
					vp += 1;
					p = next;
				}
				Err(HexError::BadChar { .. }) => {
					panic!("palettes.txt: colors must be hex digits")
				}
				Err(HexError::WrongDigits(_)) => {
					panic!("palettes.txt: each color must be 6 hex digits")
				}
			}
			// skip the space separators
			while p < colors.len() && colors[p] == b' ' {
				p += 1;
			}
		}
		s += 1;
	}
	val_off[N] = vp;

	StringMap {
		key_pool,
		value_pool,
		key_off,
		starts,
		val_off,
		name_at,
	}
};

pub fn get(name: &str) -> Option<&'static [[u8; 3]]> {
	let key = name.as_bytes();
	let len = key.len();
	if len < MIN_LEN || len > MAX_LEN {
		return None;
	}

	let bin = len - MIN_LEN;
	let bucket = &MAP.key_pool[MAP.key_off[bin]..MAP.key_off[bin + 1]];
	for (chunk, i) in bucket.chunks_exact(len).zip(MAP.starts[bin]..) {
		if chunk == key {
			return Some(&MAP.value_pool[MAP.val_off[i]..MAP.val_off[i + 1]]);
		}
	}
	None
}

pub const NAMES: [&'static [u8]; N] = {
	let mut names: [&[u8]; N] = [&[]; N];
	let mut j = 0;
	while j < N {
		let (off, len) = MAP.name_at[j];
		names[j] = subbytes(&MAP.key_pool, off, off + len);
		j += 1;
	}
	names
};

pub enum ResolveError<'a> {
	NotFound(&'a str),
	ParseFailed(ParseError<'a>),
}

pub fn resolve<'a>(
	preset: Option<&'a str>,
	custom: Option<&'a str>,
) -> Result<Palette, ResolveError<'a>> {
	use ResolveError::*;

	match (preset, custom) {
		(Some(name), _) => {
			let colors = get(name).ok_or(NotFound(name))?;
			Ok(Palette::from_colors(colors))
		}
		(None, Some(s)) => Palette::parse(s).map_err(ParseFailed),
		(None, None) => Ok(Palette::default()),
	}
}
