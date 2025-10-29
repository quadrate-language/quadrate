#ifndef QD_QC_SEMANTIC_VALIDATOR_H
#define QD_QC_SEMANTIC_VALIDATOR_H

#include "ast.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Qd {

	class IAstNode;

	// Stack value type for type checking
	enum class StackValueType {
		INT,
		FLOAT,
		STRING,
		ANY,	// For operations that accept any type
		UNKNOWN // For unresolved types
	};

	// Function signature - describes stack effect of a function
	struct FunctionSignature {
		std::vector<StackValueType> consumes; // Types popped from stack (bottom to top)
		std::vector<StackValueType> produces; // Types pushed to stack (bottom to top)
	};

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

		// Pass 3a: Analyze function signatures (what each function consumes/produces)
		void analyzeFunctionSignatures(IAstNode* node);

		// Pass 3b: Type check the AST
		void typeCheckFunction(IAstNode* node);
		void typeCheckBlock(IAstNode* node, std::vector<StackValueType>& type_stack);
		void typeCheckInstruction(const char* name, std::vector<StackValueType>& type_stack);

		// Helper: Analyze a block in isolation (for determining function signatures)
		void analyzeBlockInIsolation(IAstNode* node, std::vector<StackValueType>& type_stack);

		// Helper: Type check an instruction (with optional error suppression for signature analysis)
		void typeCheckInstructionInternal(const char* name, std::vector<StackValueType>& type_stack,
				bool report_errors);

		// Check if a name is a built-in instruction
		bool isBuiltInInstruction(const char* name) const;

		// Helper: Check if type is numeric (int or float)
		bool isNumericType(StackValueType type) const;

		// Helper: Get string representation of type
		const char* typeToString(StackValueType type) const;

		// Report an error (gcc/clang style)
		void reportError(const char* message);

		// Report an error conditionally (for signature analysis)
		void reportErrorConditional(const char* message, bool should_report);

		// Current filename being validated
		const char* filename_;

		// Symbol table: all defined functions
		std::unordered_set<std::string> defined_functions_;

		// Function signatures: stack effect of each function
		std::unordered_map<std::string, FunctionSignature> function_signatures_;

		// Error count
		size_t error_count_;
	};

} // namespace Qd

#endif // QD_QC_SEMANTIC_VALIDATOR_H
