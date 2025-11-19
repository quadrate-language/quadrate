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
		PTR,	 // For function pointers
		ANY,	 // For operations that accept any type
		UNKNOWN, // For unresolved types
		TAINTED	 // For error-tainted values from throws functions
	};

	// Function signature - describes stack effect of a function
	struct FunctionSignature {
		std::vector<StackValueType> consumes; // Types popped from stack (bottom to top)
		std::vector<StackValueType> produces; // Types pushed to stack (bottom to top)
		bool throws = false;				  // Whether the function can throw errors
	};

	// Semantic validator - checks for errors that would slip through to GCC/runtime
	class SemanticValidator {
	public:
		SemanticValidator();

		// Validate an AST and return error count
		// Returns 0 if valid, > 0 if errors were found
		// If isModuleFile is true, missing module imports will not be reported as errors
		// If werror is true, warnings are treated as errors
		size_t validate(
				IAstNode* program, const char* filename = nullptr, bool isModuleFile = false, bool werror = false);

		// Get error count
		size_t errorCount() const {
			return mErrorCount;
		}

		// Get warning count
		size_t warningCount() const {
			return mWarningCount;
		}

		// Get the set of imported modules
		const std::unordered_set<std::string>& importedModules() const {
			return mImportedModules;
		}

		// Get the source directory (extracted from validated filename)
		const std::string& sourceDirectory() const {
			return mSourceDirectory;
		}

		// Get the module constants map (maps module name -> (constant name -> isPublic))
		const std::unordered_map<std::string, std::unordered_map<std::string, bool>>& moduleConstants() const {
			return mModuleConstants;
		}

		// Get the module constant values map
		const std::unordered_map<std::string, std::string>& moduleConstantValues() const {
			return mModuleConstantValues;
		}

	private:
		// Pass 1: Collect all function definitions
		void collectDefinitions(IAstNode* node);

		// Helper: Load module definitions from a module file
		void loadModuleDefinitions(
				const std::string& moduleName, const std::string& currentPackage, bool reportErrors = true);

		// Helper: Parse module source and collect function definitions
		void parseModuleAndCollectFunctions(const std::string& moduleName, const std::string& source);

		// Helper: Collect function definitions from a module AST
		void collectModuleFunctions(IAstNode* node, std::unordered_map<std::string, bool>& functions);
		void collectModuleConstants(IAstNode* node, std::unordered_map<std::string, bool>& constants);
		void collectModuleConstantValues(IAstNode* node, const std::string& moduleName);

		// Helper: Analyze function signatures in a module
		void analyzeModuleFunctionSignatures(IAstNode* node, const std::string& moduleName);

		// Pass 2: Validate all function calls and references
		void validateReferences(IAstNode* node, bool insideForLoop = false);
		void validateReferencesInternal(
				IAstNode* node, bool insideForLoop, std::unordered_set<std::string>& localVariables);

		// Pass 3a: Analyze function signatures (what each function consumes/produces)
		void analyzeFunctionSignatures(IAstNode* node);

		// Pass 3b: Type check the AST
		void typeCheckFunction(IAstNode* node);
		void typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack,
				std::unordered_map<std::string, StackValueType>& localVariables);
		void typeCheckInstruction(IAstNode* node, const char* name, std::vector<StackValueType>& typeStack);

		// Helper: Analyze a block in isolation (for determining function signatures)
		void analyzeBlockInIsolation(IAstNode* node, std::vector<StackValueType>& typeStack);

		// Helper: Type check an instruction (with optional error suppression for signature analysis)
		void typeCheckInstructionInternal(
				IAstNode* node, const char* name, std::vector<StackValueType>& typeStack, bool reportErrors);

		// Check if a name is a built-in instruction
		bool isBuiltInInstruction(const char* name) const;

		// Helper: Check if type is numeric (int or float)
		bool isNumericType(StackValueType type) const;

	// Helper: Determine the type of a constant value from its string representation
	StackValueType getConstantType(const std::string& value) const;

		// Helper: Get string representation of type
		const char* typeToString(StackValueType type) const;

		// Report an error (gcc/clang style)
		void reportError(const char* message);
		void reportError(const IAstNode* node, const char* message);

		// Report an error conditionally (for signature analysis)
		void reportErrorConditional(const char* message, bool shouldReport);
		void reportErrorConditional(const IAstNode* node, const char* message, bool shouldReport);

		// Report a warning (gcc/clang style)
		void reportWarning(const IAstNode* node, const char* message);

		// Current filename being validated
		const char* mFilename;

		// Source file directory (extracted from mFilename)
		std::string mSourceDirectory;

		// Current package name (extracted from mFilename)
		std::string mCurrentPackage;

		// Symbol table: all defined functions
		std::unordered_set<std::string> mDefinedFunctions;

		// Symbol table: all defined constants
		std::unordered_set<std::string> mDefinedConstants;

	// Constant values: maps constant name -> value string
	std::unordered_map<std::string, std::string> mConstantValues;

		// Imported modules: tracks which modules have been imported via 'use' statements
		std::unordered_set<std::string> mImportedModules;

		// Imported libraries: maps namespace -> library name (e.g., "std" -> "libstdqd.so")
		std::unordered_map<std::string, std::string> mImportedLibraries;

		// Imported library functions: maps namespace::function -> true (e.g., "std::printf" -> true)
		std::unordered_set<std::string> mImportedLibraryFunctions;

		// Loaded module files: tracks which specific files have been loaded (to prevent duplicate loads)
		std::unordered_set<std::string> mLoadedModuleFiles;

		// Module functions: maps module name -> set of function names in that module
		// Maps module name -> (function name -> isPublic flag)
		std::unordered_map<std::string, std::unordered_map<std::string, bool>> mModuleFunctions;

		// Module constants: maps module name -> (constant name -> isPublic flag)
		std::unordered_map<std::string, std::unordered_map<std::string, bool>> mModuleConstants;

		// Module constant values: maps "module::name" -> value string
		std::unordered_map<std::string, std::string> mModuleConstantValues;

		// Module directories: maps module name -> directory path where module was found
		std::unordered_map<std::string, std::string> mModuleDirectories;

		// Function signatures: stack effect of each function
		std::unordered_map<std::string, FunctionSignature> mFunctionSignatures;

		// Error count
		size_t mErrorCount;

		// Warning count
		size_t mWarningCount;

		// Whether warnings should be treated as errors
		bool mWerror;

		// Whether this is validating a module file (vs main entry point)
		bool mIsModuleFile;
	};

} // namespace Qd

#endif // QD_QC_SEMANTIC_VALIDATOR_H
