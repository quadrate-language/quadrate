#include "generator_impl.h"

namespace Qd {

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

		// Check if instruction name shadows a local variable - if so, push the variable
		auto localIt = localVariables.find(name);
		if (localIt != localVariables.end()) {
			// Push the local variable onto the stack (variable shadowing builtin instruction)
			// This replicates the local variable push logic from generateIdentifier
			llvm::AllocaInst* localAlloca = localIt->second;

			// For indirect (captured) variables, load the actual storage pointer first
			llvm::Value* storagePtr = localAlloca;
			bool isIndirect = indirectLocalVariables.find(name) != indirectLocalVariables.end();
			if (isIndirect) {
				storagePtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), localAlloca, name + "_storage");
			}

			// Extract type field
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 1, name + "_type_ptr");
			llvm::Value* type = builder->CreateLoad(builder->getInt32Ty(), typePtr, name + "_type");

			// Switch on type and push appropriate value
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_value_ptr");

			// Create basic blocks for type-based dispatch
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* intBlock = llvm::BasicBlock::Create(*context, "local_int", currentFn);
			llvm::BasicBlock* floatBlock = llvm::BasicBlock::Create(*context, "local_float", currentFn);
			llvm::BasicBlock* strBlock = llvm::BasicBlock::Create(*context, "local_str", currentFn);
			llvm::BasicBlock* ptrBlock = llvm::BasicBlock::Create(*context, "local_ptr", currentFn);
			llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context, "local_end", currentFn);

			llvm::SwitchInst* sw = builder->CreateSwitch(type, endBlock, 4);
			sw->addCase(builder->getInt32(0), intBlock);   // QD_STACK_TYPE_INT
			sw->addCase(builder->getInt32(1), floatBlock); // QD_STACK_TYPE_FLOAT
			sw->addCase(builder->getInt32(3), strBlock);   // QD_STACK_TYPE_STR
			sw->addCase(builder->getInt32(2), ptrBlock);   // QD_STACK_TYPE_PTR

			// INT block
			builder->SetInsertPoint(intBlock);
			llvm::Value* intVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, name + "_i");
			generateInlinePushIntValue(ctx, intVal);
			builder->CreateBr(endBlock);

			// FLOAT block
			builder->SetInsertPoint(floatBlock);
			llvm::Value* floatVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_f");
			builder->CreateCall(pushFloatFn, {ctx, floatVal});
			builder->CreateBr(endBlock);

			// STR block
			builder->SetInsertPoint(strBlock);
			llvm::Value* strVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_s");
			builder->CreateCall(pushStrRefFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), valuePtr, name + "_p");
			builder->CreateCall(qdPtrRetainFn, {ptrVal});
			builder->CreateCall(pushPtrFn, {ctx, ptrVal});
			builder->CreateBr(endBlock);

			builder->SetInsertPoint(endBlock);

			// If this local variable has a known struct type, track it for the next pop
			auto structTypeIt = localVariableStructTypes.find(name);
			if (structTypeIt != localVariableStructTypes.end()) {
				lastStructConstructed = structTypeIt->second;
			}
			return;
		}

		// Handle method call on struct (marked by semantic validator)
		if (inst->isMethodCall()) {
			const std::string& receiverType = inst->receiverType();
			if (!receiverType.empty()) {
				// Generate method call - use qualified function name
				std::string mangledFnName = "usr_" + receiverType + "_" + name;
				// Replace :: with _ for valid function names
				for (size_t pos = 0; (pos = mangledFnName.find("::")) != std::string::npos;) {
					mangledFnName.replace(pos, 2, "_");
				}

				llvm::Function* methodFn = module->getFunction(mangledFnName);
				if (!methodFn) {
					// Method not found - this shouldn't happen if validation passed
					llvm::errs() << "Method function not found: " << mangledFnName << "\n";
					return;
				}

				builder->CreateCall(methodFn, {ctx});
				return;
			}
		}

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
			bool isIndirect = indirectLocalVariables.find(name) != indirectLocalVariables.end();
			if (isIndirect) {
				storagePtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), localAlloca, name + "_storage");
			}

			// Fast path for integer-only functions with non-captured locals
			// Skip type switch - we know all locals are integers
			if (currentFunctionIsIntegerOnly && !isIndirect) {
				llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_value_ptr");
				llvm::Value* intVal = builder->CreateLoad(builder->getInt64Ty(), valuePtr, name + "_i");
				generateInlinePushIntValue(ctx, intVal);
				lastIdentifierPushed = name;
				return;
			}

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

			// If this local variable has a known struct type, track it for the next pop
			auto structTypeIt = localVariableStructTypes.find(name);
			if (structTypeIt != localVariableStructTypes.end()) {
				lastStructConstructed = structTypeIt->second;
			}
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
		const StructLayout* structLayout = findStructDefinition(name);
		if (structLayout != nullptr) {
			generateStructConstruction(structLayout->name, ctx);
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
		// First check if this is a method call (identified by semantic validator)
		// For methods, lookup uses ReceiverType::methodName
		auto it = userFunctions.end();
		std::string lookupName = name;
		if (ident->isMethodCall()) {
			// Method call - use mangled name
			lookupName = ident->receiverType() + "::" + name;
			it = userFunctions.find(lookupName);
		} else {
			// Regular function call
			// First try the plain name, then try with current module prefix for intra-module calls
			it = userFunctions.find(name);
			if (it == userFunctions.end() && currentModulePrefix != "main") {
				std::string qualifiedName = currentModulePrefix + "::" + name;
				it = userFunctions.find(qualifiedName);
				if (it != userFunctions.end()) {
					lookupName = qualifiedName;
				}
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

			// Track return struct type for local variable binding (-> name)
			auto returnTypeIt = functionReturnStructType.find(lookupName);
			if (returnTypeIt != functionReturnStructType.end()) {
				lastStructConstructed = returnTypeIt->second;
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
			auto closureStructTy = llvm::StructType::get(
					*context, {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
									  llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

			// Allocate environment array (array of pointers for capture-by-reference)
			size_t envSize = captures.size() * 8; // sizeof(pointer) = 8 bytes on 64-bit

			// Get or create malloc function
			llvm::Function* closureMallocFn = module->getFunction("malloc");
			if (!closureMallocFn) {
				auto closureMallocFnTy =
						llvm::FunctionType::get(llvm::PointerType::getUnqual(*context), {builder->getInt64Ty()}, false);
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
						llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capturePtr,
								builder->getInt64(static_cast<uint64_t>(-8)), capName + "_refcount_ptr");
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
			llvm::Value* closureAlloc = builder->CreateCall(closureMallocFn, {builder->getInt64(32)}, "closure_alloc");

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

		// Check if this is an explicit method call (scope is struct type, name is method)
		// The semantic validator marks these with isMethodCall()
		if (scopedIdent->isMethodCall()) {
			// Method call - look up in userFunctions with mangled name
			auto it = userFunctions.find(fullName);
			if (it != userFunctions.end()) {
				// Generate any needed type casts before the function call
				generateCastInstructions(scopedIdent->parameterCasts(), ctx);

				// Clear error_code for fallible methods
				auto preFallibleIt = fallibleFunctions.find(fullName);
				if (preFallibleIt != fallibleFunctions.end() && preFallibleIt->second) {
					auto contextStructTy = llvm::StructType::get(*context,
							{llvm::PointerType::getUnqual(*context), builder->getInt64Ty(),
									llvm::PointerType::getUnqual(*context), builder->getInt32Ty(),
									llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)});
					auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "pre_call_error_code_ptr");
					builder->CreateStore(builder->getInt64(0), errorCodePtr);
				}

				// Call the method
				builder->CreateCall(it->second, {ctx});

				// Handle fallible method return
				auto fallibleIt = fallibleFunctions.find(fullName);
				if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
					auto contextStructTy = llvm::StructType::get(*context,
							{llvm::PointerType::getUnqual(*context), builder->getInt64Ty(),
									llvm::PointerType::getUnqual(*context), builder->getInt32Ty(),
									llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)});

					auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
					auto errorCode = builder->CreateLoad(builder->getInt64Ty(), errorCodePtr, "error_code");
					auto hasError = builder->CreateICmpNE(errorCode, builder->getInt64(0), "has_error");

					if (scopedIdent->abortOnError()) {
						llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(
								*context, "error_abort", builder->GetInsertBlock()->getParent());
						llvm::BasicBlock* continueBlock =
								llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());
						builder->CreateCondBr(hasError, errorBlock, continueBlock);

						builder->SetInsertPoint(errorBlock);
						llvm::Value* errorMsg =
								builder->CreateGlobalString("Fatal error: method '" + name + "' failed\n");
						auto fprintfFn = module->getOrInsertFunction(
								"fprintf", llvm::FunctionType::get(builder->getInt32Ty(),
												   {llvm::PointerType::getUnqual(*context),
														   llvm::PointerType::getUnqual(*context)},
												   true));
						auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
						auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal);
						builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
						auto abortFn = module->getOrInsertFunction(
								"abort", llvm::FunctionType::get(builder->getVoidTy(), false));
						builder->CreateCall(abortFn);
						builder->CreateUnreachable();

						builder->SetInsertPoint(continueBlock);
					} else {
						auto successStatus = builder->CreateSelect(
								hasError, builder->getInt64(0), builder->getInt64(1), "success_status");
						builder->CreateCall(pushIntFn, {ctx, successStatus});
					}
				}
				return;
			}
		}

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
		// Use qualified name for module struct lookup
		std::string qualifiedName = scope + "::" + name;
		const StructLayout* structLayout = findStructDefinition(qualifiedName);
		if (structLayout != nullptr) {
			// This is a struct construction from a module
			// Generate struct allocation and field initialization
			generateStructConstruction(structLayout->name, ctx);
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

		// Track return struct type for local variable binding (-> name)
		auto returnTypeIt = functionReturnStructType.find(fullName);
		if (returnTypeIt != functionReturnStructType.end()) {
			lastStructConstructed = returnTypeIt->second;
		}

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
					localMallocFn =
							llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
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
		bool isIndirect = indirectLocalVariables.find(name) != indirectLocalVariables.end();
		if (isIndirect) {
			storagePtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), localAlloca, name + "_storage");
		}

		// Fast path for integer-only functions with non-captured locals
		// Skip old value release (integers don't need release) and use inline pop
		if (currentFunctionIsIntegerOnly && !isCaptured && !isIndirect) {
			generateInlinePopIntToStorage(ctx, storagePtr);
			return;
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
			auto closureStructTy = llvm::StructType::get(
					*context, {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
									  llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

			auto closureIsValidFnTy =
					llvm::FunctionType::get(builder->getInt32Ty(), {llvm::PointerType::getUnqual(*context)}, false);
			auto closureIsValidFn = module->getOrInsertFunction("qd_closure_is_valid", closureIsValidFnTy);
			llvm::Value* isClosureResult = builder->CreateCall(closureIsValidFn, {oldPtrVal}, "is_closure_result");
			llvm::Value* isMagicValid = builder->CreateICmpNE(isClosureResult, builder->getInt32(0), "old_is_closure");

			llvm::BasicBlock* releaseClosureBlock =
					llvm::BasicBlock::Create(*context, name + "_release_old_closure", currentFn);
			llvm::BasicBlock* releaseGenericBlock =
					llvm::BasicBlock::Create(*context, name + "_release_old_generic", currentFn);
			builder->CreateCondBr(isMagicValid, releaseClosureBlock, releaseGenericBlock);

			// Release as closure
			builder->SetInsertPoint(releaseClosureBlock);
			llvm::Value* envPtrSlot = builder->CreateStructGEP(closureStructTy, oldPtrVal, 2, name + "_old_env_slot");
			llvm::Value* envPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), envPtrSlot, "old_env_ptr");
			llvm::Value* capCountSlot =
					builder->CreateStructGEP(closureStructTy, oldPtrVal, 3, name + "_old_cap_count_slot");
			llvm::Value* capCount = builder->CreateLoad(builder->getInt64Ty(), capCountSlot, "old_cap_count");

			// Loop through captured variables and decrement refcounts
			llvm::BasicBlock* loopHeader = llvm::BasicBlock::Create(*context, name + "_old_cap_loop_header", currentFn);
			llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(*context, name + "_old_cap_loop_body", currentFn);
			llvm::BasicBlock* afterLoop = llvm::BasicBlock::Create(*context, name + "_old_cap_loop_done", currentFn);

			llvm::AllocaInst* loopIdxAlloca =
					builder->CreateAlloca(builder->getInt64Ty(), nullptr, name + "_old_cap_idx");
			builder->CreateStore(builder->getInt64(0), loopIdxAlloca);
			builder->CreateBr(loopHeader);

			builder->SetInsertPoint(loopHeader);
			llvm::Value* loopIdx = builder->CreateLoad(builder->getInt64Ty(), loopIdxAlloca, "idx");
			llvm::Value* loopCond = builder->CreateICmpSLT(loopIdx, capCount, "loop_cond");
			builder->CreateCondBr(loopCond, loopBody, afterLoop);

			builder->SetInsertPoint(loopBody);
			llvm::Value* capSlot =
					builder->CreateGEP(llvm::PointerType::getUnqual(*context), envPtr, loopIdx, name + "_old_cap_slot");
			llvm::Value* capVarPtr =
					builder->CreateLoad(llvm::PointerType::getUnqual(*context), capSlot, "old_cap_var_ptr");
			llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capVarPtr,
					builder->getInt64(static_cast<uint64_t>(-8)), name + "_old_refcount_ptr");
			llvm::Value* refCount = builder->CreateLoad(builder->getInt64Ty(), refCountPtr, "old_refcount");
			llvm::Value* newRefCount = builder->CreateSub(refCount, builder->getInt64(1), "old_new_refcount");
			builder->CreateStore(newRefCount, refCountPtr);

			llvm::BasicBlock* freeCapBlock = llvm::BasicBlock::Create(*context, name + "_old_free_cap", currentFn);
			llvm::BasicBlock* capContinue = llvm::BasicBlock::Create(*context, name + "_old_cap_continue", currentFn);
			llvm::Value* shouldFree = builder->CreateICmpEQ(newRefCount, builder->getInt64(0), "old_should_free");
			builder->CreateCondBr(shouldFree, freeCapBlock, capContinue);

			builder->SetInsertPoint(freeCapBlock);
			llvm::Function* localFreeFn = module->getFunction("free");
			if (!localFreeFn) {
				auto freeFnTy =
						llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
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
				auto freeFnTy =
						llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
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
		auto fprintfFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
				{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true);
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
				llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), heapPtr,
						builder->getInt64(static_cast<uint64_t>(-8)), varName + "_refcount_cleanup");
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
				llvm::BasicBlock* capCheckPtr =
						llvm::BasicBlock::Create(*context, varName + "_cap_check_ptr", currentFn);
				llvm::BasicBlock* capIsPtr = llvm::BasicBlock::Create(*context, varName + "_cap_is_ptr", currentFn);
				llvm::BasicBlock* capDoFree = llvm::BasicBlock::Create(*context, varName + "_cap_do_free", currentFn);

				// Check if type == 3 (string)
				llvm::Value* capIsStrCond = builder->CreateICmpEQ(capType, builder->getInt32(3), "cap_is_str");
				builder->CreateCondBr(capIsStrCond, capIsStr, capCheckPtr);

				// Release string
				builder->SetInsertPoint(capIsStr);
				llvm::Value* capStrVal =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), heapPtr, "cap_str");
				if (!this->qdStringReleaseFn) {
					auto qdStringReleaseFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
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
				llvm::Value* capPtrVal =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), heapPtr, "cap_ptr");
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
				auto closureStructTy = llvm::StructType::get(
						*context, {builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
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
				llvm::BasicBlock* notClosure = llvm::BasicBlock::Create(*context, varName + "_not_closure", currentFn);
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
				llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(*context, varName + "_cap_loop_body", currentFn);
				llvm::BasicBlock* afterLoop = llvm::BasicBlock::Create(*context, varName + "_cap_loop_done", currentFn);

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
				llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capVarPtr,
						builder->getInt64(static_cast<uint64_t>(-8)), varName + "_refcount_ptr");
				llvm::Value* refCount = builder->CreateLoad(builder->getInt64Ty(), refCountPtr, "refcount");

				// Decrement refcount
				llvm::Value* newRefCount = builder->CreateSub(refCount, builder->getInt64(1), "new_refcount");
				builder->CreateStore(newRefCount, refCountPtr);

				// If refcount == 0, free the heap block
				llvm::BasicBlock* freeCapBlock = llvm::BasicBlock::Create(*context, varName + "_free_cap", currentFn);
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
				llvm::BasicBlock* loopCapIsStr =
						llvm::BasicBlock::Create(*context, varName + "_loop_cap_is_str", currentFn);
				llvm::BasicBlock* loopCapCheckPtr =
						llvm::BasicBlock::Create(*context, varName + "_loop_cap_check_ptr", currentFn);
				llvm::BasicBlock* loopCapIsPtr =
						llvm::BasicBlock::Create(*context, varName + "_loop_cap_is_ptr", currentFn);
				llvm::BasicBlock* loopCapDoFree =
						llvm::BasicBlock::Create(*context, varName + "_loop_cap_do_free", currentFn);

				// Check if type == 3 (string)
				llvm::Value* loopCapIsStrCond =
						builder->CreateICmpEQ(loopCapType, builder->getInt32(3), "loop_cap_is_str");
				builder->CreateCondBr(loopCapIsStrCond, loopCapIsStr, loopCapCheckPtr);

				// Release string
				builder->SetInsertPoint(loopCapIsStr);
				llvm::Value* loopCapStrVal =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), capVarPtr, "loop_cap_str");
				if (!this->qdStringReleaseFn) {
					auto qdStringReleaseFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
					this->qdStringReleaseFn = llvm::Function::Create(
							qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);
				}
				builder->CreateCall(this->qdStringReleaseFn, {loopCapStrVal});
				builder->CreateBr(loopCapDoFree);

				// Check if type == 2 (pointer - could be struct, array, or closure)
				builder->SetInsertPoint(loopCapCheckPtr);
				llvm::Value* loopCapIsPtrCond =
						builder->CreateICmpEQ(loopCapType, builder->getInt32(2), "loop_cap_is_ptr");
				builder->CreateCondBr(loopCapIsPtrCond, loopCapIsPtr, loopCapDoFree);

				// Release pointer (works for structs and arrays)
				builder->SetInsertPoint(loopCapIsPtr);
				llvm::Value* loopCapPtrVal =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), capVarPtr, "loop_cap_ptr");
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

} // namespace Qd
