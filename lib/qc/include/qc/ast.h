/**
 * @file ast.h
 * @brief Abstract Syntax Tree (AST) generation for Quadrate
 *
 * Provides AST parsing and construction for Quadrate source code.
 */

#ifndef QD_QC_AST_H
#define QD_QC_AST_H

#include "error_reporter.h"
#include <u8t/scanner.h>
#include <vector>

namespace Qd {

	class IAstNode;

	/**
	 * @brief Abstract Syntax Tree parser for Quadrate source code
	 *
	 * The Ast class provides parsing functionality to convert Quadrate source code
	 * into an abstract syntax tree representation. It uses a recursive descent
	 * parser and provides comprehensive error reporting.
	 *
	 * @par Usage:
	 * @code
	 * Qd::Ast ast;
	 * Qd::IAstNode* root = ast.generate(source, false, "myfile.qd");
	 * if (ast.hasErrors()) {
	 *     for (const auto& error : ast.getErrors()) {
	 *         // Handle error
	 *     }
	 * }
	 * @endcode
	 */
	class Ast {
	public:
		/**
		 * @brief Destructor - cleans up allocated AST nodes
		 */
		~Ast();

		/**
		 * @brief Parse source code and generate AST
		 *
		 * Parses the provided source code string and constructs an abstract
		 * syntax tree. The AST is owned by this Ast instance and will be
		 * freed when the Ast object is destroyed.
		 *
		 * @param src Null-terminated source code string
		 * @param dumpTokens If true, prints token stream to stdout (for debugging)
		 * @param filename Filename for error reporting (can be nullptr)
		 * @return Root node of the generated AST, or nullptr if parsing failed
		 *
		 * @note The returned pointer is owned by this Ast object
		 * @note Call hasErrors() to check if parsing succeeded
		 *
		 * @see hasErrors()
		 * @see getErrors()
		 */
		IAstNode* generate(const char* src, bool dumpTokens, const char* filename);

		/**
		 * @brief Get the number of parse errors encountered
		 *
		 * @return Number of errors that occurred during parsing
		 */
		size_t errorCount() const {
			return mErrorCount;
		}

		/**
		 * @brief Check if there were any parse errors
		 *
		 * @return true if one or more errors occurred, false otherwise
		 */
		bool hasErrors() const {
			return mErrorCount > 0;
		}

		/**
		 * @brief Get detailed error information
		 *
		 * Returns a vector of all errors encountered during parsing,
		 * including location, message, and severity information.
		 *
		 * @return Vector of error details
		 *
		 * @see ErrorInfo
		 */
		const std::vector<ErrorInfo>& getErrors() const {
			return mErrors;
		}

	private:
		IAstNode* mRoot = nullptr;		///< Root node of the AST
		size_t mErrorCount = 0;			///< Number of errors encountered
		std::vector<ErrorInfo> mErrors; ///< Detailed error information
	};

} // namespace Qd

#endif // QD_QC_AST_H
