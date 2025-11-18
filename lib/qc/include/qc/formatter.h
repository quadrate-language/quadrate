/**
 * @file formatter.h
 * @brief Quadrate source code formatter
 *
 * Provides source code formatting functionality for Quadrate programs.
 */

#ifndef QD_QC_FORMATTER_H
#define QD_QC_FORMATTER_H

#include <string>

namespace Qd {

	/**
	 * @brief Format Quadrate source code with consistent style
	 *
	 * This formatter works directly on source text without building an AST.
	 * It formats structural elements including:
	 * - Function signatures and bodies
	 * - Control flow statements (if/for/loop)
	 * - Consistent indentation (tabs)
	 * - Use statement organization
	 *
	 * The formatter preserves user-defined line breaks within expressions
	 * and only modifies structural formatting.
	 *
	 * @param source The source code to format
	 * @return Formatted source code as a string
	 *
	 * @note This function does not validate syntax - it performs best-effort formatting
	 * @note The formatter uses tabs for indentation
	 *
	 * @par Example:
	 * @code
	 * std::string code = "fn main(--){print(42)}";
	 * std::string formatted = formatSource(code);
	 * // Result:
	 * // fn main( -- ) {
	 * //     print(42)
	 * // }
	 * @endcode
	 */
	std::string formatSource(const std::string& source);

} // namespace Qd

#endif // QD_QC_FORMATTER_H
