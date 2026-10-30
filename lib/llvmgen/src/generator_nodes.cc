#include "generator_impl.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace Qd {

	namespace {
		// What consumes the status value a fallible call leaves on the stack. The semantic
		// validator already guarantees a bare fallible call is immediately followed by `if` or
		// `switch`, so these are the only cases; None covers `!`/`?`, which consume nothing.
		enum class FallibleConsumer {
			None,
			If,
			Switch
		};

		// A fallible call has to push a different status depending on who reads it:
		//
		//   `if`     tests the value for truthiness, so it needs a boolean -- 1 success, 0 failure.
		//            Pushing the raw error code here is what made `io::open ... if {} else {}`
		//            take the *success* branch on a missing file and then underflow, because any
		//            code >= 2 is truthy.
		//   `switch` matches against Ok and specific error codes, so it needs the code itself --
		//            1 on success, error_code on failure. Pushing a bare 0/1 here is what made
		//            `switch { Ok {} SomeErr {} }` never match a specific code for user-defined
		//            functions.
		//
		// Determined by looking at the call's next sibling in the AST, which is exactly what the
		// validator inspects when it enforces the rule.
		FallibleConsumer fallibleConsumerOf(IAstNode* callNode) {
			if (!callNode) {
				return FallibleConsumer::None;
			}
			IAstNode* parent = callNode->parent();
			if (!parent) {
				return FallibleConsumer::None;
			}
			for (size_t i = 0; i + 1 < parent->childCount(); i++) {
				if (parent->child(i) != callNode) {
					continue;
				}
				IAstNode* next = parent->child(i + 1);
				if (!next) {
					return FallibleConsumer::None;
				}
				if (next->type() == IAstNode::Type::IF_STATEMENT) {
					return FallibleConsumer::If;
				}
				if (next->type() == IAstNode::Type::SWITCH_STATEMENT) {
					return FallibleConsumer::Switch;
				}
				return FallibleConsumer::None;
			}
			return FallibleConsumer::None;
		}
	}

	void LlvmGenerator::Impl::generateClearErrorState(llvm::Value* ctx) {
		auto errorCodePtr =
				builder->CreateStructGEP(contextStructTy, ctx, CTX_FIELD_ERROR_CODE, "pre_call_error_code_ptr");
		builder->CreateStore(builder->getInt64(0), errorCodePtr);
		auto hasErrorPtr =
				builder->CreateStructGEP(contextStructTy, ctx, CTX_FIELD_HAS_ERROR, "pre_call_has_error_ptr");
		builder->CreateStore(builder->getInt64(0), hasErrorPtr);
	}

	LlvmGenerator::Impl::ErrorState LlvmGenerator::Impl::generateReadErrorState(llvm::Value* ctx, const char* name) {
		// `has_error` is the authoritative signal for Quadrate-defined functions: qd_panic sets it
		// whatever code it carries, so `"msg" Err panic` (code 0) still reads as a failure.
		auto hasErrorPtr = builder->CreateStructGEP(contextStructTy, ctx, CTX_FIELD_HAS_ERROR, "has_error_ptr");
		auto hasErrorFlag = builder->CreateLoad(int64Ty, hasErrorPtr, "has_error_flag");
		auto flagSet = builder->CreateICmpNE(hasErrorFlag, builder->getInt64(0), "has_error_flag_set");

		// Imported C functions are inconsistent about error state -- some set error_code, some
		// only push the code -- and none of them set the flag, so a non-zero code still counts.
		auto errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, CTX_FIELD_ERROR_CODE, "error_code_ptr");
		auto errorCode = builder->CreateLoad(int64Ty, errorCodePtr, "error_code");
		auto codeSet = builder->CreateICmpNE(errorCode, builder->getInt64(0), "error_code_set");

		return ErrorState{builder->CreateOr(flagSet, codeSet, name), errorCode};
	}

	// The tail of a fallible call: `!` aborts, `?` returns to the caller, and a bare call pushes
	// a status for the `if` or `switch` that reads it.
	void LlvmGenerator::Impl::generateFallibleCallEpilogue(llvm::Value* ctx, const char* name, IAstNode* callNode,
			bool abortOnError, bool propagateOnError, bool calleePushesStatus) {
		auto errorState = generateReadErrorState(ctx);
		auto hasError = errorState.failed;
		auto errorCode = errorState.code;

		if (abortOnError) {
			llvm::BasicBlock* errorBlock =
					llvm::BasicBlock::Create(*context, "error_abort", builder->GetInsertBlock()->getParent());
			llvm::BasicBlock* continueBlock =
					llvm::BasicBlock::Create(*context, "no_error", builder->GetInsertBlock()->getParent());
			builder->CreateCondBr(hasError, errorBlock, continueBlock);

			builder->SetInsertPoint(errorBlock);
			auto funcNameStr = builder->CreateGlobalString(name);
			builder->CreateCall(fatalRecoverFn, {ctx, funcNameStr, llvm::ConstantPointerNull::get(ptrTy)});
			builder->CreateCall(printErrorMsgFn, {ctx, funcNameStr});
			builder->CreateCall(printStackTraceFn, {ctx});
			builder->CreateCall(exitFn, {});
			builder->CreateUnreachable();

			builder->SetInsertPoint(continueBlock);
			if (calleePushesStatus) {
				// `!` handles the failure through the error state, so the status goes
				generateInlineDrop(ctx);
			}
			return;
		}

		if (propagateOnError) {
			if (calleePushesStatus) {
				// `?` handles the failure through the error state, so the status goes either way
				generateInlineDrop(ctx);
			}
			llvm::BasicBlock* errorBlock =
					llvm::BasicBlock::Create(*context, "error_propagate", builder->GetInsertBlock()->getParent());
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
			return;
		}

		// Bare call: the status is shaped for whoever consumes it -- 1/0 for an `if`, the error
		// code itself for a `switch`, which matches against specific codes.
		const bool wantsCode = fallibleConsumerOf(callNode) == FallibleConsumer::Switch;
		if (calleePushesStatus) {
			// An imported C function pushes its raw error code, which is what a `switch` wants.
			// For an `if` any code >= 2 is truthy, which selected the success branch on failure,
			// so it becomes a boolean.
			if (fallibleConsumerOf(callNode) == FallibleConsumer::If) {
				generateInlineDrop(ctx);
				builder->CreateCall(pushIntFn, {ctx, builder->CreateSelect(hasError, builder->getInt64(0),
															 builder->getInt64(1), "success_status")});
			}
			return;
		}
		llvm::Value* onFailure = wantsCode ? static_cast<llvm::Value*>(errorCode) : builder->getInt64(0);
		auto successStatus = builder->CreateSelect(hasError, onFailure, builder->getInt64(1), "success_status");
		builder->CreateCall(pushIntFn, {ctx, successStatus});
	}

	// A d128 literal is read here, by the runtime's own reader, so it rounds exactly as
	// strconv::parse_d128 rounds the same digits, and what is pushed is the encoding as a
	// constant: nothing is read at run time.
	void LlvmGenerator::Impl::generateD128Literal(const std::string& text, llvm::Value* ctx) {
		uint64_t low = 0;
		uint64_t high = 0;
		if (!d128LiteralWords(text, low, high)) {
			std::cerr << "quadc: error: Invalid d128 literal '" << text << "'" << std::endl;
			compilationFailed = true;
		}
		builder->CreateCall(pushD128BitsFn, {ctx, builder->getInt64(low), builder->getInt64(high)});
	}

	void LlvmGenerator::Impl::generateLiteral(AstNodeLiteral* lit, llvm::Value* ctx) {
		auto type = lit->literalType();
		const auto& value = lit->value();

		switch (type) {
		case AstNodeLiteral::LiteralType::INTEGER: {
			int64_t val = 0;
			if (!parseIntegerLiteral(value, val)) {
				std::cerr << "quadc: error: Invalid integer literal '" << value << "' (out of range or invalid format)"
						  << std::endl;
				compilationFailed = true;
			}
			if (useCompileTimeStack) {
				// Compile-time stack: push LLVM constant directly
				compileTimeStack.push_back(builder->getInt64(static_cast<uint64_t>(val)));
			} else if (debugInfoEnabled) {
				// Use function calls when debug info is enabled for better debuggability
				builder->CreateCall(pushIntFn, {ctx, builder->getInt64(static_cast<uint64_t>(val))});
			} else {
				// Use inline code for release builds for better performance
				generateInlinePushInt(ctx, val);
			}
			lastPushedType = LastPushedType::INTEGER;
			break;
		}
		case AstNodeLiteral::LiteralType::FLOAT: {
			// readFloatLiteral rather than a strtod of its own, so what the validator
			// accepted is exactly what reaches LLVM: a subnormal is a value like any
			// other, and only an overflow to infinity is rejected.
			double dval = 0.0;
			if (readFloatLiteral(value, dval) != std::errc()) {
				std::cerr << "quadc: error: Invalid float literal '" << value << "' (out of range or invalid format)"
						  << std::endl;
				compilationFailed = true;
				dval = 0.0;
			}
			auto val = llvm::ConstantFP::get(builder->getDoubleTy(), dval);
			if (useCompileTimeStack) {
				compileTimeStack.push_back(val);
			} else {
				builder->CreateCall(pushFloatFn, {ctx, val});
			}
			lastPushedType = LastPushedType::FLOAT;
			break;
		}
		case AstNodeLiteral::LiteralType::D128:
			generateD128Literal(value, ctx);
			lastPushedType = LastPushedType::UNKNOWN;
			break;
		case AstNodeLiteral::LiteralType::STRING: {
			auto strValue = builder->CreateGlobalString(decodeStringLiteral(value), ".str");
			builder->CreateCall(pushStrFn, {ctx, strValue});
			lastPushedType = LastPushedType::STRING;
			break;
		}
		case AstNodeLiteral::LiteralType::BOOL: {
			// true/Ok → 1, false/Err → 0
			int64_t val = (value == "true" || value == "Ok") ? 1 : 0;
			if (useCompileTimeStack) {
				compileTimeStack.push_back(builder->getInt64(static_cast<uint64_t>(val)));
			} else if (debugInfoEnabled) {
				builder->CreateCall(pushIntFn, {ctx, builder->getInt64(static_cast<uint64_t>(val))});
			} else {
				generateInlinePushInt(ctx, val);
			}
			lastPushedType = LastPushedType::INTEGER;
			break;
		}
		case AstNodeLiteral::LiteralType::NULL_PTR: {
			// A ptr, as the type checker and the interpreter have it. This pushed the i64 0, so a
			// native that checks for a ptr, `null 0 mem::get_i64`, refused it as the wrong type.
			if (useCompileTimeStack) {
				compileTimeStack.push_back(builder->getInt64(0));
			} else {
				builder->CreateCall(pushPtrFn, {ctx, llvm::ConstantPointerNull::get(ptrTy)});
			}
			lastPushedType = LastPushedType::POINTER;
			break;
		}
		}
	}

	// generateInstruction is in generator_nodes_instructions.cc

	// Pushes the local variable `name` when there is one: a native local onto the compile-time
	// stack, any other onto the runtime stack, as its stored type says. False when `name` is not
	// a local. An instruction calls this too, for a local that shares a builtin's name.
	bool LlvmGenerator::Impl::generateLocalPush(const std::string& name, llvm::Value* ctx) {
		// Check if it's a native local variable (compile-time stack mode)
		if (useCompileTimeStack) {
			auto nativeLocalIt = nativeLocalVariables.find(name);
			if (nativeLocalIt != nativeLocalVariables.end()) {
				// Load from typed alloca and push to compile-time stack
				llvm::Value* val =
						builder->CreateLoad(nativeLocalIt->second->getAllocatedType(), nativeLocalIt->second, name);
				compileTimeStack.push_back(val);
				return true;
			}
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
				storagePtr = builder->CreateLoad(ptrTy, localAlloca, name + "_storage");
			}

			// Fast path for numeric functions with non-captured locals
			// Check stored type to handle both int and float locals
			if (currentFunctionIsIntegerOnly && !isIndirect) {
				llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 1, name + "_type_ptr");
				llvm::Value* typeVal = builder->CreateLoad(int32Ty, typePtr, name + "_type");
				llvm::Value* isFloat = builder->CreateICmpEQ(typeVal, builder->getInt32(1), name + "_is_float");

				llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
				llvm::BasicBlock* floatBB = llvm::BasicBlock::Create(*context, name + "_float_load", currentFn);
				llvm::BasicBlock* intBB = llvm::BasicBlock::Create(*context, name + "_int_load", currentFn);
				llvm::BasicBlock* contBB = llvm::BasicBlock::Create(*context, name + "_load_done", currentFn);
				builder->CreateCondBr(isFloat, floatBB, intBB);

				builder->SetInsertPoint(floatBB);
				llvm::Value* fValuePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_fvalue_ptr");
				llvm::Value* fVal = builder->CreateLoad(builder->getDoubleTy(), fValuePtr, name + "_f");
				generateInlinePushFloatValue(ctx, fVal);
				builder->CreateBr(contBB);

				builder->SetInsertPoint(intBB);
				llvm::Value* iValuePtr = builder->CreateStructGEP(stackElementTy, storagePtr, 0, name + "_ivalue_ptr");
				llvm::Value* iVal = builder->CreateLoad(int64Ty, iValuePtr, name + "_i");
				generateInlinePushIntValue(ctx, iVal);
				builder->CreateBr(contBB);

				builder->SetInsertPoint(contBB);
				return true;
			}

			pushStackElement(storagePtr, ctx, name, localArrayVariables.count(name) > 0);

			// If this local variable has a known struct type, track it for the next pop
			auto structTypeIt = localVariableStructTypes.find(name);
			if (structTypeIt != localVariableStructTypes.end()) {
				lastStructConstructed = structTypeIt->second;
			}
			return true;
		}
		return false;
	}

	// Push the value a qd_stack_element_t holds, as its type tag says. A pointer is retained
	// for the stack's reference; `isArray` says it is known to be an array.
	void LlvmGenerator::Impl::pushStackElement(
			llvm::Value* elementPtr, llvm::Value* ctx, const std::string& name, bool isArray) {
		llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, elementPtr, 1, name + "_type_ptr");
		llvm::Value* type = builder->CreateLoad(int32Ty, typePtr, name + "_type");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elementPtr, 0, name + "_value_ptr");

		llvm::Function* fn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* intBlock = llvm::BasicBlock::Create(*context, name + "_int", fn);
		llvm::BasicBlock* floatBlock = llvm::BasicBlock::Create(*context, name + "_float", fn);
		llvm::BasicBlock* ptrBlock = llvm::BasicBlock::Create(*context, name + "_ptr", fn);
		llvm::BasicBlock* strBlock = llvm::BasicBlock::Create(*context, name + "_str", fn);
		llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context, name + "_pushed", fn);

		llvm::SwitchInst* switchInst = builder->CreateSwitch(type, endBlock, 4);
		switchInst->addCase(builder->getInt32(0), intBlock);   // QD_STACK_TYPE_INT
		switchInst->addCase(builder->getInt32(1), floatBlock); // QD_STACK_TYPE_FLOAT
		switchInst->addCase(builder->getInt32(2), ptrBlock);   // QD_STACK_TYPE_PTR
		switchInst->addCase(builder->getInt32(3), strBlock);   // QD_STACK_TYPE_STR
		addD128PushCase(switchInst, valuePtr, ctx, endBlock, name);

		builder->SetInsertPoint(intBlock);
		generateInlinePushIntValue(ctx, builder->CreateLoad(int64Ty, valuePtr, name + "_i"));
		builder->CreateBr(endBlock);

		builder->SetInsertPoint(floatBlock);
		builder->CreateCall(pushFloatFn, {ctx, builder->CreateLoad(builder->getDoubleTy(), valuePtr, name + "_f")});
		builder->CreateBr(endBlock);

		// The push retains the string
		builder->SetInsertPoint(strBlock);
		builder->CreateCall(pushStrRefFn, {ctx, builder->CreateLoad(ptrTy, valuePtr, name + "_s")});
		builder->CreateBr(endBlock);

		builder->SetInsertPoint(ptrBlock);
		llvm::Value* ptrVal = builder->CreateLoad(ptrTy, valuePtr, name + "_p");
		if (isArray) {
			llvm::Function* arrayRetainFn = module->getFunction("qd_array_retain");
			if (!arrayRetainFn) {
				auto arrayRetainFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				arrayRetainFn = llvm::Function::Create(
						arrayRetainFnTy, llvm::Function::ExternalLinkage, "qd_array_retain", *module);
			}
			builder->CreateCall(arrayRetainFn, {ptrVal});
		} else {
			// Could be an array or a struct; qd_ptr_retain tells them apart
			builder->CreateCall(qdPtrRetainFn, {ptrVal});
		}
		builder->CreateCall(pushPtrFn, {ctx, ptrVal});
		builder->CreateBr(endBlock);

		builder->SetInsertPoint(endBlock);
	}

	// A variable a closure captured by reference: its slot holds a pointer to the outer one
	bool LlvmGenerator::Impl::generateCapturedPush(const std::string& name, llvm::Value* ctx) {
		auto capIt = capturedVariableRefs.find(name);
		if (capIt == capturedVariableRefs.end()) {
			return false;
		}
		llvm::Value* outerVarPtr = builder->CreateLoad(ptrTy, capIt->second, name + "_cap_ptr");
		pushStackElement(outerVarPtr, ctx, name + "_cap", false);
		// A captured struct carries its type for a following `<<`, as a plain local does.
		auto capStructIt = localVariableStructTypes.find(name);
		if (capStructIt != localVariableStructTypes.end()) {
			lastStructConstructed = capStructIt->second;
		}
		return true;
	}

	// A `for` loop's iterator
	bool LlvmGenerator::Impl::generateIteratorPush(const std::string& name, llvm::Value* ctx) {
		auto iterIt = iteratorVars.find(name);
		if (iterIt == iteratorVars.end()) {
			return false;
		}
		if (useCompileTimeStack) {
			compileTimeStack.push_back(iterIt->second);
			return true;
		}

		// A loop whose bounds came off the runtime stack carries both halves,
		// because only the bounds' type tags say which one counts. Push the
		// bits and the tag the same flag selects rather than branching.
		auto floatIt = iteratorFloatVars.find(name);
		if (floatIt != iteratorFloatVars.end()) {
			llvm::Value* asBits = builder->CreateBitCast(floatIt->second.value, int64Ty, "iter_f_bits");
			llvm::Value* bits = builder->CreateSelect(floatIt->second.isFloat, asBits, iterIt->second, "iter_bits");
			llvm::Value* tag = builder->CreateSelect(
					floatIt->second.isFloat, builder->getInt32(1), builder->getInt32(0), "iter_type");
			generateInlinePushTaggedValue(ctx, bits, tag, "pushiter");
			return true;
		}

		// Push loop iterator as integer (inline for performance)
		generateInlinePushIntValue(ctx, iterIt->second);
		return true;
	}

	// A module-level `var`, loaded from its LLVM global. Works in both native (compile-time
	// stack) mode and runtime stack mode. The load is volatile so GVN/DCE can't hoist it above
	// an intervening function call that stores the global (AlwaysInliner can produce that
	// pattern when a helper is inlined multiple times into main).
	bool LlvmGenerator::Impl::generateGlobalVarPush(const std::string& name, llvm::Value* ctx) {
		auto gvIt = moduleGlobalVars.find(name);
		if (gvIt == moduleGlobalVars.end()) {
			return false;
		}
		llvm::GlobalVariable* gv = gvIt->second;
		const std::string& typeName = moduleGlobalVarTypes[name];

		// Classify for load/push: any non-scalar name (including struct
		// types from `var p = Point { ... }`) is treated as a pointer.
		bool isFloat = (typeName == "f64");
		bool isStr = (typeName == "str");
		bool isD128 = (typeName == "d128");
		bool isPtr = (typeName == "ptr") || isKnownStruct(typeName);

		llvm::Type* loadTy = int64Ty;
		if (isFloat) {
			loadTy = builder->getDoubleTy();
		} else if (isD128) {
			loadTy = slotValueTy;
		} else if (isStr || isPtr) {
			loadTy = ptrTy;
		}

		llvm::Value* v = builder->CreateLoad(loadTy, gv, name + "_g");
		llvm::cast<llvm::LoadInst>(v)->setVolatile(true);

		if (useCompileTimeStack) {
			compileTimeStack.push_back(v);
		} else if (isFloat) {
			builder->CreateCall(pushFloatFn, {ctx, v});
		} else if (isD128) {
			// A global holds its value from the start, its literal's encoding the initializer
			builder->CreateCall(pushD128BitsFn, {ctx, builder->CreateExtractValue(v, 0, name + "_g_low"),
														builder->CreateExtractValue(v, 1, name + "_g_high")});
		} else if (isStr) {
			// qd_push_s_ref refuses a null string and pushes nothing, returning -2
			// that nothing here reads -- so a `str` global read before it was ever
			// assigned left the stack one short and the *next* instruction died of
			// an underflow naming itself. A global that holds no string reads as the
			// empty one, which is what `str`'s zero is everywhere else.
			llvm::Function* strFn = builder->GetInsertBlock()->getParent();
			auto* emptyBB = llvm::BasicBlock::Create(*context, "gstr_empty", strFn);
			auto* valueBB = llvm::BasicBlock::Create(*context, "gstr_value", strFn);
			auto* contBB = llvm::BasicBlock::Create(*context, "gstr_cont", strFn);
			llvm::Value* isNull = builder->CreateICmpEQ(v, llvm::ConstantPointerNull::get(ptrTy), name + "_g_null");
			builder->CreateCondBr(isNull, emptyBB, valueBB);

			builder->SetInsertPoint(emptyBB);
			builder->CreateCall(pushStrFn, {ctx, getOrCreateGlobalString("")});
			builder->CreateBr(contBB);

			builder->SetInsertPoint(valueBB);
			builder->CreateCall(pushStrRefFn, {ctx, v});
			builder->CreateBr(contBB);

			builder->SetInsertPoint(contBB);
		} else if (isPtr) {
			// A pointer on the stack owns a reference: reading a local retains before
			// pushing, and every consumer releases -- `<<field` pops and calls
			// qd_ptr_release on the assumption that the push retained. Reading a global
			// did not, so the release was cancelling the one reference the global itself
			// owns. `var origin = Point { ... }` freed the struct on the *first* field
			// read, and every read after that was a use-after-free.
			// (qd_push_s_ref retains internally, which is why the str case above needs
			// nothing; the compile-time-stack path above pushes an SSA value that no
			// runtime release ever sees.)
			builder->CreateCall(qdPtrRetainFn, {v});
			builder->CreateCall(pushPtrFn, {ctx, v});
		} else {
			// Integer (sized or unknown → i64 default).
			builder->CreateCall(pushIntFn, {ctx, v});
		}
		// Remember the struct type of this var so subsequent field
		// accesses (`var.<<field`) resolve correctly.
		if (isKnownStruct(typeName)) {
			lastFieldAccessResultType = typeName;
		}
		return true;
	}

	// A `const`, pushed as its spelling says
	bool LlvmGenerator::Impl::generateConstantPush(const std::string& name, llvm::Value* ctx) {
		auto constIt = moduleConstants.find(name);
		if (constIt == moduleConstants.end()) {
			return false;
		}
		const std::string& value = constIt->second;

		// Compile-time stack path for numeric constants
		if (useCompileTimeStack) {
			if (value.empty() || (value.size() >= 2 && value.front() == '"')) {
				// String constant - fall through to runtime path
			} else if (isFloatLiteralText(value)) {
				// Float constant
				compileTimeStack.push_back(llvm::ConstantFP::get(builder->getDoubleTy(), floatLiteralOr(value, 0.0)));
				return true;
			} else {
				int64_t intValue = 0;
				parseIntegerLiteral(value, intValue);
				compileTimeStack.push_back(builder->getInt64(static_cast<uint64_t>(intValue)));
				return true;
			}
		}

		// Determine the type of the constant and push it
		if (!value.empty() && value.size() >= 2 && value.front() == '"' && value.back() == '"') {
			llvm::Value* strConst = builder->CreateGlobalString(decodeStringLiteral(value));
			builder->CreateCall(pushStrFn, {ctx, strConst});
		} else if (isD128LiteralText(value)) {
			generateD128Literal(value, ctx);
		} else if (isFloatLiteralText(value)) {
			// Float constant
			llvm::Value* floatConst = llvm::ConstantFP::get(builder->getDoubleTy(), floatLiteralOr(value, 0.0));
			builder->CreateCall(pushFloatFn, {ctx, floatConst});
		} else {
			// Integer constant
			int64_t intValue = 0;
			if (!parseIntegerLiteral(value, intValue)) {
				std::cerr << "quadc: error: Invalid integer constant '" << value << "' (out of range or invalid format)"
						  << std::endl;
				compilationFailed = true;
			}
			llvm::Value* intConst = builder->getInt64(static_cast<uint64_t>(intValue));
			builder->CreateCall(pushIntFn, {ctx, intConst});
		}
		return true;
	}

	// A function pointer bound with `-> name` from an anonymous function; `call` takes it
	bool LlvmGenerator::Impl::generateFunctionAliasPush(const std::string& name, llvm::Value* ctx) {
		auto fpAliasIt = functionPointerAliases.find(name);
		if (fpAliasIt == functionPointerAliases.end()) {
			return false;
		}
		builder->CreateCall(pushPtrFn, {ctx, builder->CreateBitCast(fpAliasIt->second, ptrTy)});
		return true;
	}

	std::string LlvmGenerator::Impl::resolveCallee(AstNodeIdentifier* ident, bool& resolvedAsMethod) {
		const std::string& name = ident->name();
		resolvedAsMethod = false;
		// A method call the semantic validator identified: ReceiverType::methodName
		if (ident->isMethodCall()) {
			std::string method = ident->receiverType() + "::" + name;
			resolvedAsMethod = userFunctions.count(method) > 0;
			return resolvedAsMethod ? method : std::string();
		}
		// The plain name, then the current module's, for an intra-module call
		if (userFunctions.count(name) > 0) {
			return name;
		}
		if (currentModulePrefix != "main" && userFunctions.count(currentModulePrefix + "::" + name) > 0) {
			return currentModulePrefix + "::" + name;
		}
		// A method on the most recently pushed struct. This handles intra-module method calls
		// in cached module function bodies, where the semantic validator didn't type-check
		// (so isMethodCall was never set). The struct's name is tried qualified, then bare.
		if (!lastStructConstructed.empty()) {
			std::vector<std::string> receivers = {lastStructConstructed};
			const size_t sep = lastStructConstructed.rfind("::");
			if (sep != std::string::npos) {
				receivers.push_back(lastStructConstructed.substr(sep + 2));
			}
			for (const std::string& receiver : receivers) {
				std::string method = receiver + "::" + name;
				if (userFunctions.count(method) > 0) {
					resolvedAsMethod = true;
					return method;
				}
			}
		}
		return std::string();
	}

	// Integer-only caller to integer-only callee: pass the values on the compile-time stack to
	// the callee's native version. False when the callee has none.
	bool LlvmGenerator::Impl::generateCompileTimeNativeCall(const std::string& callee, llvm::Value* ctx) {
		auto calleeNativeIt = nativeFunctions.find(callee);
		if (calleeNativeIt == nativeFunctions.end()) {
			return false;
		}
		auto& calleeInfo = nativeFuncInfo[callee];
		// Pop args from compile-time stack (in reverse order)
		std::vector<llvm::Value*> args;
		args.push_back(ctx); // ctx is always first
		std::vector<llvm::Value*> poppedArgs;
		for (size_t i = 0; i < calleeInfo.inputCount; i++) {
			poppedArgs.push_back(compileTimeStack.back());
			compileTimeStack.pop_back();
		}
		// Reverse to get correct parameter order
		for (auto rit = poppedArgs.rbegin(); rit != poppedArgs.rend(); ++rit) {
			args.push_back(*rit);
		}
		// Call native function directly
		if (calleeInfo.outputCount == 1) {
			llvm::Value* result = builder->CreateCall(calleeNativeIt->second, args, "call_result");
			compileTimeStack.push_back(result);
		} else {
			builder->CreateCall(calleeNativeIt->second, args);
		}
		return true;
	}

	// The bits:: words that lower to one instruction
	bool LlvmGenerator::Impl::generateInlineBitsCall(const std::string& callee, llvm::Value* ctx) {
		static const std::map<std::string, void (Impl::*)(llvm::Value*)> inlined = {
				{"bits::and", &Impl::generateInlineBitAnd},
				{"bits::or", &Impl::generateInlineBitOr},
				{"bits::xor", &Impl::generateInlineBitXor},
				{"bits::not", &Impl::generateInlineBitNot},
				{"bits::lshift", &Impl::generateInlineBitLshift},
				{"bits::rshift", &Impl::generateInlineBitRshift},
		};
		auto it = inlined.find(callee);
		if (it == inlined.end()) {
			return false;
		}
		(this->*(it->second))(ctx);
		return true;
	}

	// A method takes its receiver on top; roll it there from `positionFromTop` when it is deeper.
	// roll N brings the Nth element (0-indexed from top) to top.
	void LlvmGenerator::Impl::generateReceiverRoll(size_t positionFromTop, llvm::Value* ctx) {
		if (positionFromTop == 0) {
			return;
		}
		builder->CreateCall(pushIntFn, {ctx, builder->getInt64(positionFromTop)});
		auto rollFnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		auto rollFn = module->getOrInsertFunction("qd_roll", rollFnTy);
		builder->CreateCall(rollFn, {ctx});
	}

	// Call the callee's native version directly when it has one, skipping the wrapper that
	// moves its arguments through the stack; otherwise the wrapper
	llvm::Value* LlvmGenerator::Impl::generateDirectCall(
			const std::string& callee, llvm::Function* wrapper, llvm::Value* ctx) {
		auto nativeIt = nativeFunctions.find(callee);
		if (nativeIt == nativeFunctions.end()) {
			return builder->CreateCall(wrapper, {ctx}, "call_result");
		}
		auto& info = nativeFuncInfo[callee];
		std::vector<llvm::Value*> nativeArgs;
		nativeArgs.push_back(ctx);
		std::vector<llvm::Value*> poppedArgs;
		for (size_t j = info.inputCount; j > 0; j--) {
			if (info.inputTypes[j - 1] == NativeParamType::F64) {
				poppedArgs.push_back(generateInlinePopFloat(ctx));
			} else {
				poppedArgs.push_back(generateInlinePopInt(ctx));
			}
		}
		for (auto rit = poppedArgs.rbegin(); rit != poppedArgs.rend(); ++rit) {
			nativeArgs.push_back(*rit);
		}
		if (info.outputCount == 1) {
			llvm::Value* result = builder->CreateCall(nativeIt->second, nativeArgs, "inline_call");
			if (info.outputType == NativeParamType::F64) {
				generateInlinePushFloatValue(ctx, result);
			} else {
				generateInlinePushIntValue(ctx, result);
			}
		} else {
			builder->CreateCall(nativeIt->second, nativeArgs);
		}
		// The native version reports nothing; a failure goes through the error state
		return builder->getInt32(0);
	}

	void LlvmGenerator::Impl::generateUserCall(
			AstNodeIdentifier* ident, const std::string& callee, bool resolvedAsMethod, llvm::Value* ctx) {
		// Callee without a native version: the stack-based call below. For an integer-only
		// caller that shouldn't happen.
		if (useCompileTimeStack && generateCompileTimeNativeCall(callee, ctx)) {
			return;
		}

		// Generate any needed type casts before the function call
		generateCastInstructions(ident->parameterCasts(), callee, resolvedAsMethod, ctx);

		// Each call to a fallible function starts with a clean error state
		auto fallibleIt = fallibleFunctions.find(callee);
		const bool fallible = fallibleIt != fallibleFunctions.end() && fallibleIt->second;
		if (fallible) {
			generateClearErrorState(ctx);
		}

		if (!generateInlineBitsCall(callee, ctx)) {
			if (resolvedAsMethod) {
				// A method the validator did not resolve keeps its receiver position from its definition
				size_t receiverPosition = ident->methodReceiverPositionFromTop();
				if (!ident->isMethodCall()) {
					auto posIt = methodReceiverPosition.find(callee);
					receiverPosition = posIt != methodReceiverPosition.end() ? posIt->second : 0;
				}
				generateReceiverRoll(receiverPosition, ctx);
			}
			generateDirectCall(callee, userFunctions[callee], ctx);
		}

		// Track return struct type for local variable binding (-> name)
		auto returnTypeIt = functionReturnStructType.find(callee);
		if (returnTypeIt != functionReturnStructType.end()) {
			lastStructConstructed = returnTypeIt->second;
		}

		if (fallible) {
			generateFallibleCallEpilogue(
					ctx, ident->name().c_str(), ident, ident->abortOnError(), ident->propagateOnError());
		}
	}

	void LlvmGenerator::Impl::generateIdentifier(AstNodeIdentifier* ident, llvm::Value* ctx) {
		const std::string& name = ident->name();
		if (generateCapturedPush(name, ctx) || generateLocalPush(name, ctx) || generateIteratorPush(name, ctx) ||
				generateGlobalVarPush(name, ctx) || generateConstantPush(name, ctx)) {
			return;
		}
		if (const StructLayout* structLayout = findStructDefinition(name)) {
			generateCastInstructions(ident->parameterCasts(), structLayout->name, false, ctx);
			generateStructConstruction(structLayout->name, ctx);
			return;
		}
		if (generateFunctionAliasPush(name, ctx)) {
			return;
		}
		bool resolvedAsMethod = false;
		const std::string callee = resolveCallee(ident, resolvedAsMethod);
		if (!callee.empty()) {
			generateUserCall(ident, callee, resolvedAsMethod, ctx);
		}
		// Anything else names nothing the validator let through
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
		const std::string funcName = "__anon_" + std::to_string(anonymousFunctionCounter++);
		const auto& captures = anonFunc->capturedVariables();

		EnclosingScope enclosing = enterLambdaScope();
		llvm::Function* fn = generateLambdaFunction(anonFunc, funcName, enclosing);
		leaveLambdaScope(std::move(enclosing));

		if (captures.empty()) {
			// No captures: the function pointer itself
			builder->CreateCall(pushPtrFn, {ctx, builder->CreateBitCast(fn, ptrTy)});
			lastGeneratedWasClosure = false;
		} else {
			pushClosure(fn, generateClosureEnvironment(captures), captures.size(), ctx);
			lastGeneratedWasClosure = true;
		}

		// Track the function name for potential aliasing via -> name
		lastGeneratedAnonFuncName = funcName;
	}

	// Sets aside what the enclosing function was being generated with, leaving a clean slate
	// for a lambda's body
	LlvmGenerator::Impl::EnclosingScope LlvmGenerator::Impl::enterLambdaScope() {
		EnclosingScope scope;
		scope.localVariables = std::exchange(localVariables, {});
		scope.localVariableStructTypes = std::exchange(localVariableStructTypes, {});
		scope.localArrayVariables = std::exchange(localArrayVariables, {});
		scope.capturedVariableRefs = std::exchange(capturedVariableRefs, {});
		scope.heapAllocatedCaptures = std::exchange(heapAllocatedCaptures, {});
		scope.indirectLocalVariables = std::exchange(indirectLocalVariables, {});
		scope.closureVariables = std::exchange(closureVariables, {});
		scope.lastStructConstructed = std::exchange(lastStructConstructed, {});
		scope.lastFieldAccessResultType = std::exchange(lastFieldAccessResultType, {});
		scope.returnBlock = currentFunctionReturnBlock;
		scope.isFallible = currentFunctionIsFallible;
		scope.insertBlock = builder->GetInsertBlock();
		scope.insertPoint = builder->GetInsertPoint();
		return scope;
	}

	void LlvmGenerator::Impl::leaveLambdaScope(EnclosingScope&& scope) {
		localVariables = std::move(scope.localVariables);
		localVariableStructTypes = std::move(scope.localVariableStructTypes);
		localArrayVariables = std::move(scope.localArrayVariables);
		capturedVariableRefs = std::move(scope.capturedVariableRefs);
		heapAllocatedCaptures = std::move(scope.heapAllocatedCaptures);
		indirectLocalVariables = std::move(scope.indirectLocalVariables);
		closureVariables = std::move(scope.closureVariables);
		lastStructConstructed = std::move(scope.lastStructConstructed);
		lastFieldAccessResultType = std::move(scope.lastFieldAccessResultType);
		currentFunctionReturnBlock = scope.returnBlock;
		currentFunctionIsFallible = scope.isFallible;
		builder->SetInsertPoint(scope.insertBlock, scope.insertPoint);
	}

	// The lambda's own function. A closure takes (context, environment), the environment an
	// array of pointers to the captured variables' slots; anything else takes (context).
	llvm::Function* LlvmGenerator::Impl::generateLambdaFunction(
			AstNodeAnonymousFunction* anonFunc, const std::string& funcName, const EnclosingScope& enclosing) {
		const auto& captures = anonFunc->capturedVariables();
		std::vector<llvm::Type*> params = {contextPtrTy};
		if (!captures.empty()) {
			params.push_back(ptrTy);
		}
		auto fn = llvm::Function::Create(llvm::FunctionType::get(execResultTy, params, false),
				llvm::Function::InternalLinkage, "usr_" + mainModuleName + "_" + funcName, *module);
		userFunctions[funcName] = fn;

		auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
		auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);
		builder->SetInsertPoint(entryBB);

		llvm::Value* anonCtx = fn->getArg(0);
		anonCtx->setName("ctx");

		if (!captures.empty()) {
			llvm::Value* envPtr = fn->getArg(1);
			envPtr->setName("env");
			llvm::IRBuilder<> entryBuilder(entryBB, entryBB->begin());
			for (size_t i = 0; i < captures.size(); i++) {
				const std::string& capName = captures[i];
				// A captured variable is read through a pointer to its slot, held here
				llvm::AllocaInst* ref = entryBuilder.CreateAlloca(ptrTy, nullptr, capName + "_ref");
				llvm::Value* envSlot = builder->CreateGEP(ptrTy, envPtr, builder->getInt64(i), capName + "_env_slot");
				builder->CreateStore(builder->CreateLoad(ptrTy, envSlot, capName + "_outer_ptr"), ref);
				capturedVariableRefs[capName] = ref;
				// The struct type it has outside, for a following `<<field` to resolve the field
				auto structType = enclosing.localVariableStructTypes.find(capName);
				if (structType != enclosing.localVariableStructTypes.end()) {
					localVariableStructTypes[capName] = structType->second;
				}
			}
		}

		currentFunctionReturnBlock = returnBB;
		// A lambda marked `!` is fallible, exactly as a named function marked `!` is: `panic` in
		// its body returns from it, and a call site handles or propagates the error.
		currentFunctionIsFallible = anonFunc->throws();

		// The lambda's own variables that lambdas inside it capture
		collectAllCapturesFromAST(anonFunc->body(), heapAllocatedCaptures);
		bindLambdaParameters(anonFunc, anonCtx);
		if (anonFunc->body()) {
			generateNode(anonFunc->body(), anonCtx);
		}
		generateLocalCleanup();

		llvm::BasicBlock* currentBlock = builder->GetInsertBlock();
		if (currentBlock && !isTerminated(currentBlock)) {
			builder->CreateBr(returnBB);
		}
		builder->SetInsertPoint(returnBB);
		builder->CreateRet(builder->getInt32(0));
		return fn;
	}

	// Named parameters become locals, popped last first. `stack fn` leaves them on the stack
	// instead, which is the rule a named function uses.
	void LlvmGenerator::Impl::bindLambdaParameters(AstNodeAnonymousFunction* anonFunc, llvm::Value* anonCtx) {
		if (anonFunc->isStack()) {
			return;
		}
		const auto& inputs = anonFunc->inputParameters();
		for (size_t idx = inputs.size(); idx > 0; idx--) {
			const auto* param = static_cast<const AstNodeParameter*>(inputs[idx - 1].get());
			if (param->hasName()) {
				bindParameter(param->name(), anonCtx);
			}
		}
	}

	// The environment a closure carries: for each capture, a pointer to the variable's slot,
	// whose block the closure holds a reference to. Releasing the closure gives them back.
	llvm::Value* LlvmGenerator::Impl::generateClosureEnvironment(const std::vector<std::string>& captures) {
		llvm::Value* env =
				builder->CreateCall(mallocFn, {builder->getInt64(captures.size() * sizeof(void*))}, "env_alloc");
		for (size_t i = 0; i < captures.size(); i++) {
			const std::string& capName = captures[i];
			llvm::Value* slot = captureReference(capName);
			if (!slot) {
				std::cerr << "quadc: error: internal: captured variable '" << capName << "' has no storage here"
						  << std::endl;
				compilationFailed = true;
				slot = llvm::ConstantPointerNull::get(ptrTy);
			}
			builder->CreateStore(slot, builder->CreateGEP(ptrTy, env, builder->getInt64(i), capName + "_slot"));
		}
		return env;
	}

	// A pointer to the slot of the variable `name` as this function sees it, with a reference
	// taken for the closure being built: null when there is no such variable
	llvm::Value* LlvmGenerator::Impl::captureReference(const std::string& name) {
		if (auto it = localVariables.find(name); it != localVariables.end()) {
			// A captured variable always lives in a block; a stack slot has no refcount for the
			// closure's release to give back
			if (!indirectLocalVariables.count(name)) {
				return nullptr;
			}
			llvm::Value* slot = builder->CreateLoad(ptrTy, it->second, name + "_heap_ptr");
			retainCaptureBlock(slot, name);
			return slot;
		}

		// A variable this lambda itself captured, which a lambda nested in it captures in turn:
		// the same slot, through the reference this one holds
		if (auto it = capturedVariableRefs.find(name); it != capturedVariableRefs.end()) {
			llvm::Value* slot = builder->CreateLoad(ptrTy, it->second, name + "_outer_ptr");
			retainCaptureBlock(slot, name);
			return slot;
		}

		// A captured `for` iterator has no alloca to point at: it is an SSA value, replaced
		// each iteration, so there is no address whose contents could be shared. It is
		// captured by value instead -- this closure gets a block of its own holding the
		// iterator as it stands now. That is the per-iteration meaning the documentation
		// already promised, and it is indistinguishable from capturing the iteration's
		// binding by reference, because assigning to the name inside the body creates a
		// shadowing local rather than moving the loop on. The block carries a refcount of
		// one, so the closure's own release frees it like any other capture.
		auto iterIt = iteratorVars.find(name);
		if (iterIt == iteratorVars.end()) {
			return nullptr;
		}
		llvm::Value* block = builder->CreateCall(mallocFn, {builder->getInt64(32)}, name + "_iter_block");
		builder->CreateStore(builder->getInt64(1), block);
		llvm::Value* elem = builder->CreateGEP(builder->getInt8Ty(), block, builder->getInt64(8), name + "_iter_elem");

		// A loop whose bounds came off the runtime stack carries both halves and picks between
		// them at run time; mirror that here so a float iterator captures as a float.
		llvm::Value* bits = iterIt->second;
		llvm::Value* tag = builder->getInt32(0);
		auto floatIt = iteratorFloatVars.find(name);
		if (floatIt != iteratorFloatVars.end()) {
			llvm::Value* asBits = builder->CreateBitCast(floatIt->second.value, int64Ty, name + "_iter_f_bits");
			bits = builder->CreateSelect(floatIt->second.isFloat, asBits, iterIt->second, name + "_iter_bits");
			tag = builder->CreateSelect(
					floatIt->second.isFloat, builder->getInt32(1), builder->getInt32(0), name + "_iter_tag");
		}
		builder->CreateStore(bits, builder->CreateStructGEP(stackElementTy, elem, 0, name + "_iter_value"));
		builder->CreateStore(tag, builder->CreateStructGEP(stackElementTy, elem, 1, name + "_iter_type"));
		return elem;
	}

	// A captured variable's block is { refcount, slot }, the refcount the 8 bytes before the slot
	void LlvmGenerator::Impl::retainCaptureBlock(llvm::Value* slot, const std::string& name) {
		llvm::Value* refCountPtr = builder->CreateGEP(
				builder->getInt8Ty(), slot, builder->getInt64(static_cast<uint64_t>(-8)), name + "_refcount_ptr");
		llvm::Value* refCount = builder->CreateLoad(int64Ty, refCountPtr, "refcount");
		builder->CreateStore(builder->CreateAdd(refCount, builder->getInt64(1), "new_refcount"), refCountPtr);
	}

	// A closure: { magic, function, environment, capture count }, registered so that `call`
	// and the releases can tell it from a bare function pointer
	void LlvmGenerator::Impl::pushClosure(llvm::Function* fn, llvm::Value* env, size_t captureCount, llvm::Value* ctx) {
		llvm::Value* closure = builder->CreateCall(mallocFn, {builder->getInt64(32)}, "closure_alloc");
		// 0xCL05UR3E, "closure" in leet
		builder->CreateStore(
				builder->getInt64(0xC105023E), builder->CreateStructGEP(closureStructTy, closure, 0, "magic_slot"));
		builder->CreateStore(builder->CreateBitCast(fn, ptrTy, "fn_ptr_cast"),
				builder->CreateStructGEP(closureStructTy, closure, 1, "fn_ptr_slot"));
		builder->CreateStore(env, builder->CreateStructGEP(closureStructTy, closure, 2, "env_ptr_slot"));
		builder->CreateStore(builder->getInt64(captureCount),
				builder->CreateStructGEP(closureStructTy, closure, 3, "cap_count_slot"));
		builder->CreateCall(module->getOrInsertFunction("qd_closure_register",
									llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false)),
				{closure});
		builder->CreateCall(pushPtrFn, {ctx, closure});
	}

	void LlvmGenerator::Impl::generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx) {
		const std::string fullName = scopedIdent->scope() + "::" + scopedIdent->name();
		if (scopedIdent->isMethodCall() && generateScopedMethodCall(scopedIdent, ctx)) {
			return;
		}
		if (generateConstantPush(fullName, ctx)) {
			return;
		}
		if (const StructLayout* structLayout = findStructDefinition(fullName)) {
			generateCastInstructions(scopedIdent->parameterCasts(), structLayout->name, false, ctx);
			generateStructConstruction(structLayout->name, ctx);
			return;
		}
		generateModuleFunctionCall(scopedIdent, fullName, ctx);
	}

	// `Type::method`, or `module::method` on a struct of the module, as the validator resolved
	// it: false when no such function was generated
	bool LlvmGenerator::Impl::generateScopedMethodCall(AstNodeScopedIdentifier* scopedIdent, llvm::Value* ctx) {
		const std::string& name = scopedIdent->name();
		const std::string callee =
				(scopedIdent->receiverType().empty() ? scopedIdent->scope() : scopedIdent->receiverType()) +
				"::" + name;
		auto it = userFunctions.find(callee);
		if (it == userFunctions.end()) {
			return false;
		}

		// The receiver is under its arguments unless it was written on top, `Type::method`
		const size_t receiverPosition = scopedIdent->methodReceiverPositionFromTop();
		generateCastInstructions(scopedIdent->parameterCasts(), callee, receiverPosition > 0, ctx);

		auto fallibleIt = fallibleFunctions.find(callee);
		const bool fallible = fallibleIt != fallibleFunctions.end() && fallibleIt->second;
		if (fallible) {
			generateClearErrorState(ctx);
		}
		generateReceiverRoll(receiverPosition, ctx);
		builder->CreateCall(it->second, {ctx});

		// Track return struct type for local variable binding (-> name)
		auto returnTypeIt = functionReturnStructType.find(callee);
		if (returnTypeIt != functionReturnStructType.end()) {
			lastStructConstructed = returnTypeIt->second;
		}

		if (fallible) {
			generateFallibleCallEpilogue(
					ctx, name.c_str(), scopedIdent, scopedIdent->abortOnError(), scopedIdent->propagateOnError());
		}
		return true;
	}

	// `module::function`: one generated here, one in a library, or an imported C function
	void LlvmGenerator::Impl::generateModuleFunctionCall(
			AstNodeScopedIdentifier* scopedIdent, const std::string& fullName, llvm::Value* ctx) {
		if (useCompileTimeStack && generateCompileTimeNativeCall(fullName, ctx)) {
			return;
		}

		const std::string mangledName = "usr_" + scopedIdent->scope() + "_" + scopedIdent->name();
		llvm::Function* fn = module->getFunction(mangledName);
		if (!fn) {
			auto linkage = exportMode ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
			fn = llvm::Function::Create(
					llvm::FunctionType::get(execResultTy, {contextPtrTy}, false), linkage, mangledName, *module);
		}

		generateCastInstructions(scopedIdent->parameterCasts(), fullName, false, ctx);

		auto fallibleIt = fallibleFunctions.find(fullName);
		const bool fallible = fallibleIt != fallibleFunctions.end() && fallibleIt->second;
		if (fallible) {
			generateClearErrorState(ctx);
		}
		llvm::Value* callResult = generateDirectCall(fullName, fn, ctx);

		// Track return struct type for local variable binding (-> name)
		auto returnTypeIt = functionReturnStructType.find(fullName);
		if (returnTypeIt != functionReturnStructType.end()) {
			lastStructConstructed = returnTypeIt->second;
		}

		// In test mode, a failed testing:: assertion fails the test
		if (testErrorAlloca && scopedIdent->scope() == "testing") {
			auto hasError = builder->CreateICmpNE(callResult, builder->getInt32(0), "has_err");
			auto currentError = builder->CreateLoad(int32Ty, testErrorAlloca, "cur_err");
			auto newError = builder->CreateSelect(hasError, callResult, currentError, "new_err");
			builder->CreateStore(newError, testErrorAlloca);
		}

		if (fallible) {
			generateFallibleCallEpilogue(ctx, scopedIdent->name().c_str(), scopedIdent, scopedIdent->abortOnError(),
					scopedIdent->propagateOnError(), importedCFunctions.count(fullName) > 0);
		}
	}

	void LlvmGenerator::Impl::generateSwitchStatement(AstNodeSwitchStatement* switchStmt, llvm::Value* ctx) {
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		SwitchScrutinee scrutinee;
		if (!takeSwitchScrutinee(scrutinee, ctx)) {
			return;
		}
		const bool fallibleStatus = switchReadsFallibleStatus(switchStmt);

		std::vector<AstNodeCase*> labelled;
		AstNodeCase* wildcard = nullptr;
		for (auto* caseNode : switchStmt->cases()) {
			if (!caseNode->isDefault()) {
				labelled.push_back(caseNode);
			} else if (!wildcard) {
				wildcard = caseNode;
			}
		}

		// Without a `_` arm, nothing matching runs an empty one: the value still has to go
		llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(*context, "switch.default", currentFn);
		llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "switch.merge", currentFn);

		// On the native path each arm starts from the same compile-time stack and may leave a
		// different set of SSA values on it; the arms are reconciled at the merge block
		const std::vector<llvm::Value*> savedStack = compileTimeStack;
		std::vector<ArmExit> armExits;

		// The value stays on the stack while the labels are tested and goes as an arm is
		// entered, so an arm that returns or breaks has already released it
		auto generateArm = [&](llvm::BasicBlock* armBB, IAstNode* body) {
			builder->SetInsertPoint(armBB);
			if (!scrutinee.native) {
				builder->CreateCall(module->getOrInsertFunction(
											"qd_drop", llvm::FunctionType::get(execResultTy, {contextPtrTy}, false)),
						{ctx});
			}
			compileTimeStack = savedStack;
			if (body) {
				generateNode(body, ctx);
			}
			llvm::BasicBlock* exitBlock = builder->GetInsertBlock();
			if (exitBlock && !isTerminated(exitBlock)) {
				armExits.emplace_back(exitBlock, compileTimeStack);
				builder->CreateBr(mergeBB);
			}
		};

		for (size_t i = 0; i < labelled.size(); i++) {
			IAstNode* label = labelled[i]->value();

			// The case's own line is its closing brace, so step on the label's
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && label && label->line() > 0) {
				builder->SetCurrentDebugLocation(llvm::DILocation::get(*context, static_cast<unsigned>(label->line()),
						static_cast<unsigned>(label->column()), debugScopeStack.back()));
			}

			llvm::Value* matches = generateCaseMatch(label, scrutinee, fallibleStatus, ctx);
			if (!matches) {
				// A label that resolves to nothing would leave this block without a branch
				std::cerr << "quadc: error: Unsupported or unresolved switch case label";
				if (label && label->line() > 0) {
					std::cerr << " at line " << label->line();
				}
				std::cerr << " (case labels must be a literal, a constant, or an enum variant)" << std::endl;
				compilationFailed = true;
				matches = builder->getFalse();
			}

			llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(*context, "switch.case", currentFn);
			llvm::BasicBlock* nextBB =
					i + 1 < labelled.size() ? llvm::BasicBlock::Create(*context, "switch.check", currentFn) : defaultBB;
			builder->CreateCondBr(matches, caseBB, nextBB);
			generateArm(caseBB, labelled[i]->body());
			builder->SetInsertPoint(nextBB);
		}

		if (labelled.empty()) {
			builder->CreateBr(defaultBB);
		}
		generateArm(defaultBB, wildcard ? wildcard->body() : nullptr);

		builder->SetInsertPoint(mergeBB);
		if (scrutinee.native) {
			mergeArmStacks(armExits, savedStack, "switch", switchStmt->line());
		}
	}

	bool LlvmGenerator::Impl::takeSwitchScrutinee(SwitchScrutinee& scrutinee, llvm::Value* ctx) {
		// On the native path (integer-only functions) operands live in compileTimeStack as
		// SSA values and are never materialised to the runtime stack, so the scrutinee has
		// to come from there instead
		if (useCompileTimeStack) {
			if (compileTimeStack.empty()) {
				std::cerr << "quadc: error: switch has no value to match on" << std::endl;
				compilationFailed = true;
				return false;
			}
			scrutinee.native = compileTimeStack.back();
			compileTimeStack.pop_back();
			return true;
		}

		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "switch.underflow", currentFn);
		llvm::BasicBlock* peekBB = llvm::BasicBlock::Create(*context, "switch.peek", currentFn);

		auto sa = getStackAccess(ctx);
		builder->CreateCondBr(builder->CreateICmpEQ(sa.size, builder->getInt64(0), "is_empty"), underflowBB, peekBB);
		builder->SetInsertPoint(underflowBB);
		emitFatalError(ctx, "Fatal error in switch: Stack underflow (requires 1 value to match)\n");

		builder->SetInsertPoint(peekBB);
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		scrutinee.slot = builder->CreateGEP(stackElementTy, sa.data, topIdx, "switch_slot");
		return true;
	}

	// Is this switch reading the status left by a *user-defined* fallible call? If so, `Ok`
	// must mean "no error" rather than "the value equals 1". Error codes are user-chosen and
	// `Ok` is 1, so comparing by value makes a `panic` with code 1 indistinguishable from
	// success.
	//
	// Deliberately excludes imported C functions: they are inconsistent about setting
	// ctx->error_code (io::open sets it, net::connect only pushes the code), so testing the
	// error state there would report success on a genuine failure. They keep the by-value
	// comparison, which is sound because their codes start at 2 by convention.
	bool LlvmGenerator::Impl::switchReadsFallibleStatus(AstNodeSwitchStatement* switchStmt) {
		IAstNode* parent = switchStmt->parent();
		if (!parent) {
			return false;
		}
		for (size_t i = 1; i < parent->childCount(); i++) {
			if (parent->child(i) != switchStmt) {
				continue;
			}
			IAstNode* prev = parent->child(i - 1);
			std::string callee;
			if (prev && prev->type() == IAstNode::Type::IDENTIFIER) {
				callee = static_cast<AstNodeIdentifier*>(prev)->name();
			} else if (prev && prev->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
				auto* sc = static_cast<AstNodeScopedIdentifier*>(prev);
				callee = sc->scope() + "::" + sc->name();
			} else {
				return false;
			}
			if (importedCFunctions.count(callee)) {
				return false;
			}
			auto it = fallibleFunctions.find(callee);
			return it != fallibleFunctions.end() && it->second;
		}
		return false;
	}

	// Whether the scrutinee matches one case label, or null when the label means nothing
	// here: it names nothing, or asks for a float, string or d128 on the native path,
	// which is integer-only
	llvm::Value* LlvmGenerator::Impl::generateCaseMatch(
			IAstNode* label, const SwitchScrutinee& scrutinee, bool fallibleStatus, llvm::Value* ctx) {
		if (!label) {
			return nullptr;
		}
		if (label->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			auto* scoped = static_cast<AstNodeScopedIdentifier*>(label);
			return generateNamedConstantMatch(scoped->scope() + "::" + scoped->name(), scrutinee);
		}
		if (label->type() == IAstNode::Type::IDENTIFIER) {
			return generateNamedConstantMatch(static_cast<AstNodeIdentifier*>(label)->name(), scrutinee);
		}
		if (label->type() != IAstNode::Type::LITERAL) {
			return nullptr;
		}

		auto* lit = static_cast<AstNodeLiteral*>(label);
		const std::string& text = lit->value();
		switch (lit->literalType()) {
		case AstNodeLiteral::LiteralType::BOOL: {
			const bool truthy = text == "true" || text == "Ok";
			if (fallibleStatus && truthy) {
				// `Ok` on a fallible status means "the call succeeded". Test the error state
				// directly so that a panic carrying code 1 is not mistaken for success --
				// and via has_error, so that a panic carrying code 0 is not either.
				return builder->CreateNot(generateReadErrorState(ctx, "sw_has_error").failed, "case_ok");
			}
			return generateIntCaseMatch(truthy ? 1 : 0, scrutinee);
		}
		case AstNodeLiteral::LiteralType::INTEGER: {
			int64_t value = 0;
			if (!parseIntegerLiteral(text, value)) {
				std::cerr << "quadc: error: Invalid integer case label '" << text
						  << "' (out of range or invalid format)" << std::endl;
				compilationFailed = true;
			}
			return generateIntCaseMatch(value, scrutinee);
		}
		case AstNodeLiteral::LiteralType::FLOAT:
			return generateFloatCaseMatch(std::stod(text), scrutinee);
		case AstNodeLiteral::LiteralType::D128:
			return generateD128CaseMatch(text, scrutinee);
		case AstNodeLiteral::LiteralType::STRING:
			return generateStringCaseMatch(text, scrutinee);
		default:
			return nullptr;
		}
	}

	// A named constant or enum variant, compared as the literal it stands for
	llvm::Value* LlvmGenerator::Impl::generateNamedConstantMatch(
			const std::string& name, const SwitchScrutinee& scrutinee) {
		auto constIt = moduleConstants.find(name);
		if (constIt == moduleConstants.end()) {
			return nullptr;
		}
		const std::string& value = constIt->second;
		if (!value.empty() && value[0] == '"') {
			return generateStringCaseMatch(value, scrutinee);
		}
		if (isD128LiteralText(value)) {
			return generateD128CaseMatch(value, scrutinee);
		}
		if (isFloatLiteralText(value)) {
			return generateFloatCaseMatch(floatLiteralOr(value, 0.0), scrutinee);
		}
		int64_t parsed = 0;
		if (!parseIntegerLiteral(value, parsed)) {
			std::cerr << "quadc: error: Invalid integer constant value '" << value << "' for " << name << std::endl;
			compilationFailed = true;
		}
		return generateIntCaseMatch(parsed, scrutinee);
	}

	llvm::Value* LlvmGenerator::Impl::generateIntCaseMatch(int64_t label, const SwitchScrutinee& scrutinee) {
		llvm::Value* value = scrutinee.native;
		if (!value) {
			auto* valuePtr = builder->CreateStructGEP(stackElementTy, scrutinee.slot, 0, "value_ptr");
			value = builder->CreateLoad(int64Ty, valuePtr, "switch_val");
		}
		return builder->CreateICmpEQ(value, builder->getInt64(static_cast<uint64_t>(label)), "case_match");
	}

	llvm::Value* LlvmGenerator::Impl::generateFloatCaseMatch(double label, const SwitchScrutinee& scrutinee) {
		if (!scrutinee.slot) {
			return nullptr;
		}
		auto* valuePtr = builder->CreateStructGEP(stackElementTy, scrutinee.slot, 0, "value_ptr");
		auto* value = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "switch_val_f");
		return builder->CreateFCmpOEQ(value, llvm::ConstantFP::get(builder->getDoubleTy(), label), "case_match");
	}

	// The runtime compares, against a literal it reads once and caches, as a pushed d128
	// literal is
	llvm::Value* LlvmGenerator::Impl::generateD128CaseMatch(const std::string& text, const SwitchScrutinee& scrutinee) {
		uint64_t low = 0;
		uint64_t high = 0;
		if (!scrutinee.slot || !d128LiteralWords(text, low, high)) {
			return nullptr;
		}
		auto matchFn = module->getOrInsertFunction(
				"qd_d128_matches_bits", llvm::FunctionType::get(int32Ty, {ptrTy, int64Ty, int64Ty}, false));
		auto* found = builder->CreateCall(
				matchFn, {scrutinee.slot, builder->getInt64(low), builder->getInt64(high)}, "d128_case");
		return builder->CreateICmpNE(found, builder->getInt32(0), "case_match");
	}

	// Equal bytes up to the label's end and equal lengths: a string holding a NUL is not
	// equal to the part of it before the NUL
	llvm::Value* LlvmGenerator::Impl::generateStringCaseMatch(
			const std::string& literal, const SwitchScrutinee& scrutinee) {
		if (!scrutinee.slot) {
			return nullptr;
		}
		const std::string label = decodeStringLiteral(literal);
		auto* valuePtr = builder->CreateStructGEP(stackElementTy, scrutinee.slot, 0, "value_ptr");
		auto* str = builder->CreateLoad(ptrTy, valuePtr, "switch_str");

		auto strcmpFn = module->getOrInsertFunction("strcmp", llvm::FunctionType::get(int32Ty, {ptrTy, ptrTy}, false));
		auto lengthFn =
				module->getOrInsertFunction("qd_string_length", llvm::FunctionType::get(int64Ty, {ptrTy}, false));
		auto* data = builder->CreateCall(qdStringDataFn, {str}, "switch_str_data");
		auto* order = builder->CreateCall(strcmpFn, {data, builder->CreateGlobalString(label)}, "strcmp_result");
		auto* length = builder->CreateCall(lengthFn, {str}, "switch_str_len");
		return builder->CreateAnd(builder->CreateICmpEQ(order, builder->getInt32(0)),
				builder->CreateICmpEQ(length, builder->getInt64(strlen(label.c_str()))), "case_match");
	}

	// generateLocal, generateLocalOne, generateLocalCleanup, collectAllCapturesFromAST are in generator_nodes_locals.cc

	// The casts a call implies on its arguments, each converted where it lies: casts[i] is
	// for the argument i deep from the bottom of the call's arguments. In place, because
	// swapping an argument to the top and back only works for the one under it -- two swaps
	// put the stack back as it was. An `any` argument is checked against its parameter, which
	// the message numbers as `callee` does, after the receiver when `receiverFirst`.
	void LlvmGenerator::Impl::generateCastInstructions(
			const std::vector<CastDirection>& casts, const std::string& callee, bool receiverFirst, llvm::Value* ctx) {
		for (size_t i = 0; i < casts.size(); i++) {
			uint64_t type = 0;
			bool check = false;
			switch (casts[i]) {
			case CastDirection::NONE:
				continue;
			case CastDirection::INT_TO_FLOAT:
				type = 1; // QD_STACK_TYPE_FLOAT
				break;
			case CastDirection::FLOAT_TO_INT:
				type = 0; // QD_STACK_TYPE_INT
				break;
			case CastDirection::INT_TO_D128:
				type = kStackTypeD128;
				break;
			case CastDirection::ANY_TO_INT:
				type = 0; // QD_STACK_TYPE_INT
				check = true;
				break;
			case CastDirection::ANY_TO_FLOAT:
				type = 1; // QD_STACK_TYPE_FLOAT
				check = true;
				break;
			case CastDirection::ANY_TO_D128:
				type = kStackTypeD128;
				check = true;
				break;
			case CastDirection::ANY_TO_STR:
				type = 3; // QD_STACK_TYPE_STR
				check = true;
				break;
			case CastDirection::ANY_TO_PTR:
				type = 2; // QD_STACK_TYPE_PTR
				check = true;
				break;
			}
			const uint64_t depth = casts.size() - 1 - i;
			if (check) {
				llvm::Function* checkFn = module->getFunction("qd_check_param");
				if (!checkFn) {
					checkFn = llvm::Function::Create(llvm::FunctionType::get(execResultTy,
															 {contextPtrTy, int64Ty, int64Ty, int64Ty, ptrTy}, false),
							llvm::Function::ExternalLinkage, "qd_check_param", *module);
				}
				const uint64_t param = receiverFirst ? i : i + 1;
				builder->CreateCall(checkFn, {ctx, builder->getInt64(depth), builder->getInt64(type),
													 builder->getInt64(param), getOrCreateGlobalString(callee)});
				continue;
			}
			llvm::Function* castFn = module->getFunction("qd_cast_param");
			if (!castFn) {
				castFn = llvm::Function::Create(
						llvm::FunctionType::get(execResultTy, {contextPtrTy, int64Ty, int64Ty}, false),
						llvm::Function::ExternalLinkage, "qd_cast_param", *module);
			}
			builder->CreateCall(castFn, {ctx, builder->getInt64(depth), builder->getInt64(type)});
		}
	}

} // namespace Qd
