#include <llvmgen/generator.h>

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

#include <qc/ast_node.h>
#include <qc/ast_node_break.h>
#include <qc/ast_node_continue.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_loop.h>
#include <qc/ast_node_return.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_use.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Qd {

	class LlvmGenerator::Impl {
	public:
		std::unique_ptr<llvm::LLVMContext> context;
		std::unique_ptr<llvm::Module> module;
		std::unique_ptr<llvm::IRBuilder<>> builder;

		// Runtime types
		llvm::Type* contextPtrTy = nullptr;
		llvm::Type* execResultTy = nullptr;
		llvm::Type* stackElementTy = nullptr;

		// Runtime functions
		llvm::Function* createContextFn = nullptr;
		llvm::Function* freeContextFn = nullptr;
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

		// Loop context for break/continue
		struct LoopContext {
			llvm::BasicBlock* breakTarget;
			llvm::BasicBlock* continueTarget;
		};

		std::vector<LoopContext> loopStack;

		// User-defined functions
		std::map<std::string, llvm::Function*> userFunctions;
		std::map<std::string, bool> fallibleFunctions; // Track which functions can throw errors

		// Module ASTs to include (preserves insertion order for dependency resolution)
		std::vector<std::pair<std::string, IAstNode*>> moduleASTs;

		// Function context for return
		llvm::BasicBlock* currentFunctionReturnBlock = nullptr;
		bool currentFunctionIsFallible = false;

		// Defer statements collected during function generation
		std::vector<AstNodeDefer*> currentDeferStatements;

		// Counter for unique variable names
		int varCounter = 0;

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
		void generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx, llvm::Value* forIterVar);
		void generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr, llvm::Value* ctx);
		void generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx);
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

		// Check if it's the loop iterator variable ($)
		if (name == "$" && forIterVar) {
			// Push loop iterator as integer
			builder->CreateCall(pushIntFn, {ctx, forIterVar});
			return;
		}

		// Check if it's a user-defined function call
		auto it = userFunctions.find(name);
		if (it != userFunctions.end()) {
			builder->CreateCall(it->second, {ctx});

			// Check if this function is fallible
			auto fallibleIt = fallibleFunctions.find(name);
			if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
				// This is a fallible function - push error status after the call
				// Get the has_error field from context (field index 1)
				// Context layout: {qd_stack* st, bool has_error, int argc, char** argv, char* program_name}
				auto contextStructTy = llvm::StructType::get(
						*context, {
										  llvm::PointerType::getUnqual(*context), // qd_stack* st
										  builder->getInt1Ty(),					  // bool has_error
										  builder->getInt32Ty(),				  // int argc
										  llvm::PointerType::getUnqual(*context), // char** argv
										  llvm::PointerType::getUnqual(*context)  // char* program_name
								  });

				auto hasErrorPtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "has_error_ptr");
				auto hasError = builder->CreateLoad(builder->getInt1Ty(), hasErrorPtr, "has_error");

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
					builder->CreateStore(builder->getInt1(false), hasErrorPtr);

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

		// Look up scoped function: scope::name -> usr_scope_name
		std::string fullName = scope + "::" + name;
		std::string mangledName = "usr_" + scope + "_" + name;

		// Check if we have this function
		llvm::Function* fn = module->getFunction(mangledName);
		if (!fn) {
			// Function doesn't exist yet, declare it
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, *module);
		}

		// Call the scoped function
		builder->CreateCall(fn, {ctx});

		// Check if this is a fallible function (same logic as for regular identifiers)
		auto fallibleIt = fallibleFunctions.find(fullName);
		if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
			// This is a fallible function - push error status after the call
			auto contextStructTy =
					llvm::StructType::get(*context, {
															llvm::PointerType::getUnqual(*context), // qd_stack* st
															builder->getInt1Ty(),					// bool has_error
															builder->getInt32Ty(),					// int argc
															llvm::PointerType::getUnqual(*context), // char** argv
															llvm::PointerType::getUnqual(*context) // char* program_name
													});

			auto hasErrorPtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "has_error_ptr");
			auto hasError = builder->CreateLoad(builder->getInt1Ty(), hasErrorPtr, "has_error");

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
					builder->CreateStore(builder->getInt1(false), hasErrorPtr);

					// Push the success status onto the stack
					builder->CreateCall(pushIntFn, {ctx, successStatus});
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
														 builder->getInt1Ty(),					 // bool has_error
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
														 builder->getInt1Ty(),					 // bool has_error
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

	void LlvmGenerator::Impl::generateNode(IAstNode* node, llvm::Value* ctx, llvm::Value* forIterVar) {
		if (!node) {
			return;
		}

		auto nodeType = node->type();

		switch (nodeType) {
		case IAstNode::Type::LITERAL:
			generateLiteral(static_cast<AstNodeLiteral*>(node), ctx);
			break;
		case IAstNode::Type::INSTRUCTION:
			generateInstruction(static_cast<AstNodeInstruction*>(node), ctx);
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
		llvm::Function* fn = nullptr;

		if (isMain) {
			// Create main function: i32 @main(i32 %argc, i8** %argv)
			auto mainFnTy = llvm::FunctionType::get(
					builder->getInt32Ty(), {builder->getInt32Ty(), llvm::PointerType::getUnqual(*context)}, false);
			fn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);

			// Create entry basic block
			auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
			builder->SetInsertPoint(entryBB);

			// Create Quadrate context
			auto stackSize = builder->getInt64(1024);
			auto ctx = builder->CreateCall(createContextFn, {stackSize}, "ctx");

			// Push "main::main" onto call stack for debugging
			std::string fullFuncName = namePrefix + "::" + funcNode->name();
			auto funcNameStr = builder->CreateGlobalString(fullFuncName);
			builder->CreateCall(pushCallFn, {ctx, funcNameStr});

			// Generate function body
			auto body = funcNode->body();
			if (body) {
				generateNode(body, ctx, nullptr);
			}

			// Pop from call stack
			builder->CreateCall(popCallFn, {ctx});

			// Free context
			builder->CreateCall(freeContextFn, {ctx});

			// Return 0
			builder->CreateRet(builder->getInt32(0));
		} else {
			// User-defined function: qd_exec_result usr_<prefix>_<name>(qd_context* ctx)
			std::string fnName = "usr_" + namePrefix + "_" + funcNode->name();
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);

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

			// Pop function from call stack before returning
			builder->CreateCall(popCallFn, {ctx});

			// Return success
			auto result = llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(execResultTy), {builder->getInt32(0)});
			builder->CreateRet(result);
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

					for (const auto* func : importNode->functions()) {
						// Determine mangled name based on library
						std::string mangledName;
						if (library == "libstdqd.so") {
							// For libstdqd, use qd_stdqd_ prefix (matches C implementation)
							mangledName = "qd_stdqd_" + func->name;
						} else {
							// For other libraries, use usr_namespace_ prefix
							mangledName = "usr_" + namespaceName + "_" + func->name;
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
						std::string scopedName = "usr_" + namespaceName + "_" + func->name;
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

		// Process import statements from main file
		for (size_t i = 0; i < root->childCount(); i++) {
			auto child = root->child(i);
			if (auto importNode = dynamic_cast<AstNodeImport*>(child)) {
				const std::string& namespaceName = importNode->namespaceName();
				const std::string& library = importNode->library();

				for (const auto* func : importNode->functions()) {
					std::string mangledName;
					if (library == "libstdqd.so") {
						mangledName = "qd_stdqd_" + func->name;
					} else {
						mangledName = "usr_" + namespaceName + "_" + func->name;
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

	bool LlvmGenerator::generate(IAstNode* root, const std::string& moduleName) {
		if (!impl) {
			impl = std::make_unique<Impl>(moduleName);
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
		auto targetMachine = target->createTargetMachine(
				targetTriple, cpu, features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_));

		impl->module->setDataLayout(targetMachine->createDataLayout());

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

		// Link with runtime library using clang
		// Check for library paths in order of preference: QUADRATE_LIBDIR, ./dist/lib, ~/.local/lib, system paths
		std::string libraryFlags = "-lqdrt -lstdqd -lm -pthread";
		std::string libraryPaths;
		std::string rpathFlags;

		// Check QUADRATE_LIBDIR environment variable first
		if (const char* quadrateLibDir = std::getenv("QUADRATE_LIBDIR")) {
			std::filesystem::path libDir(quadrateLibDir);
			if (std::filesystem::exists(libDir)) {
				libraryPaths = " -L" + libDir.string();
				rpathFlags = " -Wl,-rpath," + libDir.string();
			}
		}
		// Check ./dist/lib (development build) - use absolute path
		else {
			std::filesystem::path distLib = std::filesystem::absolute("./dist/lib");
			if (std::filesystem::exists(distLib)) {
				libraryPaths = " -L" + distLib.string();
				rpathFlags = " -Wl,-rpath," + distLib.string();
			}
			// Check ~/.local/lib (user installation)
			else if (const char* home = std::getenv("HOME")) {
				std::filesystem::path localLib = std::filesystem::path(home) / ".local" / "lib";
				if (std::filesystem::exists(localLib)) {
					libraryPaths = " -L" + localLib.string();
					rpathFlags = " -Wl,-rpath," + localLib.string();
				}
			}
		}
		// System paths will be checked automatically by clang

		std::string linkCmd = "clang -o " + filename + " " + objFile + libraryPaths + rpathFlags + " " + libraryFlags;

		int result = system(linkCmd.c_str());

		// Clean up object file
		std::remove(objFile.c_str());

		return result == 0;
	}

} // namespace Qd
