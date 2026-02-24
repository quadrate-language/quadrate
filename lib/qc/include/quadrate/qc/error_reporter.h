#ifndef QD_QC_ERROR_REPORTER_H
#define QD_QC_ERROR_REPORTER_H

#include <cstddef>
#include <string>
#include <vector>

// Forward declaration for u8t scanner (avoid including full header)
struct u8t_scanner;

namespace Qd {
	struct ErrorInfo {
		size_t line;
		size_t column;
		std::string message;
	};

	class ErrorReporter {
	public:
		// Default maximum errors before stopping (0 = unlimited)
		static constexpr size_t DEFAULT_MAX_ERRORS = 20;

		ErrorReporter(const char* src = nullptr, const char* filename = nullptr)
			: mSource(src), mFilename(filename), mErrorCount(0), mWarningCount(0), mStoreErrors(false),
			  mMaxErrors(DEFAULT_MAX_ERRORS), mPanicMode(false) {
		}

		void reportError(u8t_scanner* scanner, const char* message);
		void reportError(size_t line, size_t column, const char* message);
		void reportErrorWithHint(size_t line, size_t column, const char* message, const char* hint);
		void reportWarning(size_t line, size_t column, const char* message);

		size_t errorCount() const {
			return mErrorCount;
		}

		size_t warningCount() const {
			return mWarningCount;
		}

		bool hasErrors() const {
			return mErrorCount > 0;
		}

		// Set maximum errors (0 = unlimited)
		void setMaxErrors(size_t max) {
			mMaxErrors = max;
		}

		// Check if we've hit the error limit
		bool atErrorLimit() const {
			return mMaxErrors > 0 && mErrorCount >= mMaxErrors;
		}

		// Panic mode - enter when we encounter cascading errors, exit at sync points
		void enterPanicMode() {
			mPanicMode = true;
		}

		void exitPanicMode() {
			mPanicMode = false;
		}

		bool inPanicMode() const {
			return mPanicMode;
		}

		// Enable error storage for LSP
		void setStoreErrors(bool store) {
			mStoreErrors = store;
		}

		const std::vector<ErrorInfo>& getErrors() const {
			return mErrors;
		}

	private:
		// Order must match constructor initialization list
		const char* mSource;
		const char* mFilename;
		size_t mErrorCount;
		size_t mWarningCount;
		bool mStoreErrors;
		size_t mMaxErrors;
		bool mPanicMode;
		std::vector<ErrorInfo> mErrors;

		void printSourceContext(size_t line, size_t column);
	};
}

#endif
