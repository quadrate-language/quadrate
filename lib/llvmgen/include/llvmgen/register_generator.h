// Register-based LLVM code generator for Quadrate
// Uses compile-time stack simulation with LLVM SSA values

#ifndef QD_LLVMGEN_REGISTER_GENERATOR_H
#define QD_LLVMGEN_REGISTER_GENERATOR_H

#include <llvmgen/generator.h>

// Suppress all warnings from LLVM headers (third-party code)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <qc/ast_node.h>
#include <qc/ast_node_anonymous_function.h>
#include <qc/ast_node_array.h>
#include <qc/ast_node_array_literal.h>
#include <qc/ast_node_break.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_continue.h>
#include <qc/ast_node_ctx.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_loop.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_return.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
#include <qc/ast_node_struct_construction.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_test.h>
#include <qc/ast_node_use.h>
#include <qc/ast_node_while.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Qd {

	// Value types for compile-time stack tracking
	enum class ValueType {
		INT,
		FLOAT,
		STRING,
		PTR,	// Struct pointer or function pointer
		BOOL,	// Result of comparisons
	};

	// A tracked value on the compile-time stack
	struct TrackedValue {
		llvm::Value* value;				// LLVM SSA value
		ValueType type;					// Type of the value
		std::string structType;			// For PTR: which struct type (empty for function pointers)
		bool needsRelease;				// True if this value is reference-counted and needs release
		bool isStringLiteral;			// True if this is a string literal (no release needed)
		llvm::FunctionType* closureFuncType;  // For closures: the function type of the closure

		TrackedValue(llvm::Value* v, ValueType t, bool release = false)
			: value(v), type(t), needsRelease(release), isStringLiteral(false), closureFuncType(nullptr) {}

		TrackedValue(llvm::Value* v, ValueType t, const std::string& st, bool release = false)
			: value(v), type(t), structType(st), needsRelease(release), isStringLiteral(false), closureFuncType(nullptr) {}

		TrackedValue(llvm::Value* v, ValueType t, const std::string& st, bool release, llvm::FunctionType* ft)
			: value(v), type(t), structType(st), needsRelease(release), isStringLiteral(false), closureFuncType(ft) {}
	};

	// Information about a local variable
	struct LocalInfo {
		llvm::AllocaInst* alloca;  // Storage location
		ValueType type;			   // Type of the variable
		std::string structType;	   // For PTR: which struct type
		bool needsRelease;		   // True if needs release on scope exit
		llvm::FunctionType* closureFuncType = nullptr;  // For closures: the function type
	};

	// Struct field information
	struct RegFieldInfo {
		std::string name;
		std::string typeName;
		size_t offset;
		size_t size;
	};

	// Struct layout information
	struct RegStructLayout {
		std::string name;
		std::vector<RegFieldInfo> fields;
		size_t totalSize;
		bool isPublic;
	};

	// Loop context for break/continue
	struct RegLoopContext {
		llvm::BasicBlock* breakTarget;
		llvm::BasicBlock* continueTarget;
		llvm::AllocaInst* conditionAlloca;  // For while loops, stores condition before continue
		std::vector<llvm::AllocaInst*> stackAllocas;  // For loop, stores stack values across iterations
		std::string savedIterName;  // For restoring shadowed iterator
		bool hasSavedIter = false;  // Whether we have a saved outer iterator
		llvm::AllocaInst* savedIterAlloca = nullptr;
		ValueType savedIterType = ValueType::INT;
		bool savedIterNeedsRelease = false;
	};

	// Function signature for user-defined functions
	struct RegFunctionSignature {
		std::vector<std::pair<std::string, ValueType>> params;			   // (name, type) pairs
		std::vector<std::pair<ValueType, std::string>> returns;			   // (type, structTypeName) pairs
		bool isFallible;												   // Can throw errors
	};

	// The register-based code generator
	class RegisterGenerator {
	public:
		RegisterGenerator();
		~RegisterGenerator();

		// Main entry points
		bool generate(IAstNode* root, const std::string& moduleName);
		void addModuleAST(const std::string& moduleName, IAstNode* moduleRoot,
						  const std::string& sourceFileName = "");

		// Output methods
		bool writeIR(const std::string& filename);
		bool writeObject(const std::string& filename);
		bool writeExecutable(const std::string& filename);
		std::string getIRString() const;

		// Configuration
		void setDebugInfo(bool enabled);
		void setOptimizationLevel(int level);
		void addLibrarySearchPath(const std::string& path);
		void setTestMode(bool enabled);
		void setStackSize(size_t size); // No-op for API compatibility

	private:
		// LLVM infrastructure
		std::unique_ptr<llvm::LLVMContext> context;
		std::unique_ptr<llvm::Module> module;
		std::unique_ptr<llvm::IRBuilder<>> builder;

		// Debug info
		std::unique_ptr<llvm::DIBuilder> debugBuilder;
		// llvm::DICompileUnit* compileUnit = nullptr;
		// llvm::DIFile* debugFile = nullptr;
		std::vector<llvm::DIScope*> debugScopeStack;
		bool debugInfoEnabled = false;
		std::string sourceFileName;

		// Configuration
		int optimizationLevel = 0;
		bool testMode = false;

		// Compile-time value stack
		std::vector<TrackedValue> valueStack;

		// Local variables in current function
		std::map<std::string, LocalInfo> localVariables;

		// Struct definitions
		std::map<std::string, RegStructLayout> structDefinitions;
		std::map<std::string, llvm::Function*> structDestructors;

		// User-defined functions
		std::map<std::string, llvm::Function*> userFunctions;
		std::map<std::string, RegFunctionSignature> functionSignatures;
		std::map<std::string, bool> fallibleFunctions;

		// Module system
		std::string currentModulePrefix = "main";
		std::string mainModuleName = "main";
		std::vector<std::pair<std::string, IAstNode*>> moduleASTs;
		std::map<std::string, std::string> moduleConstants;

		// Imported C functions
		std::set<std::string> importedCFunctions;
		std::set<std::string> importedLibraries;
		std::vector<std::string> librarySearchPaths;

		// Loop stack for break/continue
		std::vector<RegLoopContext> loopStack;

		// Defer statements
		std::vector<std::vector<AstNodeDefer*>> deferScopeStack;

		// Current function context
		llvm::Function* currentFunction = nullptr;
		// llvm::BasicBlock* currentFunctionReturnBlock = nullptr;
		bool currentFunctionIsFallible = false;

		// Closure support
		std::set<std::string> closureVariables;
		std::map<std::string, llvm::Value*> capturedVariableRefs;
		std::map<std::string, ValueType> capturedVariableTypes;
		llvm::FunctionType* lastClosureFuncType = nullptr;  // For calling closures
		// size_t anonymousFunctionCounter = 0;

		// Test mode support
		std::vector<std::pair<std::string, std::string>> collectedTestNames;

		// Runtime function declarations
		llvm::Function* mallocFn = nullptr;
		llvm::Function* freeFn = nullptr;
		// llvm::Function* printIntFn = nullptr;
		// llvm::Function* printFloatFn = nullptr;
		// llvm::Function* printStrFn = nullptr;
		// llvm::Function* nlFn = nullptr;
		llvm::Function* strdupFn = nullptr;
		llvm::Function* qdStringRetainFn = nullptr;
		llvm::Function* qdStringReleaseFn = nullptr;
		llvm::Function* qdStructAllocFn = nullptr;
		llvm::Function* qdStructRetainFn = nullptr;
		llvm::Function* qdStructReleaseFn = nullptr;

		// Counter for unique names
		int varCounter = 0;

		// Compilation status
		bool compilationFailed = false;

		// Runtime type tracking for arrays with unknown element type at compile time
		// Used when nth is called on an array retrieved from a pointer array
		llvm::AllocaInst* runtimeTypeAlloca = nullptr;	// Stores elemType (0=INT, 1=FLOAT, 2=STR, 3=PTR)
		llvm::AllocaInst* runtimeIntAlloca = nullptr;	// Stores int result if type=0
		llvm::AllocaInst* runtimeFloatAlloca = nullptr; // Stores float result if type=1
		llvm::AllocaInst* runtimePtrAlloca = nullptr;	// Stores ptr result if type=2 or 3

		// Reusable allocas for nth operations (created lazily in entry block)
		llvm::AllocaInst* nthIntAlloca = nullptr;
		llvm::AllocaInst* nthFloatAlloca = nullptr;
		llvm::AllocaInst* nthPtrAlloca = nullptr;

		// Setup methods
		void setupRuntimeDeclarations();
		void setupTargetTriple();

		// Program generation
		bool generateProgram(IAstNode* root);
		bool generateFunction(AstNodeFunctionDeclaration* funcNode, bool isMain,
							  const std::string& namePrefix = "main");
		bool generateTest(AstNodeTest* testNode, const std::string& namePrefix = "main");
		bool generateTestRunner();

		// Node generation
		void generateNode(IAstNode* node);
		void generateLiteral(AstNodeLiteral* lit);
		void generateIdentifier(AstNodeIdentifier* ident);
		void generateInstruction(AstNodeInstruction* inst);
		void generateLocal(AstNodeLocal* local);
		void generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent);
		bool generateNativeMathFunction(const std::string& name);
		bool generateNativeStrFunction(const std::string& name);
		bool generateNativeStrconvFunction(const std::string& name);
		bool generateNativeMemFunction(const std::string& name);
		bool generateNativeTermFunction(const std::string& name);
		bool generateNativeOsFunction(const std::string& name);
		bool generateNativeIoFunction(const std::string& name);
		bool generateNativeTimeFunction(const std::string& name);
		bool tryGenerateBuiltinInstruction(const std::string& name);
		void generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr);
		void generateAnonymousFunction(AstNodeAnonymousFunction* anonFunc);
		void generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral);

		// Control flow
		void generateIf(AstNodeIfStatement* ifStmt);
		void generateFor(AstNodeForStatement* forStmt);
		void generateWhile(AstNodeWhileStatement* whileStmt);
		void generateLoop(AstNodeLoopStatement* loopStmt);
		void generateSwitch(AstNodeSwitchStatement* switchStmt);
		void generateCtxBlock(AstNodeCtx* ctxNode);

		// Import handling
		void processImportStatement(AstNodeImport* importNode, const std::string& moduleName);

		// Struct handling
		void processStructDeclaration(AstNodeStructDeclaration* structDecl,
									  const std::string& moduleName);
		void generateStructConstruction(const std::string& structName);
		void generateStructConstructionNamed(AstNodeStructConstruction* structConstruction);
		void generateFieldAccess(AstNodeFieldAccess* fieldAccess);
		void generateFieldSet(AstNodeFieldSet* fieldSet);
		void generateStructDestructors();
		const RegStructLayout* findStructDefinition(const std::string& structName) const;
		size_t getTypeSize(const std::string& typeName);
		void releaseValue(const TrackedValue& val);

		// Arithmetic operations (all inline LLVM)
		void generateAdd();
		void generateSub();
		void generateMul();
		void generateDiv();
		void generateMod();

		// Comparison operations
		void generateLt();
		void generateGt();
		void generateEq();
		void generateNeq();
		void generateLte();
		void generateGte();

		// Bitwise operations
		void generateBitAnd();
		void generateBitOr();
		void generateBitXor();
		void generateBitNot();
		void generateBitLshift();
		void generateBitRshift();

		// Stack manipulation (compile-time)
		void generateDup();
		void generateDrop();
		void generateSwap();
		void generateOver();
		void generateRot();
		void generateNip();
		void generateTuck();

		// Defer scope management
		void pushDeferScope();
		void popDeferScope();
		void executeDeferScope();

		// Helper methods
		std::string mangleName(const std::string& name, const std::string& prefix);
		llvm::Type* getLlvmType(ValueType type);
		ValueType typeFromString(const std::string& typeStr);
		std::string uniqueName(const std::string& base);

		// Error handling
		void reportError(const std::string& msg, size_t line = 0);
	};

} // namespace Qd

#endif // QD_LLVMGEN_REGISTER_GENERATOR_H
