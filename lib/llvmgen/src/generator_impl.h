// Internal implementation header for LlvmGenerator
// Not part of public API - only for use by generator_*.cc files

#ifndef QD_LLVMGEN_GENERATOR_IMPL_H
#define QD_LLVMGEN_GENERATOR_IMPL_H

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
#include <fstream>
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
		std::map<std::string, bool> fallibleFunctions;
		std::set<std::string> importedCFunctions;
		std::map<std::string, std::string> functionReturnStructType;

		// Cross-module imported function info
		struct CrossModuleImportInfo {
			std::string library;
			std::string cFunctionName;
			bool throws;
		};

		std::map<std::string, CrossModuleImportInfo> crossModuleImportedFunctions;

		// Module constants
		std::map<std::string, std::string> moduleConstants;

		// Module ASTs
		std::vector<std::pair<std::string, IAstNode*>> moduleASTs;

		// Module source files for debug info
		std::map<std::string, std::string> moduleSourceFiles;

		// Per-module debug files
		std::map<std::string, llvm::DIFile*> moduleDebugFiles;

		// Imported libraries for linking
		std::set<std::string> importedLibraries;

		// Additional library search paths
		std::vector<std::string> librarySearchPaths;

		// Function context
		llvm::BasicBlock* currentFunctionReturnBlock = nullptr;
		bool currentFunctionIsFallible = false;
		bool currentFunctionIsIntegerOnly = false;

		// Current module prefix
		std::string currentModulePrefix = "main";

		// The main module name
		std::string mainModuleName = "main";

		// Defer statements
		std::vector<std::vector<AstNodeDefer*>> deferScopeStack;

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
		};

		struct StructLayout {
			std::string name;
			std::vector<FieldInfo> fields;
			size_t totalSize;
			bool isPublic;
		};

		std::map<std::string, StructLayout> structDefinitions;
		std::map<std::string, llvm::Function*> structDestructors;
		std::string currentModuleName;

		// Test mode
		bool testMode = false;
		std::vector<std::pair<std::string, std::string>> collectedTestNames;
		llvm::Value* testErrorAlloca = nullptr;

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
		bool generateProgram(IAstNode* root);
		bool generateFunction(
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

		// Control flow
		void generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx);
		void generateFor(AstNodeForStatement* forStmt, llvm::Value* ctx);
		void generateWhile(AstNodeWhileStatement* whileStmt, llvm::Value* ctx);
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

		// Inline stack operations
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
		void generateInlineDup(llvm::Value* ctx);
		void generateInlineSwap(llvm::Value* ctx);
		void generateInlineDrop(llvm::Value* ctx);
		void generateInlineOver(llvm::Value* ctx);
		void generateInlineRot(llvm::Value* ctx);

		// Type-aware operations
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

		// Defer scope management
		void pushDeferScope();
		void popDeferScope();
		void executeDeferScope(llvm::Value* ctx);
	};

} // namespace Qd

#endif // QD_LLVMGEN_GENERATOR_IMPL_H
