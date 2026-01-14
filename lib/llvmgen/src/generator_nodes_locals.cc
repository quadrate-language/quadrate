// Local variable generation for LLVM code generator
// Extracted from generator_nodes.cc for maintainability

#include "generator_impl.h"

namespace Qd {

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
			llvm::Value* outerVarPtr = builder->CreateLoad(ptrTy, ptrAlloca, name + "_cap_store_ptr");

			// Get the stack pointer from context
			llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
			llvm::Value* stackPtr = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");

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
				localAlloca = tmpBuilder.CreateAlloca(ptrTy, nullptr, name + "_ptr");

				// Allocate heap memory in ENTRY BLOCK (important for loops - only allocate once)
				// 8 bytes refcount + 24 bytes qd_stack_element_t = 32 bytes
				llvm::Value* heapBlock =
						tmpBuilder.CreateCall(mallocFn, {tmpBuilder.getInt64(32)}, name + "_heap_block");

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

				// Determine the debug type based on type hints from function parameters
				// This allows the debugger to show the logical value (e.g., 42) instead of
				// the full qd_stack_element_t struct
				llvm::DIType* debugType = stackElementDebugType;
				auto typeHintIt = localVariableTypeHints.find(name);
				if (typeHintIt != localVariableTypeHints.end()) {
					const std::string& typeStr = typeHintIt->second;
					if ((typeStr == "i64" || typeStr == "int" || typeStr == "int64" || typeStr == "i") &&
							int64DebugType) {
						debugType = int64DebugType;
					} else if ((typeStr == "f64" || typeStr == "float" || typeStr == "f") && floatDebugType) {
						debugType = floatDebugType;
					} else if ((typeStr == "str" || typeStr == "string") && stringDebugType) {
						debugType = stringDebugType;
					}
					// For struct types, keep using stackElementDebugType
				} else if (currentFunctionIsIntegerOnly && int64DebugType) {
					// For integer-only functions, default untyped locals to int64
					debugType = int64DebugType;
				} else if (lastPushedType != LastPushedType::UNKNOWN) {
					// Infer type from the last pushed value (e.g., "42 -> x" means x is int)
					// Note: Strings are kept as structs because qd_string_t is a ref-counted struct,
					// not a plain char*, so displaying as char* shows garbage
					switch (lastPushedType) {
					case LastPushedType::INTEGER:
						if (int64DebugType) {
							debugType = int64DebugType;
						}
						break;
					case LastPushedType::FLOAT:
						if (floatDebugType) {
							debugType = floatDebugType;
						}
						break;
					// STRING intentionally not handled - keep as struct for accurate display
					default:
						break;
					}
				}

				// Create local variable debug info
				// The value union is at offset 0 of qd_stack_element_t, so for primitive types
				// we can use the same location but with the primitive type for cleaner display
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (current function)
						name,															 // Variable name
						localFile,														 // File
						static_cast<unsigned>(lineNum),									 // Line number
						debugType,														 // Type
						true															 // Always preserve
				);

				// Create expression based on whether variable is captured
				// For captured variables, the alloca holds a pointer to heap memory
				// We need DW_OP_deref to follow the pointer to the actual value
				llvm::DIExpression* expr;
				if (isCaptured) {
					// Captured variable: alloca -> heap pointer -> qd_stack_element_t -> value
					// Use DW_OP_deref to dereference the pointer stored in the alloca
					expr = debugBuilder->createExpression({llvm::dwarf::DW_OP_deref});
				} else {
					// Normal variable: alloca directly contains qd_stack_element_t
					expr = debugBuilder->createExpression();
				}

				// Insert declare to make it visible in debugger
				debugBuilder->insertDeclare(localAlloca, // Storage (the alloca)
						localVar,						 // Variable
						expr,							 // Expression
						llvm::DILocation::get(*context, static_cast<unsigned>(lineNum), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}
		} else {
			// Variable already exists, reuse it
			localAlloca = it->second;
		}

		// Reset type tracking after local assignment to avoid type leakage
		lastPushedType = LastPushedType::UNKNOWN;

		// For indirect (captured) variables, load the actual storage pointer
		// localAlloca holds a pointer to heap memory, we need the heap memory address
		llvm::Value* storagePtr = localAlloca;
		bool isIndirect = indirectLocalVariables.find(name) != indirectLocalVariables.end();
		if (isIndirect) {
			storagePtr = builder->CreateLoad(ptrTy, localAlloca, name + "_storage");
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
		llvm::Value* oldType = builder->CreateLoad(int32Ty, oldTypePtr, name + "_old_type");

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
		llvm::Value* oldStrPtr = builder->CreateLoad(ptrTy, oldValuePtrStr, name + "_old_str");

		// Call qd_string_release() on the old string
		builder->CreateCall(qdStringReleaseFn, {oldStrPtr});
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
		llvm::Value* oldPtrVal = builder->CreateLoad(ptrTy, oldValuePtrPtr, name + "_old_ptr");

		// Always check for closure on ptr reassignments - the variable could hold a closure
		// even if not tracked in closureVariables (e.g., first iteration of a loop).
		// Use the closure registry to safely detect closures without reading from freed memory.
		{
			// Check closure registry to see if it's actually a closure (safe for any pointer)
			auto closureIsValidFnTy = llvm::FunctionType::get(int32Ty, {ptrTy}, false);
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
			llvm::Value* envPtr = builder->CreateLoad(ptrTy, envPtrSlot, "old_env_ptr");
			llvm::Value* capCountSlot =
					builder->CreateStructGEP(closureStructTy, oldPtrVal, 3, name + "_old_cap_count_slot");
			llvm::Value* capCount = builder->CreateLoad(int64Ty, capCountSlot, "old_cap_count");

			// Loop through captured variables and decrement refcounts
			llvm::BasicBlock* loopHeader = llvm::BasicBlock::Create(*context, name + "_old_cap_loop_header", currentFn);
			llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(*context, name + "_old_cap_loop_body", currentFn);
			llvm::BasicBlock* afterLoop = llvm::BasicBlock::Create(*context, name + "_old_cap_loop_done", currentFn);

			llvm::AllocaInst* loopIdxAlloca = builder->CreateAlloca(int64Ty, nullptr, name + "_old_cap_idx");
			builder->CreateStore(builder->getInt64(0), loopIdxAlloca);
			builder->CreateBr(loopHeader);

			builder->SetInsertPoint(loopHeader);
			llvm::Value* loopIdx = builder->CreateLoad(int64Ty, loopIdxAlloca, "idx");
			llvm::Value* loopCond = builder->CreateICmpSLT(loopIdx, capCount, "loop_cond");
			builder->CreateCondBr(loopCond, loopBody, afterLoop);

			builder->SetInsertPoint(loopBody);
			llvm::Value* capSlot = builder->CreateGEP(ptrTy, envPtr, loopIdx, name + "_old_cap_slot");
			llvm::Value* capVarPtr = builder->CreateLoad(ptrTy, capSlot, "old_cap_var_ptr");
			llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capVarPtr,
					builder->getInt64(static_cast<uint64_t>(-8)), name + "_old_refcount_ptr");
			llvm::Value* refCount = builder->CreateLoad(int64Ty, refCountPtr, "old_refcount");
			llvm::Value* newRefCount = builder->CreateSub(refCount, builder->getInt64(1), "old_new_refcount");
			builder->CreateStore(newRefCount, refCountPtr);

			llvm::BasicBlock* freeCapBlock = llvm::BasicBlock::Create(*context, name + "_old_free_cap", currentFn);
			llvm::BasicBlock* capContinue = llvm::BasicBlock::Create(*context, name + "_old_cap_continue", currentFn);
			llvm::Value* shouldFree = builder->CreateICmpEQ(newRefCount, builder->getInt64(0), "old_should_free");
			builder->CreateCondBr(shouldFree, freeCapBlock, capContinue);

			builder->SetInsertPoint(freeCapBlock);
			builder->CreateCall(freeFn, {refCountPtr});
			builder->CreateBr(capContinue);

			builder->SetInsertPoint(capContinue);
			llvm::Value* nextIdx = builder->CreateAdd(loopIdx, builder->getInt64(1), "old_next_idx");
			builder->CreateStore(nextIdx, loopIdxAlloca);
			builder->CreateBr(loopHeader);

			builder->SetInsertPoint(afterLoop);
			// Unregister closure from registry before freeing
			auto closureUnregisterFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
			auto closureUnregisterFn = module->getOrInsertFunction("qd_closure_unregister", closureUnregisterFnTy);
			builder->CreateCall(closureUnregisterFn, {oldPtrVal});

			builder->CreateCall(freeFn, {envPtr});
			builder->CreateCall(freeFn, {oldPtrVal});
			builder->CreateBr(afterReleaseBlock);

			// Release as generic pointer (not a closure)
			builder->SetInsertPoint(releaseGenericBlock);
			builder->CreateCall(qdPtrReleaseFn, {oldPtrVal});
			builder->CreateBr(afterReleaseBlock);
		}

		// Continue after release
		builder->SetInsertPoint(afterReleaseBlock);

		// Get the stack pointer from context
		llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
		llvm::Value* stackPtr = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");

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
		emitFatalError(ctx, "Fatal error: Stack underflow when assigning to local variable\n");

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
				llvm::Value* heapPtr = builder->CreateLoad(ptrTy, localAlloca, varName + "_heap_cleanup");

				// Refcount is 8 bytes before the elem pointer
				llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), heapPtr,
						builder->getInt64(static_cast<uint64_t>(-8)), varName + "_refcount_cleanup");
				llvm::Value* refCount = builder->CreateLoad(int64Ty, refCountPtr, "refcount");
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
				llvm::Value* capType = builder->CreateLoad(int32Ty, capTypePtr, "cap_type");

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
				llvm::Value* capStrVal = builder->CreateLoad(ptrTy, heapPtr, "cap_str");
				builder->CreateCall(qdStringReleaseFn, {capStrVal});
				builder->CreateBr(capDoFree);

				// Check if type == 2 (pointer - could be struct, array, or closure)
				builder->SetInsertPoint(capCheckPtr);
				llvm::Value* capIsPtrCond = builder->CreateICmpEQ(capType, builder->getInt32(2), "cap_is_ptr");
				builder->CreateCondBr(capIsPtrCond, capIsPtr, capDoFree);

				// Release pointer (works for structs and arrays)
				builder->SetInsertPoint(capIsPtr);
				llvm::Value* capPtrVal = builder->CreateLoad(ptrTy, heapPtr, "cap_ptr");
				builder->CreateCall(qdPtrReleaseFn, {capPtrVal});
				builder->CreateBr(capDoFree);

				// Free the heap block
				builder->SetInsertPoint(capDoFree);
				// Free starting at refcount (the actual malloc block start)
				builder->CreateCall(freeFn, {refCountPtr});
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
			llvm::Value* type = builder->CreateLoad(int32Ty, typePtr, varName + "_cleanup_type");

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
			llvm::Value* strPtr = builder->CreateLoad(ptrTy, valuePtr, varName + "_cleanup_str");

			// Call qd_string_release() on the string
			builder->CreateCall(qdStringReleaseFn, {strPtr});
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
				llvm::Value* closurePtr = builder->CreateLoad(ptrTy, ptrValuePtr, varName + "_cleanup_closure");

				// Check if this is a valid closure using the registry (safe - doesn't dereference)
				auto closureIsValidFnTy = llvm::FunctionType::get(int32Ty, {ptrTy}, false);
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
				llvm::Value* envPtr = builder->CreateLoad(ptrTy, envPtrSlot, "env_ptr");

				// Load capture count
				llvm::Value* capCountSlot = builder->CreateStructGEP(closureStructTy, closurePtr, 3, "cap_count_slot");
				llvm::Value* capCount = builder->CreateLoad(int64Ty, capCountSlot, "cap_count");

				// Loop through captured variables and decrement refcounts
				// Create loop blocks
				llvm::BasicBlock* loopHeader =
						llvm::BasicBlock::Create(*context, varName + "_cap_loop_header", currentFn);
				llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(*context, varName + "_cap_loop_body", currentFn);
				llvm::BasicBlock* afterLoop = llvm::BasicBlock::Create(*context, varName + "_cap_loop_done", currentFn);

				// Loop counter
				llvm::AllocaInst* loopIdxAlloca = builder->CreateAlloca(int64Ty, nullptr, varName + "_cap_idx");
				builder->CreateStore(builder->getInt64(0), loopIdxAlloca);
				builder->CreateBr(loopHeader);

				// Loop header - check if idx < capCount
				builder->SetInsertPoint(loopHeader);
				llvm::Value* loopIdx = builder->CreateLoad(int64Ty, loopIdxAlloca, "idx");
				llvm::Value* loopCond = builder->CreateICmpSLT(loopIdx, capCount, "loop_cond");
				builder->CreateCondBr(loopCond, loopBody, afterLoop);

				// Loop body - decrement refcount for this capture
				builder->SetInsertPoint(loopBody);

				// Get pointer to captured variable from environment
				llvm::Value* capSlot = builder->CreateGEP(ptrTy, envPtr, loopIdx, varName + "_cap_slot");
				llvm::Value* capVarPtr = builder->CreateLoad(ptrTy, capSlot, "cap_var_ptr");

				// The captured variable pointer points to the qd_stack_element_t at offset 8
				// Refcount is at offset 0 (8 bytes before the elem)
				llvm::Value* refCountPtr = builder->CreateGEP(builder->getInt8Ty(), capVarPtr,
						builder->getInt64(static_cast<uint64_t>(-8)), varName + "_refcount_ptr");
				llvm::Value* refCount = builder->CreateLoad(int64Ty, refCountPtr, "refcount");

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
				llvm::Value* loopCapType = builder->CreateLoad(int32Ty, loopCapTypePtr, "loop_cap_type");

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
				llvm::Value* loopCapStrVal = builder->CreateLoad(ptrTy, capVarPtr, "loop_cap_str");
				builder->CreateCall(qdStringReleaseFn, {loopCapStrVal});
				builder->CreateBr(loopCapDoFree);

				// Check if type == 2 (pointer - could be struct, array, or closure)
				builder->SetInsertPoint(loopCapCheckPtr);
				llvm::Value* loopCapIsPtrCond =
						builder->CreateICmpEQ(loopCapType, builder->getInt32(2), "loop_cap_is_ptr");
				builder->CreateCondBr(loopCapIsPtrCond, loopCapIsPtr, loopCapDoFree);

				// Release pointer (works for structs and arrays)
				builder->SetInsertPoint(loopCapIsPtr);
				llvm::Value* loopCapPtrVal = builder->CreateLoad(ptrTy, capVarPtr, "loop_cap_ptr");
				builder->CreateCall(qdPtrReleaseFn, {loopCapPtrVal});
				builder->CreateBr(loopCapDoFree);

				// Free the heap block
				builder->SetInsertPoint(loopCapDoFree);
				// Free starting at refcount (the actual malloc block)
				builder->CreateCall(freeFn, {refCountPtr});
				builder->CreateBr(capContinue);

				// Continue to next capture
				builder->SetInsertPoint(capContinue);
				llvm::Value* nextIdx = builder->CreateAdd(loopIdx, builder->getInt64(1), "next_idx");
				builder->CreateStore(nextIdx, loopIdxAlloca);
				builder->CreateBr(loopHeader);

				// After loop - free environment and closure struct
				builder->SetInsertPoint(afterLoop);
				// Unregister closure from registry before freeing
				auto closureUnregisterFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				auto closureUnregisterFn = module->getOrInsertFunction("qd_closure_unregister", closureUnregisterFnTy);
				builder->CreateCall(closureUnregisterFn, {closurePtr});

				builder->CreateCall(freeFn, {envPtr});
				builder->CreateCall(freeFn, {closurePtr});
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
				llvm::Value* arrPtr = builder->CreateLoad(ptrTy, ptrValuePtr, varName + "_cleanup_arr");

				// Call qd_array_release() on the array
				llvm::Function* arrayReleaseFn = module->getFunction("qd_array_release");
				if (!arrayReleaseFn) {
					auto arrayReleaseFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
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
				llvm::Value* ptrVal = builder->CreateLoad(ptrTy, ptrValuePtr, varName + "_cleanup_ptr");
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
		for (auto* child : node->children()) {
			collectAllCapturesFromAST(child, captures);
		}
	}

} // namespace Qd
