const RAW: &str = include_str!("palettes.txt");

#[derive(Clone, Copy)]
struct Entry {
	key: &'static str,
	value: &'static str,
}

const EMPTY: Entry = Entry { key: "", value: "" };

const fn substr(start: usize, end: usize) -> &'static str {
	RAW.split_at(end).0.split_at(start).1
}

const fn count_lines(s: &str) -> usize {
	let b = s.as_bytes();
	let mut i = 0;
	let mut n = 0;
	while i < b.len() {
		if b[i] == b'\n' {
			n += 1;
		}
		i += 1;
	}
	n
}

const N: usize = count_lines(RAW);

struct Parsed {
	min_len: usize,
	max_len: usize,
	entries: [Entry; N],
}

const fn parse() -> Parsed {
	let b = RAW.as_bytes();
	let len = b.len();
	if len == 0 {
		panic!("palettes.txt is empty")
	}
	if b[len - 1] != b'\n' {
		panic!("palettes.txt must end with a newline")
	}

	let mut entries = [EMPTY; N];
	let mut min_len = usize::MAX;
	let mut max_len = 0;
	let mut i = 0;
	let mut e = 0;
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
		let name_end = i;
		let name_len = name_end - name_start;
		if name_len == 0 {
			panic!("palettes.txt: empty preset name")
		}
		if name_len < min_len {
			min_len = name_len;
		}
		if name_len > max_len {
			max_len = name_len;
		}
		i += 1; // ':'

		// separator: zero or more spaces
		while i < len && b[i] == b' ' {
			i += 1;
		}

		// colors: comma-separated groups of exactly 6 hex digits (any case)
		let val_start = i;
		let mut digits = 0;
		while i < len && b[i] != b'\n' {
			let c = b[i];
			if c == b',' {
				if digits != 6 {
					panic!("palettes.txt: each color must be 6 hex digits")
				}
				digits = 0;
			} else if (c >= b'0' && c <= b'9') || (c >= b'a' && c <= b'f') || (c >= b'A' && c <= b'F') {
				digits += 1;
				if digits > 6 {
					panic!("palettes.txt: each color must be 6 hex digits")
				}
			} else {
				panic!("palettes.txt: colors must be hex digits")
			}
			i += 1;
		}
		if digits != 6 {
			panic!("palettes.txt: each color must be 6 hex digits")
		}
		let val_end = i;
		i += 1; // '\n'

		entries[e] = Entry {
			key: substr(name_start, name_end),
			value: substr(val_start, val_end),
		};
		e += 1;
	}

	Parsed {
		min_len,
		max_len,
		entries,
	}
}

const PARSED: Parsed = parse();
const MIN_LEN: usize = PARSED.min_len;
const MAX_LEN: usize = PARSED.max_len;
const NBINS: usize = MAX_LEN + 1 - MIN_LEN;

struct StringMap {
	entries: [Entry; N],
	starts: [usize; NBINS + 1],
}

const fn build() -> StringMap {
	let src = PARSED.entries;

	let mut cnt = [0usize; NBINS];
	let mut i = 0;
	while i < N {
		cnt[src[i].key.len() - MIN_LEN] += 1;
		i += 1;
	}

	let mut starts = [0usize; NBINS + 1];
	let mut acc = 0;
	let mut bin = 0;
	while bin < NBINS {
		starts[bin] = acc;
		acc += cnt[bin];
		bin += 1;
	}
	starts[NBINS] = acc;

	let mut cursor = starts;
	let mut entries = [EMPTY; N];
	let mut j = 0;
	while j < N {
		let bin = src[j].key.len() - MIN_LEN;
		let pos = cursor[bin];
		entries[pos] = src[j];
		cursor[bin] += 1;
		j += 1;
	}

	StringMap { entries, starts }
}

static MAP: StringMap = build();

pub fn get(name: &str) -> Option<&'static str> {
	let len = name.len();
	if len < MIN_LEN || len > MAX_LEN {
		return None;
	}

	let bin = len - MIN_LEN;
	for i in MAP.starts[bin]..MAP.starts[bin + 1] {
		let entry = MAP.entries[i];
		if entry.key == name {
			return Some(entry.value);
		}
	}
	None
}

const fn preset_names() -> [&'static str; N] {
	let mut out = [""; N];
	let mut i = 0;
	while i < N {
		out[i] = PARSED.entries[i].key;
		i += 1;
	}
	out
}

pub const NAMES: [&str; N] = preset_names();
