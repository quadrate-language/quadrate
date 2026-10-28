/**
 * @file string_literal.h
 * @brief The one decoder for a string literal's escapes, as numeric_literal.h is for numbers.
 */

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace Qd {

	/**
	 * @brief The value a string literal spells: quotes off, escapes decoded.
	 *
	 * A literal and a constant are both stored as written, quotes and backslashes
	 * included, and every tier that pushes one -- a literal, a `const`, an array
	 * element, a `switch` label -- decodes it here, so `"a\tb"` is the same three
	 * characters wherever it appears. The escapes are `\n \t \r \\ \" \0 \e` and `\xNN`;
	 * any other backslash is kept as written.
	 *
	 * @param literal The literal as written; surrounding quotes are optional.
	 * @return The decoded value.
	 */
	inline std::string decodeStringLiteral(std::string_view literal) {
		if (literal.size() >= 2 && literal.front() == '"' && literal.back() == '"') {
			literal = literal.substr(1, literal.size() - 2);
		}

		const auto hexValue = [](char c) {
			return std::isdigit(static_cast<unsigned char>(c)) ? c - '0'
															   : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10;
		};

		std::string out;
		out.reserve(literal.size());
		for (size_t i = 0; i < literal.size(); i++) {
			if (literal[i] != '\\' || i + 1 >= literal.size()) {
				out += literal[i];
				continue;
			}
			switch (literal[i + 1]) {
			case 'n':
				out += '\n';
				break;
			case 't':
				out += '\t';
				break;
			case 'r':
				out += '\r';
				break;
			case '\\':
				out += '\\';
				break;
			case '"':
				out += '"';
				break;
			case '0':
				out += '\0';
				break;
			case 'e':
				out += '\x1b';
				break;
			case 'x':
				if (i + 3 < literal.size() && std::isxdigit(static_cast<unsigned char>(literal[i + 2])) &&
						std::isxdigit(static_cast<unsigned char>(literal[i + 3]))) {
					out += static_cast<char>(hexValue(literal[i + 2]) * 16 + hexValue(literal[i + 3]));
					i += 2;
					break;
				}
				out += literal[i];
				continue;
			default:
				out += literal[i];
				continue;
			}
			i++;
		}
		return out;
	}

} // namespace Qd
