// Instruction generation for LLVM code generator
// Extracted from generator_nodes.cc for maintainability

#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::generateInstruction(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& name = inst->name();

		// Compile-time stack path for native functions
		if (useCompileTimeStack) {
			// Check if instruction name shadows a native local variable
			auto nativeLocalIt = nativeLocalVariables.find(name);
			if (nativeLocalIt != nativeLocalVariables.end()) {
				llvm::Value* val =
						builder->CreateLoad(nativeLocalIt->second->getAllocatedType(), nativeLocalIt->second, name);
				compileTimeStack.push_back(val);
				return;
			}

			// Arithmetic operations (type-aware: check operand type for int vs float)
			if (name == "+" || name == "add") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(builder->CreateFAdd(a, b, "fadd"));
				} else {
					compileTimeStack.push_back(builder->CreateNSWAdd(a, b, "add"));
				}
				return;
			}
			if (name == "-" || name == "sub") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(builder->CreateFSub(a, b, "fsub"));
				} else {
					compileTimeStack.push_back(builder->CreateNSWSub(a, b, "sub"));
				}
				return;
			}
			if (name == "*" || name == "mul") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(builder->CreateFMul(a, b, "fmul"));
				} else {
					compileTimeStack.push_back(builder->CreateNSWMul(a, b, "mul"));
				}
				return;
			}
			if (name == "/" || name == "div") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(builder->CreateFDiv(a, b, "fdiv"));
				} else {
					compileTimeStack.push_back(builder->CreateSDiv(a, b, "div"));
				}
				return;
			}
			if (name == "%" || name == "mod") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(builder->CreateFRem(a, b, "fmod"));
				} else {
					compileTimeStack.push_back(builder->CreateSRem(a, b, "mod"));
				}
				return;
			}
			// Comparison operations (always return i64)
			if (name == "<" || name == "lt") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				llvm::Value* cmp = a->getType()->isDoubleTy() ? builder->CreateFCmpOLT(a, b, "flt")
															  : builder->CreateICmpSLT(a, b, "lt");
				compileTimeStack.push_back(builder->CreateZExt(cmp, int64Ty, "lt_i64"));
				return;
			}
			if (name == ">" || name == "gt") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				llvm::Value* cmp = a->getType()->isDoubleTy() ? builder->CreateFCmpOGT(a, b, "fgt")
															  : builder->CreateICmpSGT(a, b, "gt");
				compileTimeStack.push_back(builder->CreateZExt(cmp, int64Ty, "gt_i64"));
				return;
			}
			if (name == "==" || name == "eq") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				llvm::Value* cmp = a->getType()->isDoubleTy() ? builder->CreateFCmpOEQ(a, b, "feq")
															  : builder->CreateICmpEQ(a, b, "eq");
				compileTimeStack.push_back(builder->CreateZExt(cmp, int64Ty, "eq_i64"));
				return;
			}
			if (name == "!=" || name == "neq") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				llvm::Value* cmp = a->getType()->isDoubleTy() ? builder->CreateFCmpONE(a, b, "fne")
															  : builder->CreateICmpNE(a, b, "ne");
				compileTimeStack.push_back(builder->CreateZExt(cmp, int64Ty, "ne_i64"));
				return;
			}
			if (name == "<=" || name == "lte") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				llvm::Value* cmp = a->getType()->isDoubleTy() ? builder->CreateFCmpOLE(a, b, "fle")
															  : builder->CreateICmpSLE(a, b, "le");
				compileTimeStack.push_back(builder->CreateZExt(cmp, int64Ty, "le_i64"));
				return;
			}
			if (name == ">=" || name == "gte") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				// Auto-promote: if either operand is double, promote the other
				bool aIsDouble = a->getType()->isDoubleTy();
				bool bIsDouble = b->getType()->isDoubleTy();
				if (aIsDouble && !bIsDouble) {
					b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
				} else if (!aIsDouble && bIsDouble) {
					a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
				}
				llvm::Value* cmp = a->getType()->isDoubleTy() ? builder->CreateFCmpOGE(a, b, "fge")
															  : builder->CreateICmpSGE(a, b, "ge");
				compileTimeStack.push_back(builder->CreateZExt(cmp, int64Ty, "ge_i64"));
				return;
			}
			// Bitwise operations (integer-only, no float changes needed)
			if (name == "and") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				compileTimeStack.push_back(builder->CreateAnd(a, b, "and"));
				return;
			}
			if (name == "or") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				compileTimeStack.push_back(builder->CreateOr(a, b, "or"));
				return;
			}
			if (name == "xor") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				compileTimeStack.push_back(builder->CreateXor(a, b, "xor"));
				return;
			}
			if (name == "not") {
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				compileTimeStack.push_back(builder->CreateNot(a, "not"));
				return;
			}
			if (name == "shl") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				compileTimeStack.push_back(builder->CreateShl(a, b, "shl"));
				return;
			}
			if (name == "shr") {
				llvm::Value* b = compileTimeStack.back();
				compileTimeStack.pop_back();
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				compileTimeStack.push_back(builder->CreateAShr(a, b, "shr"));
				return;
			}
			// Increment/decrement (type-aware)
			if (name == "++") {
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(
							builder->CreateFAdd(a, llvm::ConstantFP::get(builder->getDoubleTy(), 1.0), "finc"));
				} else {
					compileTimeStack.push_back(builder->CreateNSWAdd(a, builder->getInt64(1), "inc"));
				}
				return;
			}
			if (name == "--") {
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(
							builder->CreateFSub(a, llvm::ConstantFP::get(builder->getDoubleTy(), 1.0), "fdec"));
				} else {
					compileTimeStack.push_back(builder->CreateNSWSub(a, builder->getInt64(1), "dec"));
				}
				return;
			}
			if (name == "neg") {
				llvm::Value* a = compileTimeStack.back();
				compileTimeStack.pop_back();
				if (a->getType()->isDoubleTy()) {
					compileTimeStack.push_back(builder->CreateFNeg(a, "fneg"));
				} else {
					compileTimeStack.push_back(builder->CreateNeg(a, "neg"));
				}
				return;
			}
			// Stack operations
			if (name == "dup") {
				compileTimeStack.push_back(compileTimeStack.back());
				return;
			}
			if (name == "dup2") {
				size_t sz = compileTimeStack.size();
				compileTimeStack.push_back(compileTimeStack[sz - 2]);
				compileTimeStack.push_back(compileTimeStack[sz - 1]);
				return;
			}
			if (name == "swap") {
				size_t sz = compileTimeStack.size();
				std::swap(compileTimeStack[sz - 1], compileTimeStack[sz - 2]);
				return;
			}
			if (name == "drop") {
				compileTimeStack.pop_back();
				return;
			}
			if (name == "drop2") {
				compileTimeStack.pop_back();
				compileTimeStack.pop_back();
				return;
			}
			if (name == "over") {
				size_t sz = compileTimeStack.size();
				compileTimeStack.push_back(compileTimeStack[sz - 2]);
				return;
			}
			if (name == "rot") {
				size_t sz = compileTimeStack.size();
				llvm::Value* a = compileTimeStack[sz - 3];
				compileTimeStack[sz - 3] = compileTimeStack[sz - 2];
				compileTimeStack[sz - 2] = compileTimeStack[sz - 1];
				compileTimeStack[sz - 1] = a;
				return;
			}
			if (name == "nip") {
				size_t sz = compileTimeStack.size();
				compileTimeStack[sz - 2] = compileTimeStack[sz - 1];
				compileTimeStack.pop_back();
				return;
			}
			if (name == "tuck") {
				size_t sz = compileTimeStack.size();
				llvm::Value* top = compileTimeStack[sz - 1];
				compileTimeStack.insert(compileTimeStack.begin() + static_cast<long>(sz - 2), top);
				return;
			}
			if (name == "pick") {
				// pick N: copy the Nth element from top (0-indexed)
				// In compile-time stack, we can't do dynamic pick,
				// but for constant N this would work. For now, fall through.
			}
			if (name == "roll") {
				// Similar to pick but removes the element. Fall through for now.
			}
			// print: materialize top value to runtime stack, call runtime print
			if (name == "print") {
				llvm::Value* val = compileTimeStack.back();
				compileTimeStack.pop_back();
				if (val->getType()->isDoubleTy()) {
					generateInlinePushFloatValue(ctx, val);
				} else {
					generateInlinePushIntValue(ctx, val);
				}
				llvm::Function* printFn = module->getFunction("qd_print");
				if (!printFn) {
					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					printFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_print", *module);
				}
				builder->CreateCall(printFn, {ctx});
				return;
			}
			if (name == "prints") {
				builder->CreateCall(printsFn, {ctx});
				return;
			}
			if (name == "nl") {
				builder->CreateCall(nlFn, {ctx});
				return;
			}
			// For any unhandled instruction in compile-time stack mode, fall through
			// to the normal path (which will use the runtime stack)
		}

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
				storagePtr = builder->CreateLoad(ptrTy, localAlloca, name + "_storage");
			}

			// Extract type field
			llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 1, name + "_type_ptr");
			llvm::Value* type = builder->CreateLoad(int32Ty, typePtr, name + "_type");

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
			llvm::Value* intVal = builder->CreateLoad(int64Ty, valuePtr, name + "_i");
			generateInlinePushIntValue(ctx, intVal);
			builder->CreateBr(endBlock);

			// FLOAT block
			builder->SetInsertPoint(floatBlock);
			llvm::Value* floatVal = builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_f");
			builder->CreateCall(pushFloatFn, {ctx, floatVal});
			builder->CreateBr(endBlock);

			// STR block
			builder->SetInsertPoint(strBlock);
			llvm::Value* strVal = builder->CreateLoad(ptrTy, valuePtr, name + "_s");
			builder->CreateCall(pushStrRefFn, {ctx, strVal});
			builder->CreateBr(endBlock);

			// PTR block
			builder->SetInsertPoint(ptrBlock);
			llvm::Value* ptrVal = builder->CreateLoad(ptrTy, valuePtr, name + "_p");
			builder->CreateCall(qdPtrRetainFn, {ptrVal});
			builder->CreateCall(pushPtrFn, {ctx, ptrVal});
			builder->CreateBr(endBlock);

			builder->SetInsertPoint(endBlock);

			// If this local variable has a known struct type, track it for the next pop
			auto structTypeIt = localVariableStructTypes.find(name);
			if (structTypeIt != localVariableStructTypes.end()) {
				lastStructConstructed = structTypeIt->second;
				lastStructWasConstructedInPlace = false;
			}
			return;
		}

		// Handle method call on struct (marked by semantic validator)
		if (inst->isMethodCall()) {
			const std::string& receiverType = inst->receiverType();
			if (!receiverType.empty()) {
				// Generate method call. The identifier path uses the userFunctions map
				// which is populated with both qualified and unqualified method keys
				// (see generator.cc:812-817). We use the same lookup so method calls
				// that were parsed as AstNodeInstruction (because their name collides
				// with a builtin instruction, e.g. "depth", "clear", "len") resolve
				// consistently with the identifier path.
				llvm::Function* methodFn = nullptr;
				std::string methodKey = receiverType + "::" + name;
				auto userFnIt = userFunctions.find(methodKey);
				if (userFnIt != userFunctions.end()) {
					methodFn = userFnIt->second;
				}
				if (!methodFn && !currentModuleName.empty() && receiverType.find("::") == std::string::npos) {
					// Try with current module qualifier
					std::string qualifiedKey = currentModuleName + "::" + receiverType + "::" + name;
					userFnIt = userFunctions.find(qualifiedKey);
					if (userFnIt != userFunctions.end()) {
						methodFn = userFnIt->second;
					}
				}
				// Legacy fallback: construct mangled LLVM name directly
				std::string mangledFnName;
				if (!methodFn) {
					std::string qualifiedReceiverType = receiverType;
					if (receiverType.find("::") == std::string::npos && !currentModuleName.empty()) {
						qualifiedReceiverType = currentModuleName + "::" + receiverType;
					}
					mangledFnName = "usr_" + qualifiedReceiverType + "_" + name;
					for (size_t pos = 0; (pos = mangledFnName.find("::")) != std::string::npos;) {
						mangledFnName.replace(pos, 2, "_");
					}
					methodFn = module->getFunction(mangledFnName);
					if (!methodFn) {
						std::string fallbackFnName = "usr_" + receiverType + "_" + name;
						for (size_t pos = 0; (pos = fallbackFnName.find("::")) != std::string::npos;) {
							fallbackFnName.replace(pos, 2, "_");
						}
						methodFn = module->getFunction(fallbackFnName);
					}
				}
				if (!methodFn) {
					// Method not found - this shouldn't happen if validation passed
					llvm::errs() << "Method function not found: " << methodKey << "\n";
					return;
				}

				// Rotate stack so receiver is on top before calling method
				// Only roll if receiver is NOT already on top
				size_t receiverPositionFromTop = inst->methodReceiverPositionFromTop();
				if (receiverPositionFromTop > 0) {
					// Push the count and call qd_roll
					// roll N brings the Nth element (0-indexed from top) to top
					builder->CreateCall(pushIntFn, {ctx, builder->getInt64(receiverPositionFromTop)});
					auto rollFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					auto rollFn = module->getOrInsertFunction("qd_roll", rollFnTy);
					builder->CreateCall(rollFn, {ctx});
				}

				builder->CreateCall(methodFn, {ctx});

				// Handle fallible method calls
				if (inst->abortOnError() || inst->propagateOnError()) {
					// Check error code
					auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
					auto errorCode = builder->CreateLoad(int64Ty, errorCodePtr, "error_code");
					auto hasError = builder->CreateICmpNE(errorCode, builder->getInt64(0), "has_error");

					if (inst->abortOnError()) {
						llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(
								*context, "error_abort", builder->GetInsertBlock()->getParent());
						llvm::BasicBlock* continueBlock =
								llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());

						builder->CreateCondBr(hasError, errorBlock, continueBlock);

						// Error block: print message and abort
						builder->SetInsertPoint(errorBlock);

						auto funcNameStr = builder->CreateGlobalString(name);
						builder->CreateCall(printErrorMsgFn, {ctx, funcNameStr});
						builder->CreateCall(printStackTraceFn, {ctx});
						builder->CreateCall(exitFn, {builder->getInt32(1)});
						builder->CreateUnreachable();

						// Continue block - user-defined methods don't push a success status
						builder->SetInsertPoint(continueBlock);
					} else {
						// ? operator: propagate error to caller
						llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(
								*context, "error_propagate", builder->GetInsertBlock()->getParent());
						llvm::BasicBlock* continueBlock =
								llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());

						builder->CreateCondBr(hasError, errorBlock, continueBlock);

						builder->SetInsertPoint(errorBlock);
						if (currentFunctionReturnBlock) {
							builder->CreateBr(currentFunctionReturnBlock);
						} else {
							builder->CreateUnreachable();
						}

						builder->SetInsertPoint(continueBlock);
					}
				}

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

			// Compile-time stack path for numeric casts
			if (useCompileTimeStack) {
				llvm::Value* val = compileTimeStack.back();
				compileTimeStack.pop_back();
				if (typeParam == "f64" || typeParam == "f" || typeParam == "float" || typeParam == "float64") {
					if (!val->getType()->isDoubleTy()) {
						val = builder->CreateSIToFP(val, builder->getDoubleTy(), "cast_f64");
					}
				} else if (typeParam == "i64" || typeParam == "i" || typeParam == "int" || typeParam == "int64") {
					if (val->getType()->isDoubleTy()) {
						val = builder->CreateFPToSI(val, int64Ty, "cast_i64");
					}
				}
				compileTimeStack.push_back(val);
				return;
			}

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

		// Handle sizeof<T> instruction (compile-time type size)
		if (name == "sizeof" && inst->hasTypeParam()) {
			const std::string& typeParam = inst->typeParam();
			int64_t size = 0;

			// Check primitive types
			if (typeParam == "i64" || typeParam == "u64" || typeParam == "f64" || typeParam == "ptr") {
				size = 8;
			} else if (typeParam == "i32" || typeParam == "u32" || typeParam == "f32") {
				size = 4;
			} else if (typeParam == "i16" || typeParam == "u16") {
				size = 2;
			} else if (typeParam == "i8" || typeParam == "u8") {
				size = 1;
			} else if (typeParam == "str") {
				size = 8; // str is a pointer
			} else {
				// Check if it's a struct type
				auto it = structDefinitions.find(typeParam);
				if (it != structDefinitions.end()) {
					size = static_cast<int64_t>(it->second.totalSize);
				} else {
					// Try with current module prefix
					std::string qualifiedName = currentModuleName + "::" + typeParam;
					auto qualIt = structDefinitions.find(qualifiedName);
					if (qualIt != structDefinitions.end()) {
						size = static_cast<int64_t>(qualIt->second.totalSize);
					} else {
						// Unknown type - default to pointer size
						size = 8;
					}
				}
			}

			// Push the size as an integer
			generateInlinePushInt(ctx, size);
			return;
		}

		// Handle sizeof instruction without type parameter (runtime - pops value, pushes its size)
		if (name == "sizeof" && !inst->hasTypeParam()) {
			// All stack elements are 8 bytes (union of i64/f64/ptr)
			// Pop the value and push 8
			generateInlineDrop(ctx);
			generateInlinePushInt(ctx, 8);
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
			} else {
				fnName = "qd_" + name;
			}

			// Check if function already exists
			llvm::Function* runtimeFn = module->getFunction(fnName);
			if (!runtimeFn) {
				// Declare it: int fn(qd_context*)
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

} // namespace Qd
