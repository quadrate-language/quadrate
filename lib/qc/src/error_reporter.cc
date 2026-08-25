#include "source_utils.h"
#include <iostream>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/error_reporter.h>
#include <string.h>
#include <u8t/scanner.h>

namespace Qd {

	void ErrorReporter::reportError(u8t_scanner* scanner, const char* message) {
		// Don't report errors in panic mode (cascading errors)
		if (mPanicMode) {
			return;
		}

		// u8t_scanner_token_start returns a character index; calculateLineColumn
		// indexes bytes. Without the conversion every multi-byte character earlier
		// in the file drags the reported position backwards - a single em dash in a
		// header comment is enough to name the wrong line.
		size_t charPos = u8t_scanner_token_start(scanner);
		size_t bytePos = charIndexToByteOffset(mSource, charPos);
		size_t line, column;
		calculateLineColumn(mSource, bytePos, &line, &column);
		reportError(line, column, message);
	}

	void ErrorReporter::reportError(size_t line, size_t column, const char* message) {
		reportErrorWithHint(line, column, message, nullptr);
	}

	void ErrorReporter::reportWarning(size_t line, size_t column, const char* message) {
		mWarningCount++;

		// Store warning if requested (for LSP)
		if (mStoreErrors) {
			ErrorInfo warning;
			warning.line = line;
			warning.column = column;
			warning.message = std::string("[warning] ") + message;
			mErrors.push_back(warning);
			return;
		}

		// Format: filename:line:column: warning: message
		std::cerr << Colors::bold() << "quadc: " << Colors::reset();
		if (mFilename) {
			std::cerr << Colors::bold() << mFilename << ":" << line << ":" << column << ":" << Colors::reset() << " ";
		} else {
			std::cerr << Colors::bold() << line << ":" << column << ":" << Colors::reset() << " ";
		}
		std::cerr << Colors::bold() << Colors::magenta() << "warning:" << Colors::reset() << " ";
		std::cerr << message << std::endl;
		printSourceContext(line, column);
	}

	void ErrorReporter::reportErrorWithHint(size_t line, size_t column, const char* message, const char* hint) {
		// Don't report errors in panic mode (cascading errors)
		if (mPanicMode) {
			return;
		}

		mErrorCount++;

		// Check if we've hit the error limit
		if (mMaxErrors > 0 && mErrorCount == mMaxErrors) {
			// Still report this error, but warn about limit
			if (!mStoreErrors) {
				// Report the current error first
				std::cerr << Colors::bold() << "quadc: " << Colors::reset();
				if (mFilename) {
					std::cerr << Colors::bold() << mFilename << ":" << line << ":" << column << ":" << Colors::reset()
							  << " ";
				}
				std::cerr << Colors::bold() << Colors::red() << "error:" << Colors::reset() << " ";
				std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
				printSourceContext(line, column);
				if (hint) {
					std::cerr << Colors::cyan() << "  hint:" << Colors::reset() << " " << hint << std::endl;
				}

				// Then print the limit warning
				std::cerr << Colors::bold() << Colors::magenta() << "\nquadc: error limit reached (" << mMaxErrors
						  << " errors). Further errors suppressed." << Colors::reset() << std::endl;
				return;
			}
		} else if (mMaxErrors > 0 && mErrorCount > mMaxErrors) {
			// Beyond limit - store for LSP but don't print
			if (mStoreErrors) {
				ErrorInfo error;
				error.line = line;
				error.column = column;
				error.message = message;
				if (hint) {
					error.message += " (hint: ";
					error.message += hint;
					error.message += ")";
				}
				mErrors.push_back(error);
			}
			return;
		}

		// Store error if requested (for LSP)
		if (mStoreErrors) {
			ErrorInfo error;
			error.line = line;
			error.column = column;
			error.message = message;
			if (hint) {
				error.message += " (hint: ";
				error.message += hint;
				error.message += ")";
			}
			mErrors.push_back(error);
			return; // Don't print to stderr when storing
		}

		// Format: filename:line:column: error: message
		std::cerr << Colors::bold() << "quadc: " << Colors::reset();
		if (mFilename) {
			std::cerr << Colors::bold() << mFilename << ":" << line << ":" << column << ":" << Colors::reset() << " ";
		} else {
			std::cerr << Colors::bold() << line << ":" << column << ":" << Colors::reset() << " ";
		}
		std::cerr << Colors::bold() << Colors::red() << "error:" << Colors::reset() << " ";
		std::cerr << Colors::bold() << message << Colors::reset() << std::endl;
		printSourceContext(line, column);

		// Print hint if provided
		if (hint) {
			std::cerr << Colors::cyan() << "  hint:" << Colors::reset() << " " << hint << std::endl;
		}
	}

	void ErrorReporter::printSourceContext(size_t line, size_t column) {
		// Find the line in the source
		size_t currentLine = 1;
		const char* lineStart = mSource;
		const char* ptr = mSource;

		while (*ptr != '\0' && currentLine < line) {
			if (*ptr == '\n') {
				currentLine++;
				lineStart = ptr + 1;
			}
			ptr++;
		}

		// Print the line
		const char* lineEnd = lineStart;
		while (*lineEnd != '\0' && *lineEnd != '\n') {
			lineEnd++;
		}

		fprintf(stderr, "  ");
		fwrite(lineStart, 1, static_cast<size_t>(lineEnd - lineStart), stderr);
		fprintf(stderr, "\n");

		// Print the arrow pointing to the column
		fprintf(stderr, "  ");
		for (size_t i = 1; i < column; i++) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "^\n");
	}
}
