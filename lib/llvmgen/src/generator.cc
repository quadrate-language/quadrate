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
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <qc/ast_node.h>
#include <qc/ast_node_anonymous_function.h>
#include <qc/ast_node_array.h>
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
#include <qc/ast_node_switch.h>
#include <qc/ast_node_test.h>
#include <qc/ast_node_use.h>
#include <qc/ast_node_while.h>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace Qd {

	// Default stack size for runtime context creation
	static const size_t DEFAULT_STACK_SIZE = 1024;

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
		llvm::DIType* stackElementDebugType = nullptr; // qd_stack_element_t structure type

		// Optimization level (0-3)
		int optimizationLevel = 0;

		// Stack size
		size_t stackSize = DEFAULT_STACK_SIZE;

		// Runtime types
		llvm::Type* contextPtrTy = nullptr;
		llvm::Type* execResultTy = nullptr;
		llvm::Type* stackElementTy = nullptr;

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
		llvm::Function* shlFn = nullptr;
		llvm::Function* shrFn = nullptr;

		// Loop context for break/continue
		struct LoopContext {
			llvm::BasicBlock* breakTarget;
			llvm::BasicBlock* continueTarget;
		};

		std::vector<LoopContext> loopStack;

		// User-defined functions
		std::map<std::string, llvm::Function*> userFunctions;
		std::map<std::string, bool> fallibleFunctions;				 // Track which functions can throw errors
		std::set<std::string> importedCFunctions;					 // Track functions from imported C libraries
		std::map<std::string, std::string> functionReturnStructType; // Track struct type returned by functions

		// Module constants (scope::name -> value)
		std::map<std::string, std::string> moduleConstants;

		// Module ASTs to include (preserves insertion order for dependency resolution)
		std::vector<std::pair<std::string, IAstNode*>> moduleASTs;

		// Module source files for debug info (module name -> source file path)
		std::map<std::string, std::string> moduleSourceFiles;

		// Per-module debug files (module name -> DIFile)
		std::map<std::string, llvm::DIFile*> moduleDebugFiles;

		// Track imported libraries for linking
		std::set<std::string> importedLibraries;

		// Track additional library search paths (for third-party packages)
		std::vector<std::string> librarySearchPaths;

		// Function context for return
		llvm::BasicBlock* currentFunctionReturnBlock = nullptr;
		bool currentFunctionIsFallible = false;
		bool currentFunctionIsIntegerOnly = false; // For type specialization

		// Current module prefix for intra-module function calls
		std::string currentModulePrefix = "main";

		// The main module name (as passed to generate())
		std::string mainModuleName = "main";

		// Defer statements collected during function generation (scope-based)
		std::vector<std::vector<AstNodeDefer*>> deferScopeStack;

		// Counter for unique variable names
		int varCounter = 0;

		// Compilation status
		bool compilationFailed = false;

		bool safeParseInt64(const std::string& str, int64_t& out) {
			if (str.empty()) {
				return false;
			}
			// Handle hex (0x/0X) and binary (0b/0B) prefixes
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

		// Local variables (per function scope): name -> alloca instruction
		std::map<std::string, llvm::AllocaInst*> localVariables;

		// Track struct types for local variables: variable name -> struct type name
		std::map<std::string, std::string> localVariableStructTypes;

		// Track which local variables hold arrays (need ref counting)
		std::set<std::string> localArrayVariables;

		// Captured variables by reference: name -> alloca holding pointer to outer variable
		// These need extra indirection when accessed
		std::map<std::string, llvm::AllocaInst*> capturedVariableRefs;

		// Variables that need heap allocation because they're captured by closures
		// (for escaped closure support - captured vars must survive function return)
		std::set<std::string> heapAllocatedCaptures;

		// Heap-allocated captures: name -> heap pointer (for cleanup/ownership tracking)
		std::map<std::string, llvm::Value*> heapCapturePointers;

		// Track variables that need indirection (heap-allocated captured variables)
		// For these, localVariables[name] is an alloca holding a pointer to heap memory
		std::set<std::string> indirectLocalVariables;

		// Track variables that hold closures (need special cleanup)
		std::set<std::string> closureVariables;

		// Track the last identifier that pushed a value (for smart free)
		std::string lastIdentifierPushed;

		// Track the last struct type that was constructed (for local binding)
		std::string lastStructConstructed;

		// Track the result type of the last field access (for chained field access)
		std::string lastFieldAccessResultType;

		// Track whether the last pushed value was an array literal
		bool lastPushedWasArray = false;

		// Track if current function returns a pointer (structs must be heap-allocated)
		bool currentFunctionReturnsPtr = false;

		// Counter for generating unique anonymous function names
		size_t anonymousFunctionCounter = 0;

		// Track the last generated anonymous function name (for aliasing via -> name)
		std::string lastGeneratedAnonFuncName;

		// Track if the last generated anonymous function was a closure with captures
		bool lastGeneratedWasClosure = false;

		// Track the capture count of the last generated closure (for cleanup)
		size_t lastClosureCaptureCount = 0;

		// Function pointer aliases: variable name -> LLVM function
		// Used when anonymous functions are stored via -> name and referenced from nested scopes
		std::map<std::string, llvm::Function*> functionPointerAliases;

		// Struct definitions: struct name -> field information
		struct FieldInfo {
			std::string name;
			std::string typeName; // "i64", "f64", "str", "*StructName"
			size_t offset;		  // Byte offset from struct start
			size_t size;		  // Size in bytes
		};

		struct StructLayout {
			std::string name;
			std::vector<FieldInfo> fields;
			size_t totalSize;
			bool isPublic;
		};

		std::map<std::string, StructLayout> structDefinitions;
		std::map<std::string, llvm::Function*> structDestructors; // Generated destructor for each struct type

		Impl(const std::string& moduleName) {
			context = std::make_unique<llvm::LLVMContext>();
			module = std::make_unique<llvm::Module>(moduleName, *context);
			builder = std::make_unique<llvm::IRBuilder<>>(*context);
		}

		void setupRuntimeDeclarations();
		bool generateProgram(IAstNode* root);
		bool generateFunction(
				AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix = "main");
		bool generateTest(AstNodeTest* testNode, const std::string& namePrefix = "main");
		bool generateTestRunner(const std::vector<std::pair<std::string, std::string>>& testNamesWithDisplay);

		// Test mode flag and collected test names (function name, display name)
		bool testMode = false;
		std::vector<std::pair<std::string, std::string>> collectedTestNames;
		llvm::Value* testErrorAlloca = nullptr; // For tracking errors in test bodies

		// Map of iterator names to their LLVM values (for nested for loops)
		std::unordered_map<std::string, llvm::Value*> iteratorVars;

		void generateNode(IAstNode* node, llvm::Value* ctx);
		void generateInstruction(AstNodeInstruction* inst, llvm::Value* ctx);
		void generateLiteral(AstNodeLiteral* lit, llvm::Value* ctx);
		void generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx);
		void generateFor(AstNodeForStatement* forStmt, llvm::Value* ctx);
		void generateWhile(AstNodeWhileStatement* whileStmt, llvm::Value* ctx);
		void generateLoop(AstNodeLoopStatement* loopStmt, llvm::Value* ctx);
		void generateCtxBlock(AstNodeCtx* ctxNode, llvm::Value* ctx);
		void generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx);
		void generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr, llvm::Value* ctx);
		void generateAnonymousFunction(AstNodeAnonymousFunction* anonFunc, llvm::Value* ctx);
		void generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx);
		void generateSwitchStatement(AstNodeSwitchStatement* switchStmt, llvm::Value* ctx);
		void generateLocal(AstNodeLocal* local, llvm::Value* ctx);
		void generateLocalOne(const std::string& name, size_t lineNum, llvm::Value* ctx);
		void generateLocalCleanup();
		void generateCastInstructions(const std::vector<CastDirection>& casts, llvm::Value* ctx);
		void processStructDeclaration(AstNodeStructDeclaration* structDecl);
		void generateStructDestructors(); // Generate destructor functions for all struct types
		void generateStructConstruction(const std::string& structName, llvm::Value* ctx);
		void generateFieldAccess(AstNodeFieldAccess* fieldAccess, llvm::Value* ctx);
		void generateFieldSet(AstNodeFieldSet* fieldSet, llvm::Value* ctx);
		void generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral, llvm::Value* ctx);
		bool isKnownStruct(const std::string& typeName);
		size_t getTypeSize(const std::string& typeName);
		void generateStructCleanup(llvm::Value* structPtr, const std::string& structTypeName);

		// Collect all captured variables from any closures in the AST subtree
		void collectAllCapturesFromAST(IAstNode* node, std::set<std::string>& captures);

		// Inline stack operations (performance optimization)
		void generateInlinePushInt(llvm::Value* ctx, int64_t value);
		void generateInlinePushIntValue(llvm::Value* ctx, llvm::Value* value);
		void generateInlineIntAdd(llvm::Value* ctx);
		void generateInlineIntSub(llvm::Value* ctx);
		void generateInlineIntMul(llvm::Value* ctx);
		void generateInlineIntMod(llvm::Value* ctx);
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
		void generateInlineBitLshift(llvm::Value* ctx);
		void generateInlineBitRshift(llvm::Value* ctx);
		void generateTypeAwareAdd(llvm::Value* ctx);
		void generateTypeAwareSub(llvm::Value* ctx);
		void generateTypeAwareMul(llvm::Value* ctx);
		void generateTypeAwareLt(llvm::Value* ctx);
		void generateTypeAwareGt(llvm::Value* ctx);
		void generateTypeAwareEq(llvm::Value* ctx);
		void generateTypeAwareNeq(llvm::Value* ctx);
		void generateTypeAwareLte(llvm::Value* ctx);
		void generateTypeAwareGte(llvm::Value* ctx);
		void generateTypeAwareDiv(llvm::Value* ctx);
		void generateTypeAwareMod(llvm::Value* ctx);
		void generateInlineDup(llvm::Value* ctx);
		void generateInlineSwap(llvm::Value* ctx);
		void generateInlineDrop(llvm::Value* ctx);
		void generateInlineOver(llvm::Value* ctx);
		void generateInlineRot(llvm::Value* ctx);

		// Defer scope management
		void pushDeferScope();
		void popDeferScope();
		void executeDeferScope(llvm::Value* ctx);

		// Helper to find struct construction in function body
		std::string findLastStructConstruction(IAstNode* node);
	};

	void LlvmGenerator::Impl::setupRuntimeDeclarations() {
		// Context type is opaque pointer
		contextPtrTy = llvm::PointerType::getUnqual(*context);

		// exec_result is a struct with one i32 field
		execResultTy = llvm::StructType::create(*context, {builder->getInt32Ty()}, "qd_exec_result");

		// qd_stack_element_t layout: { union(i64, double, ptr, ptr), i32 type, i8 is_error_tainted }
		// Union is 8 bytes (i64/double), type is i32, bool is i8
		// For simplicity, represent union as i64 since all variants fit
		stackElementTy = llvm::StructType::create(*context,
				{
						builder->getInt64Ty(), // union value (we'll access as i64)
						builder->getInt32Ty(), // type
						builder->getInt8Ty()   // is_error_tainted (bool is 1 byte, not 1 bit)
				},
				"qd_stack_element_t");

		// qd_create_context(size_t stack_size) -> qd_context*
		auto createContextFnTy = llvm::FunctionType::get(contextPtrTy, {builder->getInt64Ty()}, false);
		createContextFn = llvm::Function::Create(
				createContextFnTy, llvm::Function::ExternalLinkage, "qd_create_context", *module);

		// qd_free_context(qd_context* ctx) -> void
		auto freeContextFnTy = llvm::FunctionType::get(builder->getVoidTy(), {contextPtrTy}, false);
		freeContextFn =
				llvm::Function::Create(freeContextFnTy, llvm::Function::ExternalLinkage, "qd_free_context", *module);

		// qd_clone_context(const qd_context* src) -> qd_context*
		auto cloneContextFnTy = llvm::FunctionType::get(contextPtrTy, {contextPtrTy}, false);
		cloneContextFn =
				llvm::Function::Create(cloneContextFnTy, llvm::Function::ExternalLinkage, "qd_clone_context", *module);

		// qd_push_i(qd_context* ctx, int64_t value) -> qd_exec_result
		auto pushIntFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, builder->getInt64Ty()}, false);
		pushIntFn = llvm::Function::Create(pushIntFnTy, llvm::Function::ExternalLinkage, "qd_push_i", *module);

		// qd_push_f(qd_context* ctx, double value) -> qd_exec_result
		auto pushFloatFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, builder->getDoubleTy()}, false);
		pushFloatFn = llvm::Function::Create(pushFloatFnTy, llvm::Function::ExternalLinkage, "qd_push_f", *module);

		// qd_push_s(qd_context* ctx, const char* value) -> qd_exec_result
		auto pushStrFnTy =
				llvm::FunctionType::get(execResultTy, {contextPtrTy, llvm::PointerType::getUnqual(*context)}, false);
		pushStrFn = llvm::Function::Create(pushStrFnTy, llvm::Function::ExternalLinkage, "qd_push_s", *module);

		// qd_push_s_ref(qd_context* ctx, qd_string_t* value) -> qd_exec_result
		auto pushStrRefFnTy =
				llvm::FunctionType::get(execResultTy, {contextPtrTy, llvm::PointerType::getUnqual(*context)}, false);
		pushStrRefFn =
				llvm::Function::Create(pushStrRefFnTy, llvm::Function::ExternalLinkage, "qd_push_s_ref", *module);

		// qd_push_p(qd_context* ctx, void* value) -> qd_exec_result
		auto pushPtrFnTy =
				llvm::FunctionType::get(execResultTy, {contextPtrTy, llvm::PointerType::getUnqual(*context)}, false);
		pushPtrFn = llvm::Function::Create(pushPtrFnTy, llvm::Function::ExternalLinkage, "qd_push_p", *module);

		// qd_call(qd_context* ctx) -> qd_exec_result
		auto callFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		callFn = llvm::Function::Create(callFnTy, llvm::Function::ExternalLinkage, "qd_call", *module);

		// qd_prints(qd_context* ctx) -> qd_exec_result
		auto printsFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		printsFn = llvm::Function::Create(printsFnTy, llvm::Function::ExternalLinkage, "qd_prints", *module);

		// qd_nl(qd_context* ctx) -> qd_exec_result
		auto nlFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		nlFn = llvm::Function::Create(nlFnTy, llvm::Function::ExternalLinkage, "qd_nl", *module);

		// qd_add/sub/mul(qd_context* ctx) -> qd_exec_result
		auto arithFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		addFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_add", *module);
		subFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_sub", *module);
		mulFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_mul", *module);

		// qd_and/or/xor/not/shl/shr(qd_context* ctx) -> qd_exec_result (bitwise operations)
		andFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_and", *module);
		orFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_or", *module);
		xorFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_xor", *module);
		notFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_not", *module);
		shlFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_shl", *module);
		shrFn = llvm::Function::Create(arithFnTy, llvm::Function::ExternalLinkage, "qd_shr", *module);

		// qd_push_call(qd_context* ctx, const char* func_name, const char* file, size_t line) -> void
		auto pushCallFnTy = llvm::FunctionType::get(builder->getVoidTy(),
				{contextPtrTy, llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context),
						builder->getInt64Ty()},
				false);
		pushCallFn = llvm::Function::Create(pushCallFnTy, llvm::Function::ExternalLinkage, "qd_push_call", *module);

		// qd_pop_call(qd_context* ctx) -> void
		auto popCallFnTy = llvm::FunctionType::get(builder->getVoidTy(), {contextPtrTy}, false);
		popCallFn = llvm::Function::Create(popCallFnTy, llvm::Function::ExternalLinkage, "qd_pop_call", *module);

		// qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name) -> void
		auto checkStackFnTy = llvm::FunctionType::get(builder->getVoidTy(),
				{contextPtrTy, builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
						llvm::PointerType::getUnqual(*context)},
				false);
		checkStackFn =
				llvm::Function::Create(checkStackFnTy, llvm::Function::ExternalLinkage, "qd_check_stack", *module);

		// For if statements, we need: qd_stack_pop and qd_stack_size
		// qd_stack_pop(qd_stack* st, qd_stack_element_t* elem) -> qd_stack_error (i32)
		auto stackPopFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
				{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, false);
		stackPopFn = llvm::Function::Create(stackPopFnTy, llvm::Function::ExternalLinkage, "qd_stack_pop", *module);

		// qd_stack_size(qd_stack* st) -> size_t
		auto stackSizeFnTy =
				llvm::FunctionType::get(builder->getInt64Ty(), {llvm::PointerType::getUnqual(*context)}, false);
		stackSizeFn = llvm::Function::Create(stackSizeFnTy, llvm::Function::ExternalLinkage, "qd_stack_size", *module);

		// strdup(const char* s) -> char*
		auto strdupFnTy = llvm::FunctionType::get(
				llvm::PointerType::getUnqual(*context), {llvm::PointerType::getUnqual(*context)}, false);
		strdupFn = llvm::Function::Create(strdupFnTy, llvm::Function::ExternalLinkage, "strdup", *module);

		// malloc(size_t size) -> void*
		auto mallocFnTy =
				llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {builder->getInt64Ty()}, false);
		mallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);

		// free(void* ptr)
		auto freeFnTy = llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		this->freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);

		// qd_struct_alloc(size_t size, destructor_fn destructor) -> void*
		// Destructor is void (*)(void*) - use opaque pointer
		auto qdStructAllocFnTy = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context),
				{builder->getInt64Ty(), llvm::PointerType::getUnqual(*context)}, false);
		this->qdStructAllocFn =
				llvm::Function::Create(qdStructAllocFnTy, llvm::Function::ExternalLinkage, "qd_struct_alloc", *module);

		// qd_struct_release(void* ptr)
		auto qdStructReleaseFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		this->qdStructReleaseFn = llvm::Function::Create(
				qdStructReleaseFnTy, llvm::Function::ExternalLinkage, "qd_struct_release", *module);

		// qd_struct_retain(void* ptr) -> void*
		auto qdStructRetainFnTy = llvm::FunctionType::get(
				llvm::PointerType::getUnqual(*context), {llvm::PointerType::getUnqual(*context)}, false);
		this->qdStructRetainFn = llvm::Function::Create(
				qdStructRetainFnTy, llvm::Function::ExternalLinkage, "qd_struct_retain", *module);

		// qd_ptr_release(void* ptr) - generic release for arrays and structs
		auto qdPtrReleaseFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		this->qdPtrReleaseFn =
				llvm::Function::Create(qdPtrReleaseFnTy, llvm::Function::ExternalLinkage, "qd_ptr_release", *module);

		// qd_ptr_retain(void* ptr) -> void* - generic retain for arrays and structs
		auto qdPtrRetainFnTy = llvm::FunctionType::get(
				llvm::PointerType::getUnqual(*context), {llvm::PointerType::getUnqual(*context)}, false);
		this->qdPtrRetainFn =
				llvm::Function::Create(qdPtrRetainFnTy, llvm::Function::ExternalLinkage, "qd_ptr_retain", *module);

		// Initialize debug info if enabled
		if (debugInfoEnabled) {
			debugBuilder = std::make_unique<llvm::DIBuilder>(*module);

			// Extract directory and filename from source path
			// Convert to absolute path for better debugger compatibility
			std::filesystem::path srcPath(sourceFileName);
			std::filesystem::path absPath = std::filesystem::absolute(srcPath);
			std::string directory = absPath.parent_path().string();
			std::string filename = absPath.filename().string();
			if (directory.empty()) {
				directory = ".";
			}

			// Create debug file with absolute path
			debugFile = debugBuilder->createFile(filename, directory);

			// Create compile unit
			compileUnit = debugBuilder->createCompileUnit(
					llvm::dwarf::DW_LANG_C99, // Use C99 as the base language (closest to stack machine)
					debugFile,
					"quadc", // Producer
					false,	 // isOptimized (set to false for debugging)
					"",		 // Flags
					0		 // Runtime version
			);

			// Set module flags for debug info
			module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
			module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);

			// Initialize scope stack with compile unit
			debugScopeStack.push_back(compileUnit);

			// Create DIFile objects for all modules
			for (const auto& pair : moduleSourceFiles) {
				const std::string& moduleName = pair.first;
				const std::string& moduleSourcePath = pair.second;

				std::filesystem::path moduleSrcPath(moduleSourcePath);
				std::filesystem::path moduleAbsPath = std::filesystem::absolute(moduleSrcPath);
				std::string moduleDirectory = moduleAbsPath.parent_path().string();
				std::string moduleFilename = moduleAbsPath.filename().string();
				if (moduleDirectory.empty()) {
					moduleDirectory = ".";
				}

				llvm::DIFile* moduleDIFile = debugBuilder->createFile(moduleFilename, moduleDirectory);
				moduleDebugFiles[moduleName] = moduleDIFile;
			}

			// Add main module to debug files map
			moduleDebugFiles["main"] = debugFile;

			// Create debug type info for runtime structures so GDB can inspect them
			// This is needed for JIT-compiled code where GDB can't access libqdrt's debug symbols

			// Basic types
			auto int64Type = debugBuilder->createBasicType("int64_t", 64, llvm::dwarf::DW_ATE_signed);
			auto doubleType = debugBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
			auto boolType = debugBuilder->createBasicType("bool", 8, llvm::dwarf::DW_ATE_boolean);
			auto charType = debugBuilder->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);
			auto sizeType = debugBuilder->createBasicType("size_t", 64, llvm::dwarf::DW_ATE_unsigned);
			auto charPtrType = debugBuilder->createPointerType(charType, 64);
			auto voidPtrType = debugBuilder->createPointerType(nullptr, 64);

			// qd_stack_type enum (just use int32 for simplicity)
			auto stackTypeEnum = debugBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);

			// qd_stack_value union (inside qd_stack_element_t)
			llvm::SmallVector<llvm::Metadata*, 4> unionFields;
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "i", debugFile, 54, 64, 64, 0, llvm::DINode::FlagZero, int64Type));
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "f", debugFile, 55, 64, 64, 0, llvm::DINode::FlagZero, doubleType));
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "p", debugFile, 56, 64, 64, 0, llvm::DINode::FlagZero, voidPtrType));
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "s", debugFile, 57, 64, 64, 0, llvm::DINode::FlagZero, charPtrType));
			auto valueUnionType = debugBuilder->createUnionType(compileUnit, "qd_stack_value", debugFile, 53, 64, 64,
					llvm::DINode::FlagZero, debugBuilder->getOrCreateArray(unionFields));

			// qd_stack_element_t structure
			llvm::SmallVector<llvm::Metadata*, 3> elementFields;
			elementFields.push_back(debugBuilder->createMemberType(
					compileUnit, "value", debugFile, 58, 64, 64, 0, llvm::DINode::FlagZero, valueUnionType));
			elementFields.push_back(debugBuilder->createMemberType(
					compileUnit, "type", debugFile, 60, 32, 32, 64, llvm::DINode::FlagZero, stackTypeEnum));
			elementFields.push_back(debugBuilder->createMemberType(
					compileUnit, "is_error_tainted", debugFile, 61, 8, 8, 96, llvm::DINode::FlagZero, boolType));
			auto elementType = debugBuilder->createStructType(compileUnit, "qd_stack_element_t", debugFile, 52, 128, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(elementFields));
			stackElementDebugType = elementType; // Store for later use
			auto elementPtrType = debugBuilder->createPointerType(elementType, 64);

			// qd_stack structure
			llvm::SmallVector<llvm::Metadata*, 3> stackFields;
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "data", debugFile, 71, 64, 64, 0, llvm::DINode::FlagZero, elementPtrType));
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "capacity", debugFile, 72, 64, 64, 64, llvm::DINode::FlagZero, sizeType));
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "size", debugFile, 73, 64, 64, 128, llvm::DINode::FlagZero, sizeType));
			auto stackStructType = debugBuilder->createStructType(compileUnit, "qd_stack", debugFile, 70, 192, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(stackFields));
			auto stackPtrType = debugBuilder->createPointerType(stackStructType, 64);

			// qd_context structure
			llvm::SmallVector<llvm::Metadata*, 8> contextFields;
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "st", debugFile, 45, 64, 64, 0, llvm::DINode::FlagZero, stackPtrType));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "error_code", debugFile, 46, 64, 64, 64, llvm::DINode::FlagZero, int64Type));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "error_msg", debugFile, 47, 64, 64, 128, llvm::DINode::FlagZero, charPtrType));
			contextFields.push_back(debugBuilder->createMemberType(compileUnit, "argc", debugFile, 48, 32, 32, 192,
					llvm::DINode::FlagZero, debugBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed)));
			auto charPtrPtrType = debugBuilder->createPointerType(charPtrType, 64);
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "argv", debugFile, 49, 64, 64, 256, llvm::DINode::FlagZero, charPtrPtrType));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "program_name", debugFile, 50, 64, 64, 320, llvm::DINode::FlagZero, charPtrType));
			// call_stack is const char* [256] at offset 48 bytes (384 bits)
			auto callStackArrayType = debugBuilder->createArrayType(16384, // 256 * 64 bits
					64,													   // alignment
					charPtrType, debugBuilder->getOrCreateArray({debugBuilder->getOrCreateSubrange(0, 256)}));
			contextFields.push_back(debugBuilder->createMemberType(compileUnit, "call_stack", debugFile, 53, 16384, 64,
					384, llvm::DINode::FlagZero, callStackArrayType));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "call_stack_depth", debugFile, 54, 64, 64, 16768, llvm::DINode::FlagZero, sizeType));

			auto contextStructType = debugBuilder->createStructType(compileUnit, "qd_context", debugFile, 44, 16832, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(contextFields));
			contextDebugType = debugBuilder->createPointerType(contextStructType, 64);
		}
	}

	void LlvmGenerator::Impl::generateInlinePushInt(llvm::Value* ctx, int64_t value) {
		// Inline implementation of qd_push_i to eliminate function call overhead
		// This directly manipulates the stack structure:
		// 1. Get ctx->st (qd_stack* at offset 0 in qd_context)
		// 2. Get st->size, st->capacity, st->data
		// 3. Check for overflow (size >= capacity)
		// 4. Calculate &data[size]
		// 5. Store value, type, is_error_tainted
		// 6. Increment size

		// Define qd_context structure: { qd_stack* st, ... }
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);

		// Get ctx->st (field 0 of qd_context)
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		// Define qd_stack structure: { qd_stack_element_t* data, size_t capacity, size_t size }
		llvm::Type* stackTy = llvm::StructType::get(*context,
				{
						llvm::PointerType::get(*context, 0), // data (opaque pointer)
						builder->getInt64Ty(),				 // capacity
						builder->getInt64Ty()				 // size
				},
				false);

		// Get st->size (field 2)
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Get st->capacity (field 1) and check for overflow
		llvm::Value* capacityPtr = builder->CreateStructGEP(stackTy, st, 1, "capacity_ptr");
		llvm::Value* capacity = builder->CreateLoad(builder->getInt64Ty(), capacityPtr, "capacity");
		llvm::Value* hasSpace = builder->CreateICmpULT(size, capacity, "has_space");

		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* overflowBB = llvm::BasicBlock::Create(*context, "push.overflow", currentFn);
		llvm::BasicBlock* pushBB = llvm::BasicBlock::Create(*context, "push.do", currentFn);
		builder->CreateCondBr(hasSpace, pushBB, overflowBB);

		// Generate overflow error
		builder->SetInsertPoint(overflowBB);
		auto fprintfFn = module->getOrInsertFunction("fprintf",
				llvm::FunctionType::get(builder->getInt32Ty(),
						{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg = builder->CreateGlobalString("Fatal error: Stack overflow (use -s to increase stack size)\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Do the push
		builder->SetInsertPoint(pushBB);

		// Get st->data (field 0)
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Calculate &data[size]
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, size, "elem_ptr");

		// Set element value: elem->value.i = value (field 0)
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valueiPtr = builder->CreateBitCast(valuePtr, llvm::PointerType::get(*context, 0));
		builder->CreateStore(builder->getInt64(static_cast<uint64_t>(value)), valueiPtr);

		// Set element type: elem->type = QD_STACK_TYPE_INT (0) (field 1)
		llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 1, "type_ptr");
		builder->CreateStore(builder->getInt32(0), typePtr);

		// Set element is_error_tainted: elem->is_error_tainted = false (field 2)
		llvm::Value* taintedPtr = builder->CreateStructGEP(stackElementTy, elemPtr, 2, "tainted_ptr");
		builder->CreateStore(builder->getInt1(false), taintedPtr);

		// Increment size: st->size++
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlinePushIntValue(llvm::Value* ctx, llvm::Value* value) {
		// Inline implementation of qd_push_i for runtime integer values
		// Same as generateInlinePushInt but takes llvm::Value* instead of int64_t

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(*context,
				{
						llvm::PointerType::get(*context, 0), // data
						builder->getInt64Ty(),				 // capacity
						builder->getInt64Ty()				 // size
				},
				false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Check for overflow
		llvm::Value* capacityPtr = builder->CreateStructGEP(stackTy, st, 1, "capacity_ptr");
		llvm::Value* capacity = builder->CreateLoad(builder->getInt64Ty(), capacityPtr, "capacity");
		llvm::Value* hasSpace = builder->CreateICmpULT(size, capacity, "has_space");

		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* overflowBB = llvm::BasicBlock::Create(*context, "pushv.overflow", currentFn);
		llvm::BasicBlock* pushBB = llvm::BasicBlock::Create(*context, "pushv.do", currentFn);
		builder->CreateCondBr(hasSpace, pushBB, overflowBB);

		// Generate overflow error
		builder->SetInsertPoint(overflowBB);
		auto fprintfFn = module->getOrInsertFunction("fprintf",
				llvm::FunctionType::get(builder->getInt32Ty(),
						{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg = builder->CreateGlobalString("Fatal error: Stack overflow (use -s to increase stack size)\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Do the push
		builder->SetInsertPoint(pushBB);

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, size, "elem_ptr");

		// Store runtime value
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valueiPtr = builder->CreateBitCast(valuePtr, llvm::PointerType::get(*context, 0));
		builder->CreateStore(value, valueiPtr);

		// Set type to integer
		llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 1, "type_ptr");
		builder->CreateStore(builder->getInt32(0), typePtr);

		// Set is_error_tainted to false
		llvm::Value* taintedPtr = builder->CreateStructGEP(stackElementTy, elemPtr, 2, "tainted_ptr");
		builder->CreateStore(builder->getInt1(false), taintedPtr);

		// Increment size
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntAdd(llvm::Value* ctx) {
		// Inline implementation of integer add: ( a:int b:int -- result:int )
		// Pops two integers from stack, adds them, pushes result
		// Assumes both operands are integers (no type checking for performance)

		// Get ctx->st
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		// Get stack structure
		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		// Get st->size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Get st->data
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load first operand: data[size - 2]
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		// Load second operand: data[size - 1]
		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform addition (nsw = no signed wrap, enables better optimization)
		llvm::Value* result = builder->CreateNSWAdd(value1, value2, "add_result");

		// Store result at data[size - 2]
		builder->CreateStore(result, value1iPtrCast);

		// Update size: size - 1 (net effect: pop 2, push 1)
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntSub(llvm::Value* ctx) {
		// Inline implementation of integer subtract: ( a:int b:int -- result:int )
		// Same as add, but with subtraction

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform subtraction (nsw = no signed wrap, enables better optimization)
		llvm::Value* result = builder->CreateNSWSub(value1, value2, "sub_result");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntMul(llvm::Value* ctx) {
		// Inline implementation of integer multiply: ( a:int b:int -- result:int )
		// Same pattern as add/sub, but with multiplication

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform multiplication (nsw = no signed wrap, enables better optimization)
		llvm::Value* result = builder->CreateNSWMul(value1, value2, "mul_result");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntMod(llvm::Value* ctx) {
		// Inline implementation of integer modulo: ( a:int b:int -- result:int )
		// Assumes both operands are integers (no type checking for performance)

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform modulo (signed remainder)
		llvm::Value* result = builder->CreateSRem(value1, value2, "mod_result");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntLt(llvm::Value* ctx) {
		// Inline implementation of integer less than: ( a:int b:int -- result:int )
		// Assumes both operands are integers (no type checking for performance)

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform comparison: value1 < value2
		llvm::Value* cmpResult = builder->CreateICmpSLT(value1, value2, "lt_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntGt(llvm::Value* ctx) {
		// Inline implementation of integer greater than: ( a:int b:int -- result:int )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform comparison: value1 > value2
		llvm::Value* cmpResult = builder->CreateICmpSGT(value1, value2, "gt_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntEq(llvm::Value* ctx) {
		// Inline implementation of integer equality: ( a:int b:int -- result:int )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform comparison: value1 == value2
		llvm::Value* cmpResult = builder->CreateICmpEQ(value1, value2, "eq_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntNeq(llvm::Value* ctx) {
		// Inline implementation of integer not-equal: ( a:int b:int -- result:int )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform comparison: value1 != value2
		llvm::Value* cmpResult = builder->CreateICmpNE(value1, value2, "neq_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntLte(llvm::Value* ctx) {
		// Inline implementation of integer less than or equal: ( a:int b:int -- result:int )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform comparison: value1 <= value2
		llvm::Value* cmpResult = builder->CreateICmpSLE(value1, value2, "lte_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntGte(llvm::Value* ctx) {
		// Inline implementation of integer greater than or equal: ( a:int b:int -- result:int )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform comparison: value1 >= value2
		llvm::Value* cmpResult = builder->CreateICmpSGE(value1, value2, "gte_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitAnd(llvm::Value* ctx) {
		// Inline implementation of bitwise AND: ( a:int b:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateAnd(value1, value2, "and_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitOr(llvm::Value* ctx) {
		// Inline implementation of bitwise OR: ( a:int b:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateOr(value1, value2, "or_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitXor(llvm::Value* ctx) {
		// Inline implementation of bitwise XOR: ( a:int b:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateXor(value1, value2, "xor_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitNot(llvm::Value* ctx) {
		// Inline implementation of bitwise NOT: ( a:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx = builder->CreateSub(size, builder->getInt64(1), "idx");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, idx, "elem_ptr");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valuePtrCast = builder->CreateBitCast(valuePtr, llvm::PointerType::get(*context, 0));
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), valuePtrCast, "value");

		llvm::Value* result = builder->CreateNot(value, "not_result");
		builder->CreateStore(result, valuePtrCast);
		// Stack size unchanged for unary operation
	}

	void LlvmGenerator::Impl::generateInlineBitLshift(llvm::Value* ctx) {
		// Inline implementation of left shift: ( value:int shift:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load value (first operand at size-2)
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value");

		// Load shift amount (second operand at size-1)
		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* shift = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "shift");

		llvm::Value* result = builder->CreateShl(value, shift, "lshift_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitRshift(llvm::Value* ctx) {
		// Inline implementation of logical right shift: ( value:int shift:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load value (first operand at size-2)
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value");

		// Load shift amount (second operand at size-1)
		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* shift = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "shift");

		// Use logical shift right (LShr) for unsigned behavior
		llvm::Value* result = builder->CreateLShr(value, shift, "rshift_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateTypeAwareAdd(llvm::Value* ctx) {
		// Type-aware inline add: checks types at runtime and uses fast path for integers
		// If both operands are integers: inline add
		// Otherwise: call qd_add() which handles all type combinations

		// Get ctx->st
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		// Get stack structure
		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		// Get st->size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Get st->data
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointers to top two elements
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		// Load types of both elements (field 1 of qd_stack_element_t)
		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		// Check if both are integers (QD_STACK_TYPE_INT = 0)
		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		// Create basic blocks for fast path (inline) and slow path (runtime call)
		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_add", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_add", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "add_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		// Fast path: inline integer add
		builder->SetInsertPoint(fastPath);
		{
			// Load integer values (field 0, accessed as i64)
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform addition (nsw = no signed wrap)
			llvm::Value* result = builder->CreateNSWAdd(value1, value2, "add_result");

			// Store result at elem1 (reuse first position)
			builder->CreateStore(result, value1iPtrCast);

			// Decrement size by 1 (net: pop 2, push 1)
			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		// Slow path: call qd_add() runtime function
		builder->SetInsertPoint(slowPath);
		{
			// addFn is now a member variable, initialized in setupRuntimeDeclarations
			builder->CreateCall(addFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareSub(llvm::Value* ctx) {
		// Similar to generateTypeAwareAdd, but with subtraction

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_sub", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_sub", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "sub_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* result = builder->CreateNSWSub(value1, value2, "sub_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			// subFn is now a member variable, initialized in setupRuntimeDeclarations
			builder->CreateCall(subFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareMul(llvm::Value* ctx) {
		// Similar to generateTypeAwareAdd, but with multiplication

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_mul", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_mul", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "mul_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* result = builder->CreateNSWMul(value1, value2, "mul_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			// mulFn is now a member variable, initialized in setupRuntimeDeclarations
			builder->CreateCall(mulFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareLt(llvm::Value* ctx) {
		// Type-aware less than: checks types and uses fast path for integers

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_lt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_lt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "lt_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform comparison: value1 < value2
			llvm::Value* cmpResult = builder->CreateICmpSLT(value1, value2, "lt_result");
			// Convert bool to i64 (0 or 1)
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* ltFn = module->getFunction("qd_lt");
			if (!ltFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				ltFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_lt", *module);
			}
			builder->CreateCall(ltFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareGt(llvm::Value* ctx) {
		// Type-aware greater than: similar to lt

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_gt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_gt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "gt_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform comparison: value1 > value2
			llvm::Value* cmpResult = builder->CreateICmpSGT(value1, value2, "gt_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* gtFn = module->getFunction("qd_gt");
			if (!gtFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				gtFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_gt", *module);
			}
			builder->CreateCall(gtFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareEq(llvm::Value* ctx) {
		// Type-aware equality: checks types and uses fast path for integers

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_eq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_eq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "eq_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform comparison: value1 == value2
			llvm::Value* cmpResult = builder->CreateICmpEQ(value1, value2, "eq_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* eqFn = module->getFunction("qd_eq");
			if (!eqFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				eqFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_eq", *module);
			}
			builder->CreateCall(eqFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateInlineDup(llvm::Value* ctx) {
		// Inline dup: duplicate top stack element
		// Simple operation, no type checking needed

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointer to top element (size - 1)
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");

		// Get pointer to new element (size)
		llvm::Value* newElemPtr = builder->CreateGEP(stackElementTy, data, size, "new_elem");

		// Copy entire element (value union, type, is_error_tainted)
		// Element size is 16 bytes (8 for union, 4 for type, 1 for bool + padding)
		llvm::Value* topValue = builder->CreateLoad(stackElementTy, topElemPtr, "top_value");
		builder->CreateStore(topValue, newElemPtr);

		// Increment size
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineSwap(llvm::Value* ctx) {
		// Inline swap: swap top two stack elements
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Check for stack underflow (need at least 2 elements)
		llvm::Value* hasEnough = builder->CreateICmpUGE(size, builder->getInt64(2), "has_enough");
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "swap.underflow", currentFn);
		llvm::BasicBlock* swapBB = llvm::BasicBlock::Create(*context, "swap.do", currentFn);
		builder->CreateCondBr(hasEnough, swapBB, underflowBB);

		// Generate underflow error
		builder->SetInsertPoint(underflowBB);
		auto fprintfFn = module->getOrInsertFunction("fprintf",
				llvm::FunctionType::get(builder->getInt32Ty(),
						{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg = builder->CreateGlobalString("Fatal error in swap: Stack underflow (requires 2 elements)\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Do the swap
		builder->SetInsertPoint(swapBB);
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointers to top two elements
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		// Load both elements
		llvm::Value* elem1 = builder->CreateLoad(stackElementTy, elem1Ptr, "elem1");
		llvm::Value* elem2 = builder->CreateLoad(stackElementTy, elem2Ptr, "elem2");

		// Store them swapped
		builder->CreateStore(elem2, elem1Ptr);
		builder->CreateStore(elem1, elem2Ptr);
	}

	void LlvmGenerator::Impl::generateInlineDrop(llvm::Value* ctx) {
		// Inline drop: remove top stack element

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Simply decrement size
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateTypeAwareNeq(llvm::Value* ctx) {
		// Type-aware not-equal: similar to eq but with !=

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_neq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_neq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "neq_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* cmpResult = builder->CreateICmpNE(value1, value2, "neq_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* neqFn = module->getFunction("qd_neq");
			if (!neqFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				neqFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_neq", *module);
			}
			builder->CreateCall(neqFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareLte(llvm::Value* ctx) {
		// Type-aware less than or equal

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_lte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_lte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "lte_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* cmpResult = builder->CreateICmpSLE(value1, value2, "lte_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* lteFn = module->getFunction("qd_lte");
			if (!lteFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				lteFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_lte", *module);
			}
			builder->CreateCall(lteFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareGte(llvm::Value* ctx) {
		// Type-aware greater than or equal

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_gte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_gte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "gte_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* cmpResult = builder->CreateICmpSGE(value1, value2, "gte_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* gteFn = module->getFunction("qd_gte");
			if (!gteFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				gteFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_gte", *module);
			}
			builder->CreateCall(gteFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareDiv(llvm::Value* ctx) {
		// Type-aware division

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_div", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_div", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "div_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Check for division by zero
			llvm::Value* isZero = builder->CreateICmpEQ(value2, builder->getInt64(0), "divisor_is_zero");
			llvm::BasicBlock* divOkBlock =
					llvm::BasicBlock::Create(*context, "div_ok", builder->GetInsertBlock()->getParent());

			// If zero, jump to slow path (which has error handling); otherwise proceed
			builder->CreateCondBr(isZero, slowPath, divOkBlock);

			// Continue division in divOkBlock
			builder->SetInsertPoint(divOkBlock);

			llvm::Value* result = builder->CreateSDiv(value1, value2, "div_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* divFn = module->getFunction("qd_div");
			if (!divFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				divFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_div", *module);
			}
			builder->CreateCall(divFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareMod(llvm::Value* ctx) {
		// Type-aware modulo

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_mod", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_mod", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "mod_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Check for division by zero
			llvm::Value* isZero = builder->CreateICmpEQ(value2, builder->getInt64(0), "divisor_is_zero");
			llvm::BasicBlock* modOkBlock =
					llvm::BasicBlock::Create(*context, "mod_ok", builder->GetInsertBlock()->getParent());

			// If zero, jump to slow path (which has error handling); otherwise proceed
			builder->CreateCondBr(isZero, slowPath, modOkBlock);

			// Continue modulo in modOkBlock
			builder->SetInsertPoint(modOkBlock);

			llvm::Value* result = builder->CreateSRem(value1, value2, "mod_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* modFn = module->getFunction("qd_mod");
			if (!modFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				modFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_mod", *module);
			}
			builder->CreateCall(modFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateInlineOver(llvm::Value* ctx) {
		// Inline over: copy second element to top ( a b -- a b a )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get second from top (size - 2)
		llvm::Value* secondIdx = builder->CreateSub(size, builder->getInt64(2), "second_idx");
		llvm::Value* secondElemPtr = builder->CreateGEP(stackElementTy, data, secondIdx, "second_elem");

		// Get new top position (size)
		llvm::Value* newElemPtr = builder->CreateGEP(stackElementTy, data, size, "new_elem");

		// Copy second element to new top
		llvm::Value* secondValue = builder->CreateLoad(stackElementTy, secondElemPtr, "second_value");
		builder->CreateStore(secondValue, newElemPtr);

		// Increment size
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineRot(llvm::Value* ctx) {
		// Inline rot: rotate top three elements ( a b c -- b c a )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointers to top three elements
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(3), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(2), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* idx3 = builder->CreateSub(size, builder->getInt64(1), "idx3");
		llvm::Value* elem3Ptr = builder->CreateGEP(stackElementTy, data, idx3, "elem3_ptr");

		// Load all three
		llvm::Value* elem1 = builder->CreateLoad(stackElementTy, elem1Ptr, "elem1");
		llvm::Value* elem2 = builder->CreateLoad(stackElementTy, elem2Ptr, "elem2");
		llvm::Value* elem3 = builder->CreateLoad(stackElementTy, elem3Ptr, "elem3");

		// Rotate: a b c -> b c a
		builder->CreateStore(elem2, elem1Ptr);
		builder->CreateStore(elem3, elem2Ptr);
		builder->CreateStore(elem1, elem3Ptr);
	}

	void LlvmGenerator::Impl::generateLiteral(AstNodeLiteral* lit, llvm::Value* ctx) {
		auto type = lit->literalType();
		const auto& value = lit->value();

		switch (type) {
		case AstNodeLiteral::LiteralType::INTEGER: {
			int64_t val = 0;
			if (!safeParseInt64(value, val)) {
				std::cerr << "quadc: error: Invalid integer literal '" << value << "' (out of range or invalid format)"
						  << std::endl;
				compilationFailed = true;
			}
			// Use function calls when debug info is enabled for better debuggability
			// Use inline code for release builds for better performance
			if (debugInfoEnabled) {
				builder->CreateCall(pushIntFn, {ctx, builder->getInt64(static_cast<uint64_t>(val))});
			} else {
				generateInlinePushInt(ctx, val);
			}
			break;
		}
		case AstNodeLiteral::LiteralType::FLOAT: {
			auto val = llvm::ConstantFP::get(builder->getDoubleTy(), std::stod(value));
			builder->CreateCall(pushFloatFn, {ctx, val});
			break;
		}
		case AstNodeLiteral::LiteralType::STRING: {
			// Extract string content (remove surrounding quotes)
			std::string content = value;
			if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
				content = value.substr(1, value.size() - 2);
			}

			// Process escape sequences
			std::string processed;
			for (size_t i = 0; i < content.size(); i++) {
				if (content[i] == '\\' && i + 1 < content.size()) {
					switch (content[i + 1]) {
					case 'n':
						processed += '\n';
						i++;
						break;
					case 't':
						processed += '\t';
						i++;
						break;
					case 'r':
						processed += '\r';
						i++;
						break;
					case '\\':
						processed += '\\';
						i++;
						break;
					case '"':
						processed += '"';
						i++;
						break;
					case '0':
						processed += '\0';
						i++;
						break;
					case 'e':
						// ANSI escape character (ESC = 0x1B)
						processed += '\x1b';
						i++;
						break;
					case 'x':
						// Hex escape sequence: \xNN
						if (i + 3 < content.size() && isxdigit(static_cast<unsigned char>(content[i + 2])) &&
								isxdigit(static_cast<unsigned char>(content[i + 3]))) {
							// Valid hex digits
							int val = 0;
							char c1 = content[i + 2];
							char c2 = content[i + 3];
							val = (isdigit(c1) ? c1 - '0' : tolower(c1) - 'a' + 10) * 16 +
								  (isdigit(c2) ? c2 - '0' : tolower(c2) - 'a' + 10);
							processed += static_cast<char>(val);
							i += 3; // Skip \xNN
						} else {
							// Invalid hex sequence, keep as-is
							processed += content[i];
						}
						break;
					default:
						processed += content[i];
						break;
					}
				} else {
					processed += content[i];
				}
			}

			auto strValue = builder->CreateGlobalString(processed, ".str");
			builder->CreateCall(pushStrFn, {ctx, strValue});
			break;
		}
		}
	}

	void LlvmGenerator::Impl::generateInstruction(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& name = inst->name();

		// Handle generic make<T> instruction
		if (name == "make" && inst->hasTypeParam()) {
			const std::string& typeParam = inst->typeParam();
			std::string fnName;
			if (typeParam == "i64" || typeParam == "i32" || typeParam == "i16" || typeParam == "i8" ||
					typeParam == "u64" || typeParam == "u32" || typeParam == "u16" || typeParam == "u8") {
				fnName = "qd_makei";
			} else if (typeParam == "f64" || typeParam == "f32") {
				fnName = "qd_makef";
			} else if (typeParam == "str" || typeParam == "string") {
				fnName = "qd_makes";
			} else {
				// Assume it's a struct type - use makep for pointer
				fnName = "qd_makep";
			}

			llvm::Function* makeFn = module->getFunction(fnName);
			if (!makeFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				makeFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
			}
			builder->CreateCall(makeFn, {ctx});
			lastPushedWasArray = true;
			return;
		}

		// Handle generic cast<T> instruction
		if (name == "cast" && inst->hasTypeParam()) {
			const std::string& typeParam = inst->typeParam();
			std::string fnName;
			if (typeParam == "i64" || typeParam == "i32" || typeParam == "i16" || typeParam == "i8" ||
					typeParam == "u64" || typeParam == "u32" || typeParam == "u16" || typeParam == "u8") {
				fnName = "qd_casti";
			} else if (typeParam == "f64" || typeParam == "f32") {
				fnName = "qd_castf";
			} else if (typeParam == "str" || typeParam == "string") {
				fnName = "qd_casts";
			} else if (typeParam == "ptr") {
				fnName = "qd_castp";
			} else {
				// Unknown type - default to casts for string representation
				fnName = "qd_casts";
			}

			llvm::Function* castFn = module->getFunction(fnName);
			if (!castFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				castFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
			}
			builder->CreateCall(castFn, {ctx});
			return;
		}

		if (name == "prints") {
			builder->CreateCall(printsFn, {ctx});
		} else if (name == "nl") {
			builder->CreateCall(nlFn, {ctx});
		} else if (name == "+" || name == "add") {
			// When debug info is enabled, use simple function calls for better debuggability
			if (debugInfoEnabled) {
				builder->CreateCall(addFn, {ctx});
			} else if (currentFunctionIsIntegerOnly) {
				// Use pure integer ops in integer-only functions (no type checking)
				generateInlineIntAdd(ctx);
			} else {
				// Use type-aware inline add (fast path for integers, runtime call for floats)
				generateTypeAwareAdd(ctx);
			}
			return;
		} else if (name == "-" || name == "sub") {
			// When debug info is enabled, use simple function calls for better debuggability
			if (debugInfoEnabled) {
				builder->CreateCall(subFn, {ctx});
			} else if (currentFunctionIsIntegerOnly) {
				// Use pure integer ops in integer-only functions
				generateInlineIntSub(ctx);
			} else {
				// Use type-aware inline subtract
				generateTypeAwareSub(ctx);
			}
			return;
		} else if (name == "*" || name == "mul") {
			// When debug info is enabled, use simple function calls for better debuggability
			if (debugInfoEnabled) {
				builder->CreateCall(mulFn, {ctx});
			} else if (currentFunctionIsIntegerOnly) {
				// Use pure integer ops in integer-only functions
				generateInlineIntMul(ctx);
			} else {
				// Use type-aware inline multiply
				generateTypeAwareMul(ctx);
			}
			return;
		} else if (name == "<") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntLt(ctx);
			} else {
				generateTypeAwareLt(ctx);
			}
			return;
		} else if (name == ">") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntGt(ctx);
			} else {
				generateTypeAwareGt(ctx);
			}
			return;
		} else if (name == "==") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntEq(ctx);
			} else {
				generateTypeAwareEq(ctx);
			}
			return;
		} else if (name == "swap") {
			// Use inline swap (no string cleanup needed - just moving elements)
			generateInlineSwap(ctx);
			return;
		} else if (name == "dup") {
			// Only use inline dup for integer-only functions
			// Strings require reference counting which inline dup doesn't handle
			if (currentFunctionIsIntegerOnly) {
				generateInlineDup(ctx);
			} else {
				// Call runtime qd_dup which handles string reference counting
				llvm::Function* qdDupFn = module->getFunction("qd_dup");
				if (!qdDupFn) {
					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					qdDupFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_dup", *module);
				}
				builder->CreateCall(qdDupFn, {ctx});
			}
			return;
		} else if (name == "!=") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntNeq(ctx);
			} else {
				generateTypeAwareNeq(ctx);
			}
			return;
		} else if (name == "<=") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntLte(ctx);
			} else {
				generateTypeAwareLte(ctx);
			}
			return;
		} else if (name == ">=" || name == "gte") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntGte(ctx);
			} else {
				generateTypeAwareGte(ctx);
			}
			return;
		} else if (name == "lt") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntLt(ctx);
			} else {
				generateTypeAwareLt(ctx);
			}
			return;
		} else if (name == "gt") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntGt(ctx);
			} else {
				generateTypeAwareGt(ctx);
			}
			return;
		} else if (name == "eq") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntEq(ctx);
			} else {
				generateTypeAwareEq(ctx);
			}
			return;
		} else if (name == "neq") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntNeq(ctx);
			} else {
				generateTypeAwareNeq(ctx);
			}
			return;
		} else if (name == "lte") {
			// Use integer-only path for pure integer functions
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntLte(ctx);
			} else {
				generateTypeAwareLte(ctx);
			}
			return;
		} else if (name == "/" || name == "div") {
			// Use type-aware inline division
			generateTypeAwareDiv(ctx);
			return;
		} else if (name == "%" || name == "mod") {
			// For integer-only functions, use simpler inline mod
			if (currentFunctionIsIntegerOnly) {
				generateInlineIntMod(ctx);
			} else {
				// Use type-aware inline modulo
				generateTypeAwareMod(ctx);
			}
			return;
		} else if (name == "and") {
			// Use inline bitwise AND for integer-only functions, runtime call otherwise
			if (currentFunctionIsIntegerOnly) {
				generateInlineBitAnd(ctx);
			} else {
				builder->CreateCall(andFn, {ctx});
			}
			return;
		} else if (name == "or") {
			if (currentFunctionIsIntegerOnly) {
				generateInlineBitOr(ctx);
			} else {
				builder->CreateCall(orFn, {ctx});
			}
			return;
		} else if (name == "xor") {
			if (currentFunctionIsIntegerOnly) {
				generateInlineBitXor(ctx);
			} else {
				builder->CreateCall(xorFn, {ctx});
			}
			return;
		} else if (name == "not") {
			if (currentFunctionIsIntegerOnly) {
				generateInlineBitNot(ctx);
			} else {
				builder->CreateCall(notFn, {ctx});
			}
			return;
		} else if (name == "shl") {
			if (currentFunctionIsIntegerOnly) {
				generateInlineBitLshift(ctx);
			} else {
				builder->CreateCall(shlFn, {ctx});
			}
			return;
		} else if (name == "shr") {
			if (currentFunctionIsIntegerOnly) {
				generateInlineBitRshift(ctx);
			} else {
				builder->CreateCall(shrFn, {ctx});
			}
			return;
		} else if (name == "free") {
			// Use qd_free for raw memory (mem::alloc, mem::from_string, etc.)
			// Structs are released automatically via local cleanup code
			lastIdentifierPushed.clear();

			llvm::Function* qdFreeFn = module->getFunction("qd_free");
			if (!qdFreeFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				qdFreeFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_free", *module);
			}
			builder->CreateCall(qdFreeFn, {ctx});
			return;
		} else {
			// Map instruction name to runtime function name
			std::string fnName;
			if (name == "/") {
				fnName = "qd_div";
			} else if (name == "%") {
				fnName = "qd_mod";
			} else if (name == ">=") {
				fnName = "qd_gte";
			} else if (name == "<=") {
				fnName = "qd_lte";
			} else if (name == "!=") {
				fnName = "qd_neq";
			} else if (name == "++") {
				fnName = "qd_inc";
			} else if (name == "--") {
				fnName = "qd_dec";
			} else if (name == "<<") {
				fnName = "qd_shl";
			} else if (name == ">>") {
				fnName = "qd_shr";
			} else {
				fnName = "qd_" + name;
			}

			// Check if function already exists
			llvm::Function* runtimeFn = module->getFunction(fnName);
			if (!runtimeFn) {
				// Declare it: qd_exec_result fn(qd_context*)
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				runtimeFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
			}

			builder->CreateCall(runtimeFn, {ctx});

			// Special handling for 'panic' instruction in fallible functions
			// After calling qd_panic, we need to return immediately to prevent further execution
			if (name == "panic" && currentFunctionIsFallible && currentFunctionReturnBlock) {
				builder->CreateBr(currentFunctionReturnBlock);
			}

			// Mark that the last pushed value was an array for make* and append instructions
			if (name == "makei" || name == "makef" || name == "makes" || name == "makep" || name == "append") {
				lastPushedWasArray = true;
			}
		}
	}

	void LlvmGenerator::Impl::generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx) {
		const std::string& name = ident->name();

		// Check if it's a captured variable (by reference)
		auto capIt = capturedVariableRefs.find(name);
		if (capIt != capturedVariableRefs.end()) {
			// Load the pointer to the outer variable, then access through it
			llvm::AllocaInst* ptrAlloca = capIt->second;
			llvm::Value* outerVarPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), ptrAlloca, name + "_cap_ptr");

			// Now outerVarPtr points to a qd_stack_element_t - use it like localAlloca below
			// Extract type field
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, outerVarPtr, 1, name + "_cap_type_ptr");
			llvm::Value* type = builder->CreateLoad(builder->getInt32Ty(), typePtr, name + "_cap_type");

			// Value pointer
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, outerVarPtr, 0, name + "_cap_value_ptr");

			// Create basic blocks for each type
			llvm::BasicBlock* intBlock =
					llvm::BasicBlock::Create(*context, "cap_int", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* floatBlock =
					llvm::BasicBlock::Create(*context, "cap_float", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* strBlock =
					llvm::BasicBlock::Create(*context, "cap_str", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* ptrBlock =
					llvm::BasicBlock::Create(*context, "cap_ptr", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* endBlock =
					llvm::BasicBlock::Create(*context, "cap_end", builder->GetInsertBlock()->getParent());

			llvm::SwitchInst* switchInst = builder->CreateSwitch(type, endBlock, 4);
			switchInst->addCase(builder->getInt32(0), intBlock);
			switchInst->addCase(builder->getInt32(1), floatBlock);
			switchInst->addCase(builder->getInt32(2), ptrBlock);
			switchInst->addCase(builder->getInt32(3), strBlock);

			// INT block
			builder->SetInsertPoint(intBlock);
			llvm::Value* intVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, name + "_cap_i");
			generateInlinePushIntValue(ctx, intVal);
			builder->CreateBr(endBlock);

			// FLOAT block
			builder->SetInsertPoint(floatBlock);
			llvm::Value* floatVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_cap_f");
			builder->CreateCall(pushFloatFn, {ctx, floatVal});
			builder->CreateBr(endBlock);

			// STR block
			builder->SetInsertPoint(strBlock);
			llvm::Value* strVal =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_cap_s");
			builder->CreateCall(pushStrRefFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_cap_p");
			builder->CreateCall(qdPtrRetainFn, {ptrVal});
			builder->CreateCall(pushPtrFn, {ctx, ptrVal});
			builder->CreateBr(endBlock);

			builder->SetInsertPoint(endBlock);
			lastIdentifierPushed = name;
			return;
		}

		// Check if it's a local variable
		auto localIt = localVariables.find(name);
		if (localIt != localVariables.end()) {
			// Load from local variable and push to runtime stack
			llvm::AllocaInst* localAlloca = localIt->second;

			// For indirect (captured) variables, load the actual storage pointer first
			llvm::Value* storagePtr = localAlloca;
			if (indirectLocalVariables.find(name) != indirectLocalVariables.end()) {
				storagePtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), localAlloca, name + "_storage");
			}

			// Note: We can't use currentFunctionIsIntegerOnly here because even if function
			// parameters are integers, the function body may create non-integer values
			// like structs or pointers.

			// Extract type field (field index 1 in qd_stack_element_t)
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 1, name + "_type_ptr");
			llvm::Value* type = builder->CreateLoad(builder->getInt32Ty(), typePtr, name + "_type");

			// Switch on type and push appropriate value
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_value_ptr");

			// Create basic blocks for each type
			llvm::BasicBlock* intBlock =
					llvm::BasicBlock::Create(*context, "local_int", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* floatBlock =
					llvm::BasicBlock::Create(*context, "local_float", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* strBlock =
					llvm::BasicBlock::Create(*context, "local_str", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* ptrBlock =
					llvm::BasicBlock::Create(*context, "local_ptr", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* endBlock =
					llvm::BasicBlock::Create(*context, "local_end", builder->GetInsertBlock()->getParent());

			llvm::SwitchInst* switchInst = builder->CreateSwitch(type, endBlock, 4);
			switchInst->addCase(builder->getInt32(0), intBlock);   // QD_STACK_TYPE_INT = 0
			switchInst->addCase(builder->getInt32(1), floatBlock); // QD_STACK_TYPE_FLOAT = 1
			switchInst->addCase(builder->getInt32(2), ptrBlock);   // QD_STACK_TYPE_PTR = 2
			switchInst->addCase(builder->getInt32(3), strBlock);   // QD_STACK_TYPE_STR = 3

			// INT block: load i64 and push inline
			builder->SetInsertPoint(intBlock);
			llvm::Value* intVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, name + "_i");
			generateInlinePushIntValue(ctx, intVal);
			builder->CreateBr(endBlock);

			// FLOAT block: load double and push
			builder->SetInsertPoint(floatBlock);
			llvm::Value* floatVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_f");
			builder->CreateCall(pushFloatFn, {ctx, floatVal});
			builder->CreateBr(endBlock);

			// STR block: load qd_string_t* and push with retain
			builder->SetInsertPoint(strBlock);
			llvm::Value* strVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_s");
			builder->CreateCall(pushStrRefFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block: load void* and push (retain for arrays and potential structs)
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_p");
			// Retain based on variable type
			if (localArrayVariables.find(name) != localArrayVariables.end()) {
				// Retain array variables
				llvm::Function* arrayRetainFn = module->getFunction("qd_array_retain");
				if (!arrayRetainFn) {
					auto arrayRetainFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					arrayRetainFn = llvm::Function::Create(
							arrayRetainFnTy, llvm::Function::ExternalLinkage, "qd_array_retain", *module);
				}
				builder->CreateCall(arrayRetainFn, {ptrVal});
			} else {
				// Retain pointer - could be array or struct (generic function checks both)
				builder->CreateCall(qdPtrRetainFn, {ptrVal});
			}
			builder->CreateCall(pushPtrFn, {ctx, ptrVal});
			builder->CreateBr(endBlock);

			// Continue after type switch
			builder->SetInsertPoint(endBlock);

			// Track that this identifier was just pushed (for smart free)
			lastIdentifierPushed = name;
			return;
		}

		// Check if it's a loop iterator variable
		auto iterIt = iteratorVars.find(name);
		if (iterIt != iteratorVars.end()) {
			// Push loop iterator as integer (inline for performance)
			generateInlinePushIntValue(ctx, iterIt->second);
			return;
		}

		// Check if it's a constant
		auto constIt = moduleConstants.find(name);
		if (constIt != moduleConstants.end()) {
			const std::string& value = constIt->second;

			// Determine the type of the constant and push it
			if (!value.empty() && value.size() >= 2 && value.front() == '"' && value.back() == '"') {
				// String literal - create global string and push
				std::string strValue = value.substr(1, value.length() - 2);
				llvm::Value* strConst = builder->CreateGlobalString(strValue);
				builder->CreateCall(pushStrFn, {ctx, strConst});
			} else if (value.find('.') != std::string::npos) {
				// Float constant
				double floatValue = std::stod(value);
				llvm::Value* floatConst = llvm::ConstantFP::get(builder->getDoubleTy(), floatValue);
				builder->CreateCall(pushFloatFn, {ctx, floatConst});
			} else {
				// Integer constant
				int64_t intValue = 0;
				if (!safeParseInt64(value, intValue)) {
					std::cerr << "quadc: error: Invalid integer constant '" << value
							  << "' (out of range or invalid format)" << std::endl;
					compilationFailed = true;
				}
				llvm::Value* intConst = builder->getInt64(static_cast<uint64_t>(intValue));
				builder->CreateCall(pushIntFn, {ctx, intConst});
			}
			return;
		}

		// Check if it's a struct construction
		auto structIt = structDefinitions.find(name);
		if (structIt != structDefinitions.end()) {
			generateStructConstruction(name, ctx);
			return;
		}

		// Check if it's a function pointer alias (from anonymous function stored via -> name)
		// This pushes the function pointer to the stack for use with 'call'
		auto fpAliasIt = functionPointerAliases.find(name);
		if (fpAliasIt != functionPointerAliases.end()) {
			// Get the function pointer and push it to the stack
			llvm::Function* func = fpAliasIt->second;
			llvm::Value* funcPtr = builder->CreateBitCast(func, llvm::PointerType::getUnqual(*context));
			builder->CreateCall(pushPtrFn, {ctx, funcPtr});
			return;
		}

		// Check if it's a user-defined function call
		// First try the plain name, then try with current module prefix for intra-module calls
		auto it = userFunctions.find(name);
		std::string lookupName = name;
		if (it == userFunctions.end() && currentModulePrefix != "main") {
			std::string qualifiedName = currentModulePrefix + "::" + name;
			it = userFunctions.find(qualifiedName);
			if (it != userFunctions.end()) {
				lookupName = qualifiedName;
			}
		}
		if (it != userFunctions.end()) {
			// Generate any needed type casts before the function call
			generateCastInstructions(ident->parameterCasts(), ctx);

			// For fallible functions, clear error_code before the call
			// This ensures each call starts with a clean state
			auto preFallibleIt = fallibleFunctions.find(lookupName);
			if (preFallibleIt != fallibleFunctions.end() && preFallibleIt->second) {
				auto contextStructTy = llvm::StructType::get(
						*context, {
										  llvm::PointerType::getUnqual(*context), // qd_stack* st
										  builder->getInt64Ty(),				  // int64_t error_code
										  llvm::PointerType::getUnqual(*context), // char* error_msg
										  builder->getInt32Ty(),				  // int argc
										  llvm::PointerType::getUnqual(*context), // char** argv
										  llvm::PointerType::getUnqual(*context)  // char* program_name
								  });
				auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "pre_call_error_code_ptr");
				builder->CreateStore(builder->getInt64(0), errorCodePtr);
			}

			// Check for inlinable bits:: functions
			if (lookupName == "bits::and") {
				generateInlineBitAnd(ctx);
			} else if (lookupName == "bits::or") {
				generateInlineBitOr(ctx);
			} else if (lookupName == "bits::xor") {
				generateInlineBitXor(ctx);
			} else if (lookupName == "bits::not") {
				generateInlineBitNot(ctx);
			} else if (lookupName == "bits::lshift") {
				generateInlineBitLshift(ctx);
			} else if (lookupName == "bits::rshift") {
				generateInlineBitRshift(ctx);
			} else {
				builder->CreateCall(it->second, {ctx});
			}

			// Check if this function is fallible
			auto fallibleIt = fallibleFunctions.find(lookupName);
			if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
				// This is a fallible function - push error status after the call
				// Get the error_code field from context (field index 1)
				// Context layout: {qd_stack* st, int64_t error_code, char* error_msg, int argc, char** argv, char*
				// program_name}
				auto contextStructTy = llvm::StructType::get(
						*context, {
										  llvm::PointerType::getUnqual(*context), // qd_stack* st
										  builder->getInt64Ty(),				  // int64_t error_code
										  llvm::PointerType::getUnqual(*context), // char* error_msg
										  builder->getInt32Ty(),				  // int argc
										  llvm::PointerType::getUnqual(*context), // char** argv
										  llvm::PointerType::getUnqual(*context)  // char* program_name
								  });

				auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
				auto errorCode = builder->CreateLoad(builder->getInt64Ty(), errorCodePtr, "error_code");
				auto hasError = builder->CreateICmpNE(errorCode, builder->getInt64(0), "has_error");

				if (ident->abortOnError()) {
					// ! operator: check error and abort if set
					llvm::BasicBlock* errorBlock =
							llvm::BasicBlock::Create(*context, "error_abort", builder->GetInsertBlock()->getParent());
					llvm::BasicBlock* continueBlock =
							llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());

					builder->CreateCondBr(hasError, errorBlock, continueBlock);

					// Error block: print message and abort
					builder->SetInsertPoint(errorBlock);
					llvm::Value* errorMsg =
							builder->CreateGlobalString("Fatal error: function '" + name + "' failed\n");
					auto fprintfFn = module->getOrInsertFunction("fprintf",
							llvm::FunctionType::get(builder->getInt32Ty(),
									{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)},
									true));
					auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
					auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal);
					builder->CreateCall(fprintfFn, {stderrVal, errorMsg});

					auto abortFn =
							module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
					builder->CreateCall(abortFn);
					builder->CreateUnreachable();

					// Continue block
					builder->SetInsertPoint(continueBlock);
				} else {
					// No operator or ? operator: push error status
					// Convert bool to success status: true (error) -> 0, false (no error) -> 1
					auto successStatus = builder->CreateSelect(
							hasError, builder->getInt64(0), builder->getInt64(1), "success_status");

					// Don't clear error_code here - leave it so 'err' instruction can retrieve it
					// The error state will be overwritten by the next fallible call anyway

					// Push the success status onto the stack
					builder->CreateCall(pushIntFn, {ctx, successStatus});
				}
			}

			return;
		}

		// Otherwise, it might be an instruction (fallback)
		// This shouldn't happen in well-formed code
	}

	void LlvmGenerator::Impl::generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr, llvm::Value* ctx) {
		const std::string& funcName = funcPtr->functionName();

		// Look up the function in the user functions map
		auto it = userFunctions.find(funcName);
		if (it != userFunctions.end()) {
			// Get pointer to the function
			llvm::Function* fn = it->second;
			// Cast function pointer to void* and push onto stack
			auto funcPtrValue = builder->CreateBitCast(fn, llvm::PointerType::getUnqual(*context));
			builder->CreateCall(pushPtrFn, {ctx, funcPtrValue});
		} else {
			// Function not found - this should have been caught by semantic analysis
			std::cerr << "Error: Function '" << funcName << "' not found for function pointer" << std::endl;
		}
	}

	void LlvmGenerator::Impl::generateAnonymousFunction(AstNodeAnonymousFunction* anonFunc, llvm::Value* ctx) {
		// Generate a unique name for this anonymous function
		std::string funcName = "__anon_" + std::to_string(anonymousFunctionCounter++);
		std::string fullFuncName = "usr_" + mainModuleName + "_" + funcName;

		const auto& captures = anonFunc->capturedVariables();
		bool hasClosure = !captures.empty();

		// Save current state
		auto savedLocalVars = localVariables;
		auto savedLocalVarStructTypes = localVariableStructTypes;
		auto savedLocalArrayVars = localArrayVariables;
		auto savedCapturedVarRefs = capturedVariableRefs;
		auto savedLastStruct = lastStructConstructed;
		auto savedLastFieldAccess = lastFieldAccessResultType;
		auto savedReturnBlock = currentFunctionReturnBlock;
		auto savedIsFallible = currentFunctionIsFallible;
		auto savedInsertPoint = builder->GetInsertBlock();
		auto savedInsertPointEnd = builder->GetInsertPoint();
		auto savedClosureVariables = closureVariables;
		auto savedIndirectLocalVars = indirectLocalVariables;
		auto savedHeapAllocatedCaptures = heapAllocatedCaptures;
		auto savedHeapCapturePointers = heapCapturePointers;

		// Clear local variables for the anonymous function
		localVariables.clear();
		localVariableStructTypes.clear();
		localArrayVariables.clear();
		capturedVariableRefs.clear();
		lastStructConstructed.clear();
		lastFieldAccessResultType.clear();
		closureVariables.clear();
		indirectLocalVariables.clear();
		heapAllocatedCaptures.clear();
		heapCapturePointers.clear();

		// Create the function type
		// For closures: takes (context, env_ptr), returns exec_result
		// For non-closures: takes (context), returns exec_result
		llvm::FunctionType* fnTy;
		if (hasClosure) {
			fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, llvm::PointerType::getUnqual(*context)}, false);
		} else {
			fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		}
		auto fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, fullFuncName, *module);

		// Register the function
		userFunctions[funcName] = fn;

		// Create basic blocks
		auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
		auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);

		builder->SetInsertPoint(entryBB);

		// Get context parameter
		llvm::Value* anonCtx = fn->getArg(0);
		anonCtx->setName("ctx");

		// For closures, load captured variable pointers from environment (capture-by-reference)
		llvm::Value* envPtr = nullptr;
		if (hasClosure) {
			envPtr = fn->getArg(1);
			envPtr->setName("env");

			// Environment is an array of pointers to qd_stack_element_t in outer scope
			for (size_t i = 0; i < captures.size(); i++) {
				const std::string& capName = captures[i];

				// Create local alloca to hold the pointer to outer variable
				llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
				llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
				llvm::AllocaInst* ptrAlloca =
						tmpBuilder.CreateAlloca(llvm::PointerType::getUnqual(*context), nullptr, capName + "_ref");

				// Load pointer from environment array
				llvm::Value* envSlot = builder->CreateGEP(
						llvm::PointerType::getUnqual(*context), envPtr, builder->getInt64(i), capName + "_env_slot");
				llvm::Value* outerPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), envSlot, capName + "_outer_ptr");

				// Store the pointer in our local alloca
				builder->CreateStore(outerPtr, ptrAlloca);

				// Register as captured variable reference (needs extra indirection when accessed)
				capturedVariableRefs[capName] = ptrAlloca;
			}
		}

		// Set the return target for this function
		currentFunctionReturnBlock = returnBB;
		currentFunctionIsFallible = false; // Anonymous functions are not fallible (yet)

		// Generate the body
		if (anonFunc->body()) {
			generateNode(anonFunc->body(), anonCtx);
		}

		// Clean up local variables before returning
		generateLocalCleanup();

		// Jump to return block if we haven't already terminated
		llvm::BasicBlock* currentBlock = builder->GetInsertBlock();
		if (currentBlock && !currentBlock->getTerminator()) {
			builder->CreateBr(returnBB);
		}

		// Generate return block
		builder->SetInsertPoint(returnBB);
		auto successResult = builder->CreateInsertValue(llvm::UndefValue::get(execResultTy), builder->getInt32(0), {0});
		builder->CreateRet(successResult);

		// Restore state
		localVariables = savedLocalVars;
		localVariableStructTypes = savedLocalVarStructTypes;
		localArrayVariables = savedLocalArrayVars;
		capturedVariableRefs = savedCapturedVarRefs;
		lastStructConstructed = savedLastStruct;
		lastFieldAccessResultType = savedLastFieldAccess;
		currentFunctionReturnBlock = savedReturnBlock;
		currentFunctionIsFallible = savedIsFallible;
		builder->SetInsertPoint(savedInsertPoint, savedInsertPointEnd);
		closureVariables = savedClosureVariables;
		indirectLocalVariables = savedIndirectLocalVars;
		heapAllocatedCaptures = savedHeapAllocatedCaptures;
		heapCapturePointers = savedHeapCapturePointers;

		if (hasClosure) {
			// Allocate closure struct: { magic, fn_ptr, env_ptr, capture_count }
			// Magic marker to identify closures: 0xCL05UR3E (closure in leet speak)
			// Closure struct type: { i64, i8*, i8*, i64 }
			auto closureStructTy = llvm::StructType::get(*context,
					{builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
							llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

			// Allocate environment array (array of pointers for capture-by-reference)
			size_t envSize = captures.size() * 8; // sizeof(pointer) = 8 bytes on 64-bit

			// Get or create malloc function
			llvm::Function* closureMallocFn = module->getFunction("malloc");
			if (!closureMallocFn) {
				auto closureMallocFnTy = llvm::FunctionType::get(
						llvm::PointerType::getUnqual(*context), {builder->getInt64Ty()}, false);
				closureMallocFn =
						llvm::Function::Create(closureMallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
			}
			llvm::Value* envAlloc = builder->CreateCall(closureMallocFn, {builder->getInt64(envSize)}, "env_alloc");

			// Store pointers to captured variables in environment (capture-by-reference)
			// Environment is now an array of pointers to qd_stack_element_t
			for (size_t i = 0; i < captures.size(); i++) {
				const std::string& capName = captures[i];

				// Get the captured variable's alloca from the outer scope
				auto it = savedLocalVars.find(capName);
				if (it != savedLocalVars.end()) {
					llvm::AllocaInst* outerAlloca = it->second;
					llvm::Value* capturePtr = outerAlloca;

					// For indirect (heap-allocated) variables, load the heap pointer and increment refcount
					if (indirectLocalVariables.find(capName) != indirectLocalVariables.end()) {
						capturePtr = builder->CreateLoad(
								llvm::PointerType::getUnqual(*context), outerAlloca, capName + "_heap_ptr");

						// Increment refcount for this capture (the closure takes ownership)
						// Refcount is 8 bytes before the elem pointer
						llvm::Value* refCountPtr = builder->CreateGEP(
								builder->getInt8Ty(), capturePtr, builder->getInt64(static_cast<uint64_t>(-8)), capName + "_refcount_ptr");
						llvm::Value* refCount = builder->CreateLoad(builder->getInt64Ty(), refCountPtr, "refcount");
						llvm::Value* newRefCount = builder->CreateAdd(refCount, builder->getInt64(1), "new_refcount");
						builder->CreateStore(newRefCount, refCountPtr);
					}

					// Store pointer to captured variable into environment array
					llvm::Value* envSlot = builder->CreateGEP(
							llvm::PointerType::getUnqual(*context), envAlloc, builder->getInt64(i), capName + "_slot");
					builder->CreateStore(capturePtr, envSlot);
				}
			}

			// Allocate closure struct (magic + 2 pointers + capture_count = 32 bytes)
			llvm::Value* closureAlloc =
					builder->CreateCall(closureMallocFn, {builder->getInt64(32)}, "closure_alloc");

			// Store magic marker (0xCL05UR3E = 0xC105023E in hex)
			llvm::Value* magicSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 0, "magic_slot");
			builder->CreateStore(builder->getInt64(0xC105023E), magicSlot);

			// Store function pointer
			llvm::Value* fnPtrSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 1, "fn_ptr_slot");
			llvm::Value* fnPtrCast = builder->CreateBitCast(fn, llvm::PointerType::getUnqual(*context), "fn_ptr_cast");
			builder->CreateStore(fnPtrCast, fnPtrSlot);

			// Store environment pointer
			llvm::Value* envPtrSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 2, "env_ptr_slot");
			builder->CreateStore(envAlloc, envPtrSlot);

			// Store capture count for cleanup
			llvm::Value* capCountSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 3, "cap_count_slot");
			builder->CreateStore(builder->getInt64(captures.size()), capCountSlot);

			// Register the closure in the closure registry for safe detection
			auto closureRegisterFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			auto closureRegisterFn = module->getOrInsertFunction("qd_closure_register", closureRegisterFnTy);
			builder->CreateCall(closureRegisterFn, {closureAlloc});

			// Push closure struct pointer to stack
			builder->CreateCall(pushPtrFn, {ctx, closureAlloc});

			// Track that we just generated a closure
			lastGeneratedWasClosure = true;
			lastClosureCaptureCount = captures.size();
		} else {
			// No captures - just push the function pointer (existing behavior)
			auto funcPtrValue = builder->CreateBitCast(fn, llvm::PointerType::getUnqual(*context));
			builder->CreateCall(pushPtrFn, {ctx, funcPtrValue});

			// Not a closure
			lastGeneratedWasClosure = false;
			lastClosureCaptureCount = 0;
		}

		// Track the function name for potential aliasing via -> name
		lastGeneratedAnonFuncName = funcName;
	}

	void LlvmGenerator::Impl::generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx) {
		const std::string& scope = scopedIdent->scope();
		const std::string& name = scopedIdent->name();

		// Look up scoped name: scope::name
		std::string fullName = scope + "::" + name;

		// Check if this is a constant first
		auto constIt = moduleConstants.find(fullName);
		if (constIt != moduleConstants.end()) {
			// This is a constant - generate a literal push
			const std::string& value = constIt->second;

			// Determine literal type from the value string
			AstNodeLiteral::LiteralType litType;
			if (!value.empty() && value[0] == '"') {
				litType = AstNodeLiteral::LiteralType::STRING;
			} else if (value.find('.') != std::string::npos) {
				litType = AstNodeLiteral::LiteralType::FLOAT;
			} else {
				litType = AstNodeLiteral::LiteralType::INTEGER;
			}

			// Create literal and generate push
			AstNodeLiteral literal(value, litType);
			generateLiteral(&literal, ctx);
			return;
		}

		// Check if this is a struct construction
		// All structs (both from main file and modules) are in structDefinitions
		if (structDefinitions.find(name) != structDefinitions.end()) {
			// This is a struct construction from a module
			// Generate struct allocation and field initialization
			generateStructConstruction(name, ctx);
			return;
		}

		// Not a constant or struct, must be a function
		std::string mangledName = "usr_" + scope + "_" + name;

		// Check if we have this function
		llvm::Function* fn = module->getFunction(mangledName);
		if (!fn) {
			// Function doesn't exist yet, declare it
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, *module);
		}

		// Generate any needed type casts before the function call
		generateCastInstructions(scopedIdent->parameterCasts(), ctx);

		// For fallible functions, clear error_code before the call
		// This ensures each call starts with a clean state
		auto preFallibleIt = fallibleFunctions.find(fullName);
		if (preFallibleIt != fallibleFunctions.end() && preFallibleIt->second) {
			auto contextStructTy =
					llvm::StructType::get(*context, {
															llvm::PointerType::getUnqual(*context), // qd_stack* st
															builder->getInt64Ty(), // int64_t error_code
															llvm::PointerType::getUnqual(*context), // char* error_msg
															builder->getInt32Ty(),					// int argc
															llvm::PointerType::getUnqual(*context), // char** argv
															llvm::PointerType::getUnqual(*context) // char* program_name
													});
			auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "pre_call_error_code_ptr");
			builder->CreateStore(builder->getInt64(0), errorCodePtr);
		}

		// Call the scoped function
		auto callResult = builder->CreateCall(fn, {ctx}, "call_result");

		// If in test mode, track any errors from the function call
		if (testErrorAlloca) {
			auto errorCode = builder->CreateExtractValue(callResult, {0}, "err_code");
			auto hasError = builder->CreateICmpNE(errorCode, builder->getInt32(0), "has_err");
			auto currentError = builder->CreateLoad(builder->getInt32Ty(), testErrorAlloca, "cur_err");
			auto newError = builder->CreateSelect(hasError, errorCode, currentError, "new_err");
			builder->CreateStore(newError, testErrorAlloca);
		}

		// Check if this is a fallible function (same logic as for regular identifiers)
		auto fallibleIt = fallibleFunctions.find(fullName);
		if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
			// This is a fallible function - push error status after the call
			auto contextStructTy =
					llvm::StructType::get(*context, {
															llvm::PointerType::getUnqual(*context), // qd_stack* st
															builder->getInt64Ty(), // int64_t error_code
															llvm::PointerType::getUnqual(*context), // char* error_msg
															builder->getInt32Ty(),					// int argc
															llvm::PointerType::getUnqual(*context), // char** argv
															llvm::PointerType::getUnqual(*context) // char* program_name
													});

			auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
			auto errorCode = builder->CreateLoad(builder->getInt64Ty(), errorCodePtr, "error_code");
			auto hasError = builder->CreateICmpNE(errorCode, builder->getInt64(0), "has_error");

			if (scopedIdent->abortOnError()) {
				// ! operator: check error and abort if set
				llvm::BasicBlock* errorBlock =
						llvm::BasicBlock::Create(*context, "error_abort", builder->GetInsertBlock()->getParent());
				llvm::BasicBlock* continueBlock =
						llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());

				builder->CreateCondBr(hasError, errorBlock, continueBlock);

				// Error block: print message and abort
				builder->SetInsertPoint(errorBlock);
				llvm::Value* errorMsg = builder->CreateGlobalString("Fatal error: function '" + name + "' failed\n");
				auto fprintfFn = module->getOrInsertFunction("fprintf",
						llvm::FunctionType::get(builder->getInt32Ty(),
								{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)},
								true));
				auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
				auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal);
				builder->CreateCall(fprintfFn, {stderrVal, errorMsg});

				auto abortFn =
						module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
				builder->CreateCall(abortFn);
				builder->CreateUnreachable();

				// Continue block
				builder->SetInsertPoint(continueBlock);

				// For imported C functions, pop the success status they pushed
				// (since ! operator handles error via error_code, not stack-based status)
				if (importedCFunctions.find(fullName) != importedCFunctions.end()) {
					generateInlineDrop(ctx);
				}
			} else {
				// No ! operator: push success status for non-imported functions
				// Imported C functions handle their own success status push
				if (importedCFunctions.find(fullName) == importedCFunctions.end()) {
					// Convert bool to success status: true (error) -> 0, false (no error) -> 1
					auto successStatus = builder->CreateSelect(
							hasError, builder->getInt64(0), builder->getInt64(1), "success_status");

					// Don't clear error_code here - leave it so 'err' instruction can retrieve it
					// The error state will be overwritten by the next fallible call anyway

					// Push the success status onto the stack
					builder->CreateCall(pushIntFn, {ctx, successStatus});
				}
			}
		}
	}

	void LlvmGenerator::Impl::generateSwitchStatement(AstNodeSwitchStatement* switchStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Get the runtime stack pop function
		auto stackPopFunc = module->getFunction("qd_stack_pop");
		auto switchElemTy = llvm::StructType::get(*context,
				{builder->getInt64Ty(),			// value (union as i64)
						builder->getInt32Ty(),	// type
						builder->getInt1Ty()}); // is_error_tainted

		// Pop the value to switch on from the stack
		auto stackFieldPtr =
				builder->CreateStructGEP(llvm::StructType::get(*context,
												 {llvm::PointerType::getUnqual(*context),		   // qd_stack* st
														 builder->getInt64Ty(),					   // int64_t error_code
														 llvm::PointerType::getUnqual(*context),   // char* error_msg
														 builder->getInt32Ty(),					   // int argc
														 llvm::PointerType::getUnqual(*context),   // char** argv
														 llvm::PointerType::getUnqual(*context)}), // char* program_name
						ctx, 0, "st_ptr");
		auto stack = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackFieldPtr, "st");

		// Create alloca in entry block to avoid stack growth in loops
		llvm::BasicBlock& entryBlock = currentFn->getEntryBlock();
		llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.getFirstInsertionPt());
		auto switchElem = entryBuilder.CreateAlloca(switchElemTy, nullptr, "switch_elem");

		builder->CreateCall(stackPopFunc, {stack, switchElem});

		// Create merge block (after all cases)
		llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "switch.merge", currentFn);

		// Generate cases as if-else chain
		const auto& cases = switchStmt->cases();
		llvm::BasicBlock* nextCaseBB = nullptr;
		llvm::BasicBlock* defaultBB = nullptr;

		// Find default case if present
		for (auto* caseNode : cases) {
			if (caseNode->isDefault()) {
				defaultBB = llvm::BasicBlock::Create(*context, "switch.default", currentFn);
				break;
			}
		}

		// If no default, merge is the default
		if (!defaultBB) {
			defaultBB = mergeBB;
		}

		// Count non-default cases
		size_t nonDefaultCaseCount = 0;
		for (auto* caseNode : cases) {
			if (!caseNode->isDefault()) {
				nonDefaultCaseCount++;
			}
		}

		// Generate each case
		size_t processedCases = 0;
		for (size_t i = 0; i < cases.size(); i++) {
			AstNodeCase* caseNode = cases[i];

			if (caseNode->isDefault()) {
				// Handle default case at the end
				continue;
			}

			// Create blocks for this case
			llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(*context, "switch.case", currentFn);
			processedCases++;
			nextCaseBB = (processedCases < nonDefaultCaseCount)
								 ? llvm::BasicBlock::Create(*context, "switch.check", currentFn)
								 : defaultBB;

			// Generate comparison
			IAstNode* caseValue = caseNode->value();
			llvm::Value* matches = nullptr;

			if (caseValue->type() == IAstNode::Type::LITERAL) {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(caseValue);

				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					// Compare switch value with case value (integer)
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
					auto switchVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "switch_val");

					int64_t parsedVal = 0;
					if (!safeParseInt64(lit->value(), parsedVal)) {
						std::cerr << "quadc: error: Invalid integer case label '" << lit->value()
								  << "' (out of range or invalid format)" << std::endl;
						compilationFailed = true;
					}
					auto caseVal = builder->getInt64(static_cast<uint64_t>(parsedVal));
					matches = builder->CreateICmpEQ(switchVal, caseVal, "case_match");
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::FLOAT) {
					// Compare float values
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
					auto switchVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "switch_val_f");
					auto caseVal = llvm::ConstantFP::get(builder->getDoubleTy(), std::stod(lit->value()));
					matches = builder->CreateFCmpOEQ(switchVal, caseVal, "case_match");
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::STRING) {
					// Compare strings using strcmp
					auto strcmpFn = module->getFunction("strcmp");
					if (!strcmpFn) {
						// Declare strcmp if not already declared
						auto charPtrTy = llvm::PointerType::getUnqual(*context);
						auto strcmpTy = llvm::FunctionType::get(builder->getInt32Ty(), {charPtrTy, charPtrTy}, false);
						strcmpFn = llvm::Function::Create(
								strcmpTy, llvm::Function::ExternalLinkage, "strcmp", module.get());
					}

					// Get switch string value (qd_string_t*)
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
					auto switchStrPtr =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "switch_str");

					// Call qd_string_data to get const char*
					if (!this->qdStringDataFn) {
						auto qdStringDataFnTy = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context),
								{llvm::PointerType::getUnqual(*context)}, false);
						this->qdStringDataFn = llvm::Function::Create(
								qdStringDataFnTy, llvm::Function::ExternalLinkage, "qd_string_data", *module);
					}
					auto switchStrData = builder->CreateCall(this->qdStringDataFn, {switchStrPtr}, "switch_str_data");

					// Create case string constant
					auto caseStr = builder->CreateGlobalString(lit->value().substr(1, lit->value().length() - 2));

					// Call strcmp
					auto cmpResult = builder->CreateCall(strcmpFn, {switchStrData, caseStr}, "strcmp_result");
					matches = builder->CreateICmpEQ(cmpResult, builder->getInt32(0), "case_match");
				}
			} else if (caseValue->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
				// Handle scoped constants (module::ConstName)
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(caseValue);
				std::string fullName = scoped->scope() + "::" + scoped->name();

				// Look up the constant value in moduleConstants map
				auto constIt = moduleConstants.find(fullName);
				if (constIt != moduleConstants.end()) {
					const std::string& value = constIt->second;
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");

					// Determine the type from the value string
					if (!value.empty() && value[0] == '"') {
						// String constant
						auto strcmpFn = module->getFunction("strcmp");
						if (!strcmpFn) {
							auto charPtrTy = llvm::PointerType::getUnqual(*context);
							auto strcmpTy =
									llvm::FunctionType::get(builder->getInt32Ty(), {charPtrTy, charPtrTy}, false);
							strcmpFn = llvm::Function::Create(
									strcmpTy, llvm::Function::ExternalLinkage, "strcmp", module.get());
						}

						auto switchStrPtr =
								builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "switch_str");

						// Call qd_string_data to get const char*
						if (!this->qdStringDataFn) {
							auto qdStringDataFnTy = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context),
									{llvm::PointerType::getUnqual(*context)}, false);
							this->qdStringDataFn = llvm::Function::Create(
									qdStringDataFnTy, llvm::Function::ExternalLinkage, "qd_string_data", *module);
						}
						auto switchStrData =
								builder->CreateCall(this->qdStringDataFn, {switchStrPtr}, "switch_str_data");

						// Create case string constant (strip quotes)
						auto caseStr = builder->CreateGlobalString(value.substr(1, value.length() - 2));
						auto cmpResult = builder->CreateCall(strcmpFn, {switchStrData, caseStr}, "strcmp_result");
						matches = builder->CreateICmpEQ(cmpResult, builder->getInt32(0), "case_match");
					} else if (value.find('.') != std::string::npos) {
						// Float constant
						auto switchVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "switch_val_f");
						auto caseVal = llvm::ConstantFP::get(builder->getDoubleTy(), std::stod(value));
						matches = builder->CreateFCmpOEQ(switchVal, caseVal, "case_match");
					} else {
						// Integer constant
						auto switchVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "switch_val");
						int64_t parsedVal = 0;
						if (!safeParseInt64(value, parsedVal)) {
							std::cerr << "quadc: error: Invalid integer constant value '" << value << "' for "
									  << fullName << std::endl;
							compilationFailed = true;
						}
						auto caseVal = builder->getInt64(static_cast<uint64_t>(parsedVal));
						matches = builder->CreateICmpEQ(switchVal, caseVal, "case_match");
					}
				}
			}

			if (matches) {
				builder->CreateCondBr(matches, caseBB, nextCaseBB);

				// Generate case body
				builder->SetInsertPoint(caseBB);
				if (caseNode->body()) {
					generateNode(caseNode->body(), ctx);
				}
				// Branch to merge (automatic break)
				llvm::BasicBlock* caseBlock = builder->GetInsertBlock();
				if (caseBlock && !caseBlock->getTerminator()) {
					builder->CreateBr(mergeBB);
				}

				// Set up for next case
				builder->SetInsertPoint(nextCaseBB);
			}
		}

		// Generate default case if present
		if (defaultBB != mergeBB) {
			builder->SetInsertPoint(defaultBB);
			for (auto* caseNode : cases) {
				if (caseNode->isDefault() && caseNode->body()) {
					generateNode(caseNode->body(), ctx);
					break;
				}
			}
			llvm::BasicBlock* defaultBlock = builder->GetInsertBlock();
			if (defaultBlock && !defaultBlock->getTerminator()) {
				builder->CreateBr(mergeBB);
			}
		}

		// Continue with merge block
		builder->SetInsertPoint(mergeBB);

		// Clean up switch value if it's a string (need to free the allocated memory)
		auto typePtr = builder->CreateStructGEP(switchElemTy, switchElem, 1, "type_ptr");
		auto switchType = builder->CreateLoad(builder->getInt32Ty(), typePtr, "switch_type");
		auto isString = builder->CreateICmpEQ(switchType, builder->getInt32(3), "is_string"); // QD_STACK_TYPE_STR = 3

		llvm::BasicBlock* freeStringBB = llvm::BasicBlock::Create(*context, "free_string", currentFn);
		llvm::BasicBlock* skipFreeBB = llvm::BasicBlock::Create(*context, "skip_free", currentFn);

		builder->CreateCondBr(isString, freeStringBB, skipFreeBB);

		// Release string reference
		builder->SetInsertPoint(freeStringBB);
		auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
		auto strPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "str_ptr");
		if (!this->qdStringReleaseFn) {
			auto qdStringReleaseFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			this->qdStringReleaseFn = llvm::Function::Create(
					qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
		}
		builder->CreateCall(this->qdStringReleaseFn, {strPtr});
		builder->CreateBr(skipFreeBB);

		// Skip free block
		builder->SetInsertPoint(skipFreeBB);
	}

	void LlvmGenerator::Impl::generateLocal(AstNodeLocal* local, llvm::Value* ctx) {
		// Handle multiple assignment: -> a b c pops 3 values into a, b, c respectively
		const std::vector<std::string>& names = local->names();
		for (const std::string& name : names) {
			generateLocalOne(name, local->line(), ctx);
		}
	}

	void LlvmGenerator::Impl::generateLocalOne(const std::string& name, size_t lineNum, llvm::Value* ctx) {
		// First check if this is a captured variable reference (inside closure body)
		// If so, we should store to the captured location, not create a new local
		auto capIt = capturedVariableRefs.find(name);
		if (capIt != capturedVariableRefs.end()) {
			// This is a captured variable - load the pointer and store the value there
			llvm::AllocaInst* ptrAlloca = capIt->second;
			llvm::Value* outerVarPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), ptrAlloca, name + "_cap_store_ptr");

			// Get the stack pointer from context
			auto contextStructTy = llvm::StructType::get(*context,
					{
							llvm::PointerType::getUnqual(*context), // qd_stack* st
							builder->getInt64Ty(),					// int64_t error_code
							llvm::PointerType::getUnqual(*context), // char* error_msg
							builder->getInt32Ty(),					// int argc
							llvm::PointerType::getUnqual(*context), // char** argv
							llvm::PointerType::getUnqual(*context)	// char* program_name
					});

			llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
			llvm::Value* stackPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtrPtr, "stack");

			// Pop directly into the captured variable's storage
			builder->CreateCall(stackPopFn, {stackPtr, outerVarPtr});
			return;
		}

		// Check if this variable already exists (reuse the alloca if so)
		llvm::AllocaInst* localAlloca;
		auto it = localVariables.find(name);
		bool isNewVariable = (it == localVariables.end());
		bool isCaptured = heapAllocatedCaptures.find(name) != heapAllocatedCaptures.end();

		if (isNewVariable) {
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());

			if (isCaptured) {
				// Variable will be captured by a closure - allocate on heap for escaped closure support
				// Layout: { int64_t refcount, qd_stack_element_t elem } = 32 bytes
				// Create alloca to hold the heap pointer (to the elem part, not the refcount)
				localAlloca = tmpBuilder.CreateAlloca(llvm::PointerType::getUnqual(*context), nullptr, name + "_ptr");

				// Get or create malloc function
				llvm::Function* localMallocFn = module->getFunction("malloc");
				if (!localMallocFn) {
					auto mallocFnTy = llvm::FunctionType::get(
							llvm::PointerType::getUnqual(*context), {tmpBuilder.getInt64Ty()}, false);
					localMallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
				}

				// Allocate heap memory in ENTRY BLOCK (important for loops - only allocate once)
				// 8 bytes refcount + 24 bytes qd_stack_element_t = 32 bytes
				llvm::Value* heapBlock =
						tmpBuilder.CreateCall(localMallocFn, {tmpBuilder.getInt64(32)}, name + "_heap_block");

				// Initialize refcount to 1
				tmpBuilder.CreateStore(tmpBuilder.getInt64(1), heapBlock);

				// Get pointer to the qd_stack_element_t (at offset 8)
				llvm::Value* heapPtr =
						tmpBuilder.CreateGEP(tmpBuilder.getInt8Ty(), heapBlock, tmpBuilder.getInt64(8), name + "_heap");

				// Store elem pointer in the alloca (this is what code accesses)
				tmpBuilder.CreateStore(heapPtr, localAlloca);

				// Initialize type field to -1 (uninitialized marker)
				llvm::Value* typePtr = tmpBuilder.CreateStructGEP(stackElementTy, heapPtr, 1, name + "_init_type");
				tmpBuilder.CreateStore(tmpBuilder.getInt32(static_cast<uint32_t>(-1)), typePtr);

				// Track as indirect variable and record heap pointer
				indirectLocalVariables.insert(name);
				heapCapturePointers[name] = heapPtr;
			} else {
				// Normal case: Create alloca for the stack element in the entry block
				localAlloca = tmpBuilder.CreateAlloca(stackElementTy, nullptr, name);

				// Initialize type field to 0xFFFFFFFF (uninitialized marker) to avoid cleanup issues
				// when variable is declared in a conditional branch that isn't taken
				llvm::Value* typePtr = tmpBuilder.CreateStructGEP(stackElementTy, localAlloca, 1, name + "_init_type");
				tmpBuilder.CreateStore(tmpBuilder.getInt32(static_cast<uint32_t>(-1)), typePtr);
			}

			// Store in local variables map
			localVariables[name] = localAlloca;

			// Add debug info for the local variable
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && stackElementDebugType) {
				// Get the file from the current scope (subprogram)
				llvm::DIFile* localFile = debugFile; // Default
				if (auto* subprog = llvm::dyn_cast<llvm::DISubprogram>(debugScopeStack.back())) {
					localFile = subprog->getFile();
				}

				// Create local variable debug info
				// Note: localAlloca is an alloca of qd_stack_element_t (structure on stack),
				// so the debug type should be the structure type, not a pointer.
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (current function)
						name,															 // Variable name
						localFile,														 // File
						static_cast<unsigned>(lineNum),									 // Line number
						stackElementDebugType,											 // Type (the struct)
						true															 // Always preserve
				);

				// Insert declare to make it visible in debugger
				debugBuilder->insertDeclare(localAlloca,  // Storage (the alloca)
						localVar,						  // Variable
						debugBuilder->createExpression(), // Expression
						llvm::DILocation::get(*context, static_cast<unsigned>(lineNum), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}
		} else {
			// Variable already exists, reuse it
			localAlloca = it->second;
		}

		// For indirect (captured) variables, load the actual storage pointer
		// localAlloca holds a pointer to heap memory, we need the heap memory address
		llvm::Value* storagePtr = localAlloca;
		if (indirectLocalVariables.find(name) != indirectLocalVariables.end()) {
			storagePtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), localAlloca, name + "_storage");
		}

		// ALWAYS check and release old value before storing new one
		// This handles both explicit reassignment and loop iterations
		// The type == -1 check handles the first assignment (no old value to release)
		llvm::Value* oldTypePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 1, name + "_old_type_ptr");
		llvm::Value* oldType = builder->CreateLoad(builder->getInt32Ty(), oldTypePtr, name + "_old_type");

		// Create basic blocks for conditional release (strings only - structs are stack-allocated)
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* checkStrBlock = llvm::BasicBlock::Create(*context, name + "_check_old_str", currentFn);
		llvm::BasicBlock* releaseStrBlock = llvm::BasicBlock::Create(*context, name + "_release_old_str", currentFn);
		llvm::BasicBlock* checkPtrBlock = llvm::BasicBlock::Create(*context, name + "_check_old_ptr", currentFn);
		llvm::BasicBlock* afterReleaseBlock = llvm::BasicBlock::Create(*context, name + "_after_release", currentFn);

		// First check if type == -1 (uninitialized) - skip release if first assignment
		llvm::Value* isUninitialized =
				builder->CreateICmpEQ(oldType, builder->getInt32(static_cast<uint32_t>(-1)), name + "_was_uninit");
		builder->CreateCondBr(isUninitialized, afterReleaseBlock, checkStrBlock);

		// Check if old type == QD_STACK_TYPE_STR (3)
		builder->SetInsertPoint(checkStrBlock);
		llvm::Value* wasString = builder->CreateICmpEQ(oldType, builder->getInt32(3), name + "_was_str");
		builder->CreateCondBr(wasString, releaseStrBlock, checkPtrBlock);

		// Release old string block
		builder->SetInsertPoint(releaseStrBlock);
		llvm::Value* oldValuePtrStr =
				builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_old_value_ptr_str");
		llvm::Value* oldStrPtr =
				builder->CreateLoad(llvm::PointerType::getUnqual(*context), oldValuePtrStr, name + "_old_str");

		// Call qd_string_release() on the old string
		if (!this->qdStringReleaseFn) {
			auto qdStringReleaseFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			this->qdStringReleaseFn = llvm::Function::Create(
					qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
		}
		builder->CreateCall(this->qdStringReleaseFn, {oldStrPtr});
		builder->CreateBr(afterReleaseBlock);

		// Check if old value was a pointer - release appropriately
		builder->SetInsertPoint(checkPtrBlock);
		llvm::BasicBlock* releasePtrBlock = llvm::BasicBlock::Create(*context, name + "_release_old_ptr", currentFn);
		llvm::Value* wasPtr = builder->CreateICmpEQ(oldType, builder->getInt32(2), name + "_was_ptr");
		builder->CreateCondBr(wasPtr, releasePtrBlock, afterReleaseBlock);

		// Release old pointer - check if it's a closure and handle specially
		builder->SetInsertPoint(releasePtrBlock);
		llvm::Value* oldValuePtrPtr =
				builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_old_value_ptr_ptr");
		llvm::Value* oldPtrVal =
				builder->CreateLoad(llvm::PointerType::getUnqual(*context), oldValuePtrPtr, name + "_old_ptr");

		// Always check for closure on ptr reassignments - the variable could hold a closure
		// even if not tracked in closureVariables (e.g., first iteration of a loop).
		// Use the closure registry to safely detect closures without reading from freed memory.
		{
			// Check closure registry to see if it's actually a closure (safe for any pointer)
			auto closureStructTy = llvm::StructType::get(*context,
					{builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
							llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

			auto closureIsValidFnTy =
					llvm::FunctionType::get(builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context)}, false);
			auto closureIsValidFn = module->getOrInsertFunction("qd_closure_is_valid", closureIsValidFnTy);
			llvm::Value* isClosureResult = builder->CreateCall(closureIsValidFn, {oldPtrVal}, "is_closure_result");
			llvm::Value* isMagicValid =
					builder->CreateICmpNE(isClosureResult, builder->getInt32(0), "old_is_closure");

			llvm::BasicBlock* releaseClosureBlock =
					llvm::BasicBlock::Create(*context, name + "_release_old_closure", currentFn);
			llvm::BasicBlock* releaseGenericBlock =
					llvm::BasicBlock::Create(*context, name + "_release_old_generic", currentFn);
			builder->CreateCondBr(isMagicValid, releaseClosureBlock, releaseGenericBlock);

			// Release as closure
			builder->SetInsertPoint(releaseClosureBlock);
			llvm::Value* envPtrSlot =
					builder->CreateStructGEP(closureStructTy, oldPtrVal, 2, name + "_old_env_slot");
			llvm::Value* envPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), envPtrSlot, "old_env_ptr");
			llvm::Value* capCountSlot =
					builder->CreateStructGEP(closureStructTy, oldPtrVal, 3, name + "_old_cap_count_slot");
			llvm::Value* capCount = builder->CreateLoad(builder->getInt64Ty(), capCountSlot, "old_cap_count");

			// Loop through captured variables and decrement refcounts
			llvm::BasicBlock* loopHeader =
					llvm::BasicBlock::Create(*context, name + "_old_cap_loop_header", currentFn);
			llvm::BasicBlock* loopBody =
					llvm::BasicBlock::Create(*context, name + "_old_cap_loop_body", currentFn);
			llvm::BasicBlock* afterLoop =
					llvm::BasicBlock::Create(*context, name + "_old_cap_loop_done", currentFn);

			llvm::AllocaInst* loopIdxAlloca =
					builder->CreateAlloca(builder->getInt64Ty(), nullptr, name + "_old_cap_idx");
			builder->CreateStore(builder->getInt64(0), loopIdxAlloca);
			builder->CreateBr(loopHeader);

			builder->SetInsertPoint(loopHeader);
			llvm::Value* loopIdx = builder->CreateLoad(builder->getInt64Ty(), loopIdxAlloca, "idx");
			llvm::Value* loopCond = builder->CreateICmpSLT(loopIdx, capCount, "loop_cond");
			builder->CreateCondBr(loopCond, loopBody, afterLoop);

			builder->SetInsertPoint(loopBody);
			llvm::Value* capSlot = builder->CreateGEP(
					llvm::PointerType::getUnqual(*context), envPtr, loopIdx, name + "_old_cap_slot");
			llvm::Value* capVarPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), capSlot, "old_cap_var_ptr");
			llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capVarPtr,
					builder->getInt64(static_cast<uint64_t>(-8)), name + "_old_refcount_ptr");
			llvm::Value* refCount = builder->CreateLoad(builder->getInt64Ty(), refCountPtr, "old_refcount");
			llvm::Value* newRefCount = builder->CreateSub(refCount, builder->getInt64(1), "old_new_refcount");
			builder->CreateStore(newRefCount, refCountPtr);

			llvm::BasicBlock* freeCapBlock =
					llvm::BasicBlock::Create(*context, name + "_old_free_cap", currentFn);
			llvm::BasicBlock* capContinue =
					llvm::BasicBlock::Create(*context, name + "_old_cap_continue", currentFn);
			llvm::Value* shouldFree = builder->CreateICmpEQ(newRefCount, builder->getInt64(0), "old_should_free");
			builder->CreateCondBr(shouldFree, freeCapBlock, capContinue);

			builder->SetInsertPoint(freeCapBlock);
			llvm::Function* localFreeFn = module->getFunction("free");
			if (!localFreeFn) {
				auto freeFnTy = llvm::FunctionType::get(
						builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
				localFreeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(localFreeFn, {refCountPtr});
			builder->CreateBr(capContinue);

			builder->SetInsertPoint(capContinue);
			llvm::Value* nextIdx = builder->CreateAdd(loopIdx, builder->getInt64(1), "old_next_idx");
			builder->CreateStore(nextIdx, loopIdxAlloca);
			builder->CreateBr(loopHeader);

			builder->SetInsertPoint(afterLoop);
			// Unregister closure from registry before freeing
			auto closureUnregisterFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			auto closureUnregisterFn = module->getOrInsertFunction("qd_closure_unregister", closureUnregisterFnTy);
			builder->CreateCall(closureUnregisterFn, {oldPtrVal});

			llvm::Function* closureFreeFn = module->getFunction("free");
			if (!closureFreeFn) {
				auto freeFnTy = llvm::FunctionType::get(
						builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
				closureFreeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(closureFreeFn, {envPtr});
			builder->CreateCall(closureFreeFn, {oldPtrVal});
			builder->CreateBr(afterReleaseBlock);

			// Release as generic pointer (not a closure)
			builder->SetInsertPoint(releaseGenericBlock);
			builder->CreateCall(qdPtrReleaseFn, {oldPtrVal});
			builder->CreateBr(afterReleaseBlock);
		}

		// Continue after release
		builder->SetInsertPoint(afterReleaseBlock);

		// Get the stack pointer from context
		// Context layout: {qd_stack* st, int64_t error_code, char* error_msg, int argc, char** argv, char*
		// program_name}
		auto contextStructTy = llvm::StructType::get(*context,
				{
						llvm::PointerType::getUnqual(*context), // qd_stack* st
						builder->getInt64Ty(),					// int64_t error_code
						llvm::PointerType::getUnqual(*context), // char* error_msg
						builder->getInt32Ty(),					// int argc
						llvm::PointerType::getUnqual(*context), // char** argv
						llvm::PointerType::getUnqual(*context)	// char* program_name
				});

		llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
		llvm::Value* stackPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtrPtr, "stack");

		// Call qd_stack_pop to pop the value from the runtime stack
		// Note: We can't optimize this for "integer-only" functions because even if the
		// function parameters are integers, the function body may create/use non-integer
		// values like structs and pointers.
		llvm::Value* popResult = builder->CreateCall(stackPopFn, {stackPtr, storagePtr});

		// Check result for errors (0 = success, non-zero = error)
		llvm::Value* popFailed = builder->CreateICmpNE(popResult, builder->getInt32(0), name + "_pop_failed");
		llvm::BasicBlock* popErrorBB = llvm::BasicBlock::Create(*context, name + "_pop_error", currentFn);
		llvm::BasicBlock* popOkBB = llvm::BasicBlock::Create(*context, name + "_pop_ok", currentFn);
		builder->CreateCondBr(popFailed, popErrorBB, popOkBB);

		// Error block: print error and abort
		builder->SetInsertPoint(popErrorBB);
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto fprintfFnTy = llvm::FunctionType::get(
				builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true);
		auto fprintfFn = module->getOrInsertFunction("fprintf", fprintfFnTy);
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg = builder->CreateGlobalString("Fatal error: Stack underflow when assigning to local variable\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Continue in success block
		builder->SetInsertPoint(popOkBB);

		// If we just constructed a struct, record its type for this local variable
		if (!lastStructConstructed.empty()) {
			localVariableStructTypes[name] = lastStructConstructed;
			lastStructConstructed.clear();
		}
		// NOTE: We do NOT retain when storing to a local.
		// Refcount management is handled by retaining when PUSHING a local to pass to a function,
		// and releasing when the function's local goes out of scope.
		// This correctly handles both owned structs (created locally) and borrowed structs
		// (received from function calls).

		// If the last pushed value was an array, track this variable as an array variable
		if (lastPushedWasArray) {
			localArrayVariables.insert(name);
			lastPushedWasArray = false;
		}

		// If the stored value was an anonymous function, create an alias for function pointer lookup
		// This allows nested anonymous functions to reference outer function pointer variables
		if (!lastGeneratedAnonFuncName.empty()) {
			auto anonFuncIt = userFunctions.find(lastGeneratedAnonFuncName);
			if (anonFuncIt != userFunctions.end()) {
				functionPointerAliases[name] = anonFuncIt->second;
			}
			lastGeneratedAnonFuncName.clear();
		}

		// If the stored value was a closure with captures, track it for cleanup
		if (lastGeneratedWasClosure) {
			closureVariables.insert(name);
			lastGeneratedWasClosure = false;
		}
	}

	void LlvmGenerator::Impl::generateLocalCleanup() {
		// Free any string and array locals to prevent memory leaks
		// Iterate through all local variables and check their type
		for (const auto& pair : localVariables) {
			const std::string& varName = pair.first;
			llvm::AllocaInst* localAlloca = pair.second;

			// Handle heap-allocated captured variables - decrement refcount
			// The original owner releases its reference; closures will release theirs when cleaned up
			if (indirectLocalVariables.find(varName) != indirectLocalVariables.end()) {
				// Load the heap pointer (elem location)
				llvm::Value* heapPtr = builder->CreateLoad(
						llvm::PointerType::getUnqual(*context), localAlloca, varName + "_heap_cleanup");

				// Refcount is 8 bytes before the elem pointer
				llvm::Value* refCountPtr = builder->CreateGEP(
						builder->getInt8Ty(), heapPtr, builder->getInt64(static_cast<uint64_t>(-8)), varName + "_refcount_cleanup");
				llvm::Value* refCount = builder->CreateLoad(builder->getInt64Ty(), refCountPtr, "refcount");
				llvm::Value* newRefCount = builder->CreateSub(refCount, builder->getInt64(1), "new_refcount");
				builder->CreateStore(newRefCount, refCountPtr);

				// If refcount == 0, free the heap block
				llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
				llvm::BasicBlock* freeCapBlock =
						llvm::BasicBlock::Create(*context, varName + "_free_cap_heap", currentFn);
				llvm::BasicBlock* skipFreeCapBlock =
						llvm::BasicBlock::Create(*context, varName + "_skip_free_cap", currentFn);
				llvm::Value* shouldFree = builder->CreateICmpEQ(newRefCount, builder->getInt64(0), "should_free_cap");
				builder->CreateCondBr(shouldFree, freeCapBlock, skipFreeCapBlock);

				builder->SetInsertPoint(freeCapBlock);
				// Before freeing, release the value inside if it's a string or pointer
				// heapPtr points to qd_stack_element_t; type is at offset 8 within it
				llvm::Value* capTypePtr = builder->CreateGEP(
						builder->getInt8Ty(), heapPtr, builder->getInt64(8), varName + "_cap_type_ptr");
				llvm::Value* capType = builder->CreateLoad(builder->getInt32Ty(), capTypePtr, "cap_type");

				// Create blocks for type-specific cleanup
				llvm::BasicBlock* capIsStr = llvm::BasicBlock::Create(*context, varName + "_cap_is_str", currentFn);
				llvm::BasicBlock* capCheckPtr = llvm::BasicBlock::Create(*context, varName + "_cap_check_ptr", currentFn);
				llvm::BasicBlock* capIsPtr = llvm::BasicBlock::Create(*context, varName + "_cap_is_ptr", currentFn);
				llvm::BasicBlock* capDoFree = llvm::BasicBlock::Create(*context, varName + "_cap_do_free", currentFn);

				// Check if type == 3 (string)
				llvm::Value* capIsStrCond = builder->CreateICmpEQ(capType, builder->getInt32(3), "cap_is_str");
				builder->CreateCondBr(capIsStrCond, capIsStr, capCheckPtr);

				// Release string
				builder->SetInsertPoint(capIsStr);
				llvm::Value* capStrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), heapPtr, "cap_str");
				if (!this->qdStringReleaseFn) {
					auto qdStringReleaseFnTy =
							llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					this->qdStringReleaseFn = llvm::Function::Create(
							qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
				}
				builder->CreateCall(this->qdStringReleaseFn, {capStrVal});
				builder->CreateBr(capDoFree);

				// Check if type == 2 (pointer - could be struct, array, or closure)
				builder->SetInsertPoint(capCheckPtr);
				llvm::Value* capIsPtrCond = builder->CreateICmpEQ(capType, builder->getInt32(2), "cap_is_ptr");
				builder->CreateCondBr(capIsPtrCond, capIsPtr, capDoFree);

				// Release pointer (works for structs and arrays)
				builder->SetInsertPoint(capIsPtr);
				llvm::Value* capPtrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), heapPtr, "cap_ptr");
				builder->CreateCall(qdPtrReleaseFn, {capPtrVal});
				builder->CreateBr(capDoFree);

				// Free the heap block
				builder->SetInsertPoint(capDoFree);
				llvm::Function* capFreeFn = module->getFunction("free");
				if (!capFreeFn) {
					auto freeFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					capFreeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
				}
				// Free starting at refcount (the actual malloc block start)
				builder->CreateCall(capFreeFn, {refCountPtr});
				builder->CreateBr(skipFreeCapBlock);

				builder->SetInsertPoint(skipFreeCapBlock);
				continue;
			}

			// Check if this is a known array variable (needs ref counting cleanup)
			bool isArray = (localArrayVariables.find(varName) != localArrayVariables.end());

			// Check if this is a closure variable (needs closure-specific cleanup)
			bool isClosure = (closureVariables.find(varName) != closureVariables.end());

			// Load the type field to check if it's a string or array
			llvm::Value* typePtr =
					builder->CreateStructGEP(stackElementTy, localAlloca, 1, varName + "_cleanup_type_ptr");
			llvm::Value* type = builder->CreateLoad(builder->getInt32Ty(), typePtr, varName + "_cleanup_type");

			// Create basic blocks for conditional free
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* checkInitBlock = llvm::BasicBlock::Create(*context, varName + "_check_init", currentFn);
			llvm::BasicBlock* freeStrBlock = llvm::BasicBlock::Create(*context, varName + "_free_str", currentFn);
			llvm::BasicBlock* checkPtrBlock = llvm::BasicBlock::Create(*context, varName + "_check_ptr", currentFn);
			llvm::BasicBlock* skipFreeBlock = llvm::BasicBlock::Create(*context, varName + "_skip_free", currentFn);

			// First check if type == 0xFFFFFFFF (uninitialized) - skip cleanup if never assigned
			llvm::Value* isUninitialized =
					builder->CreateICmpEQ(type, builder->getInt32(static_cast<uint32_t>(-1)), varName + "_is_uninit");
			builder->CreateCondBr(isUninitialized, skipFreeBlock, checkInitBlock);

			// Variable was initialized, check its type
			builder->SetInsertPoint(checkInitBlock);

			// Check if type == QD_STACK_TYPE_STR (3)
			llvm::Value* isString = builder->CreateICmpEQ(type, builder->getInt32(3), varName + "_is_str");
			builder->CreateCondBr(isString, freeStrBlock, checkPtrBlock);

			// Free string block
			builder->SetInsertPoint(freeStrBlock);
			llvm::Value* valuePtr =
					builder->CreateStructGEP(stackElementTy, localAlloca, 0, varName + "_cleanup_value_ptr");
			llvm::Value* strPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, varName + "_cleanup_str");

			// Call qd_string_release() on the string
			if (!this->qdStringReleaseFn) {
				auto qdStringReleaseFnTy =
						llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
				this->qdStringReleaseFn = llvm::Function::Create(
						qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
			}
			builder->CreateCall(this->qdStringReleaseFn, {strPtr});
			builder->CreateBr(skipFreeBlock);

			// Check if type == QD_STACK_TYPE_PTR (2) and handle appropriately
			builder->SetInsertPoint(checkPtrBlock);
			if (isClosure) {
				// Closure variable - need to free closure struct, environment, and decrement capture refcounts
				llvm::BasicBlock* freeClosureBlock =
						llvm::BasicBlock::Create(*context, varName + "_free_closure", currentFn);
				llvm::Value* isPtr = builder->CreateICmpEQ(type, builder->getInt32(2), varName + "_is_ptr");
				builder->CreateCondBr(isPtr, freeClosureBlock, skipFreeBlock);

				builder->SetInsertPoint(freeClosureBlock);
				llvm::Value* ptrValuePtr =
						builder->CreateStructGEP(stackElementTy, localAlloca, 0, varName + "_cleanup_ptr_value_ptr");
				llvm::Value* closurePtr = builder->CreateLoad(
						llvm::PointerType::getUnqual(*context), ptrValuePtr, varName + "_cleanup_closure");

				// Closure struct layout: { i64 magic, ptr fn, ptr env, i64 capture_count }
				auto closureStructTy = llvm::StructType::get(*context,
						{builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
								llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

				// Check if this is a valid closure using the registry (safe - doesn't dereference)
				auto closureIsValidFnTy =
						llvm::FunctionType::get(builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context)}, false);
				auto closureIsValidFn = module->getOrInsertFunction("qd_closure_is_valid", closureIsValidFnTy);
				llvm::Value* isClosureResult = builder->CreateCall(closureIsValidFn, {closurePtr}, "is_closure_result");
				llvm::Value* isMagicValid =
						builder->CreateICmpNE(isClosureResult, builder->getInt32(0), "is_closure_magic");

				llvm::BasicBlock* doClosureCleanup =
						llvm::BasicBlock::Create(*context, varName + "_do_closure_cleanup", currentFn);
				llvm::BasicBlock* notClosure =
						llvm::BasicBlock::Create(*context, varName + "_not_closure", currentFn);
				builder->CreateCondBr(isMagicValid, doClosureCleanup, notClosure);

				// Not actually a closure (variable was reassigned) - do generic ptr release
				builder->SetInsertPoint(notClosure);
				builder->CreateCall(qdPtrReleaseFn, {closurePtr});
				builder->CreateBr(skipFreeBlock);

				// Confirmed closure - do full cleanup
				builder->SetInsertPoint(doClosureCleanup);

				// Load environment pointer
				llvm::Value* envPtrSlot = builder->CreateStructGEP(closureStructTy, closurePtr, 2, "env_slot");
				llvm::Value* envPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), envPtrSlot, "env_ptr");

				// Load capture count
				llvm::Value* capCountSlot = builder->CreateStructGEP(closureStructTy, closurePtr, 3, "cap_count_slot");
				llvm::Value* capCount = builder->CreateLoad(builder->getInt64Ty(), capCountSlot, "cap_count");

				// Loop through captured variables and decrement refcounts
				// Create loop blocks
				llvm::BasicBlock* loopHeader =
						llvm::BasicBlock::Create(*context, varName + "_cap_loop_header", currentFn);
				llvm::BasicBlock* loopBody =
						llvm::BasicBlock::Create(*context, varName + "_cap_loop_body", currentFn);
				llvm::BasicBlock* afterLoop =
						llvm::BasicBlock::Create(*context, varName + "_cap_loop_done", currentFn);

				// Loop counter
				llvm::AllocaInst* loopIdxAlloca =
						builder->CreateAlloca(builder->getInt64Ty(), nullptr, varName + "_cap_idx");
				builder->CreateStore(builder->getInt64(0), loopIdxAlloca);
				builder->CreateBr(loopHeader);

				// Loop header - check if idx < capCount
				builder->SetInsertPoint(loopHeader);
				llvm::Value* loopIdx = builder->CreateLoad(builder->getInt64Ty(), loopIdxAlloca, "idx");
				llvm::Value* loopCond = builder->CreateICmpSLT(loopIdx, capCount, "loop_cond");
				builder->CreateCondBr(loopCond, loopBody, afterLoop);

				// Loop body - decrement refcount for this capture
				builder->SetInsertPoint(loopBody);

				// Get pointer to captured variable from environment
				llvm::Value* capSlot = builder->CreateGEP(
						llvm::PointerType::getUnqual(*context), envPtr, loopIdx, varName + "_cap_slot");
				llvm::Value* capVarPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), capSlot, "cap_var_ptr");

				// The captured variable pointer points to the qd_stack_element_t at offset 8
				// Refcount is at offset 0 (8 bytes before the elem)
				llvm::Value* refCountPtr = builder->CreateGEP(
						builder->getInt8Ty(), capVarPtr, builder->getInt64(static_cast<uint64_t>(-8)), varName + "_refcount_ptr");
				llvm::Value* refCount = builder->CreateLoad(builder->getInt64Ty(), refCountPtr, "refcount");

				// Decrement refcount
				llvm::Value* newRefCount = builder->CreateSub(refCount, builder->getInt64(1), "new_refcount");
				builder->CreateStore(newRefCount, refCountPtr);

				// If refcount == 0, free the heap block
				llvm::BasicBlock* freeCapBlock =
						llvm::BasicBlock::Create(*context, varName + "_free_cap", currentFn);
				llvm::BasicBlock* capContinue =
						llvm::BasicBlock::Create(*context, varName + "_cap_continue", currentFn);
				llvm::Value* shouldFree = builder->CreateICmpEQ(newRefCount, builder->getInt64(0), "should_free");
				builder->CreateCondBr(shouldFree, freeCapBlock, capContinue);

				// Free the captured variable's heap block
				builder->SetInsertPoint(freeCapBlock);
				// Before freeing, release the value inside if it's a string or pointer
				// capVarPtr points to qd_stack_element_t; type is at offset 8 within it
				llvm::Value* loopCapTypePtr = builder->CreateGEP(
						builder->getInt8Ty(), capVarPtr, builder->getInt64(8), varName + "_loop_cap_type_ptr");
				llvm::Value* loopCapType = builder->CreateLoad(builder->getInt32Ty(), loopCapTypePtr, "loop_cap_type");

				// Create blocks for type-specific cleanup
				llvm::BasicBlock* loopCapIsStr = llvm::BasicBlock::Create(*context, varName + "_loop_cap_is_str", currentFn);
				llvm::BasicBlock* loopCapCheckPtr = llvm::BasicBlock::Create(*context, varName + "_loop_cap_check_ptr", currentFn);
				llvm::BasicBlock* loopCapIsPtr = llvm::BasicBlock::Create(*context, varName + "_loop_cap_is_ptr", currentFn);
				llvm::BasicBlock* loopCapDoFree = llvm::BasicBlock::Create(*context, varName + "_loop_cap_do_free", currentFn);

				// Check if type == 3 (string)
				llvm::Value* loopCapIsStrCond = builder->CreateICmpEQ(loopCapType, builder->getInt32(3), "loop_cap_is_str");
				builder->CreateCondBr(loopCapIsStrCond, loopCapIsStr, loopCapCheckPtr);

				// Release string
				builder->SetInsertPoint(loopCapIsStr);
				llvm::Value* loopCapStrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), capVarPtr, "loop_cap_str");
				if (!this->qdStringReleaseFn) {
					auto qdStringReleaseFnTy =
							llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					this->qdStringReleaseFn = llvm::Function::Create(
							qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
				}
				builder->CreateCall(this->qdStringReleaseFn, {loopCapStrVal});
				builder->CreateBr(loopCapDoFree);

				// Check if type == 2 (pointer - could be struct, array, or closure)
				builder->SetInsertPoint(loopCapCheckPtr);
				llvm::Value* loopCapIsPtrCond = builder->CreateICmpEQ(loopCapType, builder->getInt32(2), "loop_cap_is_ptr");
				builder->CreateCondBr(loopCapIsPtrCond, loopCapIsPtr, loopCapDoFree);

				// Release pointer (works for structs and arrays)
				builder->SetInsertPoint(loopCapIsPtr);
				llvm::Value* loopCapPtrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), capVarPtr, "loop_cap_ptr");
				builder->CreateCall(qdPtrReleaseFn, {loopCapPtrVal});
				builder->CreateBr(loopCapDoFree);

				// Free the heap block
				builder->SetInsertPoint(loopCapDoFree);
				llvm::Function* localFreeFn = module->getFunction("free");
				if (!localFreeFn) {
					auto freeFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					localFreeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
				}
				// Free starting at refcount (the actual malloc block)
				builder->CreateCall(localFreeFn, {refCountPtr});
				builder->CreateBr(capContinue);

				// Continue to next capture
				builder->SetInsertPoint(capContinue);
				llvm::Value* nextIdx = builder->CreateAdd(loopIdx, builder->getInt64(1), "next_idx");
				builder->CreateStore(nextIdx, loopIdxAlloca);
				builder->CreateBr(loopHeader);

				// After loop - free environment and closure struct
				builder->SetInsertPoint(afterLoop);
				// Unregister closure from registry before freeing
				auto closureUnregisterFnTy =
						llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
				auto closureUnregisterFn = module->getOrInsertFunction("qd_closure_unregister", closureUnregisterFnTy);
				builder->CreateCall(closureUnregisterFn, {closurePtr});

				llvm::Function* closureFreeFn = module->getFunction("free");
				if (!closureFreeFn) {
					auto freeFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					closureFreeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
				}
				builder->CreateCall(closureFreeFn, {envPtr});
				builder->CreateCall(closureFreeFn, {closurePtr});
				builder->CreateBr(skipFreeBlock);
			} else if (isArray) {
				// Array variable - call qd_array_release
				llvm::BasicBlock* freePtrBlock = llvm::BasicBlock::Create(*context, varName + "_free_ptr", currentFn);
				llvm::Value* isPtr = builder->CreateICmpEQ(type, builder->getInt32(2), varName + "_is_ptr");
				builder->CreateCondBr(isPtr, freePtrBlock, skipFreeBlock);

				// Free array block
				builder->SetInsertPoint(freePtrBlock);
				llvm::Value* ptrValuePtr =
						builder->CreateStructGEP(stackElementTy, localAlloca, 0, varName + "_cleanup_ptr_value_ptr");
				llvm::Value* arrPtr = builder->CreateLoad(
						llvm::PointerType::getUnqual(*context), ptrValuePtr, varName + "_cleanup_arr");

				// Call qd_array_release() on the array
				llvm::Function* arrayReleaseFn = module->getFunction("qd_array_release");
				if (!arrayReleaseFn) {
					auto arrayReleaseFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					arrayReleaseFn = llvm::Function::Create(
							arrayReleaseFnTy, llvm::Function::ExternalLinkage, "qd_array_release", *module);
				}
				builder->CreateCall(arrayReleaseFn, {arrPtr});
				builder->CreateBr(skipFreeBlock);
			} else {
				// Non-array, non-closure pointer - could be a struct pointer
				// Call qd_ptr_release - works for both arrays and structs
				llvm::BasicBlock* freePtrBlock = llvm::BasicBlock::Create(*context, varName + "_free_ptr", currentFn);
				llvm::Value* isPtr = builder->CreateICmpEQ(type, builder->getInt32(2), varName + "_is_ptr");
				builder->CreateCondBr(isPtr, freePtrBlock, skipFreeBlock);

				builder->SetInsertPoint(freePtrBlock);
				llvm::Value* ptrValuePtr =
						builder->CreateStructGEP(stackElementTy, localAlloca, 0, varName + "_cleanup_ptr_value_ptr");
				llvm::Value* ptrVal = builder->CreateLoad(
						llvm::PointerType::getUnqual(*context), ptrValuePtr, varName + "_cleanup_ptr");
				builder->CreateCall(qdPtrReleaseFn, {ptrVal});
				builder->CreateBr(skipFreeBlock);
			}

			// Skip free block
			builder->SetInsertPoint(skipFreeBlock);
		}
	}

	void LlvmGenerator::Impl::collectAllCapturesFromAST(IAstNode* node, std::set<std::string>& captures) {
		// Recursively scan AST to find all variables captured by any anonymous functions
		// This is used to identify which variables need heap allocation for escaped closures
		if (!node) {
			return;
		}

		// If this is an anonymous function, add its captured variables
		if (node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
			AstNodeAnonymousFunction* anonFunc = static_cast<AstNodeAnonymousFunction*>(node);
			const auto& capturedVars = anonFunc->capturedVariables();
			for (const std::string& varName : capturedVars) {
				captures.insert(varName);
			}
			// Also scan the body of the anonymous function for nested closures
			if (anonFunc->body()) {
				collectAllCapturesFromAST(anonFunc->body(), captures);
			}
			return;
		}

		// Recursively scan all children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectAllCapturesFromAST(node->child(i), captures);
		}
	}

	void LlvmGenerator::Impl::generateCastInstructions(const std::vector<CastDirection>& casts, llvm::Value* ctx) {
		// Generate cast instructions for parameters that need type conversion
		// Casts are indexed from bottom of stack (first parameter = index 0)
		// We need to apply casts in reverse order since the stack grows upward

		for (size_t i = 0; i < casts.size(); i++) {
			if (casts[i] == CastDirection::NONE) {
				continue;
			}

			// Calculate how deep in the stack this parameter is
			// Parameter 0 is at depth (casts.size() - 1)
			// Parameter 1 is at depth (casts.size() - 2), etc.
			size_t depth = casts.size() - 1 - i;

			// We need to:
			// 1. Rotate the value to the top of the stack (if not already there)
			// 2. Apply the cast
			// 3. Rotate it back (if needed)

			// For now, use a simpler approach: pop all values, cast the one we need, push them back
			// This is less efficient but correct

			// Actually, the easiest approach is to use qd_pick to duplicate the value at depth,
			// cast it, then use qd_put to replace the original
			// But Quadrate doesn't have those operations in the standard set

			// Simplest working approach: generate the cast operation at the right position
			// The stack-based casts work on the top elements in order
			// Since parameters are pushed left-to-right, and we check them left-to-right,
			// we can apply casts in the order they appear

			// Actually, re-reading the semantic validator code:
			// Parameters are indexed from the bottom of the required values
			// So param 0 is deepest, param N-1 is on top
			// We need to cast them before calling the function

			// Use stack manipulation: for each parameter that needs casting from bottom:
			// - Calculate its position from top (casts.size() - i)
			// - Use qd_pick to get it, cast it, use qd_poke to put it back

			// For MVP, let's use a simpler but less efficient approach:
			// Save all parameters, cast the ones that need it, restore them

			std::string castFnName;
			if (casts[i] == CastDirection::INT_TO_FLOAT) {
				castFnName = "qd_castf";
			} else if (casts[i] == CastDirection::FLOAT_TO_INT) {
				castFnName = "qd_casti";
			} else {
				continue;
			}

			// For depth 0 (top of stack), just cast directly
			// For depth > 0, we need to use pick/poke or rotate operations

			if (depth == 0) {
				// Value is on top, cast it directly
				llvm::Function* castFn = module->getFunction(castFnName);
				if (!castFn) {
					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					castFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, castFnName, *module);
				}
				builder->CreateCall(castFn, {ctx});
			} else {
				// Value is at depth, need to manipulate stack
				// Use pattern: rot (depth times) -> cast -> rot (casts.size() - depth times)
				// Actually, use qd_over and qd_swap to avoid complex rotations

				// Simplified: use qd_stackops
				// pick depth -> cast -> stack_size -> depth - 1 -> poke

				// Let's just use the rot instruction repeatedly for now
				llvm::Function* swapFn = module->getFunction("qd_swap");
				if (!swapFn) {
					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					swapFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_swap", *module);
				}

				// Rotate value to top: swap depth times
				for (size_t j = 0; j < depth; j++) {
					builder->CreateCall(swapFn, {ctx});
				}

				// Cast it
				llvm::Function* castFn = module->getFunction(castFnName);
				if (!castFn) {
					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					castFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, castFnName, *module);
				}
				builder->CreateCall(castFn, {ctx});

				// Rotate back: swap depth times
				for (size_t j = 0; j < depth; j++) {
					builder->CreateCall(swapFn, {ctx});
				}
			}
		}
	}

	void LlvmGenerator::Impl::generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Create basic blocks
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "if.underflow", currentFn);
		llvm::BasicBlock* popBB = llvm::BasicBlock::Create(*context, "if.pop", currentFn);
		llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "if.then", currentFn);
		llvm::BasicBlock* elseBB =
				ifStmt->elseBody() ? llvm::BasicBlock::Create(*context, "if.else", currentFn) : nullptr;
		llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "if.merge", currentFn);

		// Inline pop for condition - direct stack access for performance
		// ctx is a pointer to a struct with first field being qd_stack* st
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		// Stack type: { data*, capacity, size }
		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Check for stack underflow before popping
		llvm::Value* isEmpty = builder->CreateICmpEQ(size, builder->getInt64(0), "is_empty");
		builder->CreateCondBr(isEmpty, underflowBB, popBB);

		// Generate underflow error block
		builder->SetInsertPoint(underflowBB);
		// Call fprintf(stderr, ...) and abort
		auto fprintfFn = module->getOrInsertFunction("fprintf",
				llvm::FunctionType::get(builder->getInt32Ty(),
						{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg =
				builder->CreateGlobalString("Fatal error in if: Stack underflow (requires 1 value for condition)\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		// Call qd_print_stack_trace(ctx)
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Continue with normal pop in popBB
		builder->SetInsertPoint(popBB);

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Access top element value directly: data[size-1].value
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value64 = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "value64");

		// Decrement size (inline pop)
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		// Convert to condition value
		auto condValue = builder->CreateTrunc(value64, builder->getInt32Ty(), "cond");

		// Check if condition is non-zero
		auto isTrue = builder->CreateICmpNE(condValue, builder->getInt32(0), "is_true");

		// Branch based on condition
		if (elseBB) {
			builder->CreateCondBr(isTrue, thenBB, elseBB);
		} else {
			builder->CreateCondBr(isTrue, thenBB, mergeBB);
		}

		// Generate then block
		builder->SetInsertPoint(thenBB);
		if (ifStmt->thenBody()) {
			generateNode(ifStmt->thenBody(), ctx);
		}
		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* thenBlock = builder->GetInsertBlock();
		if (thenBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
			if (!thenBlock->getTerminator()) {
#pragma GCC diagnostic pop
				builder->CreateBr(mergeBB);
			}
		}

		// Generate else block if present
		if (elseBB) {
			builder->SetInsertPoint(elseBB);
			if (ifStmt->elseBody()) {
				generateNode(ifStmt->elseBody(), ctx);
			}
			// Only add branch if block doesn't already have a terminator
			llvm::BasicBlock* elseBlock = builder->GetInsertBlock();
			if (elseBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
				if (!elseBlock->getTerminator()) {
#pragma GCC diagnostic pop
					builder->CreateBr(mergeBB);
				}
			}
		}

		// Continue in merge block
		builder->SetInsertPoint(mergeBB);
	}

	void LlvmGenerator::Impl::generateFor(AstNodeForStatement* forStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Pop start, end, step from stack (in reverse order: step, end, start)
		auto stackFieldPtr =
				builder->CreateStructGEP(llvm::StructType::get(*context,
												 {
														 llvm::PointerType::getUnqual(*context), // qd_stack* st
														 builder->getInt64Ty(),					 // int64_t error_code
														 llvm::PointerType::getUnqual(*context), // char* error_msg
														 builder->getInt32Ty(),					 // int argc
														 llvm::PointerType::getUnqual(*context), // char** argv
														 llvm::PointerType::getUnqual(*context)	 // char* program_name
												 }),
						ctx, 0, "st_ptr");
		auto stack = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackFieldPtr, "st");

		// Create allocas in entry block to avoid stack growth in nested loops
		llvm::BasicBlock& entryBlock = currentFn->getEntryBlock();
		llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.getFirstInsertionPt());
		auto stepElemPtr = entryBuilder.CreateAlloca(stackElementTy, nullptr, "step_elem");
		auto endElemPtr = entryBuilder.CreateAlloca(stackElementTy, nullptr, "end_elem");
		auto startElemPtr = entryBuilder.CreateAlloca(stackElementTy, nullptr, "start_elem");

		builder->CreateCall(stackPopFn, {stack, stepElemPtr});
		builder->CreateCall(stackPopFn, {stack, endElemPtr});
		builder->CreateCall(stackPopFn, {stack, startElemPtr});

		// Extract values (stackElementTy layout: { i64 value, i32 type, i1 is_error_tainted })
		// The i64 field is a union that holds either int or float bits
		// Type field: 0=INT, 1=FLOAT, 2=PTR, 3=STR

		// Check the type of start element to determine if we're using int or float loop
		auto startTypePtr = builder->CreateStructGEP(stackElementTy, startElemPtr, 1, "start_type_ptr");
		auto startType = builder->CreateLoad(builder->getInt32Ty(), startTypePtr, "start_type");
		auto isFloatLoop = builder->CreateICmpEQ(startType, builder->getInt32(1), "is_float_loop");

		// Extract start value
		auto startValuePtr = builder->CreateStructGEP(stackElementTy, startElemPtr, 0, "start_value_ptr");
		auto startBits = builder->CreateLoad(builder->getInt64Ty(), startValuePtr, "start_bits");

		// Convert start based on type
		auto startAsFloat = builder->CreateBitCast(startBits, builder->getDoubleTy(), "start_as_float");
		auto startFloatToInt = builder->CreateFPToSI(startAsFloat, builder->getInt64Ty(), "start_float_to_int");
		auto startValue = builder->CreateSelect(isFloatLoop, startFloatToInt, startBits, "start");

		// Extract end value
		auto endValuePtr = builder->CreateStructGEP(stackElementTy, endElemPtr, 0, "end_value_ptr");
		auto endBits = builder->CreateLoad(builder->getInt64Ty(), endValuePtr, "end_bits");

		auto endAsFloat = builder->CreateBitCast(endBits, builder->getDoubleTy(), "end_as_float");
		auto endFloatToInt = builder->CreateFPToSI(endAsFloat, builder->getInt64Ty(), "end_float_to_int");
		auto endValue = builder->CreateSelect(isFloatLoop, endFloatToInt, endBits, "end");

		// Extract step value
		auto stepValuePtr = builder->CreateStructGEP(stackElementTy, stepElemPtr, 0, "step_value_ptr");
		auto stepBits = builder->CreateLoad(builder->getInt64Ty(), stepValuePtr, "step_bits");

		auto stepAsFloat = builder->CreateBitCast(stepBits, builder->getDoubleTy(), "step_as_float");
		auto stepFloatToInt = builder->CreateFPToSI(stepAsFloat, builder->getInt64Ty(), "step_float_to_int");
		auto stepValue = builder->CreateSelect(isFloatLoop, stepFloatToInt, stepBits, "step");

		// Create basic blocks
		llvm::BasicBlock* loopHeaderBB = llvm::BasicBlock::Create(*context, "for.header", currentFn);
		llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "for.body", currentFn);
		llvm::BasicBlock* loopIncBB = llvm::BasicBlock::Create(*context, "for.inc", currentFn);
		llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "for.exit", currentFn);

		// Remember the predecessor block for PHI node
		llvm::BasicBlock* preBB = builder->GetInsertBlock();

		// Jump to loop header
		builder->CreateBr(loopHeaderBB);

		// Loop header: check condition
		builder->SetInsertPoint(loopHeaderBB);
		llvm::PHINode* iterVar = builder->CreatePHI(builder->getInt64Ty(), 2, "i");
		iterVar->addIncoming(startValue, preBB);

		// Check if step is negative to determine comparison direction
		auto stepIsNegative = builder->CreateICmpSLT(stepValue, builder->getInt64(0), "step_neg");
		auto condPositive = builder->CreateICmpSLT(iterVar, endValue, "cmp_pos");
		auto condNegative = builder->CreateICmpSGT(iterVar, endValue, "cmp_neg");
		auto cond = builder->CreateSelect(stepIsNegative, condNegative, condPositive, "cmp");

		// Add branch weights: loop body is likely (1000), exit is unlikely (1)
		llvm::MDBuilder mdBuilder(*context);
		llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
		auto* br = builder->CreateCondBr(cond, loopBodyBB, loopExitBB);
		br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

		// Loop body
		builder->SetInsertPoint(loopBodyBB);

		// Push loop context for break/continue
		loopStack.push_back({loopExitBB, loopIncBB});

		// Push defer scope for this loop - defers will be generated at end of iteration
		pushDeferScope();

		// Register the iterator variable with its name
		const std::string& iterName = forStmt->iteratorName();
		llvm::Value* prevIterVar = nullptr;
		auto prevIt = iteratorVars.find(iterName);
		if (prevIt != iteratorVars.end()) {
			prevIterVar = prevIt->second; // Save previous value for restoration
		}
		iteratorVars[iterName] = iterVar;

		if (forStmt->body()) {
			generateNode(forStmt->body(), ctx);
		}

		// Restore previous iterator value (for nested loops with same name)
		if (prevIterVar) {
			iteratorVars[iterName] = prevIterVar;
		} else {
			iteratorVars.erase(iterName);
		}

		// Generate defer execution code at end of loop body
		// This code will execute at runtime for each iteration
		if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
			auto& currentScope = deferScopeStack.back();
			// Generate IR to execute defers in REVERSE order (LIFO)
			for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				// Generate defer body inline
				for (size_t i = 0; i < deferNode->childCount(); i++) {
					IAstNode* child = deferNode->child(i);
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (size_t j = 0; j < child->childCount(); j++) {
							generateNode(child->child(j), ctx);
						}
					} else {
						generateNode(child, ctx);
					}
				}
			}
		}

		// Pop defer scope (compilation-time cleanup)
		popDeferScope();

		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* loopBodyBlock = builder->GetInsertBlock();
		if (loopBodyBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
			if (!loopBodyBlock->getTerminator()) {
#pragma GCC diagnostic pop
				builder->CreateBr(loopIncBB);
			}
		}

		loopStack.pop_back();

		// Loop increment (nsw enables loop optimizations like strength reduction)
		builder->SetInsertPoint(loopIncBB);
		auto nextIter = builder->CreateNSWAdd(iterVar, stepValue, "next_i");
		iterVar->addIncoming(nextIter, loopIncBB);
		builder->CreateBr(loopHeaderBB);

		// Continue after loop
		builder->SetInsertPoint(loopExitBB);
	}

	void LlvmGenerator::Impl::generateWhile(AstNodeWhileStatement* whileStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Create basic blocks
		llvm::BasicBlock* whileCondBB = llvm::BasicBlock::Create(*context, "while.cond", currentFn);
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "while.underflow", currentFn);
		llvm::BasicBlock* popBB = llvm::BasicBlock::Create(*context, "while.pop", currentFn);
		llvm::BasicBlock* whileBodyBB = llvm::BasicBlock::Create(*context, "while.body", currentFn);
		llvm::BasicBlock* whileExitBB = llvm::BasicBlock::Create(*context, "while.exit", currentFn);

		// Jump to condition check
		builder->CreateBr(whileCondBB);

		// Condition block - pop value and check
		builder->SetInsertPoint(whileCondBB);

		// Inline pop for condition - direct stack access for performance
		// ctx is a pointer to a struct with first field being qd_stack* st
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		// Stack type: { data*, capacity, size }
		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Check for stack underflow before popping
		llvm::Value* isEmpty = builder->CreateICmpEQ(size, builder->getInt64(0), "is_empty");
		builder->CreateCondBr(isEmpty, underflowBB, popBB);

		// Generate underflow error block
		builder->SetInsertPoint(underflowBB);
		// Call fprintf(stderr, ...) and abort
		auto fprintfFn = module->getOrInsertFunction("fprintf",
				llvm::FunctionType::get(builder->getInt32Ty(),
						{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg =
				builder->CreateGlobalString("Fatal error in while: Stack underflow (requires 1 value for condition)\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		// Call qd_print_stack_trace(ctx)
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Continue with normal pop in popBB
		builder->SetInsertPoint(popBB);

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Access top element value directly: data[size-1].value
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value64 = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "value64");

		// Decrement size (inline pop)
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		// Convert to condition value
		auto condValue = builder->CreateTrunc(value64, builder->getInt32Ty(), "cond");

		// Check if condition is non-zero
		auto isTrue = builder->CreateICmpNE(condValue, builder->getInt32(0), "is_true");

		// Branch based on condition with weights (loop body is likely)
		llvm::MDBuilder mdBuilder(*context);
		llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
		auto* br = builder->CreateCondBr(isTrue, whileBodyBB, whileExitBB);
		br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

		// While body
		builder->SetInsertPoint(whileBodyBB);

		// Push loop context for break/continue
		// break jumps to whileExitBB, continue jumps back to whileCondBB
		loopStack.push_back({whileExitBB, whileCondBB});

		// Push defer scope for this loop iteration
		pushDeferScope();

		if (whileStmt->body()) {
			generateNode(whileStmt->body(), ctx);
		}

		// Generate defer execution code at end of loop body
		if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
			auto& currentScope = deferScopeStack.back();
			for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				for (size_t i = 0; i < deferNode->childCount(); i++) {
					IAstNode* child = deferNode->child(i);
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (size_t j = 0; j < child->childCount(); j++) {
							generateNode(child->child(j), ctx);
						}
					} else {
						generateNode(child, ctx);
					}
				}
			}
		}

		// Pop defer scope (compilation-time cleanup)
		popDeferScope();

		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* whileBlock = builder->GetInsertBlock();
		if (whileBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
			if (!whileBlock->getTerminator()) {
#pragma GCC diagnostic pop
				builder->CreateBr(whileCondBB); // Loop back to condition
			}
		}

		loopStack.pop_back();

		// Continue after loop (reached when condition is false or via break)
		builder->SetInsertPoint(whileExitBB);
	}

	void LlvmGenerator::Impl::generateLoop(AstNodeLoopStatement* loopStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Create basic blocks
		llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "loop.body", currentFn);
		llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "loop.exit", currentFn);

		// Jump to loop body
		builder->CreateBr(loopBodyBB);

		// Loop body
		builder->SetInsertPoint(loopBodyBB);

		// Push loop context for break/continue
		loopStack.push_back({loopExitBB, loopBodyBB});

		// Push defer scope for this loop iteration
		pushDeferScope();

		if (loopStmt->body()) {
			generateNode(loopStmt->body(), ctx);
		}

		// Generate defer execution code at end of loop body
		if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
			auto& currentScope = deferScopeStack.back();
			for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				for (size_t i = 0; i < deferNode->childCount(); i++) {
					IAstNode* child = deferNode->child(i);
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (size_t j = 0; j < child->childCount(); j++) {
							generateNode(child->child(j), ctx);
						}
					} else {
						generateNode(child, ctx);
					}
				}
			}
		}

		// Pop defer scope (compilation-time cleanup)
		popDeferScope();

		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* loopBlock = builder->GetInsertBlock();
		if (loopBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
			if (!loopBlock->getTerminator()) {
#pragma GCC diagnostic pop
				builder->CreateBr(loopBodyBB); // Loop forever
			}
		}

		loopStack.pop_back();

		// Continue after loop (only reached via break)
		builder->SetInsertPoint(loopExitBB);
	}

	void LlvmGenerator::Impl::generateCtxBlock(AstNodeCtx* ctxNode, llvm::Value* ctx) {
		// Clone the parent context
		auto clonedCtx = builder->CreateCall(cloneContextFn, {ctx}, "cloned_ctx");

		// Execute the block with the cloned context
		for (size_t i = 0; i < ctxNode->childCount(); i++) {
			generateNode(ctxNode->child(i), clonedCtx);
		}

		// Get the stack from cloned context
		auto stackFieldPtr =
				builder->CreateStructGEP(llvm::StructType::get(*context,
												 {
														 llvm::PointerType::getUnqual(*context), // qd_stack* st
														 builder->getInt64Ty(),					 // int64_t error_code
														 llvm::PointerType::getUnqual(*context), // char* error_msg
														 builder->getInt32Ty(),					 // int argc
														 llvm::PointerType::getUnqual(*context), // char** argv
														 llvm::PointerType::getUnqual(*context)	 // char* program_name
												 }),
						clonedCtx, 0, "cloned_st_ptr");
		auto clonedStack = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackFieldPtr, "cloned_st");

		// Pop exactly one value from the cloned stack
		auto resultElemPtr = builder->CreateAlloca(stackElementTy, nullptr, "ctx_result_elem");
		builder->CreateCall(stackPopFn, {clonedStack, resultElemPtr});

		// Get the result value and type
		auto resultValuePtr = builder->CreateStructGEP(stackElementTy, resultElemPtr, 0, "result_value_ptr");
		auto resultValue = builder->CreateLoad(builder->getInt64Ty(), resultValuePtr, "result_value");
		auto resultTypePtr = builder->CreateStructGEP(stackElementTy, resultElemPtr, 1, "result_type_ptr");
		auto resultType = builder->CreateLoad(builder->getInt32Ty(), resultTypePtr, "result_type");

		// Push the result to the parent context based on its type BEFORE freeing cloned context
		// This is critical for strings: qd_push_s will duplicate the string, so we need
		// the original string to still be valid when we call it
		// Type field: 0=INT, 1=FLOAT, 2=PTR, 3=STR
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* pushIntBB = llvm::BasicBlock::Create(*context, "ctx.push_int", currentFn);
		llvm::BasicBlock* pushFloatBB = llvm::BasicBlock::Create(*context, "ctx.push_float", currentFn);
		llvm::BasicBlock* pushPtrBB = llvm::BasicBlock::Create(*context, "ctx.push_ptr", currentFn);
		llvm::BasicBlock* pushStrBB = llvm::BasicBlock::Create(*context, "ctx.push_str", currentFn);
		llvm::BasicBlock* pushDoneBB = llvm::BasicBlock::Create(*context, "ctx.push_done", currentFn);

		// Switch on type
		auto switchInst = builder->CreateSwitch(resultType, pushDoneBB, 4);
		switchInst->addCase(builder->getInt32(0), pushIntBB);	// INT
		switchInst->addCase(builder->getInt32(1), pushFloatBB); // FLOAT
		switchInst->addCase(builder->getInt32(2), pushPtrBB);	// PTR
		switchInst->addCase(builder->getInt32(3), pushStrBB);	// STR

		// Push INT
		builder->SetInsertPoint(pushIntBB);
		builder->CreateCall(pushIntFn, {ctx, resultValue});
		builder->CreateBr(pushDoneBB);

		// Push FLOAT
		builder->SetInsertPoint(pushFloatBB);
		auto floatValue = builder->CreateBitCast(resultValue, builder->getDoubleTy(), "float_value");
		builder->CreateCall(pushFloatFn, {ctx, floatValue});
		builder->CreateBr(pushDoneBB);

		// Push PTR
		builder->SetInsertPoint(pushPtrBB);
		auto ptrValue = builder->CreateIntToPtr(resultValue, llvm::PointerType::getUnqual(*context), "ptr_value");
		builder->CreateCall(pushPtrFn, {ctx, ptrValue});
		builder->CreateBr(pushDoneBB);

		// Push STR
		builder->SetInsertPoint(pushStrBB);
		auto strPtr = builder->CreateIntToPtr(resultValue, llvm::PointerType::getUnqual(*context), "str_ptr");
		// Call qd_string_data to get const char* from qd_string_t*
		if (!this->qdStringDataFn) {
			auto qdStringDataFnTy = llvm::FunctionType::get(
					llvm::PointerType::getUnqual(*context), {llvm::PointerType::getUnqual(*context)}, false);
			this->qdStringDataFn = llvm::Function::Create(
					qdStringDataFnTy, llvm::Function::ExternalLinkage, "qd_string_data", *module);
		}
		auto strData = builder->CreateCall(this->qdStringDataFn, {strPtr}, "str_data");
		builder->CreateCall(pushStrFn, {ctx, strData});
		// Release the string reference from cloned context (qd_push_s has created a new copy)
		if (!this->qdStringReleaseFn) {
			auto qdStringReleaseFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			this->qdStringReleaseFn = llvm::Function::Create(
					qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
		}
		builder->CreateCall(this->qdStringReleaseFn, {strPtr});
		builder->CreateBr(pushDoneBB);

		// Free the cloned context AFTER pushing (qd_push_s has now duplicated the string)
		builder->SetInsertPoint(pushDoneBB);
		builder->CreateCall(freeContextFn, {clonedCtx});

		// Continue after push
		builder->SetInsertPoint(pushDoneBB);
	}

	void LlvmGenerator::Impl::generateNode(IAstNode* node, llvm::Value* ctx) {
		if (!node) {
			return;
		}

		// Set debug location for this node
		if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty()) {
			unsigned line = static_cast<unsigned>(node->line());
			unsigned column = static_cast<unsigned>(node->column());
			if (line > 0) {
				auto debugLoc = llvm::DILocation::get(*context, line, column, debugScopeStack.back());
				builder->SetCurrentDebugLocation(debugLoc);
			}
		}

		auto nodeType = node->type();

		switch (nodeType) {
		case IAstNode::Type::LITERAL:
			generateLiteral(static_cast<AstNodeLiteral*>(node), ctx);
			break;
		case IAstNode::Type::INSTRUCTION:
			generateInstruction(static_cast<AstNodeInstruction*>(node), ctx);
			break;
		case IAstNode::Type::LOCAL:
			generateLocal(static_cast<AstNodeLocal*>(node), ctx);
			break;
		case IAstNode::Type::IF_STATEMENT:
			generateIf(static_cast<AstNodeIfStatement*>(node), ctx);
			break;
		case IAstNode::Type::FOR_STATEMENT:
			generateFor(static_cast<AstNodeForStatement*>(node), ctx);
			break;
		case IAstNode::Type::WHILE_STATEMENT:
			generateWhile(static_cast<AstNodeWhileStatement*>(node), ctx);
			break;
		case IAstNode::Type::LOOP_STATEMENT:
			generateLoop(static_cast<AstNodeLoopStatement*>(node), ctx);
			break;
		case IAstNode::Type::SWITCH_STATEMENT:
			generateSwitchStatement(static_cast<AstNodeSwitchStatement*>(node), ctx);
			break;
		case IAstNode::Type::BREAK_STATEMENT:
			// Execute defer scope before breaking from current loop
			if (!loopStack.empty()) {
				// Generate IR to execute defers before breaking
				if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
					auto& currentScope = deferScopeStack.back();
					for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
						AstNodeDefer* deferNode = *it;
						for (size_t i = 0; i < deferNode->childCount(); i++) {
							IAstNode* child = deferNode->child(i);
							if (child && child->type() == IAstNode::Type::BLOCK) {
								for (size_t j = 0; j < child->childCount(); j++) {
									generateNode(child->child(j), ctx);
								}
							} else {
								generateNode(child, ctx);
							}
						}
					}
				}
				builder->CreateBr(loopStack.back().breakTarget);
			}
			break;
		case IAstNode::Type::CONTINUE_STATEMENT:
			// Execute defer scope before continuing to next iteration
			if (!loopStack.empty()) {
				// Generate IR to execute defers before continuing
				if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
					auto& currentScope = deferScopeStack.back();
					for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
						AstNodeDefer* deferNode = *it;
						for (size_t i = 0; i < deferNode->childCount(); i++) {
							IAstNode* child = deferNode->child(i);
							if (child && child->type() == IAstNode::Type::BLOCK) {
								for (size_t j = 0; j < child->childCount(); j++) {
									generateNode(child->child(j), ctx);
								}
							} else {
								generateNode(child, ctx);
							}
						}
					}
				}
				builder->CreateBr(loopStack.back().continueTarget);
			}
			break;
		case IAstNode::Type::RETURN_STATEMENT:
			// Return from current function
			if (currentFunctionReturnBlock) {
				builder->CreateBr(currentFunctionReturnBlock);
			}
			break;
		case IAstNode::Type::DEFER_STATEMENT:
			// Collect defer statement for later execution at scope end
			if (deferScopeStack.empty()) {
				pushDeferScope();
			}
			deferScopeStack.back().push_back(static_cast<AstNodeDefer*>(node));
			// Don't generate code now - will be generated at scope end
			break;
		case IAstNode::Type::CTX_STATEMENT:
			generateCtxBlock(static_cast<AstNodeCtx*>(node), ctx);
			break;
		case IAstNode::Type::IDENTIFIER:
			generateIdentifier(static_cast<AstNodeIdentifier*>(node), ctx);
			break;
		case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
			generateFunctionPointer(static_cast<AstNodeFunctionPointerReference*>(node), ctx);
			break;
		case IAstNode::Type::ANONYMOUS_FUNCTION:
			generateAnonymousFunction(static_cast<AstNodeAnonymousFunction*>(node), ctx);
			break;
		case IAstNode::Type::BLOCK:
			// For blocks, just recursively generate all children
			for (size_t i = 0; i < node->childCount(); i++) {
				generateNode(node->child(i), ctx);
				// Stop if we've added a terminator (return, break, continue)
				llvm::BasicBlock* currentBlock = builder->GetInsertBlock();
				if (currentBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
					if (currentBlock->getTerminator()) {
#pragma GCC diagnostic pop
						break;
					}
				}
			}
			break;
		case IAstNode::Type::FUNCTION_DECLARATION:
			// Skip - functions are handled at the top level
			break;
		case IAstNode::Type::TEST_DECLARATION:
			// Skip - tests are handled at the top level
			break;
		case IAstNode::Type::SCOPED_IDENTIFIER:
			generateScopedIdentifier(static_cast<AstNodeScopedIdentifier*>(node), ctx);
			break;
		case IAstNode::Type::USE_STATEMENT:
			// Use statements are handled during program generation, not during execution
			break;
		case IAstNode::Type::FIELD_ACCESS:
			generateFieldAccess(static_cast<AstNodeFieldAccess*>(node), ctx);
			break;
		case IAstNode::Type::FIELD_SET:
			generateFieldSet(static_cast<AstNodeFieldSet*>(node), ctx);
			break;
		case IAstNode::Type::STRUCT_DECLARATION:
			// Struct declarations are processed during program generation, not during execution
			break;
		case IAstNode::Type::STRUCT_CONSTRUCTION: {
			// Struct construction with named fields: StructName { field1: expr1 field2: expr2 }
			AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(node);
			const std::string& structName = construct->structName();
			const auto& fieldInits = construct->fieldInits();

			// Get struct layout to know field order
			auto layoutIt = structDefinitions.find(structName);
			if (layoutIt == structDefinitions.end()) {
				std::cerr << "Error: Unknown struct type in construction: " << structName << std::endl;
				break;
			}

			const StructLayout& layout = layoutIt->second;

			// Build a map from field name to initializer
			std::unordered_map<std::string, const StructFieldInit*> initMap;
			for (const auto& init : fieldInits) {
				initMap[init.fieldName] = &init;
			}

			// Generate code for field initializers in struct definition order
			// (generateStructConstruction pops them in reverse order)
			for (const auto& field : layout.fields) {
				auto initIt = initMap.find(field.name);
				if (initIt != initMap.end()) {
					// Generate code for the field initializer expression
					for (IAstNode* valueNode : initIt->second->valueNodes) {
						generateNode(valueNode, ctx);
					}
				} else {
					// Missing field - should have been caught by semantic validator
					std::cerr << "Error: Missing field initializer for '" << field.name << "' in struct " << structName
							  << std::endl;
				}
			}

			// Now construct the struct (pops values from stack)
			generateStructConstruction(structName, ctx);
			break;
		}
		case IAstNode::Type::ARRAY_LITERAL:
			generateArrayLiteral(static_cast<AstNodeArrayLiteral*>(node), ctx);
			break;
		case IAstNode::Type::ARRAY_INDEX:
			// Array indexing is handled via 'nth' instruction
			break;
		default:
			// Ignore other node types for now
			break;
		}
	}

	bool LlvmGenerator::Impl::generateFunction(
			AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix) {
		// Clear local variables for this function
		localVariables.clear();
		localVariableStructTypes.clear();
		localArrayVariables.clear();
		lastStructConstructed.clear();
		lastFieldAccessResultType.clear();
		heapAllocatedCaptures.clear();
		heapCapturePointers.clear();
		indirectLocalVariables.clear();
		closureVariables.clear();

		// Check if function returns a pointer (structs must be heap-allocated in such functions)
		// Note: Quadrate allows implicit returns (values left on stack), so we check both:
		// Always use heap allocation for structs to ensure proper cleanup via destructors.
		// Stack allocation was tried but causes memory leaks because stack-allocated structs
		// aren't registered and qd_struct_release can't clean up their string fields.
		currentFunctionReturnsPtr = true;

		// Set current module prefix for intra-module function calls
		currentModulePrefix = namePrefix;

		// Get the correct DIFile for this module
		llvm::DIFile* funcDebugFile = debugFile; // Default to main file
		if (debugInfoEnabled && debugBuilder) {
			auto it = moduleDebugFiles.find(namePrefix);
			if (it != moduleDebugFiles.end()) {
				funcDebugFile = it->second;
			}
		}

		llvm::Function* fn = nullptr;

		if (isMain) {
			// Create main function: i32 @main(i32 %argc, i8** %argv)
			auto mainFnTy = llvm::FunctionType::get(
					builder->getInt32Ty(), {builder->getInt32Ty(), llvm::PointerType::getUnqual(*context)}, false);
			fn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);

			// Add debug info for main function
			if (debugInfoEnabled && debugBuilder) {
				auto funcType = debugBuilder->createSubroutineType(debugBuilder->getOrCreateTypeArray({}));
				auto subprogram = debugBuilder->createFunction(compileUnit, // Scope
						funcNode->name(),									// Name
						"main",												// Linkage name
						funcDebugFile,										// File
						static_cast<unsigned>(funcNode->line()),			// Line number
						funcType,											// Type
						static_cast<unsigned>(funcNode->line()),			// Scope line
						llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
				fn->setSubprogram(subprogram);
				debugScopeStack.push_back(subprogram);

				// Emit function start location
				auto loc = llvm::DILocation::get(*context, static_cast<unsigned>(funcNode->line()), 0, subprogram);
				builder->SetCurrentDebugLocation(loc);
			}

			// Create entry basic block
			auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
			builder->SetInsertPoint(entryBB);

			// Create Quadrate context
			auto stackSizeVal = builder->getInt64(stackSize);
			auto ctx = builder->CreateCall(createContextFn, {stackSizeVal}, "ctx");

			// Create alloca for ctx so debugger can reliably access it
			llvm::AllocaInst* ctxAlloca = builder->CreateAlloca(ctx->getType(), nullptr, "ctx.addr");
			builder->CreateStore(ctx, ctxAlloca);

			// Store argc and argv in the context
			// Context layout: {qd_stack*, int64_t error_code, char* error_msg, int argc, char** argv, ...}
			auto argcArg = fn->getArg(0); // i32 argc
			auto argvArg = fn->getArg(1); // i8** argv

			// Define the context struct type to access fields
			auto contextStructTy = llvm::StructType::get(*context,
					{llvm::PointerType::getUnqual(*context),		  // st
							builder->getInt64Ty(),					  // error_code
							llvm::PointerType::getUnqual(*context),	  // error_msg
							builder->getInt32Ty(),					  // argc
							llvm::PointerType::getUnqual(*context),	  // argv
							llvm::PointerType::getUnqual(*context)}); // program_name

			// Store argc (field index 3)
			auto argcPtr = builder->CreateStructGEP(contextStructTy, ctx, 3, "argc.ptr");
			builder->CreateStore(argcArg, argcPtr);

			// Store argv (field index 4)
			auto argvPtr = builder->CreateStructGEP(contextStructTy, ctx, 4, "argv.ptr");
			builder->CreateStore(argvArg, argvPtr);

			// Add debug info for ctx local variable in main
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && contextDebugType) {
				// contextDebugType is already qd_context* (pointer), so use it directly
				auto ctxPtrType = contextDebugType;

				// Create local variable for ctx
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (main function)
						"ctx",															 // Name
						funcDebugFile,													 // File
						static_cast<unsigned>(funcNode->line()),						 // Line
						ctxPtrType,														 // Type
						true															 // Always preserve
				);

				// Insert declare to make it visible in debugger (use alloca, not SSA value)
				debugBuilder->insertDeclare(ctxAlloca,	  // Storage (alloca, not SSA)
						localVar,						  // Variable
						debugBuilder->createExpression(), // Expression
						llvm::DILocation::get(
								*context, static_cast<unsigned>(funcNode->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}

			// Push "main::main" onto call stack for debugging
			std::string fullFuncName = namePrefix + "::" + funcNode->name();
			auto funcNameStr = builder->CreateGlobalString(fullFuncName);
			std::string sourceFile;
			auto srcIt = moduleSourceFiles.find(namePrefix);
			if (srcIt != moduleSourceFiles.end()) {
				sourceFile = srcIt->second;
			}
			auto sourceFileStr = builder->CreateGlobalString(sourceFile);
			auto lineNum = builder->getInt64(funcNode->line());
			builder->CreateCall(pushCallFn, {ctx, funcNameStr, sourceFileStr, lineNum});

			// Create return basic block for defer execution
			auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);

			// Initialize defer scope stack for this function
			deferScopeStack.clear();
			pushDeferScope();

			// Scan for captured variables to enable escaped closures
			// Variables captured by closures need heap allocation so they survive function return
			auto body = funcNode->body();
			if (body) {
				collectAllCapturesFromAST(body, heapAllocatedCaptures);
			}

			// Generate function body
			if (body) {
				generateNode(body, ctx);
			}

			// Branch to return block if no terminator
			if (!builder->GetInsertBlock()->getTerminator()) {
				builder->CreateBr(returnBB);
			}

			// Generate return block - this is where function-level defers execute
			builder->SetInsertPoint(returnBB);

			// Execute function-level defer scope
			executeDeferScope(ctx);

			// Clean up local variables (free strings)
			generateLocalCleanup();

			// Pop from call stack
			builder->CreateCall(popCallFn, {ctx});

			// Free context
			builder->CreateCall(freeContextFn, {ctx});

			// Return 0
			builder->CreateRet(builder->getInt32(0));

			// Pop debug scope for main function
			if (debugInfoEnabled && !debugScopeStack.empty()) {
				debugScopeStack.pop_back();
			}
		} else {
			// User-defined function: qd_exec_result usr_<prefix>_<name>(qd_context* ctx)
			std::string fnName = "usr_" + namePrefix + "_" + funcNode->name();
			// Check if function was already declared in pre-pass (for forward references)
			fn = module->getFunction(fnName);
			if (!fn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
			}

			// Add debug info for user function
			if (debugInfoEnabled && debugBuilder) {
				auto funcType = debugBuilder->createSubroutineType(debugBuilder->getOrCreateTypeArray({}));
				auto subprogram = debugBuilder->createFunction(compileUnit, // Scope
						funcNode->name(),									// Name
						fnName.c_str(),										// Linkage name
						funcDebugFile,										// File
						static_cast<unsigned>(funcNode->line()),			// Line number
						funcType,											// Type
						static_cast<unsigned>(funcNode->line()),			// Scope line
						llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
				fn->setSubprogram(subprogram);
				debugScopeStack.push_back(subprogram);

				// Emit function start location
				auto loc = llvm::DILocation::get(*context, static_cast<unsigned>(funcNode->line()), 0, subprogram);
				builder->SetCurrentDebugLocation(loc);
			}

			// Register the function with appropriate scope
			std::string registerName =
					(namePrefix == "main") ? funcNode->name() : (namePrefix + "::" + funcNode->name());
			userFunctions[registerName] = fn;
			fallibleFunctions[registerName] = funcNode->throws();

			// Create basic blocks
			auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
			auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);

			builder->SetInsertPoint(entryBB);

			// Get context parameter
			llvm::Value* ctx = fn->getArg(0);
			ctx->setName("ctx");

			// Create alloca for ctx parameter for debug info
			// Store the parameter value to it so debugger can always find it
			llvm::AllocaInst* ctxAlloca = builder->CreateAlloca(ctx->getType(), nullptr, "ctx.addr");
			builder->CreateStore(ctx, ctxAlloca);

			// Add debug info for ctx parameter (treat as local variable, not parameter)
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && contextDebugType) {
				// contextDebugType is already qd_context* (pointer), so use it directly
				auto ctxPtrType = contextDebugType;

				// Create local variable for ctx (not a parameter variable)
				// This prevents LLVM from tracking the parameter value flow
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (current function)
						"ctx",															 // Name
						funcDebugFile,													 // File
						static_cast<unsigned>(funcNode->line()),						 // Line
						ctxPtrType,														 // Type
						true															 // Always preserve
				);

				// Insert declare on the alloca so debugger can always find it
				// Use DW_OP_deref since the alloca is a pointer to where the value is stored
				// No deref needed - ctxAlloca stores the pointer value directly
				debugBuilder->insertDeclare(ctxAlloca,	  // Storage (the alloca)
						localVar,						  // Variable
						debugBuilder->createExpression(), // Empty expression (no deref)
						llvm::DILocation::get(
								*context, static_cast<unsigned>(funcNode->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}

			// Set the return target for this function
			currentFunctionReturnBlock = returnBB;
			currentFunctionIsFallible = funcNode->throws();

			// Detect if function only uses integers (for type specialization)
			// Do this BEFORE generating call tracking so we can skip it for integer-only functions
			// Only consider it integer-only if it has at least one explicit integer parameter
			// Functions with no parameters can't be assumed integer-only (they might use strings, floats, etc.)
			bool hasIntegerParams = false;
			currentFunctionIsIntegerOnly = true; // Assume true, set false if we find non-integer
			for (const auto* param : funcNode->inputParameters()) {
				if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param)) {
					const std::string& typeStr = paramNode->typeString();
					if (typeStr == "i64" || typeStr == "int" || typeStr == "int64" || typeStr == "i") {
						hasIntegerParams = true;
					} else {
						currentFunctionIsIntegerOnly = false;
						break;
					}
				}
			}
			if (currentFunctionIsIntegerOnly) {
				for (const auto* param : funcNode->outputParameters()) {
					if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param)) {
						const std::string& typeStr = paramNode->typeString();
						if (typeStr == "i64" || typeStr == "int" || typeStr == "int64" || typeStr == "i") {
							hasIntegerParams = true;
						} else {
							currentFunctionIsIntegerOnly = false;
							break;
						}
					}
				}
			}
			// Only enable integer-only optimizations if there's at least one integer parameter
			currentFunctionIsIntegerOnly = currentFunctionIsIntegerOnly && hasIntegerParams;

			// Push function name onto call stack for debugging
			// Skip for integer-only functions for performance (stack traces less useful for pure int math)
			std::string fullFuncName = namePrefix + "::" + funcNode->name();
			llvm::Value* funcNameStr = nullptr;
			if (!currentFunctionIsIntegerOnly) {
				funcNameStr = builder->CreateGlobalString(fullFuncName);
				std::string sourceFile;
				auto srcIt = moduleSourceFiles.find(namePrefix);
				if (srcIt != moduleSourceFiles.end()) {
					sourceFile = srcIt->second;
				}
				auto sourceFileStr = builder->CreateGlobalString(sourceFile);
				auto lineNum = builder->getInt64(funcNode->line());
				builder->CreateCall(pushCallFn, {ctx, funcNameStr, sourceFileStr, lineNum});
			}

			// Generate type check for input parameters
			// Skip for integer-only functions - semantic validator has already verified types
			if (!funcNode->inputParameters().empty() && !currentFunctionIsIntegerOnly) {
				// Create array of types
				std::vector<llvm::Constant*> typeValues;
				for (auto* paramNode : funcNode->inputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode);
					std::string typeStr = param->typeString();
					uint32_t typeValue;
					if (typeStr.empty()) {
						typeValue = 2; // QD_STACK_TYPE_PTR - untyped
					} else if (typeStr == "i") {
						typeValue = 0; // QD_STACK_TYPE_INT
					} else if (typeStr == "f") {
						typeValue = 1; // QD_STACK_TYPE_FLOAT
					} else if (typeStr == "s") {
						typeValue = 3; // QD_STACK_TYPE_STR
					} else if (typeStr == "p") {
						typeValue = 2; // QD_STACK_TYPE_PTR
					} else {
						typeValue = 2; // QD_STACK_TYPE_PTR - unknown type
					}
					typeValues.push_back(builder->getInt32(typeValue));
				}

				// Create global array constant
				auto arrayType = llvm::ArrayType::get(builder->getInt32Ty(), typeValues.size());
				auto arrayInit = llvm::ConstantArray::get(arrayType, typeValues);
				auto globalArray = new llvm::GlobalVariable(
						*module, arrayType, true, llvm::GlobalValue::PrivateLinkage, arrayInit, "input_types");

				// Call qd_check_stack(ctx, count, types, func_name)
				auto arrayPtr = builder->CreateBitCast(globalArray, llvm::PointerType::getUnqual(*context));
				builder->CreateCall(checkStackFn,
						{ctx, builder->getInt64(funcNode->inputParameters().size()), arrayPtr, funcNameStr});
			}

			// Initialize defer scope stack for this function
			deferScopeStack.clear();
			pushDeferScope();

			// Pre-register struct-typed parameters so they get released at cleanup
			// Parameters with struct type annotations will be stored via `-> name` in the body
			for (const auto* paramNode : funcNode->inputParameters()) {
				if (const auto* param = dynamic_cast<const AstNodeParameter*>(paramNode)) {
					const std::string& typeStr = param->typeString();
					// Check if this is a struct type (not a primitive type)
					if (!typeStr.empty() && typeStr != "i" && typeStr != "i64" && typeStr != "int" &&
							typeStr != "int64" && typeStr != "f" && typeStr != "f64" && typeStr != "float" &&
							typeStr != "s" && typeStr != "str" && typeStr != "string" && typeStr != "p" &&
							typeStr != "ptr" && typeStr != "pointer") {
						// This is a struct type - mark the parameter name as a struct local
						// so it gets released at function cleanup
						localVariableStructTypes[param->name()] = typeStr;
					}
				}
			}

			// Scan for captured variables to enable escaped closures
			// Variables captured by closures need heap allocation so they survive function return
			auto body = funcNode->body();
			if (body) {
				collectAllCapturesFromAST(body, heapAllocatedCaptures);
			}

			// Generate function body
			if (body) {
				generateNode(body, ctx);
			}

			// Clear return target (but save integer-only flag for return block)
			currentFunctionReturnBlock = nullptr;
			currentFunctionIsFallible = false;
			bool wasIntegerOnly = currentFunctionIsIntegerOnly;
			currentFunctionIsIntegerOnly = false;

			// If the block doesn't end with a terminator, branch to return block
			llvm::BasicBlock* funcBodyBlock = builder->GetInsertBlock();
			if (funcBodyBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
				if (!funcBodyBlock->getTerminator()) {
#pragma GCC diagnostic pop
					builder->CreateBr(returnBB);
				}
			}

			// Generate return block - this is where function-level defers execute
			builder->SetInsertPoint(returnBB);

			// Execute function-level defer scope
			executeDeferScope(ctx);

			// Clean up local variables (free strings)
			generateLocalCleanup();

			// Pop function from call stack before returning
			// Skip for integer-only functions (we didn't push in that case)
			if (!wasIntegerOnly) {
				builder->CreateCall(popCallFn, {ctx});
			}

			// Return success
			auto result = llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(execResultTy), {builder->getInt32(0)});
			builder->CreateRet(result);

			// Pop debug scope for user function
			if (debugInfoEnabled && !debugScopeStack.empty()) {
				debugScopeStack.pop_back();
			}
		}

		return true;
	}

	bool LlvmGenerator::Impl::generateTest(AstNodeTest* testNode, const std::string& namePrefix) {
		// Clear local variables for this test
		localVariables.clear();
		localVariableStructTypes.clear();
		localArrayVariables.clear();
		lastStructConstructed.clear();
		lastFieldAccessResultType.clear();

		currentFunctionReturnsPtr = true;
		currentModulePrefix = namePrefix;

		// Generate test function name: test_<sanitized_name>
		std::string sanitizedName = testNode->name();
		for (char& c : sanitizedName) {
			if (!std::isalnum(c)) {
				c = '_';
			}
		}
		std::string funcName = "test_" + namePrefix + "_" + sanitizedName;

		// Create function type: qd_exec_result function(qd_context*)
		auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		auto fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, funcName, *module);

		// Track the test function
		userFunctions[funcName] = fn;

		// Create entry block
		auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
		builder->SetInsertPoint(entryBB);

		// Get context parameter
		auto ctx = fn->arg_begin();
		ctx->setName("ctx");

		// Create error tracking alloca for this test
		testErrorAlloca = builder->CreateAlloca(builder->getInt32Ty(), nullptr, "test_error");
		builder->CreateStore(builder->getInt32(0), testErrorAlloca);

		// Initialize defer scope for this test
		deferScopeStack.clear();
		deferScopeStack.push_back({});

		// Create return block
		auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);
		currentFunctionReturnBlock = returnBB;

		// Push test function onto call stack
		std::string testDisplayName = namePrefix + "::test(" + testNode->name() + ")";
		auto funcNameStr = builder->CreateGlobalString(testDisplayName);
		std::string sourceFile;
		auto srcIt = moduleSourceFiles.find(namePrefix);
		if (srcIt != moduleSourceFiles.end()) {
			sourceFile = srcIt->second;
		}
		auto sourceFileStr = builder->CreateGlobalString(sourceFile);
		auto lineNum = builder->getInt64(testNode->line());
		builder->CreateCall(pushCallFn, {ctx, funcNameStr, sourceFileStr, lineNum});

		// Generate test body
		auto body = testNode->body();
		if (body) {
			generateNode(body, ctx);
		}

		// Branch to return block if no terminator
		llvm::BasicBlock* testBodyBlock = builder->GetInsertBlock();
		if (testBodyBlock) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
			if (!testBodyBlock->getTerminator()) {
#pragma GCC diagnostic pop
				builder->CreateBr(returnBB);
			}
		}

		// Generate return block
		builder->SetInsertPoint(returnBB);

		// Execute defer scope
		executeDeferScope(ctx);

		// Clean up local variables
		generateLocalCleanup();

		// Pop from call stack
		builder->CreateCall(popCallFn, {ctx});

		// Return the accumulated error status
		auto finalError = builder->CreateLoad(builder->getInt32Ty(), testErrorAlloca, "final_error");
		auto result = builder->CreateInsertValue(llvm::UndefValue::get(execResultTy), finalError, {0}, "test_result");
		builder->CreateRet(result);

		currentFunctionReturnBlock = nullptr;
		testErrorAlloca = nullptr; // Clear test context

		return true;
	}

	bool LlvmGenerator::Impl::generateTestRunner(
			const std::vector<std::pair<std::string, std::string>>& testNamesWithDisplay) {
		// Create main function: i32 @main(i32 %argc, i8** %argv)
		auto mainFnTy = llvm::FunctionType::get(
				builder->getInt32Ty(), {builder->getInt32Ty(), llvm::PointerType::getUnqual(*context)}, false);
		auto mainFn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);

		auto entryBB = llvm::BasicBlock::Create(*context, "entry", mainFn);
		builder->SetInsertPoint(entryBB);

		// Check NO_COLOR environment variable
		auto getenvFnTy = llvm::FunctionType::get(
				llvm::PointerType::getUnqual(*context), {llvm::PointerType::getUnqual(*context)}, false);
		auto getenvFn = module->getOrInsertFunction("getenv", getenvFnTy);
		auto noColorStr = builder->CreateGlobalString("NO_COLOR", "no_color_env");
		auto noColorVal = builder->CreateCall(getenvFn, {noColorStr}, "no_color");
		auto noColorNull = builder->CreateICmpEQ(
				noColorVal, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)), "no_color_null");
		// useColor = (getenv("NO_COLOR") == NULL)
		auto useColor = noColorNull;

		// Create Quadrate context
		auto stackSizeVal = builder->getInt64(stackSize);
		auto ctx = builder->CreateCall(createContextFn, {stackSizeVal}, "ctx");

		// Create printf function for direct output
		auto printfFnTy =
				llvm::FunctionType::get(builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context)}, true);
		auto printfFn = module->getOrInsertFunction("printf", printfFnTy);

		// Counters for passed/failed tests
		auto passedCountAlloca = builder->CreateAlloca(builder->getInt32Ty(), nullptr, "passed_count");
		auto failedCountAlloca = builder->CreateAlloca(builder->getInt32Ty(), nullptr, "failed_count");
		builder->CreateStore(builder->getInt32(0), passedCountAlloca);
		builder->CreateStore(builder->getInt32(0), failedCountAlloca);

		// Print header using printf directly
		auto headerStr = builder->CreateGlobalString(
				"Running " + std::to_string(testNamesWithDisplay.size()) + " tests...\n", "test_header");
		builder->CreateCall(printfFn, {headerStr});

		// Run each test
		for (const auto& [funcName, displayName] : testNamesWithDisplay) {
			// Get the test function
			auto testFn = userFunctions[funcName];
			if (!testFn) {
				continue;
			}

			// Call the test function
			auto result = builder->CreateCall(testFn, {ctx}, "test_result");

			// Extract the error code (first field of qd_exec_result)
			auto errorCode = builder->CreateExtractValue(result, {0}, "error_code");

			// Check if test passed
			auto isSuccess = builder->CreateICmpEQ(errorCode, builder->getInt32(0), "is_success");

			auto successBB = llvm::BasicBlock::Create(*context, "test_success", mainFn);
			auto failBB = llvm::BasicBlock::Create(*context, "test_fail", mainFn);
			auto contBB = llvm::BasicBlock::Create(*context, "test_continue", mainFn);

			builder->CreateCondBr(isSuccess, successBB, failBB);

			// Success block
			builder->SetInsertPoint(successBB);
			auto passedCount = builder->CreateLoad(builder->getInt32Ty(), passedCountAlloca, "passed");
			auto newPassed = builder->CreateAdd(passedCount, builder->getInt32(1), "new_passed");
			builder->CreateStore(newPassed, passedCountAlloca);

			// Select colored or plain pass message
			auto passStrColor =
					builder->CreateGlobalString("  \x1b[32m\xe2\x9c\x93\x1b[0m " + displayName + "\n", "pass_color");
			auto passStrPlain = builder->CreateGlobalString("  \xe2\x9c\x93 " + displayName + "\n", "pass_plain");
			auto passStr = builder->CreateSelect(useColor, passStrColor, passStrPlain, "pass_msg");
			builder->CreateCall(printfFn, {passStr});
			builder->CreateBr(contBB);

			// Fail block
			builder->SetInsertPoint(failBB);
			auto failedCount = builder->CreateLoad(builder->getInt32Ty(), failedCountAlloca, "failed");
			auto newFailed = builder->CreateAdd(failedCount, builder->getInt32(1), "new_failed");
			builder->CreateStore(newFailed, failedCountAlloca);

			// Select colored or plain fail message
			auto failStrColor =
					builder->CreateGlobalString("  \x1b[31m\xe2\x9c\x97\x1b[0m " + displayName + "\n", "fail_color");
			auto failStrPlain = builder->CreateGlobalString("  \xe2\x9c\x97 " + displayName + "\n", "fail_plain");
			auto failStr = builder->CreateSelect(useColor, failStrColor, failStrPlain, "fail_msg");
			builder->CreateCall(printfFn, {failStr});
			builder->CreateBr(contBB);

			// Continue to next test
			builder->SetInsertPoint(contBB);
		}

		// Print summary using printf with colors
		auto finalPassed = builder->CreateLoad(builder->getInt32Ty(), passedCountAlloca, "final_passed");
		auto finalFailed = builder->CreateLoad(builder->getInt32Ty(), failedCountAlloca, "final_failed");

		// Select colored or plain summary
		auto summaryFmtColor =
				builder->CreateGlobalString("\n\x1b[32m%d passed\x1b[0m, \x1b[31m%d failed\x1b[0m\n", "summary_color");
		auto summaryFmtPlain = builder->CreateGlobalString("\n%d passed, %d failed\n", "summary_plain");
		auto summaryFmt = builder->CreateSelect(useColor, summaryFmtColor, summaryFmtPlain, "summary_fmt");
		builder->CreateCall(printfFn, {summaryFmt, finalPassed, finalFailed});

		// Free context
		builder->CreateCall(freeContextFn, {ctx});

		// Return 0 if all passed, 1 if any failed
		auto hasFailures = builder->CreateICmpNE(finalFailed, builder->getInt32(0), "has_failures");
		auto exitCode = builder->CreateSelect(hasFailures, builder->getInt32(1), builder->getInt32(0), "exit_code");
		builder->CreateRet(exitCode);

		return true;
	}

	bool LlvmGenerator::Impl::generateProgram(IAstNode* root) {
		if (!root) {
			std::cerr << "Error: Root node is null" << std::endl;
			return false;
		}

		setupRuntimeDeclarations();

		// Process import statements from all modules
		for (const auto& modulePair : moduleASTs) {
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (size_t i = 0; i < moduleRoot->childCount(); i++) {
				auto child = moduleRoot->child(i);
				if (auto importNode = dynamic_cast<AstNodeImport*>(child)) {
					// Process import: create external function declarations
					const std::string& namespaceName = importNode->namespaceName();
					const std::string& library = importNode->library();

					// Track library for linking
					importedLibraries.insert(library);

					for (const auto* func : importNode->functions()) {
						// Determine mangled name based on library
						std::string mangledName;
						if (library == "libstdqd.so") {
							// For libstdqd, use qd_stdqd_ prefix (matches C implementation)
							mangledName = "qd_stdqd_" + func->name;
						} else {
							// For other libraries, use plain C function name
							// Check for standard library imports (libqd*.a or legacy libstd*qd*.a)
							if ((library.rfind("libqd", 0) == 0 && library.find(".a") != std::string::npos) ||
									(library.rfind("libstd", 0) == 0 &&
											(library.find("qd_static.a") != std::string::npos ||
													library.find("qd.so") != std::string::npos))) {
								mangledName = "usr_" + namespaceName + "_" + func->name;
							} else {
								mangledName = func->name;
							}
						}

						// Check if function already exists
						if (module->getFunction(mangledName)) {
							continue; // Already declared
						}

						// Create function type: qd_exec_result function(qd_context*)
						auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
						auto fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, *module);

						// Register fallibility for this imported function
						std::string fullName = namespaceName + "::" + func->name;
						fallibleFunctions[fullName] = func->throws;
						// Mark as imported C function (handles its own success status)
						importedCFunctions.insert(fullName);

						// Also register this function in userFunctions with the scoped name
						// so that namespace::function calls work
						std::string scopedName;
						if (library == "libstdqd.so") {
							scopedName = "usr_" + namespaceName + "_" + func->name;
						} else {
							// For external C libraries, use the namespace::function format directly
							scopedName = "usr_" + namespaceName + "_" + func->name;
						}
						if (scopedName != mangledName && !module->getFunction(scopedName)) {
							// Create alias with usr_ prefix that calls the actual function
							auto aliasFn =
									llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, scopedName, *module);
							// Create a simple wrapper that forwards to the real function
							auto entryBB = llvm::BasicBlock::Create(*context, "entry", aliasFn);
							builder->SetInsertPoint(entryBB);
							auto ctx = aliasFn->arg_begin();
							auto result = builder->CreateCall(fn, {ctx});
							builder->CreateRet(result);
						}
					}
				}
			}
		}

		// Collect constants from all modules
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (size_t i = 0; i < moduleRoot->childCount(); i++) {
				auto child = moduleRoot->child(i);
				if (auto constNode = dynamic_cast<AstNodeConstant*>(child)) {
					// Store constant with scope::name key
					std::string fullName = moduleName + "::" + constNode->name();
					moduleConstants[fullName] = constNode->value();
					// Also store without scope for module-internal access
					moduleConstants[constNode->name()] = constNode->value();
				}
			}
		}

		// Collect constants from main file
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto constNode = dynamic_cast<AstNodeConstant*>(child)) {
				// Store constant with just the name (no scope prefix for main file)
				moduleConstants[constNode->name()] = constNode->value();
			}
		}

		// Process struct declarations from all modules
		for (const auto& modulePair : moduleASTs) {
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (size_t i = 0; i < moduleRoot->childCount(); i++) {
				auto child = moduleRoot->child(i);
				if (auto structNode = dynamic_cast<AstNodeStructDeclaration*>(child)) {
					processStructDeclaration(structNode);
				}
			}
		}

		// Process struct declarations from main file
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto structNode = dynamic_cast<AstNodeStructDeclaration*>(child)) {
				processStructDeclaration(structNode);
			}
		}

		// Generate destructor functions for all struct types (after all structs are known)
		generateStructDestructors();

		// Process import statements from main file
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto importNode = dynamic_cast<AstNodeImport*>(child)) {
				const std::string& namespaceName = importNode->namespaceName();
				const std::string& library = importNode->library();

				// Track library for linking
				importedLibraries.insert(library);

				for (const auto* func : importNode->functions()) {
					std::string mangledName;
					if (library == "libstdqd.so") {
						mangledName = "qd_stdqd_" + func->name;
					} else {
						// For other libraries, use plain C function name
						// Check for standard library imports (libqd*.a or legacy libstd*qd*.a)
						if ((library.rfind("libqd", 0) == 0 && library.find(".a") != std::string::npos) ||
								(library.rfind("libstd", 0) == 0 &&
										(library.find("qd_static.a") != std::string::npos ||
												library.find("qd.so") != std::string::npos))) {
							mangledName = "usr_" + namespaceName + "_" + func->name;
						} else {
							mangledName = func->name;
						}
					}

					if (module->getFunction(mangledName)) {
						continue;
					}

					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					auto fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, *module);

					// Register fallibility for this imported function
					std::string fullName = namespaceName + "::" + func->name;
					fallibleFunctions[fullName] = func->throws;
					// Mark as imported C function (handles its own success status)
					importedCFunctions.insert(fullName);

					std::string scopedName = "usr_" + namespaceName + "_" + func->name;
					if (scopedName != mangledName && !module->getFunction(scopedName)) {
						auto aliasFn =
								llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, scopedName, *module);
						auto entryBB = llvm::BasicBlock::Create(*context, "entry", aliasFn);
						builder->SetInsertPoint(entryBB);
						auto ctx = aliasFn->arg_begin();
						auto result = builder->CreateCall(fn, {ctx});
						builder->CreateRet(result);
					}
				}
			}
		}

		// Determine if we should generate a C main() entry point
		// Check if there's a 'main' function in the root AND the module name looks like standalone
		// (not a module name like "repl_0" which starts with "repl_")
		bool hasMainFunction = false;
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				if (funcNode->name() == "main") {
					hasMainFunction = true;
					break;
				}
			}
		}
		// Only generate C main for standalone mode (main module is "main" or a file path)
		// but NOT for REPL modules (which have names like "repl_0", "repl_1", etc.)
		bool isReplModule = (mainModuleName.find("repl_") == 0);
		bool generateCMain = hasMainFunction && !isReplModule;

		// Pre-pass: declare all user-defined functions from main file (for forward references)
		// This ensures functions can call each other regardless of definition order
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				// Skip main function declaration in standalone mode - it will be the C main
				if (generateCMain && funcNode->name() == "main") {
					continue;
				}
				// Create function declaration with proper module prefix
				std::string fnName = "usr_" + mainModuleName + "_" + funcNode->name();
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				auto fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
				// Register the function for forward reference lookup
				userFunctions[funcNode->name()] = fn;
				fallibleFunctions[funcNode->name()] = funcNode->throws();
				// Track return struct type if output parameter is a struct
				const auto& outputs = funcNode->outputParameters();
				bool foundExplicitStructType = false;
				for (auto* outParam : outputs) {
					if (auto* param = dynamic_cast<AstNodeParameter*>(outParam)) {
						const std::string& typeStr = param->typeString();
						if (!typeStr.empty() && std::isupper(typeStr[0])) {
							functionReturnStructType[funcNode->name()] = typeStr;
							foundExplicitStructType = true;
							break; // Use first struct-typed output
						}
					}
				}
				// If return type is ptr but body constructs a struct, infer the type
				if (!foundExplicitStructType && funcNode->body()) {
					std::string inferredType = findLastStructConstruction(funcNode->body());
					if (!inferredType.empty()) {
						functionReturnStructType[funcNode->name()] = inferredType;
					}
				}
			}
		}

		// First pass: generate functions from all loaded modules (in dependency order)
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (size_t i = 0; i < moduleRoot->childCount(); i++) {
				auto child = moduleRoot->child(i);
				if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
					// Generate module function with module name as prefix
					if (!generateFunction(funcNode, false, moduleName)) {
						return false;
					}
				}
			}
		}

		// Second pass: generate all user-defined functions from main file
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				// Skip main function in standalone mode - handle separately
				if (generateCMain && funcNode->name() == "main" && !testMode) {
					continue;
				}
				// Generate as regular user function with module name prefix
				if (!generateFunction(funcNode, false, mainModuleName)) {
					return false;
				}
			}
		}

		// Third pass: generate test functions (in test mode)
		if (testMode) {
			collectedTestNames.clear();

			// Generate test functions from imported modules (not the main module)
			for (const auto& modulePair : moduleASTs) {
				const std::string& moduleName = modulePair.first;
				IAstNode* moduleRoot = modulePair.second;
				if (!moduleRoot) {
					continue;
				}
				// Skip the main module - it's handled separately below
				if (moduleName == mainModuleName) {
					continue;
				}

				for (size_t i = 0; i < moduleRoot->childCount(); i++) {
					auto child = moduleRoot->child(i);
					if (auto testNode = dynamic_cast<AstNodeTest*>(child)) {
						if (!generateTest(testNode, moduleName)) {
							return false;
						}
						// Sanitize test name for function lookup
						std::string sanitizedName = testNode->name();
						for (char& c : sanitizedName) {
							if (!std::isalnum(c)) {
								c = '_';
							}
						}
						std::string funcName = "test_" + moduleName + "_" + sanitizedName;
						// Display name: module::test_name
						std::string displayName = moduleName + "::" + testNode->name();
						collectedTestNames.push_back({funcName, displayName});
					}
				}
			}

			// Generate test functions from main file
			for (size_t i = 0; i < root->childCount(); i++) {
				auto child = root->child(i);
				if (auto testNode = dynamic_cast<AstNodeTest*>(child)) {
					if (!generateTest(testNode, mainModuleName)) {
						return false;
					}
					std::string sanitizedName = testNode->name();
					for (char& c : sanitizedName) {
						if (!std::isalnum(c)) {
							c = '_';
						}
					}
					std::string funcName = "test_" + mainModuleName + "_" + sanitizedName;
					// For main module, extract just the filename for display
					std::string shortName = mainModuleName;
					auto lastSlash = shortName.rfind('/');
					if (lastSlash != std::string::npos) {
						shortName = shortName.substr(lastSlash + 1);
					}
					std::string displayName = shortName + "::" + testNode->name();
					collectedTestNames.push_back({funcName, displayName});
				}
			}

			// Generate test runner
			if (!generateTestRunner(collectedTestNames)) {
				return false;
			}
		}

		// Fourth pass: generate C main function (only in standalone mode, not in test mode)
		if (generateCMain && !testMode) {
			for (size_t i = 0; i < root->childCount(); i++) {
				auto child = root->child(i);
				if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
					if (funcNode->name() == "main") {
						if (!generateFunction(funcNode, true)) {
							return false;
						}
					}
				}
			}
		}

		// Verify module
		// Finalize debug info
		if (debugInfoEnabled && debugBuilder) {
			debugBuilder->finalize();
		}

		std::string errorMsg;
		llvm::raw_string_ostream errorStream(errorMsg);
		if (llvm::verifyModule(*module, &errorStream)) {
			std::cerr << "LLVM module verification failed:\n" << errorMsg << std::endl;
			return false;
		}

		return !compilationFailed;
	}

	// LlvmGenerator implementation

	LlvmGenerator::LlvmGenerator() : mImpl(nullptr) {
	}

	LlvmGenerator::~LlvmGenerator() = default;

	void LlvmGenerator::setDebugInfo(bool enabled) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->debugInfoEnabled = enabled;
	}

	void LlvmGenerator::setOptimizationLevel(int level) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		// Clamp level to 0-3
		if (level < 0) {
			level = 0;
		}
		if (level > 3) {
			level = 3;
		}
		mImpl->optimizationLevel = level;
	}

	void LlvmGenerator::addLibrarySearchPath(const std::string& path) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->librarySearchPaths.push_back(path);
	}

	void LlvmGenerator::setStackSize(size_t size) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->stackSize = size;
	}

	void LlvmGenerator::setTestMode(bool enabled) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->testMode = enabled;
	}

	bool LlvmGenerator::generate(IAstNode* root, const std::string& moduleName) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>(moduleName);
		}
		// Store source filename for debug info
		// If moduleName looks like a file path (contains / or ends with .qd), use it directly
		// Otherwise append .qd extension
		if (moduleName.find('/') != std::string::npos || moduleName.find('\\') != std::string::npos ||
				(moduleName.size() > 3 && moduleName.substr(moduleName.size() - 3) == ".qd")) {
			mImpl->sourceFileName = moduleName;
		} else {
			mImpl->sourceFileName = moduleName + ".qd";
		}
		// Store the main module name
		mImpl->mainModuleName = moduleName;
		// Add main source file to moduleSourceFiles for stack trace info
		mImpl->moduleSourceFiles["main"] = mImpl->sourceFileName;
		return mImpl->generateProgram(root);
	}

	void LlvmGenerator::addModuleAST(
			const std::string& moduleName, IAstNode* moduleRoot, const std::string& sourceFileName) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>("quadrate_module");
		}
		mImpl->moduleASTs.push_back({moduleName, moduleRoot});
		if (!sourceFileName.empty()) {
			mImpl->moduleSourceFiles[moduleName] = sourceFileName;
		}
	}

	std::string LlvmGenerator::getIRString() const {
		if (!mImpl || !mImpl->module) {
			return "";
		}

		std::string str;
		llvm::raw_string_ostream os(str);
		mImpl->module->print(os, nullptr);
		return str;
	}

	bool LlvmGenerator::writeIR(const std::string& filename) {
		if (!mImpl || !mImpl->module) {
			return false;
		}

		std::error_code ec;
		llvm::raw_fd_ostream os(filename, ec);
		if (ec) {
			std::cerr << "Error opening file: " << ec.message() << std::endl;
			return false;
		}

		mImpl->module->print(os, nullptr);
		return true;
	}

	bool LlvmGenerator::writeObject(const std::string& filename) {
		if (!mImpl || !mImpl->module) {
			return false;
		}

		// Initialize targets
		llvm::InitializeAllTargetInfos();
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();

		auto targetTripleStr = llvm::sys::getDefaultTargetTriple();
		llvm::Triple targetTriple(targetTripleStr);
// LLVM 20+ changed API to accept Triple objects instead of strings
#if LLVM_VERSION_MAJOR >= 20
		mImpl->module->setTargetTriple(targetTriple);
#else
		mImpl->module->setTargetTriple(targetTriple.getTriple());
#endif

		std::string error;
		auto target = llvm::TargetRegistry::lookupTarget(targetTripleStr, error);
		if (!target) {
			std::cerr << "Error: " << error << std::endl;
			return false;
		}

		// Use host CPU for better optimization (instead of "generic")
		auto cpu = llvm::sys::getHostCPUName();
		auto features = "";
		llvm::TargetOptions opt;
// LLVM 20+ changed API to accept Triple objects instead of strings
#if LLVM_VERSION_MAJOR >= 20
		std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(
				targetTriple, cpu, features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_)));
#else
		std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(
				targetTriple.getTriple(), cpu, features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_)));
#endif

		mImpl->module->setDataLayout(targetMachine->createDataLayout());

		// Run optimization passes if optimization level > 0
		if (mImpl->optimizationLevel > 0) {
			// Use legacy PassManager for optimization passes
			llvm::legacy::FunctionPassManager fpm(mImpl->module.get());
			llvm::legacy::PassManager mpm;

			// Add function-level optimization passes based on level
			if (mImpl->optimizationLevel >= 1) {
				// Basic optimizations
				fpm.add(llvm::createPromoteMemoryToRegisterPass()); // mem2reg
				fpm.add(llvm::createEarlyCSEPass());				   // Early common subexpression elimination
				fpm.add(llvm::createInstructionCombiningPass());   // instcombine
				fpm.add(llvm::createReassociatePass());			   // reassociate
				fpm.add(llvm::createCFGSimplificationPass());	   // simplifycfg
			}

			if (mImpl->optimizationLevel >= 2) {
				// More aggressive optimizations
				fpm.add(llvm::createGVNPass());					// GVN (Global Value Numbering)
				fpm.add(llvm::createDeadCodeEliminationPass()); // Dead Code Elimination
				fpm.add(llvm::createSROAPass());				// Scalar Replacement of Aggregates
			}

			if (mImpl->optimizationLevel >= 3) {
				// Most aggressive optimizations
				fpm.add(llvm::createLICMPass()); // Loop Invariant Code Motion
				fpm.add(llvm::createLoopUnrollPass());
			}

			// Run function passes on all functions
			fpm.doInitialization();
			for (auto& func : *mImpl->module) {
				if (!func.isDeclaration()) {
					fpm.run(func);
				}
			}
			fpm.doFinalization();

			// Run module passes
			mpm.run(*mImpl->module);
		}

		std::error_code ec;
		llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
		if (ec) {
			std::cerr << "Could not open file: " << ec.message() << std::endl;
			return false;
		}

		llvm::legacy::PassManager pass;
		if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
			std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
			return false;
		}

		pass.run(*mImpl->module);
		dest.flush();

		return true;
	}

	bool LlvmGenerator::writeExecutable(const std::string& filename) {
		// Generate object file first
		std::string objFile = filename + ".o";
		if (!writeObject(objFile)) {
			return false;
		}

		// Determine library directory
		std::string libDir;

		// Check QUADRATE_LIBDIR environment variable first
		if (const char* quadrateLibDir = std::getenv("QUADRATE_LIBDIR")) {
			std::filesystem::path libPath(quadrateLibDir);
			// Convert to absolute path if relative
			if (libPath.is_relative()) {
				libPath = std::filesystem::absolute(libPath);
			}
			if (std::filesystem::exists(libPath)) {
				libDir = libPath.string();
			}
		}
		// Check ./dist/lib (development build) - use absolute path
		if (libDir.empty()) {
			std::filesystem::path distLib = std::filesystem::absolute("./dist/lib");
			if (std::filesystem::exists(distLib)) {
				libDir = distLib.string();
			}
		}
		// Check relative to executable (installed binaries)
		if (libDir.empty()) {
			std::error_code ec;
			std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe", ec);
			if (!ec) {
				std::filesystem::path exeDir = exePath.parent_path();
				std::filesystem::path installedLib = exeDir / ".." / "lib";
				if (std::filesystem::exists(installedLib)) {
					libDir = installedLib.string();
				}
			}
		}
		// Check ~/.local/lib (user installation)
		if (libDir.empty()) {
			if (const char* home = std::getenv("HOME")) {
				std::filesystem::path localLib = std::filesystem::path(home) / ".local" / "lib";
				if (std::filesystem::exists(localLib)) {
					libDir = localLib.string();
				}
			}
		}
		// Check system library path
		if (libDir.empty()) {
			if (std::filesystem::exists("/usr/lib")) {
				libDir = "/usr/lib";
			}
		}

		// Build library flags - link static libraries directly
		// Check for nested structure (build directory) first, then flat structure (dist)
		std::string qdrtStaticPath;
		// Try new naming convention first (libqdrt.a), then legacy (libqdrt_static.a)
		std::string nestedPath = libDir + "/qdrt/libqdrt_static.a";
		std::string flatPath = libDir + "/libqdrt.a";
		std::string flatPathLegacy = libDir + "/libqdrt_static.a";

		if (std::filesystem::exists(flatPath)) {
			qdrtStaticPath = flatPath;
		} else if (std::filesystem::exists(nestedPath)) {
			qdrtStaticPath = nestedPath;
		} else if (std::filesystem::exists(flatPathLegacy)) {
			qdrtStaticPath = flatPathLegacy;
		} else {
			// Fallback to flat path (will error later if doesn't exist)
			qdrtStaticPath = flatPath;
		}

		std::string libraryFlags = qdrtStaticPath;

		// Add imported libraries
		for (const auto& library : mImpl->importedLibraries) {
			// Check if it's already a .a file (static library)
			if (library.size() >= 2 && library.substr(library.size() - 2) == ".a") {
				// It's a static library, link it directly
				std::string foundLibPath;

				// First, check in additional library search paths (third-party packages)
				for (const auto& searchPath : mImpl->librarySearchPaths) {
					std::string candidatePath = searchPath + "/" + library;
					if (std::filesystem::exists(candidatePath)) {
						foundLibPath = candidatePath;
						break;
					}
				}

				// If not found in search paths, check main libDir
				if (foundLibPath.empty()) {
					std::string flatLib = libDir + "/" + library;

					// Extract library name for nested search
					// Examples: "libqdmath.a" -> "qdmath", "libqdrt_static.a" -> "qdrt"
					std::string libBaseName = library;
					if (libBaseName.rfind("lib", 0) == 0) {
						libBaseName = libBaseName.substr(3); // Remove "lib" prefix
					}
					// Remove ".a" suffix first
					if (libBaseName.size() > 2 && libBaseName.substr(libBaseName.size() - 2) == ".a") {
						libBaseName = libBaseName.substr(0, libBaseName.size() - 2);
					}
					// Remove "_static" suffix if present
					if (libBaseName.size() > 7 && libBaseName.substr(libBaseName.size() - 7) == "_static") {
						libBaseName = libBaseName.substr(0, libBaseName.size() - 7);
					}
					std::string nestedLib = libDir + "/" + libBaseName + "/" + library;

					if (std::filesystem::exists(flatLib)) {
						foundLibPath = flatLib;
					} else if (std::filesystem::exists(nestedLib)) {
						foundLibPath = nestedLib;
					} else {
						// Try without libDir prefix (fallback)
						foundLibPath = library;
					}
				}

				libraryFlags += " " + foundLibPath;
			} else {
				// Handle .so libraries (dynamic linking)
				std::string libName = library;

				// Remove "lib" prefix and ".so" suffix to get library name
				if (libName.rfind("lib", 0) == 0) {
					libName = libName.substr(3);
				}
				if (libName.size() >= 3 && libName.substr(libName.size() - 3) == ".so") {
					libName = libName.substr(0, libName.size() - 3);
				}

				libraryFlags += " -l" + libName;
			}
		}

		// Add standard system libraries
		// Note: C11 threads don't need -lpthread, but we need -lstdc++ for C++ filesystem code
		libraryFlags += " -lm -lstdc++";

		// Build -L flags for additional library search paths (third-party packages)
		std::string librarySearchFlags;
		for (const auto& searchPath : mImpl->librarySearchPaths) {
			librarySearchFlags += " -L" + searchPath;
		}

		std::string linkCmd = "clang -o " + filename + " " + objFile + " " + librarySearchFlags + " " + libraryFlags;

		int result = system(linkCmd.c_str());

		// Clean up object file
		std::remove(objFile.c_str());

		return result == 0;
	}

	// Helper function to check if a type name is a known struct
	bool LlvmGenerator::Impl::isKnownStruct(const std::string& typeName) {
		return structDefinitions.find(typeName) != structDefinitions.end();
	}

	// Helper function to get size of a type
	size_t LlvmGenerator::Impl::getTypeSize(const std::string& typeName) {
		if (typeName == "i64" || typeName == "f64") {
			return 8;
		} else if (typeName == "i32" || typeName == "f32") {
			return 4;
		} else if (typeName == "str" || typeName.find('*') != std::string::npos) {
			return 8; // Pointer size
		} else if (!typeName.empty() && std::isupper(typeName[0]) && isKnownStruct(typeName)) {
			// Struct-typed field - stored as pointer, not inline
			return 8; // Pointer size
		}
		return 8; // Default to pointer size (also handles type parameters like T, U)
	}

	// Process a struct declaration and calculate field offsets
	void LlvmGenerator::Impl::processStructDeclaration(AstNodeStructDeclaration* structDecl) {
		StructLayout layout;
		layout.name = structDecl->name();
		layout.isPublic = structDecl->isPublic();
		layout.totalSize = 0;

		// Calculate field offsets
		for (const auto* field : structDecl->fields()) {
			FieldInfo fieldInfo;
			fieldInfo.name = field->name();
			fieldInfo.typeName = field->typeName();
			fieldInfo.offset = layout.totalSize;
			fieldInfo.size = getTypeSize(field->typeName());

			// Add field to layout
			layout.fields.push_back(fieldInfo);

			// Update total size with alignment (8-byte alignment)
			layout.totalSize += fieldInfo.size;
			if (layout.totalSize % 8 != 0) {
				layout.totalSize = (layout.totalSize + 7) & ~static_cast<size_t>(7); // Round up to next 8-byte boundary
			}
		}

		// Store struct definition
		structDefinitions[layout.name] = layout;
	}

	// Generate cleanup code for a struct - frees nested struct fields and string fields
	void LlvmGenerator::Impl::generateStructCleanup(llvm::Value* structPtr, const std::string& structTypeName) {
		auto structDefIt = structDefinitions.find(structTypeName);
		if (structDefIt == structDefinitions.end()) {
			return;
		}

		const StructLayout& layout = structDefIt->second;

		// First, recursively cleanup nested struct fields
		for (const auto& field : layout.fields) {
			// Check if field is a known struct type (not a type parameter like T)
			if (!field.typeName.empty() && std::isupper(field.typeName[0]) && isKnownStruct(field.typeName)) {
				// Load the nested struct pointer from this field
				auto fieldOffset = builder->getInt64(field.offset);
				auto fieldBytePtr =
						builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "nested_field_ptr");
				llvm::Value* nestedStructPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), fieldBytePtr, "nested_struct_ptr");

				// Recursively cleanup the nested struct
				generateStructCleanup(nestedStructPtr, field.typeName);

				// Free the nested struct memory
				builder->CreateCall(this->freeFn, {nestedStructPtr});
			}
		}

		// Then, release string fields
		for (const auto& field : layout.fields) {
			if (field.typeName == "str") {
				// Calculate field offset and load string pointer
				auto fieldOffset = builder->getInt64(field.offset);
				auto fieldBytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "str_field_ptr");
				llvm::Value* stringPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), fieldBytePtr, "string_ptr");

				// Call qd_string_release() on the string
				if (!this->qdStringReleaseFn) {
					auto qdStringReleaseFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					this->qdStringReleaseFn = llvm::Function::Create(
							qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
				}
				builder->CreateCall(this->qdStringReleaseFn, {stringPtr});
			}
		}
	}

	// Generate destructor functions for all struct types
	void LlvmGenerator::Impl::generateStructDestructors() {
		// Destructor function type: void (*)(void*)
		auto destructorFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);

		// Ensure qd_string_release is declared (needed for string field cleanup)
		if (!qdStringReleaseFn) {
			auto qdStringReleaseFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			qdStringReleaseFn = llvm::Function::Create(
					qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
		}

		for (const auto& [structName, layout] : structDefinitions) {
			// Check if this struct has any fields that need cleanup
			bool needsDestructor = false;
			for (const auto& field : layout.fields) {
				// Check for struct type (Inner) or pointer to struct (*Inner)
				std::string baseTypeName = field.typeName;
				if (!baseTypeName.empty() && baseTypeName[0] == '*') {
					baseTypeName = baseTypeName.substr(1);
				}
				if (field.typeName == "str" ||
						(!baseTypeName.empty() && std::isupper(baseTypeName[0]) && isKnownStruct(baseTypeName))) {
					needsDestructor = true;
					break;
				}
			}

			if (!needsDestructor) {
				// No destructor needed - will pass nullptr to qd_struct_alloc
				structDestructors[structName] = nullptr;
				continue;
			}

			// Generate destructor function
			std::string dtorName = "__qd_dtor_" + structName;
			auto dtorFn = llvm::Function::Create(destructorFnTy, llvm::Function::InternalLinkage, dtorName, *module);

			// Save current insertion point
			auto savedBlock = builder->GetInsertBlock();
			auto savedPoint = builder->GetInsertPoint();

			// Create entry block for destructor
			auto entryBlock = llvm::BasicBlock::Create(*context, "entry", dtorFn);
			builder->SetInsertPoint(entryBlock);

			// Get struct pointer argument
			llvm::Value* structPtr = dtorFn->getArg(0);

			// Release nested struct fields first (call qd_struct_release)
			for (const auto& field : layout.fields) {
				// Check for struct type (Inner) or pointer to struct (*Inner)
				std::string baseTypeName = field.typeName;
				if (!baseTypeName.empty() && baseTypeName[0] == '*') {
					baseTypeName = baseTypeName.substr(1);
				}
				if (!baseTypeName.empty() && std::isupper(baseTypeName[0]) && isKnownStruct(baseTypeName)) {
					// Nested struct field
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "nested_field_ptr");
					llvm::Value* nestedStructPtr = builder->CreateLoad(
							llvm::PointerType::getUnqual(*context), fieldBytePtr, "nested_struct_ptr");
					builder->CreateCall(qdStructReleaseFn, {nestedStructPtr});
				}
			}

			// Release string fields
			for (const auto& field : layout.fields) {
				if (field.typeName == "str") {
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "str_field_ptr");
					llvm::Value* stringPtr =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), fieldBytePtr, "string_ptr");
					builder->CreateCall(qdStringReleaseFn, {stringPtr});
				}
			}

			builder->CreateRetVoid();

			// Restore insertion point
			if (savedBlock) {
				builder->SetInsertPoint(savedBlock, savedPoint);
			}

			structDestructors[structName] = dtorFn;
		}
	}

	// Generate struct construction: pop values from stack, malloc, initialize, push pointer
	void LlvmGenerator::Impl::generateStructConstruction(const std::string& structName, llvm::Value* ctx) {
		auto it = structDefinitions.find(structName);
		if (it == structDefinitions.end()) {
			std::cerr << "Error: Unknown struct type: " << structName << std::endl;
			return;
		}

		const StructLayout& layout = it->second;

		// Define context and stack types (stackElementTy is already a member variable)
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::getUnqual(*context)}, false);
		llvm::Type* stackTy = llvm::StructType::get(*context,
				{
						llvm::PointerType::getUnqual(*context), // data
						builder->getInt64Ty(),					// size
						builder->getInt64Ty()					// capacity
				},
				false);

		// Allocate struct - use stack if function doesn't return pointer, heap otherwise
		llvm::Value* structPtr = nullptr;

		if (!currentFunctionReturnsPtr) {
			// Stack allocation - struct lives only within this function
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::IRBuilder<> entryBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
			auto structAlloca = entryBuilder.CreateAlloca(
					llvm::ArrayType::get(builder->getInt8Ty(), layout.totalSize), nullptr, structName + "_stack");
			structPtr = builder->CreateBitCast(structAlloca, llvm::PointerType::getUnqual(*context), "struct_ptr");
		} else {
			// Heap allocation - struct may be returned, needs refcounting
			llvm::Value* destructorPtr = nullptr;
			auto destructorIt = structDestructors.find(structName);
			if (destructorIt != structDestructors.end() && destructorIt->second != nullptr) {
				destructorPtr = destructorIt->second;
			} else {
				destructorPtr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context));
			}
			structPtr = builder->CreateCall(
					qdStructAllocFn, {builder->getInt64(layout.totalSize), destructorPtr}, "struct_ptr");
		}

		// Pop values from stack in reverse order and write to struct fields
		for (auto fieldIt = layout.fields.rbegin(); fieldIt != layout.fields.rend(); ++fieldIt) {
			const FieldInfo& field = *fieldIt;

			// Calculate field pointer
			auto fieldOffset = builder->getInt64(field.offset);
			auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

			// Pop value from stack
			llvm::Value* stackPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
			llvm::Value* st = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtr, "st");

			// Get stack size
			llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
			llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

			// Decrement size
			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			// Get element pointer
			llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
			llvm::Value* data = builder->CreateLoad(llvm::PointerType::getUnqual(*context), dataPtr, "data");
			llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, newSize, "elem_ptr");

			// Load value from stack element
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");

			// Store to struct field based on type
			if (field.typeName == "f64") {
				llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "float_val");
				builder->CreateStore(floatValue, bytePtr);
			} else if (field.typeName == "i64") {
				llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "int_val");
				builder->CreateStore(intValue, bytePtr);
			} else if (field.typeName == "i32") {
				// Load as i64 from stack (stack elements are always 64-bit), truncate to i32
				llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "int_val");
				llvm::Value* truncValue = builder->CreateTrunc(intValue, builder->getInt32Ty(), "int32_val");
				builder->CreateStore(truncValue, bytePtr);
			} else if (field.typeName == "ptr" || field.typeName == "str" ||
					   field.typeName.find('*') != std::string::npos ||
					   (!field.typeName.empty() && std::isupper(field.typeName[0]) && isKnownStruct(field.typeName))) {
				// Pointer type (including ptr, str, raw pointers, and struct-typed fields)
				llvm::Value* ptrValue =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "ptr_val");
				// NOTE: We do NOT retain nested struct fields here.
				// Retain happens when pushing a local to the stack (in generateIdentifier).
				// The containing struct's destructor will release nested structs.
				builder->CreateStore(ptrValue, bytePtr);
			} else {
				// Unknown type (including type parameters like T) - treat as i64
				llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "generic_val");
				builder->CreateStore(intValue, bytePtr);
			}
		}

		// Push struct pointer onto stack
		builder->CreateCall(pushPtrFn, {ctx, structPtr});

		// Track that we just constructed this struct type
		lastStructConstructed = structName;
	}

	// Generate field access: load pointer from local, calculate offset, load value, push to stack
	void LlvmGenerator::Impl::generateFieldAccess(AstNodeFieldAccess* fieldAccess, llvm::Value* ctx) {
		const std::string& varName = fieldAccess->varName();
		const std::string& fieldName = fieldAccess->fieldName();

		llvm::Value* structPtr = nullptr;
		std::string structTypeName;
		bool needsReleaseAfterAccess = false; // Track if we need to release the struct after field access

		if (varName.empty()) {
			// Stack-based field access - could be:
			// 1. Chained field access (p @origin @x) - use lastFieldAccessResultType
			// 2. Direct access after struct construction (Point @x) - use lastStructConstructed
			if (!lastFieldAccessResultType.empty()) {
				structTypeName = lastFieldAccessResultType;
			} else if (!lastStructConstructed.empty()) {
				structTypeName = lastStructConstructed;
				lastStructConstructed.clear(); // Consume it
			}

			// Pop struct pointer from stack
			llvm::Type* contextStructTy =
					llvm::StructType::get(*context, {llvm::PointerType::getUnqual(*context)}, false);
			llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
			llvm::Value* stackPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtrPtr, "stack");

			// Allocate temp for popped element
			llvm::Value* tempElem = builder->CreateAlloca(stackElementTy, nullptr, "temp_elem");
			builder->CreateCall(stackPopFn, {stackPtr, tempElem});

			// Load the pointer value from the element
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
			structPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "struct_ptr");

			// The struct pointer was retained when pushed to stack, needs release after access
			needsReleaseAfterAccess = true;
		} else {
			// Check if varName is actually a struct type (e.g., Point @x after 1 2 Point @x)
			// The parser created AstNodeFieldAccess("Point", "x") but Point is a struct, not a variable
			auto structDefIt = structDefinitions.find(varName);
			if (structDefIt != structDefinitions.end()) {
				// varName is a struct type - generate struct construction first, then stack-based access
				generateStructConstruction(varName, ctx);
				structTypeName = varName;

				// Pop the just-constructed struct pointer from stack
				llvm::Type* contextStructTy =
						llvm::StructType::get(*context, {llvm::PointerType::getUnqual(*context)}, false);
				llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
				llvm::Value* stackPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtrPtr, "stack");

				llvm::Value* tempElem = builder->CreateAlloca(stackElementTy, nullptr, "temp_elem");
				builder->CreateCall(stackPopFn, {stackPtr, tempElem});

				llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
				structPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "struct_ptr");
			} else {
				// Check if varName is a function - call it first, then do stack-based field access
				auto funcIt = userFunctions.find(varName);
				std::string funcLookupName = varName;
				if (funcIt == userFunctions.end() && currentModulePrefix != "main") {
					// Try with current module prefix
					std::string qualifiedName = currentModulePrefix + "::" + varName;
					funcIt = userFunctions.find(qualifiedName);
					if (funcIt != userFunctions.end()) {
						funcLookupName = qualifiedName;
					}
				}

				if (funcIt != userFunctions.end()) {
					// varName is a function - call it first
					builder->CreateCall(funcIt->second, {ctx});

					// Pop the result (struct pointer) from stack
					llvm::Type* contextStructTy =
							llvm::StructType::get(*context, {llvm::PointerType::getUnqual(*context)}, false);
					llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
					llvm::Value* stackPtr =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtrPtr, "stack");

					llvm::Value* tempElem = builder->CreateAlloca(stackElementTy, nullptr, "temp_elem");
					builder->CreateCall(stackPopFn, {stackPtr, tempElem});

					llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
					structPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "struct_ptr");

					// Look up the return struct type from function signature
					auto returnTypeIt = functionReturnStructType.find(funcLookupName);
					if (returnTypeIt != functionReturnStructType.end()) {
						structTypeName = returnTypeIt->second;
					}
				} else {
					// Check if it's a captured variable (by reference)
					auto capIt = capturedVariableRefs.find(varName);
					if (capIt != capturedVariableRefs.end()) {
						// Load the pointer to the outer variable, then access the value
						llvm::AllocaInst* ptrAlloca = capIt->second;
						llvm::Value* outerVarPtr = builder->CreateLoad(
								llvm::PointerType::getUnqual(*context), ptrAlloca, varName + "_cap_ptr");

						// Look up the struct type from local variable tracking
						auto typeIt = localVariableStructTypes.find(varName);
						if (typeIt != localVariableStructTypes.end()) {
							structTypeName = typeIt->second;
						}

						// Extract the value field from the outer variable (qd_stack_element_t)
						llvm::Value* valuePtr =
								builder->CreateStructGEP(stackElementTy, outerVarPtr, 0, varName + "_cap_value_ptr");
						structPtr =
								builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "struct_ptr");
					} else {
						// Normal field access from local variable
						auto it = localVariables.find(varName);
						if (it == localVariables.end()) {
							std::cerr << "Error: Undefined variable: " << varName << std::endl;
							return;
						}

						// Look up the struct type from local variable tracking
						auto typeIt = localVariableStructTypes.find(varName);
						if (typeIt != localVariableStructTypes.end()) {
							structTypeName = typeIt->second;
						}

						// Local variables are stored as qd_stack_element_t, need to extract the value field
						llvm::Value* structPtrAlloca = it->second;
						// For indirect (captured) variables, load the actual storage pointer first
						if (indirectLocalVariables.find(varName) != indirectLocalVariables.end()) {
							structPtrAlloca = builder->CreateLoad(
									llvm::PointerType::getUnqual(*context), it->second, varName + "_storage");
						}
						llvm::Value* valuePtr =
								builder->CreateStructGEP(stackElementTy, structPtrAlloca, 0, varName + "_value_ptr");
						structPtr =
								builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "struct_ptr");
					}
				}
			}
		}

		// Find the field in the specific struct type if known
		const FieldInfo* matchingField = nullptr;

		if (!structTypeName.empty()) {
			// Look up field in the specific struct type
			auto structIt = structDefinitions.find(structTypeName);
			if (structIt != structDefinitions.end()) {
				for (const auto& field : structIt->second.fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
			}
		}

		// Fallback: search all struct types if we don't know the type
		if (!matchingField) {
			for (const auto& pair : structDefinitions) {
				for (const auto& field : pair.second.fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
				if (matchingField) {
					break;
				}
			}
		}

		if (!matchingField) {
			std::cerr << "Error: Unknown field: " << fieldName << std::endl;
			return;
		}

		// Calculate field offset
		auto fieldOffset = builder->getInt64(matchingField->offset);
		auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

		// Load value from field based on type and update lastFieldAccessResultType for chaining
		if (matchingField->typeName == "f64") {
			llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), bytePtr, "field_value");
			builder->CreateCall(pushFloatFn, {ctx, floatValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "i64") {
			llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), bytePtr, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "i32") {
			// Load i32, sign-extend to i64 for the stack
			llvm::Value* int32Value = builder->CreateLoad(builder->getInt32Ty(), bytePtr, "field_value_i32");
			llvm::Value* intValue = builder->CreateSExt(int32Value, builder->getInt64Ty(), "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "str") {
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), fieldPtr, "field_value");
			builder->CreateCall(pushStrRefFn, {ctx, ptrValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "ptr" || matchingField->typeName.find('*') != std::string::npos) {
			// Handle ptr type and raw pointer types
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), fieldPtr, "field_value");
			// Retain the pointer before pushing (it could be an array/struct that will be released after use)
			builder->CreateCall(qdPtrRetainFn, {ptrValue});
			builder->CreateCall(pushPtrFn, {ctx, ptrValue});
			lastFieldAccessResultType.clear(); // Raw pointer, not a known struct type
		} else if (!matchingField->typeName.empty() && std::isupper(matchingField->typeName[0]) &&
				   isKnownStruct(matchingField->typeName)) {
			// Struct-typed field - stored as pointer, push as PTR
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), fieldPtr, "field_value");
			// Retain the struct pointer before pushing
			builder->CreateCall(qdPtrRetainFn, {ptrValue});
			builder->CreateCall(pushPtrFn, {ctx, ptrValue});
			// Track the struct type for chained field access
			lastFieldAccessResultType = matchingField->typeName;
		} else {
			// Type parameter or unknown type - treat as i64 value
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), fieldPtr, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		}

		// Release the struct pointer if it was popped from stack (was retained when pushed)
		if (needsReleaseAfterAccess) {
			builder->CreateCall(qdPtrReleaseFn, {structPtr});
		}
	}

	// Generate field set: pop value from stack, get struct pointer from local, write to field
	void LlvmGenerator::Impl::generateFieldSet(AstNodeFieldSet* fieldSet, llvm::Value* ctx) {
		const std::string& varName = fieldSet->varName();
		const std::string& fieldName = fieldSet->fieldName();

		llvm::Value* structPtr = nullptr;
		std::string structTypeName;

		// Check if it's a captured variable (by reference)
		auto capIt = capturedVariableRefs.find(varName);
		if (capIt != capturedVariableRefs.end()) {
			// Load the pointer to the outer variable, then access the value
			llvm::AllocaInst* ptrAlloca = capIt->second;
			llvm::Value* outerVarPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), ptrAlloca, varName + "_cap_ptr");

			// Look up the struct type from local variable tracking
			auto typeIt = localVariableStructTypes.find(varName);
			if (typeIt != localVariableStructTypes.end()) {
				structTypeName = typeIt->second;
			}

			// Extract the value field from the outer variable (qd_stack_element_t)
			llvm::Value* structValuePtr =
					builder->CreateStructGEP(stackElementTy, outerVarPtr, 0, varName + "_cap_value_ptr");
			structPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), structValuePtr, "struct_ptr");
		} else {
			// Get struct pointer from local variable
			auto it = localVariables.find(varName);
			if (it == localVariables.end()) {
				std::cerr << "Error: Undefined variable in field set: " << varName << std::endl;
				return;
			}

			// Look up the struct type from local variable tracking
			auto typeIt = localVariableStructTypes.find(varName);
			if (typeIt != localVariableStructTypes.end()) {
				structTypeName = typeIt->second;
			}

			// Local variables are stored as qd_stack_element_t, need to extract the value field
			llvm::Value* structPtrAlloca = it->second;
			// For indirect (captured) variables, load the actual storage pointer first
			if (indirectLocalVariables.find(varName) != indirectLocalVariables.end()) {
				structPtrAlloca = builder->CreateLoad(
						llvm::PointerType::getUnqual(*context), it->second, varName + "_storage");
			}
			llvm::Value* structValuePtr =
					builder->CreateStructGEP(stackElementTy, structPtrAlloca, 0, varName + "_value_ptr");
			structPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), structValuePtr, "struct_ptr");
		}

		// Find the field in the specific struct type if known
		const FieldInfo* matchingField = nullptr;

		if (!structTypeName.empty()) {
			// Look up field in the specific struct type
			auto structIt = structDefinitions.find(structTypeName);
			if (structIt != structDefinitions.end()) {
				for (const auto& field : structIt->second.fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
			}
		}

		// Fallback: search all struct types if we don't know the type
		if (!matchingField) {
			for (const auto& pair : structDefinitions) {
				for (const auto& field : pair.second.fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
				if (matchingField) {
					break;
				}
			}
		}

		if (!matchingField) {
			std::cerr << "Error: Unknown field in field set: " << fieldName << std::endl;
			return;
		}

		// Calculate field offset
		auto fieldOffset = builder->getInt64(matchingField->offset);
		auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

		// Define stack types
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::getUnqual(*context)}, false);
		llvm::Type* stackTy = llvm::StructType::get(*context,
				{
						llvm::PointerType::getUnqual(*context), // data
						builder->getInt64Ty(),					// size
						builder->getInt64Ty()					// capacity
				},
				false);

		// Pop value from stack
		llvm::Value* stackPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stackPtr, "st");

		// Get stack size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Decrement size
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		// Get element pointer
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::getUnqual(*context), dataPtr, "data");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, newSize, "elem_ptr");

		// Load value from stack element
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");

		// Store to struct field based on type
		if (matchingField->typeName == "f64") {
			llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "float_val");
			builder->CreateStore(floatValue, bytePtr);
		} else if (matchingField->typeName == "i64") {
			llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "int_val");
			builder->CreateStore(intValue, bytePtr);
		} else if (matchingField->typeName == "i32") {
			// Load as i64 from stack (stack elements are always 64-bit), truncate to i32
			llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "int_val");
			llvm::Value* truncValue = builder->CreateTrunc(intValue, builder->getInt32Ty(), "int32_val");
			builder->CreateStore(truncValue, bytePtr);
		} else if (matchingField->typeName == "ptr" || matchingField->typeName == "str" ||
				   matchingField->typeName.find('*') != std::string::npos ||
				   (!matchingField->typeName.empty() && std::isupper(matchingField->typeName[0]) &&
						   isKnownStruct(matchingField->typeName))) {
			// Pointer type (including ptr, str, raw pointers, and struct-typed fields)
			llvm::Value* ptrValue = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "ptr_val");
			builder->CreateStore(ptrValue, bytePtr);
		} else {
			// Type parameter or unknown type - treat as i64 value
			llvm::Value* intValue = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "generic_val");
			builder->CreateStore(intValue, bytePtr);
		}
	}

	// Generate array literal: create array, push elements, push array pointer
	void LlvmGenerator::Impl::generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral, llvm::Value* ctx) {
		const auto& elements = arrayLiteral->elements();
		size_t numElements = elements.size();

		if (numElements == 0) {
			// Empty array - create with default type (INT)
			llvm::Function* createArrayFn = module->getFunction("qd_array_create");
			if (!createArrayFn) {
				auto fnTy = llvm::FunctionType::get(
						llvm::PointerType::getUnqual(*context), {builder->getInt64Ty(), builder->getInt32Ty()}, false);
				createArrayFn =
						llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
			}
			llvm::Value* arrPtr =
					builder->CreateCall(createArrayFn, {builder->getInt64(8), builder->getInt32(0)}, "empty_arr");
			builder->CreateCall(pushPtrFn, {ctx, arrPtr});
			return;
		}

		// Determine array element type from first element
		// QD_ARRAY_TYPE_INT = 0, QD_ARRAY_TYPE_FLOAT = 1, QD_ARRAY_TYPE_STR = 2, QD_ARRAY_TYPE_PTR = 3
		int32_t arrayType = 0; // Default to INT
		IAstNode* firstElem = elements[0];
		if (firstElem->type() == IAstNode::Type::LITERAL) {
			auto* lit = static_cast<AstNodeLiteral*>(firstElem);
			if (lit->literalType() == AstNodeLiteral::LiteralType::FLOAT) {
				arrayType = 1; // FLOAT
			} else if (lit->literalType() == AstNodeLiteral::LiteralType::STRING) {
				arrayType = 2; // STR
			}
		}

		// Declare array functions if not already declared
		llvm::Function* createArrayFn = module->getFunction("qd_array_create");
		if (!createArrayFn) {
			auto fnTy = llvm::FunctionType::get(
					llvm::PointerType::getUnqual(*context), {builder->getInt64Ty(), builder->getInt32Ty()}, false);
			createArrayFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
		}

		llvm::Function* pushIntArrFn = module->getFunction("qd_array_push_int");
		if (!pushIntArrFn) {
			auto fnTy = llvm::FunctionType::get(
					builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context), builder->getInt64Ty()}, false);
			pushIntArrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_int", *module);
		}

		llvm::Function* pushFloatArrFn = module->getFunction("qd_array_push_float");
		if (!pushFloatArrFn) {
			auto fnTy = llvm::FunctionType::get(
					builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context), builder->getDoubleTy()}, false);
			pushFloatArrFn =
					llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_float", *module);
		}

		llvm::Function* pushPtrArrFn = module->getFunction("qd_array_push_ptr");
		if (!pushPtrArrFn) {
			auto fnTy = llvm::FunctionType::get(builder->getInt32Ty(),
					{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, false);
			pushPtrArrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_ptr", *module);
		}

		// Create array with initial capacity
		llvm::Value* arrPtr = builder->CreateCall(createArrayFn,
				{builder->getInt64(numElements), builder->getInt32(static_cast<uint32_t>(arrayType))}, "arr_ptr");

		// Push elements to the array
		for (IAstNode* elem : elements) {
			if (elem->type() == IAstNode::Type::LITERAL) {
				auto* lit = static_cast<AstNodeLiteral*>(elem);
				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					int64_t val = 0;
					safeParseInt64(lit->value(), val);
					if (arrayType == 1) {
						// Coerce int to float
						builder->CreateCall(pushFloatArrFn,
								{arrPtr, llvm::ConstantFP::get(builder->getDoubleTy(), static_cast<double>(val))});
					} else {
						builder->CreateCall(pushIntArrFn, {arrPtr, builder->getInt64(static_cast<uint64_t>(val))});
					}
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::FLOAT) {
					double val = std::stod(lit->value());
					if (arrayType == 0) {
						// Coerce float to int for int array
						builder->CreateCall(pushIntArrFn,
								{arrPtr, builder->getInt64(static_cast<uint64_t>(static_cast<int64_t>(val)))});
					} else {
						builder->CreateCall(
								pushFloatArrFn, {arrPtr, llvm::ConstantFP::get(builder->getDoubleTy(), val)});
					}
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::STRING) {
					// Create string constant
					std::string strVal = lit->value();
					// Remove quotes if present
					if (strVal.size() >= 2 && strVal.front() == '"' && strVal.back() == '"') {
						strVal = strVal.substr(1, strVal.size() - 2);
					}
					// Create qd_string and push
					llvm::Function* createStrFn = module->getFunction("qd_string_create");
					if (!createStrFn) {
						auto fnTy = llvm::FunctionType::get(llvm::PointerType::getUnqual(*context),
								{llvm::PointerType::getUnqual(*context)}, false);
						createStrFn = llvm::Function::Create(
								fnTy, llvm::Function::ExternalLinkage, "qd_string_create", *module);
					}
					// Ensure qdStringReleaseFn is available
					if (!qdStringReleaseFn) {
						auto qdStringReleaseFnTy = llvm::FunctionType::get(
								builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
						qdStringReleaseFn = llvm::Function::Create(
								qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
					}
					llvm::Value* strConstant = builder->CreateGlobalString(strVal, "arr_str");
					llvm::Value* qdStr = builder->CreateCall(createStrFn, {strConstant}, "qd_str");
					builder->CreateCall(pushPtrArrFn, {arrPtr, qdStr});
					// Release our reference since array now owns it
					builder->CreateCall(qdStringReleaseFn, {qdStr});
				}
			}
		}

		// Push array pointer onto the Quadrate stack
		builder->CreateCall(pushPtrFn, {ctx, arrPtr});

		// Mark that the last pushed value was an array
		lastPushedWasArray = true;
	}

	void LlvmGenerator::Impl::pushDeferScope() {
		deferScopeStack.push_back(std::vector<AstNodeDefer*>());
	}

	void LlvmGenerator::Impl::popDeferScope() {
		if (!deferScopeStack.empty()) {
			deferScopeStack.pop_back();
		}
	}

	void LlvmGenerator::Impl::executeDeferScope(llvm::Value* ctx) {
		if (deferScopeStack.empty()) {
			return;
		}

		auto& currentScope = deferScopeStack.back();
		// Execute defers in REVERSE order (LIFO)
		for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
			AstNodeDefer* deferNode = *it;
			// Generate defer body
			for (size_t i = 0; i < deferNode->childCount(); i++) {
				IAstNode* child = deferNode->child(i);
				// If the child is a block, generate its children directly
				if (child && child->type() == IAstNode::Type::BLOCK) {
					for (size_t j = 0; j < child->childCount(); j++) {
						generateNode(child->child(j), ctx);
					}
				} else {
					generateNode(child, ctx);
				}
			}
		}

		deferScopeStack.pop_back();
	}

	std::string LlvmGenerator::Impl::findLastStructConstruction(IAstNode* node) {
		if (!node) {
			return "";
		}

		std::string result;

		// Check if this node is a struct construction (identifier that's a struct name)
		if (auto* ident = dynamic_cast<AstNodeIdentifier*>(node)) {
			if (structDefinitions.find(ident->name()) != structDefinitions.end()) {
				result = ident->name();
			}
		}

		// Check if this node is an explicit struct construction ('new StructName')
		if (auto* construct = dynamic_cast<AstNodeStructConstruction*>(node)) {
			result = construct->structName();
		}

		// Recursively search children
		for (size_t i = 0; i < node->childCount(); i++) {
			std::string childResult = findLastStructConstruction(node->child(i));
			if (!childResult.empty()) {
				result = childResult; // Keep the last one found
			}
		}

		return result;
	}

} // namespace Qd
