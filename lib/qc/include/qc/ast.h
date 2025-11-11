#ifndef QD_QC_AST_H
#define QD_QC_AST_H

#include <u8t/scanner.h>
#include <vector>
#include "error_reporter.h"

namespace Qd {
	class IAstNode;

	class Ast {
	public:
		~Ast();

		IAstNode* generate(const char* src, bool dumpTokens, const char* filename);

		// Get the number of parse errors encountered
		size_t errorCount() const {
			return mErrorCount;
		}

		// Check if there were any parse errors
		bool hasErrors() const {
			return mErrorCount > 0;
		}

		// Get detailed error information
		const std::vector<ErrorInfo>& getErrors() const {
			return mErrors;
		}

	private:
		IAstNode* mRoot = nullptr;
		size_t mErrorCount = 0;
		std::vector<ErrorInfo> mErrors;
	};
}

#endif
