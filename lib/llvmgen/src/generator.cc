#include <llvmgen/generator.h>

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
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

#include <qc/ast_node.h>
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
#include <qc/ast_node_return.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
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

		// Optimization level (0-3)
		int optimizationLevel = 0;

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

		// Loop context for break/continue
		struct LoopContext {
			llvm::BasicBlock* breakTarget;
			llvm::BasicBlock* continueTarget;
		};

		std::vector<LoopContext> loopStack;

		// User-defined functions
		std::map<std::string, llvm::Function*> userFunctions;
		std::map<std::string, bool> fallibleFunctions; // Track which functions can throw errors

		// Module constants (scope::name -> value)
		std::map<std::string, std::string> moduleConstants;

		// Module ASTs to include (preserves insertion order for dependency resolution)
		std::vector<std::pair<std::string, IAstNode*>> moduleASTs;

		// Track imported libraries for linking
		std::set<std::string> importedLibraries;

		// Track additional library search paths (for third-party packages)
		std::vector<std::string> librarySearchPaths;

		// Function context for return
		llvm::BasicBlock* currentFunctionReturnBlock = nullptr;
		bool currentFunctionIsFallible = false;

		// Defer statements collected during function generation
		std::vector<AstNodeDefer*> currentDeferStatements;

		// Counter for unique variable names
		int varCounter = 0;

		// Local variables (per function scope): name -> alloca instruction
		std::map<std::string, llvm::AllocaInst*> localVariables;

		Impl(const std::string& moduleName) {
			context = std::make_unique<llvm::LLVMContext>();
			module = std::make_unique<llvm::Module>(moduleName, *context);
			builder = std::make_unique<llvm::IRBuilder<>>(*context);
		}

		void setupRuntimeDeclarations();
		bool generateProgram(IAstNode* root);
		bool generateFunction(
				AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix = "main");
		void generateNode(IAstNode* node, llvm::Value* ctx, llvm::Value* forIterVar = nullptr);
		void generateInstruction(AstNodeInstruction* inst, llvm::Value* ctx);
		void generateLiteral(AstNodeLiteral* lit, llvm::Value* ctx);
		void generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx, llvm::Value* forIterVar);
		void generateFor(AstNodeForStatement* forStmt, llvm::Value* ctx);
		void generateLoop(AstNodeLoopStatement* loopStmt, llvm::Value* ctx);
		void generateCtxBlock(AstNodeCtx* ctxNode, llvm::Value* ctx, llvm::Value* forIterVar);
		void generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx, llvm::Value* forIterVar);
		void generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr, llvm::Value* ctx);
		void generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx);
		void generateSwitchStatement(AstNodeSwitchStatement* switchStmt, llvm::Value* ctx, llvm::Value* forIterVar);
		void generateLocal(AstNodeLocal* local, llvm::Value* ctx);
		void generateLocalCleanup();
		void generateCastInstructions(const std::vector<CastDirection>& casts, llvm::Value* ctx);
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
						builder->getInt1Ty()   // is_error_tainted
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

		// qd_push_call(qd_context* ctx, const char* func_name) -> void
		auto pushCallFnTy = llvm::FunctionType::get(
				builder->getVoidTy(), {contextPtrTy, llvm::PointerType::getUnqual(*context)}, false);
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

			// Create complete qd_context struct definition for debugging
			// This allows GDB to inspect ctx members like ctx->st->size

			// First create basic debug types
			auto int64Type = debugBuilder->createBasicType("int64_t", 64, llvm::dwarf::DW_ATE_signed);
			auto charPtrType = debugBuilder->createPointerType(
					debugBuilder->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char), 64);
			auto intType = debugBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);
			auto sizeType = debugBuilder->createBasicType("size_t", 64, llvm::dwarf::DW_ATE_unsigned);

			// Create qd_stack_element_t union (simplified - just show as 64-bit int)
			auto stackElemType = debugBuilder->createStructType(compileUnit, "qd_stack_element_t", debugFile, 0, 128,
					64, llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray({}));

			// Create qd_stack struct with data, capacity, size
			llvm::SmallVector<llvm::Metadata*, 3> stackFields;
			auto stackElemPtrType = debugBuilder->createPointerType(stackElemType, 64);
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "data", debugFile, 0, 64, 64, 0, llvm::DINode::FlagZero, stackElemPtrType));
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "capacity", debugFile, 0, 64, 64, 64, llvm::DINode::FlagZero, sizeType));
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "size", debugFile, 0, 64, 64, 128, llvm::DINode::FlagZero, sizeType));

			auto stackType = debugBuilder->createStructType(compileUnit, "qd_stack", debugFile, 0, 192, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(stackFields));

			auto stackPtrType = debugBuilder->createPointerType(stackType, 64);

			// Create qd_context struct
			llvm::SmallVector<llvm::Metadata*, 8> contextFields;

			// qd_stack* st
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "st", debugFile, 0, 64, 64, 0, llvm::DINode::FlagZero, stackPtrType));

			// int64_t error_code
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "error_code", debugFile, 0, 64, 64, 64, llvm::DINode::FlagZero, int64Type));

			// char* error_msg
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "error_msg", debugFile, 0, 64, 64, 128, llvm::DINode::FlagZero, charPtrType));

			// int argc
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "argc", debugFile, 0, 32, 32, 192, llvm::DINode::FlagZero, intType));

			// char** argv
			auto charPtrPtrType = debugBuilder->createPointerType(charPtrType, 64);
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "argv", debugFile, 0, 64, 64, 256, llvm::DINode::FlagZero, charPtrPtrType));

			// char* program_name
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "program_name", debugFile, 0, 64, 64, 320, llvm::DINode::FlagZero, charPtrType));

			// const char* call_stack[256] - array at offset 48 bytes = 384 bits
			auto callStackArrayType =
					debugBuilder->createArrayType(16384, // Size in bits: 256 elements * 64 bits = 16384 bits
							64,							 // Alignment in bits
							charPtrType,				 // Element type
							debugBuilder->getOrCreateArray({
									debugBuilder->getOrCreateSubrange(0, 255) // 256 elements (0-255)
							}));
			contextFields.push_back(debugBuilder->createMemberType(compileUnit, "call_stack", debugFile, 0, 16384, 64,
					384, llvm::DINode::FlagZero, callStackArrayType));

			// size_t call_stack_depth - at offset 2096 bytes = 16768 bits
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "call_stack_depth", debugFile, 0, 64, 64, 16768, llvm::DINode::FlagZero, sizeType));

			contextDebugType = debugBuilder->createStructType(compileUnit, "qd_context", debugFile, 0, 16832, 64,
					llvm::DINode::FlagZero, nullptr, // Total: 2104 bytes = 16832 bits
					debugBuilder->getOrCreateArray(contextFields));
		}
	}

	void LlvmGenerator::Impl::generateLiteral(AstNodeLiteral* lit, llvm::Value* ctx) {
		auto type = lit->literalType();
		const auto& value = lit->value();

		switch (type) {
		case AstNodeLiteral::LiteralType::INTEGER: {
			auto val = builder->getInt64(static_cast<uint64_t>(std::stoll(value)));
			builder->CreateCall(pushIntFn, {ctx, val});
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

		if (name == "prints") {
			builder->CreateCall(printsFn, {ctx});
		} else if (name == "nl") {
			builder->CreateCall(nlFn, {ctx});
		} else {
			// Map instruction name to runtime function name
			std::string fnName;
			if (name == ".") {
				fnName = "qd_print";
			} else if (name == "+") {
				fnName = "qd_add";
			} else if (name == "-") {
				fnName = "qd_sub";
			} else if (name == "*") {
				fnName = "qd_mul";
			} else if (name == "/") {
				fnName = "qd_div";
			} else if (name == "%") {
				fnName = "qd_mod";
			} else if (name == ">") {
				fnName = "qd_gt";
			} else if (name == "<") {
				fnName = "qd_lt";
			} else if (name == ">=") {
				fnName = "qd_gte";
			} else if (name == "<=") {
				fnName = "qd_lte";
			} else if (name == "==") {
				fnName = "qd_eq";
			} else if (name == "!=") {
				fnName = "qd_neq";
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

			// Special handling for 'error' instruction in fallible functions
			// After calling qd_error, we need to return immediately to prevent further execution
			if (name == "error" && currentFunctionIsFallible && currentFunctionReturnBlock) {
				builder->CreateBr(currentFunctionReturnBlock);
			}
		}
	}

	void LlvmGenerator::Impl::generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx, llvm::Value* forIterVar) {
		const std::string& name = ident->name();

		// Check if it's a local variable
		auto localIt = localVariables.find(name);
		if (localIt != localVariables.end()) {
			// Load from local variable and push to runtime stack
			llvm::AllocaInst* localAlloca = localIt->second;

			// Extract type field (field index 1 in qd_stack_element_t)
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, localAlloca, 1, name + "_type_ptr");
			llvm::Value* type = builder->CreateLoad(builder->getInt32Ty(), typePtr, name + "_type");

			// Switch on type and push appropriate value
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, localAlloca, 0, name + "_value_ptr");

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

			// INT block: load i64 and push
			builder->SetInsertPoint(intBlock);
			llvm::Value* intVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, name + "_i");
			builder->CreateCall(pushIntFn, {ctx, intVal});
			builder->CreateBr(endBlock);

			// FLOAT block: load double and push
			builder->SetInsertPoint(floatBlock);
			llvm::Value* floatVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_f");
			builder->CreateCall(pushFloatFn, {ctx, floatVal});
			builder->CreateBr(endBlock);

			// STR block: load char* and push
			builder->SetInsertPoint(strBlock);
			llvm::Value* strVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_s");
			builder->CreateCall(pushStrFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block: load void* and push
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_p");
			builder->CreateCall(pushPtrFn, {ctx, ptrVal});
			builder->CreateBr(endBlock);

			// Continue after type switch
			builder->SetInsertPoint(endBlock);
			return;
		}

		// Check if it's the loop iterator variable ($)
		if (name == "$" && forIterVar) {
			// Push loop iterator as integer
			builder->CreateCall(pushIntFn, {ctx, forIterVar});
			return;
		}

		// Check if it's a user-defined function call
		auto it = userFunctions.find(name);
		if (it != userFunctions.end()) {
			// Generate any needed type casts before the function call
			generateCastInstructions(ident->parameterCasts(), ctx);

			builder->CreateCall(it->second, {ctx});

			// Check if this function is fallible
			auto fallibleIt = fallibleFunctions.find(name);
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

					// Clear the error flag
					builder->CreateStore(builder->getInt64(0), errorCodePtr);

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

		// Not a constant, must be a function
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

		// Call the scoped function
		builder->CreateCall(fn, {ctx});

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
			} else {
				// No operator or ? operator: push error status
				if (scopedIdent->abortOnError()) {
					// ! operator: check error and abort if set
					llvm::BasicBlock* errorBlock =
							llvm::BasicBlock::Create(*context, "error_abort", builder->GetInsertBlock()->getParent());
					llvm::BasicBlock* continueBlock =
							llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());

					builder->CreateCondBr(hasError, errorBlock, continueBlock);

					// Error block: print message and abort
					builder->SetInsertPoint(errorBlock);
					llvm::Value* errorMsg =
							builder->CreateGlobalString("Fatal error: function '" + fullName + "' failed\n");
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

					// Clear the error flag
					builder->CreateStore(builder->getInt64(0), errorCodePtr);

					// Push the success status onto the stack
					builder->CreateCall(pushIntFn, {ctx, successStatus});
				}
			}
		}
	}

	void LlvmGenerator::Impl::generateSwitchStatement(
			AstNodeSwitchStatement* switchStmt, llvm::Value* ctx, llvm::Value* forIterVar) {
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
		auto switchElem = builder->CreateAlloca(switchElemTy, nullptr, "switch_elem");
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
					auto caseVal = builder->getInt64(static_cast<uint64_t>(std::stoll(lit->value())));
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

					// Get switch string value
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
					auto switchStrPtr =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "switch_str");

					// Create case string constant
					auto caseStr = builder->CreateGlobalString(lit->value().substr(1, lit->value().length() - 2));

					// Call strcmp
					auto cmpResult = builder->CreateCall(strcmpFn, {switchStrPtr, caseStr}, "strcmp_result");
					matches = builder->CreateICmpEQ(cmpResult, builder->getInt32(0), "case_match");
				}
			} else if (caseValue->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
				// Handle scoped constants (module::ConstName)
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(caseValue);
				std::string constName = scoped->scope() + "_" + scoped->name();

				// Get the constant global variable
				auto constGlobal = module->getNamedGlobal(constName);
				if (constGlobal) {
					auto constType = constGlobal->getValueType();
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");

					if (constType->isIntegerTy(64)) {
						// Integer constant
						auto switchVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "switch_val");
						auto caseVal = builder->CreateLoad(builder->getInt64Ty(), constGlobal, "const_val");
						matches = builder->CreateICmpEQ(switchVal, caseVal, "case_match");
					} else if (constType->isDoubleTy()) {
						// Float constant
						auto switchVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "switch_val_f");
						auto caseVal = builder->CreateLoad(builder->getDoubleTy(), constGlobal, "const_val");
						matches = builder->CreateFCmpOEQ(switchVal, caseVal, "case_match");
					} else if (constType->isPointerTy()) {
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
						auto caseStrPtr =
								builder->CreateLoad(llvm::PointerType::getUnqual(*context), constGlobal, "const_str");
						auto cmpResult = builder->CreateCall(strcmpFn, {switchStrPtr, caseStrPtr}, "strcmp_result");
						matches = builder->CreateICmpEQ(cmpResult, builder->getInt32(0), "case_match");
					}
				}
			}

			if (matches) {
				builder->CreateCondBr(matches, caseBB, nextCaseBB);

				// Generate case body
				builder->SetInsertPoint(caseBB);
				if (caseNode->body()) {
					generateNode(caseNode->body(), ctx, forIterVar);
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
					generateNode(caseNode->body(), ctx, forIterVar);
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

		// Free string block
		builder->SetInsertPoint(freeStringBB);
		auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
		auto strPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, "str_ptr");
		auto freeFn = module->getFunction("free");
		if (!freeFn) {
			auto freeFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
		}
		builder->CreateCall(freeFn, {strPtr});
		builder->CreateBr(skipFreeBB);

		// Skip free block
		builder->SetInsertPoint(skipFreeBB);
	}

	void LlvmGenerator::Impl::generateLocal(AstNodeLocal* local, llvm::Value* ctx) {
		const std::string& name = local->name();

		// Check if this variable already exists (reuse the alloca if so)
		llvm::AllocaInst* localAlloca;
		auto it = localVariables.find(name);
		bool isNewVariable = (it == localVariables.end());
		if (isNewVariable) {
			// Create alloca for the stack element in the entry block
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
			localAlloca = tmpBuilder.CreateAlloca(stackElementTy, nullptr, name);

			// Store in local variables map
			localVariables[name] = localAlloca;

			// Add debug info for the local variable
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty()) {
				// Create a pointer type to qd_stack_element_t
				auto stackElemPtrType = debugBuilder->createPointerType(
						debugBuilder->createBasicType("qd_stack_element_t", 128, llvm::dwarf::DW_ATE_unsigned), 64);

				// Create local variable debug info
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (current function)
						name,															 // Variable name
						debugFile,														 // File
						static_cast<unsigned>(local->line()),							 // Line number
						stackElemPtrType,												 // Type
						true															 // Always preserve
				);

				// Insert declare to make it visible in debugger
				debugBuilder->insertDeclare(localAlloca,  // Storage (the alloca)
						localVar,						  // Variable
						debugBuilder->createExpression(), // Expression
						llvm::DILocation::get(
								*context, static_cast<unsigned>(local->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}
		} else {
			// Variable already exists, reuse it
			localAlloca = it->second;
		}

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
		builder->CreateCall(stackPopFn, {stackPtr, localAlloca});

		// TODO: Check result for errors (0 = success, non-zero = error)
		// For now, we assume success
	}

	void LlvmGenerator::Impl::generateLocalCleanup() {
		// Free any string locals to prevent memory leaks
		// Iterate through all local variables and check their type
		for (const auto& pair : localVariables) {
			const std::string& varName = pair.first;
			llvm::AllocaInst* localAlloca = pair.second;

			// Load the type field to check if it's a string
			llvm::Value* typePtr =
					builder->CreateStructGEP(stackElementTy, localAlloca, 1, varName + "_cleanup_type_ptr");
			llvm::Value* type = builder->CreateLoad(builder->getInt32Ty(), typePtr, varName + "_cleanup_type");

			// Create basic blocks for conditional free
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* freeStrBlock = llvm::BasicBlock::Create(*context, varName + "_free_str", currentFn);
			llvm::BasicBlock* skipFreeBlock = llvm::BasicBlock::Create(*context, varName + "_skip_free", currentFn);

			// Check if type == QD_STACK_TYPE_STR (3)
			llvm::Value* isString = builder->CreateICmpEQ(type, builder->getInt32(3), varName + "_is_str");
			builder->CreateCondBr(isString, freeStrBlock, skipFreeBlock);

			// Free string block
			builder->SetInsertPoint(freeStrBlock);
			llvm::Value* valuePtr =
					builder->CreateStructGEP(stackElementTy, localAlloca, 0, varName + "_cleanup_value_ptr");
			llvm::Value* strPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, varName + "_cleanup_str");

			// Call free() on the string
			// Create free function declaration if not exists
			llvm::Function* freeFn = module->getFunction("free");
			if (!freeFn) {
				auto freeFnTy =
						llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
				freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(freeFn, {strPtr});
			builder->CreateBr(skipFreeBlock);

			// Skip free block
			builder->SetInsertPoint(skipFreeBlock);
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

	void LlvmGenerator::Impl::generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx, llvm::Value* forIterVar) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Create basic blocks
		llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "if.then", currentFn);
		llvm::BasicBlock* elseBB =
				ifStmt->elseBody() ? llvm::BasicBlock::Create(*context, "if.else", currentFn) : nullptr;
		llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "if.merge", currentFn);

		// We need to access ctx->st to pop from the stack
		// ctx is a pointer to a struct with first field being qd_stack* st
		// Cast ctx to the correct pointer type and load the stack field
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

		// Allocate space for the popped element
		auto elemPtr = builder->CreateAlloca(stackElementTy, nullptr, "cond_elem");

		// Pop the condition value from the stack
		builder->CreateCall(stackPopFn, {stack, elemPtr});

		// Access the value field: elem.value.i
		// stackElementTy layout: { i64 value, i32 type, i1 is_error_tainted }
		// Get the value field (index 0) as i64, then truncate to i32
		auto valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		auto value64 = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "value64");
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
			generateNode(ifStmt->thenBody(), ctx, forIterVar);
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
				generateNode(ifStmt->elseBody(), ctx, forIterVar);
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

		auto stepElemPtr = builder->CreateAlloca(stackElementTy, nullptr, "step_elem");
		auto endElemPtr = builder->CreateAlloca(stackElementTy, nullptr, "end_elem");
		auto startElemPtr = builder->CreateAlloca(stackElementTy, nullptr, "start_elem");

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

		auto cond = builder->CreateICmpSLT(iterVar, endValue, "cmp");
		builder->CreateCondBr(cond, loopBodyBB, loopExitBB);

		// Loop body
		builder->SetInsertPoint(loopBodyBB);

		// Push loop context for break/continue
		loopStack.push_back({loopExitBB, loopIncBB});

		if (forStmt->body()) {
			generateNode(forStmt->body(), ctx, iterVar);
		}

		loopStack.pop_back();

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

		// Loop increment
		builder->SetInsertPoint(loopIncBB);
		auto nextIter = builder->CreateAdd(iterVar, stepValue, "next_i");
		iterVar->addIncoming(nextIter, loopIncBB);
		builder->CreateBr(loopHeaderBB);

		// Continue after loop
		builder->SetInsertPoint(loopExitBB);
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

		if (loopStmt->body()) {
			generateNode(loopStmt->body(), ctx, nullptr);
		}

		loopStack.pop_back();

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

		// Continue after loop (only reached via break)
		builder->SetInsertPoint(loopExitBB);
	}

	void LlvmGenerator::Impl::generateCtxBlock(AstNodeCtx* ctxNode, llvm::Value* ctx, llvm::Value* forIterVar) {
		// Clone the parent context
		auto clonedCtx = builder->CreateCall(cloneContextFn, {ctx}, "cloned_ctx");

		// Execute the block with the cloned context
		for (size_t i = 0; i < ctxNode->childCount(); i++) {
			generateNode(ctxNode->child(i), clonedCtx, forIterVar);
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
		auto strValue = builder->CreateIntToPtr(resultValue, llvm::PointerType::getUnqual(*context), "str_value");
		builder->CreateCall(pushStrFn, {ctx, strValue});
		// Free the popped string from cloned context (qd_push_s has duplicated it, so we own the original)
		// When we popped with non-NULL element, the string wasn't freed, so we must free it manually
		llvm::Function* freeFn = module->getFunction("free");
		if (!freeFn) {
			auto freeFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
		}
		builder->CreateCall(freeFn, {strValue});
		builder->CreateBr(pushDoneBB);

		// Free the cloned context AFTER pushing (qd_push_s has now duplicated the string)
		builder->SetInsertPoint(pushDoneBB);
		builder->CreateCall(freeContextFn, {clonedCtx});

		// Continue after push
		builder->SetInsertPoint(pushDoneBB);
	}

	void LlvmGenerator::Impl::generateNode(IAstNode* node, llvm::Value* ctx, llvm::Value* forIterVar) {
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
			generateIf(static_cast<AstNodeIfStatement*>(node), ctx, forIterVar);
			break;
		case IAstNode::Type::FOR_STATEMENT:
			generateFor(static_cast<AstNodeForStatement*>(node), ctx);
			break;
		case IAstNode::Type::LOOP_STATEMENT:
			generateLoop(static_cast<AstNodeLoopStatement*>(node), ctx);
			break;
		case IAstNode::Type::SWITCH_STATEMENT:
			generateSwitchStatement(static_cast<AstNodeSwitchStatement*>(node), ctx, forIterVar);
			break;
		case IAstNode::Type::BREAK_STATEMENT:
			// Break from current loop
			if (!loopStack.empty()) {
				builder->CreateBr(loopStack.back().breakTarget);
			}
			break;
		case IAstNode::Type::CONTINUE_STATEMENT:
			// Continue to next iteration
			if (!loopStack.empty()) {
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
			// Collect defer statement for later execution at function end
			currentDeferStatements.push_back(static_cast<AstNodeDefer*>(node));
			// Don't generate code now - will be generated in return block
			break;
		case IAstNode::Type::CTX_STATEMENT:
			generateCtxBlock(static_cast<AstNodeCtx*>(node), ctx, forIterVar);
			break;
		case IAstNode::Type::IDENTIFIER:
			generateIdentifier(static_cast<AstNodeIdentifier*>(node), ctx, forIterVar);
			break;
		case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
			generateFunctionPointer(static_cast<AstNodeFunctionPointerReference*>(node), ctx);
			break;
		case IAstNode::Type::BLOCK:
			// For blocks, just recursively generate all children
			for (size_t i = 0; i < node->childCount(); i++) {
				generateNode(node->child(i), ctx, forIterVar);
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
		case IAstNode::Type::SCOPED_IDENTIFIER:
			generateScopedIdentifier(static_cast<AstNodeScopedIdentifier*>(node), ctx);
			break;
		case IAstNode::Type::USE_STATEMENT:
			// Use statements are handled during program generation, not during execution
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
						debugFile,											// File
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
			auto stackSize = builder->getInt64(DEFAULT_STACK_SIZE);
			auto ctx = builder->CreateCall(createContextFn, {stackSize}, "ctx");

			// Add debug info for ctx local variable in main
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && contextDebugType) {
				// Create pointer to qd_context struct
				auto ctxPtrType = debugBuilder->createPointerType(contextDebugType, 64);

				// Create local variable for ctx
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (main function)
						"ctx",															 // Name
						debugFile,														 // File
						static_cast<unsigned>(funcNode->line()),						 // Line
						ctxPtrType,														 // Type
						true															 // Always preserve
				);

				// Insert declare to make it visible in debugger
				debugBuilder->insertDeclare(ctx,		  // Storage
						localVar,						  // Variable
						debugBuilder->createExpression(), // Expression
						llvm::DILocation::get(
								*context, static_cast<unsigned>(funcNode->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}

			// Push "main::main" onto call stack for debugging
			std::string fullFuncName = namePrefix + "::" + funcNode->name();
			auto funcNameStr = builder->CreateGlobalString(fullFuncName);
			builder->CreateCall(pushCallFn, {ctx, funcNameStr});

			// Generate function body
			auto body = funcNode->body();
			if (body) {
				generateNode(body, ctx, nullptr);
			}

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
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);

			// Add debug info for user function
			if (debugInfoEnabled && debugBuilder) {
				auto funcType = debugBuilder->createSubroutineType(debugBuilder->getOrCreateTypeArray({}));
				auto subprogram = debugBuilder->createFunction(compileUnit, // Scope
						funcNode->name(),									// Name
						fnName.c_str(),										// Linkage name
						debugFile,											// File
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
			auto ctx = fn->getArg(0);
			ctx->setName("ctx");

			// Add debug info for ctx parameter
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && contextDebugType) {
				// Create pointer to qd_context struct
				auto ctxPtrType = debugBuilder->createPointerType(contextDebugType, 64);

				// Create parameter variable for ctx
				auto paramVar =
						debugBuilder->createParameterVariable(debugScopeStack.back(), // Scope (current function)
								"ctx",												  // Name
								1,													  // Argument number
								debugFile,											  // File
								static_cast<unsigned>(funcNode->line()),			  // Line
								ctxPtrType,											  // Type
								true												  // Always preserve
						);

				// Insert declare to make it visible in debugger
				debugBuilder->insertDeclare(ctx,		  // Storage
						paramVar,						  // Variable
						debugBuilder->createExpression(), // Expression
						llvm::DILocation::get(
								*context, static_cast<unsigned>(funcNode->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}

			// Push function name onto call stack for debugging
			std::string fullFuncName = namePrefix + "::" + funcNode->name();
			auto funcNameStr = builder->CreateGlobalString(fullFuncName);
			builder->CreateCall(pushCallFn, {ctx, funcNameStr});

			// Generate type check for input parameters
			if (!funcNode->inputParameters().empty()) {
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

			// Set the return target for this function
			currentFunctionReturnBlock = returnBB;
			currentFunctionIsFallible = funcNode->throws();

			// Clear defer statements from any previous function
			currentDeferStatements.clear();

			// Generate function body
			auto body = funcNode->body();
			if (body) {
				generateNode(body, ctx, nullptr);
			}

			// Clear return target
			currentFunctionReturnBlock = nullptr;
			currentFunctionIsFallible = false;

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

			// Generate return block - this is where defers execute
			builder->SetInsertPoint(returnBB);

			// Execute defer statements in REVERSE order (LIFO)
			for (auto it = currentDeferStatements.rbegin(); it != currentDeferStatements.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				// Generate defer body
				for (size_t i = 0; i < deferNode->childCount(); i++) {
					IAstNode* child = deferNode->child(i);
					// If the child is a block, generate its children directly
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (size_t j = 0; j < child->childCount(); j++) {
							generateNode(child->child(j), ctx, nullptr);
						}
					} else {
						generateNode(child, ctx, nullptr);
					}
				}
			}

			// Clear defer statements after use
			currentDeferStatements.clear();

			// Clean up local variables (free strings)
			generateLocalCleanup();

			// Pop function from call stack before returning
			builder->CreateCall(popCallFn, {ctx});

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
							if (library.rfind("libstd", 0) == 0 &&
							    (library.find("qd_static.a") != std::string::npos || library.find("qd.so") != std::string::npos)) {
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
				}
			}
		}

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
						if (library.rfind("libstd", 0) == 0 &&
						    (library.find("qd_static.a") != std::string::npos || library.find("qd.so") != std::string::npos)) {
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

		// Second pass: generate all user-defined functions from main file (not main)
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				if (funcNode->name() != "main") {
					if (!generateFunction(funcNode, false)) {
						return false;
					}
				}
			}
		}

		// Second pass: generate main function
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

		return true;
	}

	// LlvmGenerator implementation

	LlvmGenerator::LlvmGenerator() : impl(nullptr) {
	}

	LlvmGenerator::~LlvmGenerator() = default;

	void LlvmGenerator::setDebugInfo(bool enabled) {
		if (!impl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			impl = std::make_unique<Impl>("temp");
		}
		impl->debugInfoEnabled = enabled;
	}

	void LlvmGenerator::setOptimizationLevel(int level) {
		if (!impl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			impl = std::make_unique<Impl>("temp");
		}
		// Clamp level to 0-3
		if (level < 0) {
			level = 0;
		}
		if (level > 3) {
			level = 3;
		}
		impl->optimizationLevel = level;
	}

	void LlvmGenerator::addLibrarySearchPath(const std::string& path) {
		if (!impl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			impl = std::make_unique<Impl>("temp");
		}
		impl->librarySearchPaths.push_back(path);
	}

	bool LlvmGenerator::generate(IAstNode* root, const std::string& moduleName) {
		if (!impl) {
			impl = std::make_unique<Impl>(moduleName);
		}
		// Store source filename for debug info
		// If moduleName looks like a file path (contains / or ends with .qd), use it directly
		// Otherwise append .qd extension
		if (moduleName.find('/') != std::string::npos || moduleName.find('\\') != std::string::npos ||
				(moduleName.size() > 3 && moduleName.substr(moduleName.size() - 3) == ".qd")) {
			impl->sourceFileName = moduleName;
		} else {
			impl->sourceFileName = moduleName + ".qd";
		}
		return impl->generateProgram(root);
	}

	void LlvmGenerator::addModuleAST(const std::string& moduleName, IAstNode* moduleRoot) {
		if (!impl) {
			impl = std::make_unique<Impl>("quadrate_module");
		}
		impl->moduleASTs.push_back({moduleName, moduleRoot});
	}

	std::string LlvmGenerator::getIRString() const {
		if (!impl || !impl->module) {
			return "";
		}

		std::string str;
		llvm::raw_string_ostream os(str);
		impl->module->print(os, nullptr);
		return str;
	}

	bool LlvmGenerator::writeIR(const std::string& filename) {
		if (!impl || !impl->module) {
			return false;
		}

		std::error_code ec;
		llvm::raw_fd_ostream os(filename, ec);
		if (ec) {
			std::cerr << "Error opening file: " << ec.message() << std::endl;
			return false;
		}

		impl->module->print(os, nullptr);
		return true;
	}

	bool LlvmGenerator::writeObject(const std::string& filename) {
		if (!impl || !impl->module) {
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
		impl->module->setTargetTriple(targetTriple);

		std::string error;
		auto target = llvm::TargetRegistry::lookupTarget(targetTripleStr, error);
		if (!target) {
			std::cerr << "Error: " << error << std::endl;
			return false;
		}

		auto cpu = "generic";
		auto features = "";
		llvm::TargetOptions opt;
		std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(
				targetTriple, cpu, features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_)));

		impl->module->setDataLayout(targetMachine->createDataLayout());

		// Run optimization passes if optimization level > 0
		if (impl->optimizationLevel > 0) {
			// Use legacy PassManager for optimization passes
			llvm::legacy::FunctionPassManager fpm(impl->module.get());
			llvm::legacy::PassManager mpm;

			// Add function-level optimization passes based on level
			if (impl->optimizationLevel >= 1) {
				// Basic optimizations
				fpm.add(llvm::createPromoteMemoryToRegisterPass()); // mem2reg
				fpm.add(llvm::createInstructionCombiningPass());	// instcombine
				fpm.add(llvm::createReassociatePass());				// reassociate
				fpm.add(llvm::createCFGSimplificationPass());		// simplifycfg
			}

			if (impl->optimizationLevel >= 2) {
				// More aggressive optimizations
				fpm.add(llvm::createGVNPass());					// GVN (Global Value Numbering)
				fpm.add(llvm::createDeadCodeEliminationPass()); // Dead Code Elimination
				fpm.add(llvm::createSROAPass());				// Scalar Replacement of Aggregates
			}

			if (impl->optimizationLevel >= 3) {
				// Most aggressive optimizations
				fpm.add(llvm::createLICMPass()); // Loop Invariant Code Motion
				fpm.add(llvm::createLoopUnrollPass());
			}

			// Run function passes on all functions
			fpm.doInitialization();
			for (auto& func : *impl->module) {
				if (!func.isDeclaration()) {
					fpm.run(func);
				}
			}
			fpm.doFinalization();

			// Run module passes
			mpm.run(*impl->module);
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

		pass.run(*impl->module);
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
		std::string nestedPath = libDir + "/qdrt/libqdrt_static.a";
		std::string flatPath = libDir + "/libqdrt_static.a";

		if (std::filesystem::exists(nestedPath)) {
			qdrtStaticPath = nestedPath;
		} else if (std::filesystem::exists(flatPath)) {
			qdrtStaticPath = flatPath;
		} else {
			// Fallback to flat path (will error later if doesn't exist)
			qdrtStaticPath = flatPath;
		}

		std::string libraryFlags = qdrtStaticPath;

		// Add imported libraries
		for (const auto& library : impl->importedLibraries) {
			// Check if it's already a .a file (static library)
			if (library.size() >= 2 && library.substr(library.size() - 2) == ".a") {
				// It's a static library, link it directly
				std::string foundLibPath;

				// First, check in additional library search paths (third-party packages)
				for (const auto& searchPath : impl->librarySearchPaths) {
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
					// Examples: "libstdmathqd_static.a" -> "stdmathqd", "libqdrt_static.a" -> "qdrt"
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
		libraryFlags += " -lm -lpthread";

		// Build -L flags for additional library search paths (third-party packages)
		std::string librarySearchFlags;
		for (const auto& searchPath : impl->librarySearchPaths) {
			librarySearchFlags += " -L" + searchPath;
		}

		std::string linkCmd = "clang -o " + filename + " " + objFile + " " + librarySearchFlags + " " + libraryFlags;

		int result = system(linkCmd.c_str());

		// Clean up object file
		std::remove(objFile.c_str());

		return result == 0;
	}

} // namespace Qd
