/**
 * @file numeric_literal.h
 * @brief The one reader for an integer literal, in the spelling the language gives it.
 */

#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace Qd {

	/**
	 * @brief Whether a literal carries a `0x` or `0b` prefix, sign and all.
	 * @see readIntegerLiteral
	 */
	inline bool hasRadixPrefix(std::string_view text) {
		const size_t pos = (!text.empty() && text.front() == '-') ? 1 : 0;
		return text.size() > pos + 2 && text[pos] == '0' &&
			   (text[pos + 1] == 'x' || text[pos + 1] == 'X' || text[pos + 1] == 'b' || text[pos + 1] == 'B');
	}

	/**
	 * @brief Read an integer literal, reporting why if it will not read.
	 *
	 * The accepted spelling is the one section 2.3.4 of the specification gives: decimal
	 * digits, `0x`/`0X` hex or `0b`/`0B` binary, with an optional leading `-`. Anything
	 * else -- an empty string, a prefix with no digits, trailing characters -- is
	 * `invalid_argument`, and a value outside i64 is `result_out_of_range`.
	 *
	 * `strtoll` with base 0 is not this function, and using it was a bug twice over. It
	 * reads a leading zero as octal, which the language has no notion of, so an enum
	 * value of `010` was 8 while the same literal in an expression was 10. And whether it
	 * takes a `0b` prefix at all is up to the C library -- glibc 2.38 and newer do, older
	 * ones and musl do not -- so `-0b101` was -5 on one machine and 0 on the next.
	 *
	 * @param text The literal as written.
	 * @param out Set to the value on success, untouched otherwise.
	 * @return `std::errc()` on success.
	 */
	inline std::errc readIntegerLiteral(std::string_view text, int64_t& out) {
		if (text.empty()) {
			return std::errc::invalid_argument;
		}

		// The sign comes before the radix prefix, so it has to be stepped over to find one:
		// `-0x10` is a hex literal, and testing text[0] for '0' says it is decimal and then
		// chokes on the 'x'.
		const bool negative = text.front() == '-';
		const size_t pos = negative ? 1 : 0;

		int base = 10;
		size_t digitsAt = pos;
		if (hasRadixPrefix(text)) {
			base = (text[pos + 1] == 'x' || text[pos + 1] == 'X') ? 16 : 2;
			digitsAt = pos + 2;
		}

		// from_chars does not accept the `0x`/`0b` prefix but does accept a sign in any
		// base, so hand it the sign and the digits. Keeping the sign attached is what makes
		// the bound exact: the magnitude of the most negative i64 does not fit a positive one.
		std::string digits;
		if (negative) {
			digits += '-';
		}
		digits.append(text, digitsAt, std::string_view::npos);
		if (digits.size() == (negative ? 1u : 0u)) {
			return std::errc::invalid_argument;
		}

		const char* const begin = digits.data();
		const char* const finish = begin + digits.size();
		const auto [stopped, ec] = std::from_chars(begin, finish, out, base);
		if (ec != std::errc()) {
			return ec;
		}
		return stopped == finish ? std::errc() : std::errc::invalid_argument;
	}

	/**
	 * @brief Read an integer literal, where only whether it read matters.
	 * @see readIntegerLiteral
	 */
	inline bool parseIntegerLiteral(std::string_view text, int64_t& out) {
		return readIntegerLiteral(text, out) == std::errc();
	}

	/**
	 * @brief Read an integer literal, substituting a value for one that will not read.
	 *
	 * For the places downstream of the validator, where the literal has already been
	 * checked and a second diagnostic would only be noise.
	 * @see readIntegerLiteral
	 */
	inline int64_t integerLiteralOr(std::string_view text, int64_t fallback) {
		int64_t value = 0;
		return parseIntegerLiteral(text, value) ? value : fallback;
	}
}
