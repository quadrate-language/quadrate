#ifndef QD_QC_SEMANTIC_VALIDATOR_H
#define QD_QC_SEMANTIC_VALIDATOR_H

#include "ast.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Qd {

	class IAstNode;
	class AstNodeFunctionDeclaration;

	// Stack value type for type checking
	enum class StackValueType {
		INT,
		FLOAT,
		STRING,
		PTR,	 // For function pointers
		ANY,	 // For operations that accept any type
		UNKNOWN, // For unresolved types
		TAINTED, // For error-tainted values from throws functions
		TYPEVAR	 // For generic type variables (T, U, etc.)
	};

	// Information about an imported C function exposed by a module
	struct ImportedFunctionInfo {
		std::string library;		 // e.g., "libglut.so"
		std::string importNamespace; // e.g., "native" - the namespace used within the module
		std::string cFunctionName;	 // The actual C function name
		bool throws;				 // Whether the function can throw errors
	};

	// Cached parsed module AST
	struct ParsedModuleAst {
		std::unique_ptr<Ast> ast;
		IAstNode* root = nullptr;
		std::string source;
		std::string filePath;
	};

	// Function signature - describes stack effect of a function
	struct FunctionSignature {
		std::vector<StackValueType> consumes; // Types popped from stack (bottom to top)
		std::vector<StackValueType> produces; // Types pushed to stack (bottom to top)
		// For each PTR parameter: map of parameter name -> map of (field name -> expected field type)
		std::unordered_map<std::string, std::unordered_map<std::string, StackValueType>> parameterFieldAccess;
		// For each PTR parameter index, expected struct type name (if determinable)
		// Key: parameter index (0-based), Value: struct type name (e.g., "Point", "WithStr")
		std::unordered_map<size_t, std::string> parameterStructTypes;
		// For each PTR return value index, the struct type name (if determinable)
		// Key: produces index (0-based), Value: struct type name
		std::unordered_map<size_t, std::string> producesStructTypes;
		bool throws = false; // Whether the function can throw errors
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

		// Get the module imported functions map (public imported C functions)
		const std::unordered_map<std::string, std::unordered_map<std::string, ImportedFunctionInfo>>&
		moduleImportedFunctions() const {
			return mModuleImportedFunctions;
		}

		// Get the imported FFI libraries map (namespace -> library path)
		const std::unordered_map<std::string, std::string>& importedLibraries() const {
			return mImportedLibraries;
		}

		// Enable error storage for LSP (instead of printing to stderr)
		void setStoreErrors(bool store) {
			mStoreErrors = store;
		}

		// Get stored errors (only available when setStoreErrors(true) was called)
		const std::vector<ErrorInfo>& getErrors() const {
			return mStoredErrors;
		}

		// Set minimum line number for warnings (warnings on earlier lines are suppressed)
		// This is useful for REPL-style incremental compilation where previous code
		// has already been validated and we don't want to re-show warnings for it
		void setWarningMinLine(size_t line) {
			mWarningMinLine = line;
		}

		// Set additional module search paths (from -I flags)
		void setIncludePaths(const std::vector<std::string>& paths) {
			mIncludePaths = paths;
		}

		// Freestanding mode: reject `use` of stdlib modules that allocate or
		// call libc, and reject builtins (`print`, `nl`, `read`, `panic`,
		// etc.) that aren't usable without a hosted runtime. Only `bits`
		// and `limits` from the stdlib are allowed; everything else either
		// allocates, calls libc, or both.
		void setFreestandingMode(bool enabled) {
			mFreestandingMode = enabled;
		}

		// Set source text for error context printing
		void setSource(const char* source) {
			mSource = source;
		}

		// Set sibling files for directory-based namespace system
		// These files are in the same directory as the main file and will be
		// automatically merged into the main namespace without explicit imports
		void setSiblingFiles(const std::vector<std::string>& files) {
			mSiblingFiles = files;
		}

		// Get cached parsed module ASTs (populated during validation)
		// Key: file path, Value: ParsedModuleAst with AST ownership
		std::unordered_map<std::string, ParsedModuleAst>& getParsedModuleAsts() {
			return mParsedModuleAsts;
		}

	private:
		// Print source context with line number and caret pointer
		void printSourceContext(size_t line, size_t column);
		// Pass 1: Collect all function definitions
		void collectDefinitions(IAstNode* node);

		void loadModuleDefinitions(
				const std::string& moduleName, const std::string& currentPackage, bool reportErrors = true);

		// Helper: Try to load a module from a directory (module.qd or glob *.qd)
		bool tryLoadModuleFromDirectory(const std::string& moduleDir, const std::string& moduleName);

		void parseModuleAndCollectFunctions(const std::string& moduleName, const std::string& source,
				const std::string& filePath, bool mergeIntoMain = false);

		void collectModuleFunctions(IAstNode* node, std::unordered_map<std::string, bool>& functions);
		void collectModuleConstants(IAstNode* node, std::unordered_map<std::string, bool>& constants);
		void collectModuleConstantValues(IAstNode* node, const std::string& moduleName, bool mergeIntoMain = false);
		void collectModuleStructs(IAstNode* node, std::unordered_map<std::string, bool>& structs);
		void collectModuleStructFieldTypes(IAstNode* node, const std::string& moduleName, bool mergeIntoMain = false);
		void collectModuleMethods(IAstNode* node, const std::string& moduleName);

		// Helper: Look up struct field types, handling both qualified and unqualified names
		const std::unordered_map<std::string, StackValueType>* lookupStructFieldTypes(
				const std::string& typeName) const;
		void collectModuleImportedFunctions(IAstNode* node, const std::string& moduleName,
				std::unordered_map<std::string, ImportedFunctionInfo>& imports);

		void analyzeModuleFunctionSignatures(IAstNode* node, const std::string& moduleName, bool mergeIntoMain = false);

		// Pass 2: Validate all function calls and references
		void validateReferences(IAstNode* node);
		// Records the function or test enclosing 'node' as having referenced a
		// removed builtin, so type checking can skip its stack simulation.
		void markBodyWithRemovedBuiltin(const IAstNode* node);

		void validateReferencesInternal(IAstNode* node, std::unordered_set<std::string>& localVariables,
				std::unordered_set<std::string>& iteratorNames);

		// Helper: Collect captured variables for closures (anonymous functions accessing outer scope)
		void collectCapturedVariables(IAstNode* node, std::unordered_set<std::string>& localVariables,
				std::unordered_set<std::string>& iteratorNames,
				const std::unordered_set<std::string>& outerScopeVariables, class AstNodeAnonymousFunction* anonFunc);

		// Pass 3a: Analyze function signatures (what each function consumes/produces)
		void analyzeFunctionSignatures(IAstNode* node);

		// Helper: Collect field accesses on parameters within a function body
		void collectParameterFieldAccesses(IAstNode* node, const std::vector<std::string>& paramNames,
				std::unordered_map<std::string, std::unordered_map<std::string, StackValueType>>& fieldAccesses);

		// Pass 3b: Type check the AST
		void typeCheckFunction(IAstNode* node);
		void typeCheckTest(IAstNode* node);
		void typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack,
				std::unordered_map<std::string, StackValueType>& localVariables,
				std::vector<std::string>& structTypeStack);
		void typeCheckInstruction(IAstNode* node, const char* name, std::vector<StackValueType>& typeStack,
				std::vector<std::string>& structTypeStack);

		// Helper: Analyze a block in isolation (for determining function signatures)
		void analyzeBlockInIsolation(IAstNode* node, std::vector<StackValueType>& typeStack,
				const std::unordered_map<std::string, StackValueType>& initialLocalVars = {});

		// Helper: Type check an instruction (with optional error suppression for signature analysis)
		void typeCheckInstructionInternal(IAstNode* node, const char* name, std::vector<StackValueType>& typeStack,
				std::vector<std::string>& structTypeStack, bool reportErrors);

		// Check if a name is a built-in instruction
		bool isBuiltInInstruction(const char* name) const;

		bool isNumericType(StackValueType type) const;

		StackValueType getConstantType(const std::string& value) const;

		const char* typeToString(StackValueType type) const;

		std::string findStructTypeByFields(const std::unordered_map<std::string, StackValueType>& accessedFields) const;

		StackValueType stringToStackValueType(const std::string& typeStr) const;

		bool isValidTypeName(const std::string& typeStr) const;

		bool isStructTypeName(const std::string& typeStr) const;

		bool isCurrentTypeParam(const std::string& typeStr) const;

		// Helper: Find the registered struct type for method lookup (handles generic types)
		// Given a concrete type like Box<i64>, returns the registered type like Box<T> if method exists
		std::string findMethodStructType(const std::string& concreteType, const std::string& methodName) const;

		// Report an error (gcc/clang style)
		void reportError(const char* message);
		void reportError(const IAstNode* node, const char* message);
		void reportErrorWithHint(const IAstNode* node, const char* message, const char* hint);

		// Report an error conditionally (for signature analysis)
		void reportErrorConditional(const char* message, bool shouldReport);
		void reportErrorConditional(const IAstNode* node, const char* message, bool shouldReport);
		void reportErrorConditionalWithHint(
				const IAstNode* node, const char* message, const char* hint, bool shouldReport);

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

		// Symbol table: module-level `var` (mutable) — read like a constant,
		// writable via `-> name`. Tracked separately from constants so the
		// codegen can emit a load/store pair rather than a baked-in value.
		std::unordered_set<std::string> mDefinedGlobalVars;
		// Parallel map from global var name to declared type string ("i64",
		// "f64", "ptr", "str", or a sized-int like "i32"). Used by the
		// type-check pass to push the correct StackValueType when a global
		// is referenced.
		std::unordered_map<std::string, std::string> mGlobalVarTypes;

		// Symbol table: all defined structs
		std::unordered_set<std::string> mDefinedStructs;

		// Symbol table: all defined enums
		std::unordered_set<std::string> mDefinedEnums;

		// Symbol table: type aliases (name → target type string)
		std::unordered_map<std::string, std::string> mTypeAliases;

		// Track $"..." interpolation nodes already resolved (avoid double-resolution in merged modules)
		std::unordered_set<IAstNode*> mResolvedInterpolations;

		// Pre-collected structs and constants from main file (for sibling namespace support)
		// These are collected before sibling files are processed and should not trigger
		// duplicate errors when collectDefinitions runs on the main file
		std::unordered_set<std::string> mPreCollectedStructs;
		std::unordered_set<std::string> mPreCollectedConstants;

		// Symbol table: all defined tests
		std::unordered_set<std::string> mDefinedTests;

		// Struct declarations: maps struct name -> AST node (for local structs)
		std::unordered_map<std::string, class AstNodeStructDeclaration*> mStructDeclarations;

		// Module struct declarations: maps struct name -> AST node (for module structs)
		std::unordered_map<std::string, class AstNodeStructDeclaration*> mModuleStructDeclarations;

		// Struct field types: maps struct name -> (field name -> type)
		std::unordered_map<std::string, std::unordered_map<std::string, StackValueType>> mStructFieldTypes;

		// Struct field struct types: maps struct name -> (field name -> struct type name)
		// Only populated for fields that are struct-typed (i.e., where the type is another struct)
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> mStructFieldStructTypes;

		// Struct field order: maps struct name -> vector of field names (in declaration order)
		std::unordered_map<std::string, std::vector<std::string>> mStructFieldOrder;

		// Struct fields with defaults: maps struct name -> set of field names that have default values
		std::unordered_map<std::string, std::unordered_set<std::string>> mStructFieldsWithDefaults;

		// Track which struct type each local variable holds (for PTR types)
		// Maps variable name -> struct type name (empty string if not a struct pointer)
		std::unordered_map<std::string, std::string> mLocalVariableStructTypes;

		// Track function pointer signatures for local variables
		// Maps variable name -> function signature (for variables that hold function pointers)
		std::unordered_map<std::string, FunctionSignature> mLocalVariableFnSignatures;

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

		// Module dependency chain: tracks the current chain of module imports being processed
		// Used to detect circular dependencies (e.g., A -> B -> C -> A)
		std::vector<std::string> mModuleDependencyChain;

		// Module functions: maps module name -> set of function names in that module
		// Maps module name -> (function name -> isPublic flag)
		std::unordered_map<std::string, std::unordered_map<std::string, bool>> mModuleFunctions;

		// Module constants: maps module name -> (constant name -> isPublic flag)
		std::unordered_map<std::string, std::unordered_map<std::string, bool>> mModuleConstants;

		// Module constant values: maps "module::name" -> value string
		std::unordered_map<std::string, std::string> mModuleConstantValues;

		// Module structs: maps module name -> (struct name -> isPublic flag)
		std::unordered_map<std::string, std::unordered_map<std::string, bool>> mModuleStructs;

		// Module imported functions: maps module name -> (function name -> ImportedFunctionInfo)
		// Only contains public imported functions that are exported by the module
		std::unordered_map<std::string, std::unordered_map<std::string, ImportedFunctionInfo>> mModuleImportedFunctions;

		// Module directories: maps module name -> directory path where module was found
		std::unordered_map<std::string, std::string> mModuleDirectories;

		// Modules merged into main namespace (local file imports)
		// For these modules, "Module::Struct" and "Struct" are equivalent types
		std::unordered_set<std::string> mMergedModules;

		// Function signatures: stack effect of each function
		std::unordered_map<std::string, FunctionSignature> mFunctionSignatures;

		// Struct methods: maps structType -> (methodName -> isPublic)
		std::unordered_map<std::string, std::unordered_map<std::string, bool>> mStructMethods;

		// Struct method declarations: maps structType -> (methodName -> AstNodeFunctionDeclaration*)
		std::unordered_map<std::string, std::unordered_map<std::string, AstNodeFunctionDeclaration*>>
				mStructMethodDecls;

		// Module struct methods: maps module -> structType -> (methodName -> isPublic)
		std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, bool>>>
				mModuleStructMethods;

		// Error count
		size_t mErrorCount;

		// Warning count
		size_t mWarningCount;

		// Track reported errors to avoid duplicates (key: line:column:message)
		std::unordered_set<std::string> mReportedErrors;

		// Whether warnings should be treated as errors
		bool mWerror;

		// Whether this is validating a module file (vs main entry point)
		bool mIsModuleFile;

		// Error storage for LSP
		bool mStoreErrors;
		std::vector<ErrorInfo> mStoredErrors;

		// Minimum line for warnings (warnings on earlier lines are suppressed)
		// Default is 0 (no suppression)
		size_t mWarningMinLine;

		// Current function's type parameters (for generic functions)
		// Set when entering a generic function, cleared when leaving
		std::vector<std::string> mCurrentTypeParams;

		// Whether the current function being validated is fallible (can throw)
		// Used to restrict 'panic' to only be called in fallible functions
		bool mCurrentFunctionFallible;

		// Number of output values the current function's signature expects
		// Used to validate 'return' statements push the right number of values
		size_t mCurrentFunctionOutputCount;

		// Functions and tests whose body referenced a removed builtin. Their stack
		// simulation is meaningless from that point on - the removed name pushes
		// nothing - so type checking skips them rather than emitting a cascade of
		// underflow/arity errors on lines the user must not change.
		std::unordered_set<const IAstNode*> mBodiesWithRemovedBuiltins;

		// Pending function signature - set when an anonymous function or function pointer
		// with known signature is pushed onto the stack, used by 'call' instruction
		std::optional<FunctionSignature> mPendingFnSignature;

		// Additional module search paths from -I flags
		std::vector<std::string> mIncludePaths;

		// Freestanding mode: see setFreestandingMode().
		bool mFreestandingMode = false;

		// Whether we're currently type checking inside a loop body
		// When true, type errors are suppressed (but method calls are still marked)
		bool mInLoopBody;

		// Whether the current function has unpredictable stack effects
		// When true, output validation is skipped (e.g., 'read' has dynamic stack effect,
		// or unhandled instructions whose stack effects are unknown)
		bool mHasUnpredictableStack;

		// Source text for error context printing (optional, may be null)
		const char* mSource;

		// Cached parsed module ASTs - populated during validation for reuse by callers
		// Key: file path, Value: ParsedModuleAst with AST ownership
		std::unordered_map<std::string, ParsedModuleAst> mParsedModuleAsts;
		// Order in which modules were parsed (for dependency-aware processing)
		std::vector<std::string> mParsedModuleOrder;

		// Sibling files for directory-based namespace system
		// These are automatically merged into the main namespace
		std::vector<std::string> mSiblingFiles;
	};

} // namespace Qd

#endif // QD_QC_SEMANTIC_VALIDATOR_H
