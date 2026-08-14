// Internal implementation header for LlvmGenerator
// Not part of public API - only for use by generator_*.cc files

#ifndef QD_LLVMGEN_GENERATOR_IMPL_H
#define QD_LLVMGEN_GENERATOR_IMPL_H

#include <quadrate/llvmgen/generator.h>

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
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

// OrcJIT headers for JIT execution
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_array.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_break.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_continue.h>
#include <quadrate/qc/ast_node_ctx.h>
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_return.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/ast_node_use.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace Qd {

	// Default stack size for runtime context creation
	static const size_t DEFAULT_STACK_SIZE = 1024;

	// Forward declarations for helper functions
	bool looksLikeStructType(const std::string& typeStr);
	std::string extractStructName(const std::string& typeStr);

	class LlvmGenerator::Impl {
	public:
		std::unique_ptr<llvm::LLVMContext> context;
		std::unique_ptr<llvm::Module> module;
		std::unique_ptr<llvm::IRBuilder<>> builder;

		// Debug info generation
		std::unique_ptr<llvm::DIBuilder> debugBuilder;
		llvm::DICompileUnit* compileUnit = nullptr;
		llvm::DIFile* debugFile = nullptr;
		std::vector<llvm::DIScope*> debugScopeStack;
		bool debugInfoEnabled = false;
		std::string sourceFileName;
		llvm::DIType* contextDebugType = nullptr;
		llvm::DIType* stackElementDebugType = nullptr;
		llvm::DIType* int64DebugType = nullptr;
		llvm::DIType* floatDebugType = nullptr;
		llvm::DIType* stringDebugType = nullptr;

		// Map of local variable names to their declared types (from function parameters)
		std::unordered_map<std::string, std::string> localVariableTypeHints;

		// Track the last type pushed to the stack for debug info inference
		// This helps show proper types for untyped locals like "42 -> x"
		enum class LastPushedType {
			UNKNOWN,
			INTEGER,
			FLOAT,
			STRING,
			POINTER
		};
		LastPushedType lastPushedType = LastPushedType::UNKNOWN;

		// Optimization level (0-3)
		int optimizationLevel = 0;

		// Export mode - use external linkage for shared library compilation
		bool exportMode = false;

		// Stack size
		size_t stackSize = DEFAULT_STACK_SIZE;

		// Target triple for cross-compilation (empty = use host default)
		std::string targetTriple;

		// Runtime types
		llvm::Type* contextPtrTy = nullptr;
		llvm::Type* execResultTy = nullptr;
		llvm::Type* stackElementTy = nullptr;
		llvm::StructType* contextStructTy = nullptr; // Cached context struct layout:
													 // {st, error_code, error_msg, argc, argv, program_name, has_error}

		// Field indices into contextStructTy. Must match qd_context in
		// lib/rt/include/quadrate/rt/context.h.
		static constexpr unsigned CTX_FIELD_ERROR_CODE = 1;
		static constexpr unsigned CTX_FIELD_HAS_ERROR = 6;

		llvm::StructType* closureStructTy = nullptr; // Cached closure struct layout: {magic, fn, env, capture_count}
		llvm::StructType* stackStructTy = nullptr;	 // Cached stack struct layout: {data, capacity, size}
		llvm::PointerType* ptrTy = nullptr;			 // Cached pointer type (replaces PointerType::getUnqual(*context))
		llvm::Type* int64Ty = nullptr;				 // Cached i64 type
		llvm::Type* int32Ty = nullptr;				 // Cached i32 type

		// Runtime functions
		llvm::Function* createContextFn = nullptr;
		llvm::Function* freeContextFn = nullptr;
		llvm::Function* cloneContextFn = nullptr;
		llvm::Function* pushIntFn = nullptr;
		llvm::Function* pushFloatFn = nullptr;
		llvm::Function* pushStrFn = nullptr;
		llvm::Function* pushStrRefFn = nullptr;
		llvm::Function* pushPtrFn = nullptr;
		llvm::Function* callFn = nullptr;
		llvm::Function* printsFn = nullptr;
		llvm::Function* nlFn = nullptr;
		llvm::Function* stackPopFn = nullptr;
		llvm::Function* stackSizeFn = nullptr;
		llvm::Function* pushCallFn = nullptr;
		llvm::Function* popCallFn = nullptr;
		llvm::Function* checkStackFn = nullptr;
		llvm::Function* strdupFn = nullptr;
		llvm::Function* mallocFn = nullptr;
		llvm::Function* freeFn = nullptr;
		llvm::Function* qdStringReleaseFn = nullptr;
		llvm::Function* qdStringDataFn = nullptr;
		llvm::Function* qdStructAllocFn = nullptr;
		llvm::Function* qdStructReleaseFn = nullptr;
		llvm::Function* qdStructRetainFn = nullptr;
		llvm::Function* qdPtrReleaseFn = nullptr;
		llvm::Function* qdPtrRetainFn = nullptr;
		llvm::Function* addFn = nullptr;
		llvm::Function* subFn = nullptr;
		llvm::Function* mulFn = nullptr;
		llvm::Function* andFn = nullptr;
		llvm::Function* orFn = nullptr;
		llvm::Function* xorFn = nullptr;
		llvm::Function* notFn = nullptr;
		llvm::Function* lnotFn = nullptr;
		llvm::Function* shlFn = nullptr;
		llvm::Function* shrFn = nullptr;
		llvm::Function* exitFn = nullptr;
		llvm::Function* printStackTraceFn = nullptr;
		llvm::Function* printErrorMsgFn = nullptr;

		// Loop context for break/continue
		struct LoopContext {
			llvm::BasicBlock* breakTarget;
			llvm::BasicBlock* continueTarget;

			// For compile-time stack break/continue handling
			struct BranchInfo {
				llvm::BasicBlock* fromBlock;
				std::vector<llvm::Value*> stackState;
			};

			std::vector<BranchInfo> breakInfos;
			std::vector<BranchInfo> continueInfos;
		};

		std::vector<LoopContext> loopStack;

		// User-defined functions
		std::map<std::string, llvm::Function*> userFunctions;
		std::map<std::string, bool> fallibleFunctions;
		std::set<std::string> importedCFunctions;
		std::map<std::string, std::string> functionReturnStructType;
		// For methods: maps register name to receiver position from top of stack
		// (i.e., number of non-receiver args). Used for fallback method resolution
		// when the semantic validator didn't mark the call as a method call.
		std::map<std::string, size_t> methodReceiverPosition;

		// Native calling convention for optimized functions
		// Maps register name (e.g. "fib") to the native LLVM function
		std::map<std::string, llvm::Function*> nativeFunctions;

		enum class NativeParamType {
			I64,
			F64
		};

		struct NativeFuncInfo {
			size_t inputCount;
			size_t outputCount; // 0 or 1
			std::vector<NativeParamType> inputTypes;
			NativeParamType outputType = NativeParamType::I64;
		};

		std::map<std::string, NativeFuncInfo> nativeFuncInfo;

		// Compile-time value stack (active when useCompileTimeStack=true)
		std::vector<llvm::Value*> compileTimeStack;
		bool useCompileTimeStack = false;

		// Return value alloca for native functions (used by return statement)
		llvm::AllocaInst* nativeReturnAlloca = nullptr;

		// Native local variables: name → alloca (i64) for compile-time stack mode
		std::map<std::string, llvm::AllocaInst*> nativeLocalVariables;

		// Cross-module imported function info
		struct CrossModuleImportInfo {
			std::string library;
			std::string cFunctionName;
			bool throws;
		};

		std::map<std::string, CrossModuleImportInfo> crossModuleImportedFunctions;

		// Module constants
		std::map<std::string, std::string> moduleConstants;

		// Module-level mutable variables declared with `var`. Map a name to the
		// LLVM global that holds the value. Reads of the identifier lower to a
		// load from this global; `-> name` writes lower to a store. Populated
		// alongside moduleConstants during the initial module pass.
		std::map<std::string, llvm::GlobalVariable*> moduleGlobalVars;
		std::map<std::string, std::string> moduleGlobalVarTypes; // name -> type string

		// Module-level vars whose initializer is a struct construction; these
		// need a runtime init sequence in `main` because struct allocation
		// calls qd_struct_alloc and writes fields at runtime. The IAstNode*
		// is the AstNodeStructConstruction held by the var node.
		std::vector<std::pair<llvm::GlobalVariable*, IAstNode*>> pendingStructGlobalInits;

		// Module ASTs
		std::vector<std::pair<std::string, IAstNode*>> moduleASTs;

		// Modules that should be merged into main namespace
		// Functions from these modules are also registered with unqualified names
		std::set<std::string> mergedModules;

		// Module source files for debug info
		std::map<std::string, std::string> moduleSourceFiles;

		// Per-module debug files
		std::map<std::string, llvm::DIFile*> moduleDebugFiles;

		// Imported libraries for linking
		std::set<std::string> importedLibraries;

		// Global string cache to avoid creating duplicate constants
		std::map<std::string, llvm::Value*> globalStringCache;

		// Additional library search paths
		std::vector<std::string> librarySearchPaths;

		// Function context
		llvm::BasicBlock* currentFunctionReturnBlock = nullptr;
		bool currentFunctionIsFallible = false;
		bool currentFunctionIsIntegerOnly = false;
		bool currentFunctionIsMain = false;

		// Current module prefix
		std::string currentModulePrefix = "main";

		// The main module name
		std::string mainModuleName = "main";

		// Defer statements.
		//
		// A `defer` is collected here when codegen walks past it, and its body is emitted at
		// scope exit. Collection is lexical, but *execution* must not be: a defer written inside
		// an `if`/`switch` arm that was not taken, or after a `?` that propagated out early, was
		// never reached and must not run. Each entry therefore carries an i1 flag alloca, stored
		// `true` where the defer statement sits and tested at scope exit.
		struct DeferEntry {
			AstNodeDefer* node;
			llvm::Value* reached; ///< i1 alloca: did control actually reach the defer statement?
		};

		std::vector<std::vector<DeferEntry>> deferScopeStack;

		// Counter for unique variable names
		int varCounter = 0;

		// Compilation status
		bool compilationFailed = false;

		bool safeParseInt64(const std::string& str, int64_t& out) {
			if (str.empty()) {
				return false;
			}
			if (str.size() > 2 && str[0] == '0') {
				if (str[1] == 'x' || str[1] == 'X') {
					auto [ptr, ec] = std::from_chars(str.data() + 2, str.data() + str.size(), out, 16);
					return ec == std::errc() && ptr == str.data() + str.size();
				} else if (str[1] == 'b' || str[1] == 'B') {
					auto [ptr, ec] = std::from_chars(str.data() + 2, str.data() + str.size(), out, 2);
					return ec == std::errc() && ptr == str.data() + str.size();
				}
			}
			auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out);
			return ec == std::errc() && ptr == str.data() + str.size();
		}

		// Local variables
		std::map<std::string, llvm::AllocaInst*> localVariables;
		std::map<std::string, std::string> localVariableStructTypes;
		std::set<std::string> localArrayVariables;
		std::set<std::string> stackAllocatedStructLocals;

		// Captured variables
		std::map<std::string, llvm::AllocaInst*> capturedVariableRefs;
		std::set<std::string> heapAllocatedCaptures;
		std::map<std::string, llvm::Value*> heapCapturePointers;
		std::set<std::string> indirectLocalVariables;
		std::set<std::string> closureVariables;

		// Tracking for smart operations
		std::string lastIdentifierPushed;
		std::string lastStructConstructed;
		std::string lastFieldAccessResultType;
		bool lastPushedWasArray = false;
		bool currentFunctionReturnsPtr = false;
		bool lastStructWasConstructedInPlace = false;

		// Anonymous function tracking
		size_t anonymousFunctionCounter = 0;
		std::string lastGeneratedAnonFuncName;
		bool lastGeneratedWasClosure = false;
		size_t lastClosureCaptureCount = 0;

		// Function pointer aliases
		std::map<std::string, llvm::Function*> functionPointerAliases;

		// Struct definitions
		struct FieldInfo {
			std::string name;
			std::string typeName;
			size_t offset;
			size_t size;
			bool isTypeParam = false;
			std::vector<IAstNode*> defaultValue; // Default value expression nodes (not owned)
		};

		struct StructLayout {
			std::string name;
			std::vector<FieldInfo> fields;
			size_t totalSize;
			bool isPublic;
			bool isPacked = false;
		};

		std::map<std::string, StructLayout> structDefinitions;
		std::map<std::string, llvm::Function*> structDestructors;
		std::unordered_map<std::string, std::string> typeAliases; // name → resolved type
		std::string currentModuleName;

		// Test mode
		bool testMode = false;
		std::vector<std::pair<std::string, std::string>> collectedTestNames;
		llvm::Value* testErrorAlloca = nullptr;

		// Coverage mode (only meaningful when testMode is also true).
		bool coverageMode = false;
		// Names of user functions whose bodies were instrumented for coverage,
		// in the order they were registered. Index in this vector is the
		// runtime coverage idx passed to qd_coverage_mark.
		std::vector<std::string> coverageFunctionNames;

		// Freestanding mode: no libc, no auto-main, exports _start instead.
		bool freestandingMode = false;

		// Iterator variables
		std::unordered_map<std::string, llvm::Value*> iteratorVars;

		// Constructor
		Impl(const std::string& moduleName) {
			context = std::make_unique<llvm::LLVMContext>();
			module = std::make_unique<llvm::Module>(moduleName, *context);
			builder = std::make_unique<llvm::IRBuilder<>>(*context);
		}

		// Core generation methods
		void setupRuntimeDeclarations();
		llvm::Value* getOrCreateGlobalString(const std::string& str);
		void createForwardingWrapperBody(llvm::Function* wrapperFn, llvm::Function* targetFn);
		bool generateProgram(IAstNode* root);
		bool generateFunction(
				AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix = "main");
		// Declare a function (create LLVM function and register in userFunctions, but don't generate body)
		bool declareFunction(AstNodeFunctionDeclaration* funcNode, const std::string& namePrefix);
		// Generate function body (assumes function is already declared)
		bool generateFunctionBody(
				AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix = "main");
		bool generateTest(AstNodeTest* testNode, const std::string& namePrefix = "main");
		bool generateTestRunner(const std::vector<std::pair<std::string, std::string>>& testNamesWithDisplay);

		// Node generation
		void generateNode(IAstNode* node, llvm::Value* ctx);
		void generateInstruction(AstNodeInstruction* inst, llvm::Value* ctx);
		void generateLiteral(AstNodeLiteral* lit, llvm::Value* ctx);
		void generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx);
		void generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr, llvm::Value* ctx);
		void generateAnonymousFunction(AstNodeAnonymousFunction* anonFunc, llvm::Value* ctx);
		void generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx);
		void generateLocal(AstNodeLocal* local, llvm::Value* ctx);
		void generateLocalOne(const std::string& name, size_t lineNum, llvm::Value* ctx);
		void generateLocalCleanup();
		void generateCastInstructions(const std::vector<CastDirection>& casts, llvm::Value* ctx);

		// Error state
		/** @brief The error state left by a fallible call that has just returned. */
		struct ErrorState {
			llvm::Value* failed; ///< i1, true if the call failed
			llvm::Value* code;	 ///< i64, the error code the call reported
		};

		/** @brief Clear both error fields before a fallible call, so the post-call test sees
		 *		   only this call's outcome. */
		void generateClearErrorState(llvm::Value* ctx);
		/** @brief Read the error state after a fallible call returns.
		 *
		 * `failed` tests `has_error` -- set unconditionally by `qd_panic`, so a `panic` carrying
		 * code 0 is still a failure -- or a non-zero `error_code`, since imported C functions set
		 * the code but not the flag. */
		ErrorState generateReadErrorState(llvm::Value* ctx, const char* name = "has_error");

		// Control flow
		void generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx);
		void generateFor(AstNodeForStatement* forStmt, llvm::Value* ctx);
		void generateLoop(AstNodeLoopStatement* loopStmt, llvm::Value* ctx);
		void generateCtxBlock(AstNodeCtx* ctxNode, llvm::Value* ctx);
		void generateSwitchStatement(AstNodeSwitchStatement* switchStmt, llvm::Value* ctx);

		// Struct handling
		void processStructDeclaration(AstNodeStructDeclaration* structDecl, const std::string& moduleName);
		void generateStructDestructors();
		void generateStructConstruction(const std::string& structName, llvm::Value* ctx);
		const StructLayout* findStructDefinition(const std::string& structName) const;
		void generateFieldAccess(AstNodeFieldAccess* fieldAccess, llvm::Value* ctx);
		void generateFieldSet(AstNodeFieldSet* fieldSet, llvm::Value* ctx);
		void generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral, llvm::Value* ctx);
		bool isKnownStruct(const std::string& typeName);
		size_t getTypeSize(const std::string& typeName);
		void generateStructCleanup(llvm::Value* structPtr, const std::string& structTypeName);

		// Helpers
		void collectAllCapturesFromAST(IAstNode* node, std::set<std::string>& captures);
		std::string findLastStructConstruction(IAstNode* node);
		void collectCalledFunctions(
				IAstNode* node, const std::string& currentModule, std::set<std::string>& calledFunctions);
		void computeReachableFunctions(IAstNode* mainRoot, std::set<std::string>& reachable);

		// Set of functions reachable from main (computed before IR generation)
		std::set<std::string> reachableFunctions;
		bool useReachabilityAnalysis = true;

		// Common stack access helper - extracts the repeated pattern of accessing ctx->st
		struct StackAccess {
			llvm::Value* sizePtr;
			llvm::Value* size;
			llvm::Value* data;
		};

		StackAccess getStackAccess(llvm::Value* ctx);

		// Allocate a stack slot in the current function's entry block rather than
		// at the current insertion point. Entry-block allocas are reclaimed only
		// on function return, so this avoids unbounded native-stack growth when
		// the alloca site sits inside a loop body (at -O0 nothing hoists it).
		llvm::AllocaInst* createEntryAlloca(llvm::Type* ty, const char* name) {
			llvm::Function* fn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock& entry = fn->getEntryBlock();
			llvm::IRBuilder<> entryBuilder(&entry, entry.getFirstInsertionPt());
			return entryBuilder.CreateAlloca(ty, nullptr, name);
		}

		// Fatal error helper - emits write(2, msg, len), qd_print_stack_trace(ctx), _exit(1), unreachable
		void emitFatalError(llvm::Value* ctx, const char* message);

		// Inline stack operation helpers
		struct BinaryOpContext {
			llvm::Value* size;
			llvm::Value* sizePtr;
			llvm::Value* value1;
			llvm::Value* value2;
			llvm::Value* resultPtr; // Where to store result (value1's location)
		};

		BinaryOpContext setupBinaryOp(llvm::Value* ctx);
		void finishBinaryOp(const BinaryOpContext& boc, llvm::Value* result);

		// Inline stack operations
		void generateInlinePushInt(llvm::Value* ctx, int64_t value);
		void generateInlinePushIntValue(llvm::Value* ctx, llvm::Value* value);
		void generateInlineIntAdd(llvm::Value* ctx);
		void generateInlineIntSub(llvm::Value* ctx);
		void generateInlineIntMul(llvm::Value* ctx);
		void generateInlineIntMod(llvm::Value* ctx);
		void generateInlineIntCompare(llvm::Value* ctx, llvm::CmpInst::Predicate pred, const char* resultName);
		void generateInlineIntLt(llvm::Value* ctx);
		void generateInlineIntGt(llvm::Value* ctx);
		void generateInlineIntEq(llvm::Value* ctx);
		void generateInlineIntNeq(llvm::Value* ctx);
		void generateInlineIntLte(llvm::Value* ctx);
		void generateInlineIntGte(llvm::Value* ctx);
		void generateInlineBitAnd(llvm::Value* ctx);
		void generateInlineBitOr(llvm::Value* ctx);
		void generateInlineBitXor(llvm::Value* ctx);
		void generateInlineBitNot(llvm::Value* ctx);
		void generateInlineLogicalNot(llvm::Value* ctx);
		void generateInlineBitLshift(llvm::Value* ctx);
		void generateInlineBitRshift(llvm::Value* ctx);
		void generateInlineDup(llvm::Value* ctx);
		void generateInlineSwap(llvm::Value* ctx);
		void generateInlineDrop(llvm::Value* ctx);
		void generateInlineOver(llvm::Value* ctx);
		void generateInlineRot(llvm::Value* ctx);
		llvm::Value* generateInlinePopInt(llvm::Value* ctx);					 // Returns popped i64 value
		llvm::Value* generateInlinePopFloat(llvm::Value* ctx);					 // Returns popped f64 value
		void generateInlinePushFloatValue(llvm::Value* ctx, llvm::Value* value); // Pushes f64 value to runtime stack
		void generateInlinePopIntToStorage(llvm::Value* ctx, llvm::Value* dst);	 // Pops i64 and stores to dst alloca

		// Type-aware operation helpers
		struct TypeAwareOpContext {
			llvm::Value* size;
			llvm::Value* sizePtr;
			llvm::Value* elem1Ptr;
			llvm::Value* elem2Ptr;
			llvm::BasicBlock* fastPath;
			llvm::BasicBlock* slowPath;
			llvm::BasicBlock* endBlock;
		};

		TypeAwareOpContext setupTypeAwareOp(llvm::Value* ctx, const char* opName);
		void finishTypeAwareOp(const TypeAwareOpContext& toc, llvm::Value* result, llvm::Value* resultPtr);

		// Type-aware operations
		void generateTypeAwareAdd(llvm::Value* ctx);
		void generateTypeAwareSub(llvm::Value* ctx);
		void generateTypeAwareMul(llvm::Value* ctx);
		// Shared implementation for the type-aware comparison operators: emits
		// an integer fast path using `pred` and falls back to `runtimeFnName`.
		void generateTypeAwareCompare(
				llvm::Value* ctx, const char* opName, llvm::CmpInst::Predicate pred, const char* runtimeFnName);
		void generateTypeAwareLt(llvm::Value* ctx);
		void generateTypeAwareGt(llvm::Value* ctx);
		void generateTypeAwareEq(llvm::Value* ctx);
		void generateTypeAwareNeq(llvm::Value* ctx);
		void generateTypeAwareLte(llvm::Value* ctx);
		void generateTypeAwareGte(llvm::Value* ctx);
		void generateTypeAwareDiv(llvm::Value* ctx);
		void generateTypeAwareMod(llvm::Value* ctx);

		// Defer scope management
		void pushDeferScope();
		void popDeferScope();
		/** @brief Record a defer in the current scope and mark it reached at this point. */
		void registerDefer(AstNodeDefer* deferNode, llvm::Value* ctx);
		/** @brief Emit the current scope's defer bodies (LIFO, each guarded by its reached flag)
		 *		   without popping it. */
		void emitDeferScope(llvm::Value* ctx);
		/** @brief emitDeferScope, then pop the scope. */
		void executeDeferScope(llvm::Value* ctx);

		// Analysis helpers
		void collectLocalNames(IAstNode* node, std::set<std::string>& names);
		bool analyzeIsBodyNativeEligible(
				IAstNode* node, const std::set<std::string>& localNames, bool allowFloat = true);
		bool analyzeCalleesAllNative(IAstNode* node);
	};

} // namespace Qd

#endif // QD_LLVMGEN_GENERATOR_IMPL_H
