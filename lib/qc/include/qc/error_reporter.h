#ifndef QD_QC_ERROR_REPORTER_H
#define QD_QC_ERROR_REPORTER_H

#include <stdio.h>
#include <string>
#include <u8t/scanner.h>
#include <vector>

namespace Qd {
	struct ErrorInfo {
		size_t line;
		size_t column;
		std::string message;
	};

	class ErrorReporter {
	public:
		ErrorReporter(const char* src = nullptr, const char* filename = nullptr)
			: mSource(src), mFilename(filename), mErrorCount(0), mStoreErrors(false) {
		}

		void reportError(u8t_scanner* scanner, const char* message);
		void reportError(size_t line, size_t column, const char* message);

		size_t errorCount() const {
			return mErrorCount;
		}

		bool hasErrors() const {
			return mErrorCount > 0;
		}

		// Enable error storage for LSP
		void setStoreErrors(bool store) {
			mStoreErrors = store;
		}

		const std::vector<ErrorInfo>& getErrors() const {
			return mErrors;
		}

	private:
		const char* mSource;
		const char* mFilename;
		size_t mErrorCount;
		bool mStoreErrors;
		std::vector<ErrorInfo> mErrors;

		void printSourceContext(size_t line, size_t column);
	};
}

#endif
