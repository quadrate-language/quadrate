#include <qc/error_reporter.h>
#include <string.h>

namespace Qd {
	// Helper to calculate line and column from byte position
	static void calculateLineColumn(const char* src, size_t pos, size_t* line, size_t* column) {
		*line = 1;
		*column = 1;
		for (size_t i = 0; i < pos && src[i] != '\0'; i++) {
			if (src[i] == '\n') {
				(*line)++;
				*column = 1;
			} else {
				(*column)++;
			}
		}
	}

	void ErrorReporter::reportError(u8t_scanner* scanner, const char* message) {
		size_t pos = u8t_scanner_token_start(scanner);
		size_t line, column;
		calculateLineColumn(mSource, pos, &line, &column);
		reportError(line, column, message);
	}

	void ErrorReporter::reportError(size_t line, size_t column, const char* message) {
		fprintf(stderr, "Error at line %zu, column %zu: %s\n", line, column, message);
		printSourceContext(line, column);
		mErrorCount++;
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
