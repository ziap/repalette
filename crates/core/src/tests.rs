use crate::*;

fn check<E>(result: &Result<Palette, E>, colors: &[[u8; 3]]) {
	assert!(result.as_ref().is_ok_and(|palette| {
		if palette.as_slice() != colors {
			return false;
		}

		let roundtrip = Palette::from_colors(colors);
		roundtrip.as_slice() == colors
	}));
}

#[test]
fn works() {
	let parsed = Palette::parse("123456,789abc");
	check(&parsed, &[[0x12, 0x34, 0x56], [0x78, 0x9a, 0xbc]]);
}

#[test]
fn hex_in_both_cases() {
	let parsed = Palette::parse("abcdef,ABCDEF,aBcDeF");
	check(
		&parsed,
		&[[0xab, 0xcd, 0xef], [0xab, 0xcd, 0xef], [0xab, 0xcd, 0xef]],
	);
}

#[test]
fn empty() {
	let parsed = Palette::parse("");

	if let Err(ParseError::Hex(HexError::WrongDigits(hex))) = parsed {
		assert_eq!(hex, b"");
	} else {
		assert!(false);
	}
}

mod bad_char {
	use super::*;

	#[test]
	fn fails() {
		let parsed = Palette::parse("000000,0x00ff,ffffff");

		if let Err(ParseError::Hex(HexError::BadChar { color, pos })) = parsed {
			assert_eq!(color, b"0x00ff");
			assert_eq!(pos, 1);
		} else {
			assert!(false);
		}
	}

	#[test]
	fn take_precedence_over_wrong_digits() {
		let parsed = Palette::parse("000000,x00ff,ffffff");

		if let Err(ParseError::Hex(HexError::BadChar { color, pos })) = parsed {
			assert_eq!(color, b"x00ff");
			assert_eq!(pos, 0);
		} else {
			assert!(false);
		}
	}
}

mod wrong_digits {
	use super::*;
	use alloc::boxed::Box;
	use core::iter;

	#[test]
	fn too_long() {
		let parsed = Palette::parse("000000,00000ff,ffffff");

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, b"00000ff");
		} else {
			assert!(false);
		}
	}

	#[test]
	fn too_short() {
		let parsed = Palette::parse("000000,abc,ffffff");

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, b"abc");
		} else {
			assert!(false);
		}
	}

	#[test]
	fn offender_is_last() {
		let parsed = Palette::parse("000000,ffffff,abc");

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, b"abc");
		} else {
			assert!(false);
		}
	}

	#[test]
	fn very_long() {
		let input: Box<str> = iter::repeat('0').take(65536).collect();

		let parsed = Palette::parse(&input);

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, input.as_bytes());
		} else {
			assert!(false);
		}
	}
}
