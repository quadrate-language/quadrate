/**
 * @file numeric_literal.h
 * @brief The one reader for a numeric literal, in the spelling the language gives it.
 */

#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
	 * else -- an empty string, a prefix with no digits, a digit separator, trailing
	 * characters -- is `invalid_argument`, and a value outside i64 is
	 * `result_out_of_range`.
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

	/**
	 * @brief Whether a numeric literal's spelling makes it a float.
	 *
	 * A decimal point or an exponent is what separates `1.5` and `1e3` from `1000`. The
	 * radix prefixes are stepped over first: the `E` in `0xE1` is a hex digit, not an
	 * exponent, and the literal is an integer.
	 *
	 * A constant is stored as written and every tier re-derives its type from that text,
	 * so this is the one place the rule lives. Spelling it as `value.find('.')` -- which
	 * is what each of those tiers used to do -- types `1e3` as an integer, and then the
	 * integer reader chokes on the `e`.
	 */
	inline bool isFloatLiteralText(std::string_view text) {
		if (text.empty() || hasRadixPrefix(text)) {
			return false;
		}
		return text.find('.') != std::string_view::npos || text.find_first_of("eE") != std::string_view::npos;
	}

	/**
	 * @brief Read a float literal, reporting why if it will not read.
	 *
	 * The accepted spelling is the one section 2.3.4 of the specification gives: an
	 * optional `-`, decimal digits, an optional `.` with digits on *both* sides of it,
	 * and an optional `e`/`E` exponent with an optional sign and at least one digit.
	 * A point missing its digits -- `.5`, `5.` -- is `invalid_argument`, as is anything
	 * with characters left over.
	 *
	 * A magnitude too large for an f64 is `result_out_of_range`. Too small is not an
	 * error: it underflows to a subnormal or to zero, which is what the same arithmetic
	 * does at runtime, and `limits::F64Min` has to stay writable.
	 *
	 * @param text The literal as written.
	 * @param out Set to the value on success, untouched otherwise.
	 * @return `std::errc()` on success.
	 */
	inline std::errc readFloatLiteral(std::string_view text, double& out) {
		size_t at = 0;
		const auto digits = [&text, &at]() {
			const size_t start = at;
			while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
				++at;
			}
			return at - start;
		};

		if (at < text.size() && text[at] == '-') {
			++at;
		}
		if (digits() == 0) {
			return std::errc::invalid_argument;
		}
		if (at < text.size() && text[at] == '.') {
			++at;
			if (digits() == 0) {
				return std::errc::invalid_argument;
			}
		}
		if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
			++at;
			if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
				++at;
			}
			if (digits() == 0) {
				return std::errc::invalid_argument;
			}
		}
		if (at != text.size()) {
			return std::errc::invalid_argument;
		}

		// strtod rather than from_chars, which is what every other float read in the tree
		// uses; nothing sets a locale, so the C locale's '.' is the decimal point.
		const std::string owned(text);
		errno = 0;
		char* stopped = nullptr;
		const double value = std::strtod(owned.c_str(), &stopped);
		if (stopped != owned.c_str() + owned.size()) {
			return std::errc::invalid_argument;
		}
		// ERANGE is reported at both ends of the range. Only the overflow end has nowhere
		// to go: it returns an infinity for a literal that named a finite number.
		if (errno == ERANGE && std::isinf(value)) {
			return std::errc::result_out_of_range;
		}
		out = value;
		return std::errc();
	}

	/**
	 * @brief Read a float literal, substituting a value for one that will not read.
	 *
	 * For the places downstream of the validator, where the literal has already been
	 * checked and a second diagnostic would only be noise.
	 * @see readFloatLiteral
	 */
	inline double floatLiteralOr(std::string_view text, double fallback) {
		double value = 0.0;
		return readFloatLiteral(text, value) == std::errc() ? value : fallback;
	}
}
