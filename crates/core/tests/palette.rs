use repalette_core::*;
use std::{assert_matches, iter};

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
	check(
		&Palette::parse("123456,789abc"),
		&[[0x12, 0x34, 0x56], [0x78, 0x9a, 0xbc]],
	);
}

#[test]
fn hex_in_both_cases() {
	check(
		&Palette::parse("abcdef,ABCDEF,aBcDeF"),
		&[[0xab, 0xcd, 0xef], [0xab, 0xcd, 0xef], [0xab, 0xcd, 0xef]],
	);
}

mod edge_cases {
	use super::*;

	#[test]
	fn empty() {
		let parsed = Palette::parse("");

		if let Err(ParseError::Hex(HexError::WrongDigits(hex))) = parsed {
			assert_eq!(hex, b"");
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}

	#[test]
	fn trailing_comma() {
		let parsed = Palette::parse("123456,789abc,");

		if let Err(ParseError::Hex(HexError::WrongDigits(hex))) = parsed {
			assert_eq!(hex, b"");
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
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
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}

	#[test]
	fn leading_space() {
		let parsed = Palette::parse("000000, ffffff");

		if let Err(ParseError::Hex(HexError::BadChar { color, pos })) = parsed {
			assert_eq!(color, b" ffffff");
			assert_eq!(pos, 0);
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}

	#[test]
	fn take_precedence_over_wrong_digits() {
		let parsed = Palette::parse("000000,x00ff,ffffff");

		if let Err(ParseError::Hex(HexError::BadChar { color, pos })) = parsed {
			assert_eq!(color, b"x00ff");
			assert_eq!(pos, 0);
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}
}

mod wrong_digits {
	use super::*;

	#[test]
	fn too_long() {
		let parsed = Palette::parse("000000,00000ff,ffffff");

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, b"00000ff");
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}

	#[test]
	fn too_short() {
		let parsed = Palette::parse("000000,abc,ffffff");

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, b"abc");
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}

	#[test]
	fn offender_is_last() {
		let parsed = Palette::parse("000000,ffffff,abc");

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, b"abc");
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}

	#[test]
	fn very_long() {
		let input: Box<str> = iter::repeat('0').take(65536).collect();

		let parsed = Palette::parse(&input);

		if let Err(ParseError::Hex(HexError::WrongDigits(bytes))) = parsed {
			assert_eq!(bytes, input.as_bytes());
		} else {
			assert!(false, "{parsed:?} should not be parsed successfully");
		}
	}
}

mod too_large {
	use super::*;

	fn generate_pair<const N: usize>() -> (String, [[u8; 3]; N]) {
		let input = ["000000"; N].join(",");
		let output = [[0, 0, 0]; N];

		(input, output)
	}

	#[test]
	fn allows_256() {
		let (input, output) = generate_pair::<256>();
		check(&Palette::parse(&input), &output);
	}

	#[test]
	fn rejects_larger() {
		let (input, _output) = generate_pair::<257>();
		assert_matches!(Palette::parse(&input), Err(ParseError::TooLarge));
	}

	#[test]
	fn rejects_very_large() {
		let (input, _output) = generate_pair::<65536>();
		assert_matches!(Palette::parse(&input), Err(ParseError::TooLarge));
	}
}
