#ifndef QD_QC_ERROR_REPORTER_H
#define QD_QC_ERROR_REPORTER_H

#include <stdio.h>
#include <u8t/scanner.h>

namespace Qd {
	class ErrorReporter {
	public:
		ErrorReporter(const char* src) : mSource(src), mErrorCount(0) {
		}

		void reportError(u8t_scanner* scanner, const char* message);
		void reportError(size_t line, size_t column, const char* message);

		size_t errorCount() const {
			return mErrorCount;
		}

		bool hasErrors() const {
			return mErrorCount > 0;
		}

	private:
		const char* mSource;
		size_t mErrorCount;

		void printSourceContext(size_t line, size_t column);
	};
}

#endif
