#ifndef QD_QC_SEMANTIC_VALIDATOR_H
#define QD_QC_SEMANTIC_VALIDATOR_H

#include "ast.h"
#include <string>
#include <unordered_set>

namespace Qd {

	class IAstNode;

	// Semantic validator - checks for errors that would slip through to GCC/runtime
	class SemanticValidator {
	public:
		SemanticValidator();

		// Validate an AST and return error count
		// Returns 0 if valid, > 0 if errors were found
		size_t validate(IAstNode* program, const char* filename = nullptr);

		// Get error count
		size_t errorCount() const {
			return error_count_;
		}

	private:
		// Pass 1: Collect all function definitions
		void collectDefinitions(IAstNode* node);

		// Pass 2: Validate all function calls and references
		void validateReferences(IAstNode* node);

		// Check if a name is a built-in instruction
		bool isBuiltInInstruction(const char* name) const;

		// Report an error (gcc/clang style)
		void reportError(const char* message);

		// Current filename being validated
		const char* filename_;

		// Symbol table: all defined functions
		std::unordered_set<std::string> defined_functions_;

		// Error count
		size_t error_count_;
	};

} // namespace Qd

#endif // QD_QC_SEMANTIC_VALIDATOR_H
