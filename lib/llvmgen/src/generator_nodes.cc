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
			lastPushedType = LastPushedType::INTEGER;
			break;
		}
		case AstNodeLiteral::LiteralType::FLOAT: {
			auto val = llvm::ConstantFP::get(builder->getDoubleTy(), std::stod(value));
			builder->CreateCall(pushFloatFn, {ctx, val});
			lastPushedType = LastPushedType::FLOAT;
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
			lastPushedType = LastPushedType::STRING;
			break;
		}
		}
	}

	// generateInstruction is in generator_nodes_instructions.cc

	void LlvmGenerator::Impl::generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx) {
		const std::string& name = ident->name();

		// Check if it's a captured variable (by reference)
		auto capIt = capturedVariableRefs.find(name);
		if (capIt != capturedVariableRefs.end()) {
			// Load the pointer to the outer variable, then access through it
			llvm::AllocaInst* ptrAlloca = capIt->second;
			llvm::Value* outerVarPtr =
					builder->CreateLoad(ptrTy, ptrAlloca, name + "_cap_ptr");

			// Now outerVarPtr points to a qd_stack_element_t - use it like localAlloca below
			// Extract type field
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, outerVarPtr, 1, name + "_cap_type_ptr");
			llvm::Value* type = builder->CreateLoad(int32Ty, typePtr, name + "_cap_type");

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
			llvm::Value* intVal = builder->CreateLoad(int64Ty, valuePtr, name + "_cap_i");
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
					builder->CreateLoad(ptrTy, valuePtr, name + "_cap_s");
			builder->CreateCall(pushStrRefFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal =
					builder->CreateLoad(ptrTy, valuePtr, name + "_cap_p");
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
						builder->CreateLoad(ptrTy, localAlloca, name + "_storage");
			}

			// Fast path for integer-only functions with non-captured locals
			// Skip type switch - we know all locals are integers
			if (currentFunctionIsIntegerOnly && !isIndirect) {
				llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_value_ptr");
				llvm::Value* intVal = builder->CreateLoad(int64Ty, valuePtr, name + "_i");
				generateInlinePushIntValue(ctx, intVal);
				lastIdentifierPushed = name;
				return;
			}

			// Extract type field (field index 1 in qd_stack_element_t)
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 1, name + "_type_ptr");
			llvm::Value* type = builder->CreateLoad(int32Ty, typePtr, name + "_type");

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
			llvm::Value* intVal = builder->CreateLoad(int64Ty, valuePtr, name + "_i");
			generateInlinePushIntValue(ctx, intVal);
			builder->CreateBr(endBlock);

			// FLOAT block: load double and push
			builder->SetInsertPoint(floatBlock);
			llvm::Value* floatVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_f");
			builder->CreateCall(pushFloatFn, {ctx, floatVal});
			builder->CreateBr(endBlock);

			// STR block: load qd_string_t* and push with retain
			builder->SetInsertPoint(strBlock);
			llvm::Value* strVal = builder->CreateLoad(ptrTy, valuePtr, name + "_s");
			builder->CreateCall(pushStrRefFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block: load void* and push (retain for arrays and potential structs)
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal = builder->CreateLoad(ptrTy, valuePtr, name + "_p");
			// Retain based on variable type
			if (localArrayVariables.find(name) != localArrayVariables.end()) {
				// Retain array variables
				llvm::Function* arrayRetainFn = module->getFunction("qd_array_retain");
				if (!arrayRetainFn) {
					auto arrayRetainFnTy = llvm::FunctionType::get(
							builder->getVoidTy(), {ptrTy}, false);
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
			llvm::Value* funcPtr = builder->CreateBitCast(func, ptrTy);
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
				// For method calls, rotate stack so receiver is on top before calling
				// Only roll if receiver is NOT already on top
				if (ident->isMethodCall()) {
					size_t receiverPositionFromTop = ident->methodReceiverPositionFromTop();
					if (receiverPositionFromTop > 0) {
						// Push the count and call qd_roll
						// roll (N+1) brings the (N+1)th element (0-indexed from top) to top
						builder->CreateCall(pushIntFn, {ctx, builder->getInt64(receiverPositionFromTop + 1)});
						auto rollFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
						auto rollFn = module->getOrInsertFunction("qd_roll", rollFnTy);
						builder->CreateCall(rollFn, {ctx});
					}
				}
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
				auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
				auto errorCode = builder->CreateLoad(int64Ty, errorCodePtr, "error_code");
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

					// Print error message with context->error_msg if available
					auto funcNameStr = builder->CreateGlobalString(name);
					builder->CreateCall(printErrorMsgFn, {ctx, funcNameStr});
					builder->CreateCall(printStackTraceFn, {ctx});
					builder->CreateCall(abortFn, {});
					builder->CreateUnreachable();

					// Continue block - user-defined functions don't push success status
					builder->SetInsertPoint(continueBlock);
					// Note: don't drop here - only imported C functions push success status
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
			auto funcPtrValue = builder->CreateBitCast(fn, ptrTy);
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
			fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, ptrTy}, false);
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
						tmpBuilder.CreateAlloca(ptrTy, nullptr, capName + "_ref");

				// Load pointer from environment array
				llvm::Value* envSlot = builder->CreateGEP(
						ptrTy, envPtr, builder->getInt64(i), capName + "_env_slot");
				llvm::Value* outerPtr =
						builder->CreateLoad(ptrTy, envSlot, capName + "_outer_ptr");

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
			// Allocate environment array (array of pointers for capture-by-reference)
			size_t envSize = captures.size() * 8; // sizeof(pointer) = 8 bytes on 64-bit

			llvm::Value* envAlloc = builder->CreateCall(mallocFn, {builder->getInt64(envSize)}, "env_alloc");

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
								ptrTy, outerAlloca, capName + "_heap_ptr");

						// Increment refcount for this capture (the closure takes ownership)
						// Refcount is 8 bytes before the elem pointer
						llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capturePtr,
								builder->getInt64(static_cast<uint64_t>(-8)), capName + "_refcount_ptr");
						llvm::Value* refCount = builder->CreateLoad(int64Ty, refCountPtr, "refcount");
						llvm::Value* newRefCount = builder->CreateAdd(refCount, builder->getInt64(1), "new_refcount");
						builder->CreateStore(newRefCount, refCountPtr);
					}

					// Store pointer to captured variable into environment array
					llvm::Value* envSlot = builder->CreateGEP(
							ptrTy, envAlloc, builder->getInt64(i), capName + "_slot");
					builder->CreateStore(capturePtr, envSlot);
				}
			}

			// Allocate closure struct (magic + 2 pointers + capture_count = 32 bytes)
			llvm::Value* closureAlloc = builder->CreateCall(mallocFn, {builder->getInt64(32)}, "closure_alloc");

			// Store magic marker (0xCL05UR3E = 0xC105023E in hex)
			llvm::Value* magicSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 0, "magic_slot");
			builder->CreateStore(builder->getInt64(0xC105023E), magicSlot);

			// Store function pointer
			llvm::Value* fnPtrSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 1, "fn_ptr_slot");
			llvm::Value* fnPtrCast = builder->CreateBitCast(fn, ptrTy, "fn_ptr_cast");
			builder->CreateStore(fnPtrCast, fnPtrSlot);

			// Store environment pointer
			llvm::Value* envPtrSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 2, "env_ptr_slot");
			builder->CreateStore(envAlloc, envPtrSlot);

			// Store capture count for cleanup
			llvm::Value* capCountSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 3, "cap_count_slot");
			builder->CreateStore(builder->getInt64(captures.size()), capCountSlot);

			// Register the closure in the closure registry for safe detection
			auto closureRegisterFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
			auto closureRegisterFn = module->getOrInsertFunction("qd_closure_register", closureRegisterFnTy);
			builder->CreateCall(closureRegisterFn, {closureAlloc});

			// Push closure struct pointer to stack
			builder->CreateCall(pushPtrFn, {ctx, closureAlloc});

			// Track that we just generated a closure
			lastGeneratedWasClosure = true;
			lastClosureCaptureCount = captures.size();
		} else {
			// No captures - just push the function pointer (existing behavior)
			auto funcPtrValue = builder->CreateBitCast(fn, ptrTy);
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
			// Use receiver type if set, otherwise use scope::name
			std::string lookupName =
					scopedIdent->receiverType().empty() ? fullName : (scopedIdent->receiverType() + "::" + name);
			auto it = userFunctions.find(lookupName);
			if (it != userFunctions.end()) {
				// Generate any needed type casts before the function call
				generateCastInstructions(scopedIdent->parameterCasts(), ctx);

				// Clear error_code for fallible methods
				auto preFallibleIt = fallibleFunctions.find(lookupName);
				if (preFallibleIt != fallibleFunctions.end() && preFallibleIt->second) {
					auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "pre_call_error_code_ptr");
					builder->CreateStore(builder->getInt64(0), errorCodePtr);
				}

				// For method calls, rotate stack so receiver is on top before calling
				// Only roll if receiver is NOT already on top
				size_t receiverPositionFromTop = scopedIdent->methodReceiverPositionFromTop();
				if (receiverPositionFromTop > 0) {
					// Push the count and call qd_roll
					// roll (N+1) brings the (N+1)th element (0-indexed from top) to top
					builder->CreateCall(pushIntFn, {ctx, builder->getInt64(receiverPositionFromTop + 1)});
					auto rollFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					auto rollFn = module->getOrInsertFunction("qd_roll", rollFnTy);
					builder->CreateCall(rollFn, {ctx});
				}

				// Call the method
				builder->CreateCall(it->second, {ctx});

				// Handle fallible method return
				auto fallibleIt = fallibleFunctions.find(lookupName);
				if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
					auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
					auto errorCode = builder->CreateLoad(int64Ty, errorCodePtr, "error_code");
					auto hasError = builder->CreateICmpNE(errorCode, builder->getInt64(0), "has_error");

					if (scopedIdent->abortOnError()) {
						llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(
								*context, "error_abort", builder->GetInsertBlock()->getParent());
						llvm::BasicBlock* continueBlock =
								llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());
						builder->CreateCondBr(hasError, errorBlock, continueBlock);

						builder->SetInsertPoint(errorBlock);

						// Print error message with context->error_msg if available
						auto funcNameStr = builder->CreateGlobalString(name);
						builder->CreateCall(printErrorMsgFn, {ctx, funcNameStr});
						builder->CreateCall(printStackTraceFn, {ctx});
						builder->CreateCall(abortFn, {});
						builder->CreateUnreachable();

						// Continue - user-defined methods don't push success status
						builder->SetInsertPoint(continueBlock);
						// Note: don't drop here - only imported C functions push success status
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
			// Use InternalLinkage for user functions unless in export mode (shared library compilation)
			// InternalLinkage allows LLVM to eliminate unused functions via GlobalDCE
			auto linkage = exportMode ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			fn = llvm::Function::Create(fnTy, linkage, mangledName, *module);
		}

		// Generate any needed type casts before the function call
		generateCastInstructions(scopedIdent->parameterCasts(), ctx);

		// For fallible functions, clear error_code before the call
		// This ensures each call starts with a clean state
		auto preFallibleIt = fallibleFunctions.find(fullName);
		if (preFallibleIt != fallibleFunctions.end() && preFallibleIt->second) {
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
			auto currentError = builder->CreateLoad(int32Ty, testErrorAlloca, "cur_err");
			auto newError = builder->CreateSelect(hasError, errorCode, currentError, "new_err");
			builder->CreateStore(newError, testErrorAlloca);
		}

		// Check if this is a fallible function (same logic as for regular identifiers)
		auto fallibleIt = fallibleFunctions.find(fullName);
		if (fallibleIt != fallibleFunctions.end() && fallibleIt->second) {
			// This is a fallible function - push error status after the call
			auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
			auto errorCode = builder->CreateLoad(int64Ty, errorCodePtr, "error_code");
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

				// Print error message with context->error_msg if available
				auto funcNameStr = builder->CreateGlobalString(name);
				builder->CreateCall(printErrorMsgFn, {ctx, funcNameStr});
				builder->CreateCall(printStackTraceFn, {ctx});
				builder->CreateCall(abortFn, {});
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
				{int64Ty,			// value (union as i64)
						int32Ty,	// type
						builder->getInt1Ty()}); // is_error_tainted

		// Pop the value to switch on from the stack
		auto stackFieldPtr =
				builder->CreateStructGEP(llvm::StructType::get(*context,
												 {ptrTy,		   // qd_stack* st
														 int64Ty,					   // int64_t error_code
														 ptrTy,   // char* error_msg
														 int32Ty,					   // int argc
														 ptrTy,   // char** argv
														 ptrTy}), // char* program_name
						ctx, 0, "st_ptr");
		auto stack = builder->CreateLoad(ptrTy, stackFieldPtr, "st");

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

			// Set debug location to the case value's line for proper stepping
			// (caseNode->line() returns the closing brace position due to parsing order)
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && caseValue) {
				unsigned caseLine = static_cast<unsigned>(caseValue->line());
				unsigned caseColumn = static_cast<unsigned>(caseValue->column());
				if (caseLine > 0) {
					auto debugLoc = llvm::DILocation::get(*context, caseLine, caseColumn, debugScopeStack.back());
					builder->SetCurrentDebugLocation(debugLoc);
				}
			}
			llvm::Value* matches = nullptr;

			if (caseValue->type() == IAstNode::Type::LITERAL) {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(caseValue);

				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					// Compare switch value with case value (integer)
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
					auto switchVal = builder->CreateLoad(int64Ty, valuePtr, "switch_val");

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
						auto charPtrTy = ptrTy;
						auto strcmpTy = llvm::FunctionType::get(int32Ty, {charPtrTy, charPtrTy}, false);
						strcmpFn = llvm::Function::Create(
								strcmpTy, llvm::Function::ExternalLinkage, "strcmp", module.get());
					}

					// Get switch string value (qd_string_t*)
					auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
					auto switchStrPtr =
							builder->CreateLoad(ptrTy, valuePtr, "switch_str");

					// Call qd_string_data to get const char*
					auto switchStrData = builder->CreateCall(qdStringDataFn, {switchStrPtr}, "switch_str_data");

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
							auto charPtrTy = ptrTy;
							auto strcmpTy =
									llvm::FunctionType::get(int32Ty, {charPtrTy, charPtrTy}, false);
							strcmpFn = llvm::Function::Create(
									strcmpTy, llvm::Function::ExternalLinkage, "strcmp", module.get());
						}

						auto switchStrPtr =
								builder->CreateLoad(ptrTy, valuePtr, "switch_str");

						// Call qd_string_data to get const char*
						auto switchStrData = builder->CreateCall(qdStringDataFn, {switchStrPtr}, "switch_str_data");

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
						auto switchVal = builder->CreateLoad(int64Ty, valuePtr, "switch_val");
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
					// Debug location will be set by generateNode for the body
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
		auto switchType = builder->CreateLoad(int32Ty, typePtr, "switch_type");
		auto isString = builder->CreateICmpEQ(switchType, builder->getInt32(3), "is_string"); // QD_STACK_TYPE_STR = 3

		llvm::BasicBlock* freeStringBB = llvm::BasicBlock::Create(*context, "free_string", currentFn);
		llvm::BasicBlock* skipFreeBB = llvm::BasicBlock::Create(*context, "skip_free", currentFn);

		builder->CreateCondBr(isString, freeStringBB, skipFreeBB);

		// Release string reference
		builder->SetInsertPoint(freeStringBB);
		auto valuePtr = builder->CreateStructGEP(switchElemTy, switchElem, 0, "value_ptr");
		auto strPtr = builder->CreateLoad(ptrTy, valuePtr, "str_ptr");
		builder->CreateCall(qdStringReleaseFn, {strPtr});
		builder->CreateBr(skipFreeBB);

		// Skip free block
		builder->SetInsertPoint(skipFreeBB);
	}

	// generateLocal, generateLocalOne, generateLocalCleanup, collectAllCapturesFromAST are in generator_nodes_locals.cc

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
