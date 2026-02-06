#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::emitFatalError(llvm::Value* ctx, const char* message) {
		// Use write(2, msg, len) instead of fprintf(stderr, ...) because the
		// stderr symbol is not a portable global — on Haiku, musl, macOS, etc.
		// it is a macro, not a linker symbol, so loading it would crash.
		auto writeFn =
			module->getOrInsertFunction("write", llvm::FunctionType::get(int64Ty, {int32Ty, ptrTy, int64Ty}, false));
		auto errorMsg = builder->CreateGlobalString(message);
		builder->CreateCall(writeFn, {builder->getInt32(2), errorMsg, builder->getInt64(strlen(message))});
		builder->CreateCall(printStackTraceFn, {ctx});
		builder->CreateCall(exitFn, {builder->getInt32(1)});
		builder->CreateUnreachable();
	}

	LlvmGenerator::Impl::StackAccess LlvmGenerator::Impl::getStackAccess(llvm::Value* ctx) {
		StackAccess sa;
		// Get ctx->st
		llvm::Value* stPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(ptrTy, stPtr, "st");
		// Get st->size
		sa.sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
		sa.size = builder->CreateLoad(int64Ty, sa.sizePtr, "size");
		// Get st->data
		llvm::Value* dataPtr = builder->CreateStructGEP(stackStructTy, st, 0, "data_ptr");
		sa.data = builder->CreateLoad(ptrTy, dataPtr, "data");
		return sa;
	}

	LlvmGenerator::Impl::BinaryOpContext LlvmGenerator::Impl::setupBinaryOp(llvm::Value* ctx) {
		BinaryOpContext boc;
		auto sa = getStackAccess(ctx);
		boc.sizePtr = sa.sizePtr;
		boc.size = sa.size;

		// Load first operand: data[size - 2]
		llvm::Value* idx1 = builder->CreateSub(boc.size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, sa.data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		boc.resultPtr = builder->CreateBitCast(value1Ptr, ptrTy);
		boc.value1 = builder->CreateLoad(int64Ty, boc.resultPtr, "value1");

		// Load second operand: data[size - 1]
		llvm::Value* idx2 = builder->CreateSub(boc.size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, sa.data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2PtrCast = builder->CreateBitCast(value2Ptr, ptrTy);
		boc.value2 = builder->CreateLoad(int64Ty, value2PtrCast, "value2");

		return boc;
	}

	void LlvmGenerator::Impl::finishBinaryOp(const BinaryOpContext& boc, llvm::Value* result) {
		builder->CreateStore(result, boc.resultPtr);
		llvm::Value* newSize = builder->CreateSub(boc.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, boc.sizePtr);
	}

	void LlvmGenerator::Impl::generateInlinePushInt(llvm::Value* ctx, int64_t value) {
		// Delegate to generateInlinePushIntValue with a constant
		generateInlinePushIntValue(ctx, builder->getInt64(static_cast<uint64_t>(value)));
	}

	void LlvmGenerator::Impl::generateInlinePushIntValue(llvm::Value* ctx, llvm::Value* value) {
		// Inline implementation of qd_push_i for runtime integer values
		// Same as generateInlinePushInt but takes llvm::Value* instead of int64_t

		llvm::Value* stPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(ptrTy, stPtr, "st");

		llvm::Value* sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(int64Ty, sizePtr, "size");

		// Skip overflow check for integer-only functions (predictable stack usage)
		if (!currentFunctionIsIntegerOnly) {
			// Check for overflow
			llvm::Value* capacityPtr = builder->CreateStructGEP(stackStructTy, st, 1, "capacity_ptr");
			llvm::Value* capacity = builder->CreateLoad(int64Ty, capacityPtr, "capacity");
			llvm::Value* hasSpace = builder->CreateICmpULT(size, capacity, "has_space");

			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* overflowBB = llvm::BasicBlock::Create(*context, "pushv.overflow", currentFn);
			llvm::BasicBlock* pushBB = llvm::BasicBlock::Create(*context, "pushv.do", currentFn);
			builder->CreateCondBr(hasSpace, pushBB, overflowBB);

			// Generate overflow error
			builder->SetInsertPoint(overflowBB);
			emitFatalError(ctx, "Fatal error: Stack overflow (use -s to increase stack size)\n");

			// Do the push
			builder->SetInsertPoint(pushBB);
		}

		llvm::Value* dataPtr = builder->CreateStructGEP(stackStructTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(ptrTy, dataPtr, "data");

		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, size, "elem_ptr");

		// Store runtime value
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valueiPtr = builder->CreateBitCast(valuePtr, ptrTy);
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
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateNSWAdd(boc.value1, boc.value2, "add_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineIntSub(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateNSWSub(boc.value1, boc.value2, "sub_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineIntMul(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateNSWMul(boc.value1, boc.value2, "mul_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineIntMod(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateSRem(boc.value1, boc.value2, "mod_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineIntCompare(
			llvm::Value* ctx, llvm::CmpInst::Predicate pred, const char* resultName) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* cmpResult = builder->CreateICmp(pred, boc.value1, boc.value2, resultName);
		llvm::Value* result = builder->CreateZExt(cmpResult, int64Ty, "result_i64");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineIntLt(llvm::Value* ctx) {
		generateInlineIntCompare(ctx, llvm::CmpInst::ICMP_SLT, "lt_result");
	}

	void LlvmGenerator::Impl::generateInlineIntGt(llvm::Value* ctx) {
		generateInlineIntCompare(ctx, llvm::CmpInst::ICMP_SGT, "gt_result");
	}

	void LlvmGenerator::Impl::generateInlineIntEq(llvm::Value* ctx) {
		generateInlineIntCompare(ctx, llvm::CmpInst::ICMP_EQ, "eq_result");
	}

	void LlvmGenerator::Impl::generateInlineIntNeq(llvm::Value* ctx) {
		generateInlineIntCompare(ctx, llvm::CmpInst::ICMP_NE, "neq_result");
	}

	void LlvmGenerator::Impl::generateInlineIntLte(llvm::Value* ctx) {
		generateInlineIntCompare(ctx, llvm::CmpInst::ICMP_SLE, "lte_result");
	}

	void LlvmGenerator::Impl::generateInlineIntGte(llvm::Value* ctx) {
		generateInlineIntCompare(ctx, llvm::CmpInst::ICMP_SGE, "gte_result");
	}

	void LlvmGenerator::Impl::generateInlineBitAnd(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateAnd(boc.value1, boc.value2, "and_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineBitOr(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateOr(boc.value1, boc.value2, "or_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineBitXor(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateXor(boc.value1, boc.value2, "xor_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineBitNot(llvm::Value* ctx) {
		// Inline implementation of bitwise NOT: ( a:int -- result:int )
		auto sa = getStackAccess(ctx);
		llvm::Value* idx = builder->CreateSub(sa.size, builder->getInt64(1), "idx");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, sa.data, idx, "elem_ptr");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valuePtrCast = builder->CreateBitCast(valuePtr, ptrTy);
		llvm::Value* value = builder->CreateLoad(int64Ty, valuePtrCast, "value");
		llvm::Value* result = builder->CreateNot(value, "not_result");
		builder->CreateStore(result, valuePtrCast);
	}

	void LlvmGenerator::Impl::generateInlineBitLshift(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateShl(boc.value1, boc.value2, "lshift_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineBitRshift(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = builder->CreateLShr(boc.value1, boc.value2, "rshift_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineDup(llvm::Value* ctx) {
		// Inline dup: duplicate top stack element
		auto sa = getStackAccess(ctx);
		// Get pointer to top element (size - 1)
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "top_elem");
		// Get pointer to new element (size)
		llvm::Value* newElemPtr = builder->CreateGEP(stackElementTy, sa.data, sa.size, "new_elem");
		// Copy entire element
		llvm::Value* topValue = builder->CreateLoad(stackElementTy, topElemPtr, "top_value");
		builder->CreateStore(topValue, newElemPtr);
		// Increment size
		llvm::Value* newSize = builder->CreateAdd(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineSwap(llvm::Value* ctx) {
		// Inline swap: swap top two stack elements
		auto sa = getStackAccess(ctx);
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Check for stack underflow (need at least 2 elements)
		llvm::Value* hasEnough = builder->CreateICmpUGE(sa.size, builder->getInt64(2), "has_enough");
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "swap.underflow", currentFn);
		llvm::BasicBlock* swapBB = llvm::BasicBlock::Create(*context, "swap.do", currentFn);
		builder->CreateCondBr(hasEnough, swapBB, underflowBB);

		// Generate underflow error
		builder->SetInsertPoint(underflowBB);
		emitFatalError(ctx, "Fatal error in swap: Stack underflow (requires 2 elements)\n");

		// Do the swap
		builder->SetInsertPoint(swapBB);
		llvm::Value* idx1 = builder->CreateSub(sa.size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, sa.data, idx1, "elem1_ptr");
		llvm::Value* idx2 = builder->CreateSub(sa.size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, sa.data, idx2, "elem2_ptr");
		llvm::Value* elem1 = builder->CreateLoad(stackElementTy, elem1Ptr, "elem1");
		llvm::Value* elem2 = builder->CreateLoad(stackElementTy, elem2Ptr, "elem2");
		builder->CreateStore(elem2, elem1Ptr);
		builder->CreateStore(elem1, elem2Ptr);
	}

	void LlvmGenerator::Impl::generateInlineDrop(llvm::Value* ctx) {
		// Inline drop: remove top stack element, releasing string memory if needed
		auto sa = getStackAccess(ctx);

		// Get pointer to top element
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "drop_top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "drop_top_elem");

		// Load the type field (offset 1 in stack element struct)
		llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 1, "drop_type_ptr");
		llvm::Value* elemType = builder->CreateLoad(int32Ty, typePtr, "drop_elem_type");

		// Check if it's a string (type == 3)
		llvm::Value* isString = builder->CreateICmpEQ(elemType, builder->getInt32(3), "drop_is_str");

		// Create blocks for conditional release
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* releaseBlock = llvm::BasicBlock::Create(*context, "drop_release_str", currentFn);
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*context, "drop_after", currentFn);

		builder->CreateCondBr(isString, releaseBlock, afterBlock);

		// Release string block
		builder->SetInsertPoint(releaseBlock);
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "drop_value_ptr");
		llvm::Value* strPtr = builder->CreateLoad(ptrTy, valuePtr, "drop_str_ptr");
		builder->CreateCall(qdStringReleaseFn, {strPtr});
		builder->CreateBr(afterBlock);

		// After block - decrement size
		builder->SetInsertPoint(afterBlock);
		llvm::Value* newSize = builder->CreateSub(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineOver(llvm::Value* ctx) {
		// Inline over: copy second element to top ( a b -- a b a )
		auto sa = getStackAccess(ctx);
		// Get second from top (size - 2)
		llvm::Value* secondIdx = builder->CreateSub(sa.size, builder->getInt64(2), "second_idx");
		llvm::Value* secondElemPtr = builder->CreateGEP(stackElementTy, sa.data, secondIdx, "second_elem");
		// Get new top position (size)
		llvm::Value* newElemPtr = builder->CreateGEP(stackElementTy, sa.data, sa.size, "new_elem");
		// Copy second element to new top
		llvm::Value* secondValue = builder->CreateLoad(stackElementTy, secondElemPtr, "second_value");
		builder->CreateStore(secondValue, newElemPtr);
		// Increment size
		llvm::Value* newSize = builder->CreateAdd(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineRot(llvm::Value* ctx) {
		// Inline rot: rotate top three elements ( a b c -- b c a )
		auto sa = getStackAccess(ctx);
		// Get pointers to top three elements
		llvm::Value* idx1 = builder->CreateSub(sa.size, builder->getInt64(3), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, sa.data, idx1, "elem1_ptr");
		llvm::Value* idx2 = builder->CreateSub(sa.size, builder->getInt64(2), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, sa.data, idx2, "elem2_ptr");
		llvm::Value* idx3 = builder->CreateSub(sa.size, builder->getInt64(1), "idx3");
		llvm::Value* elem3Ptr = builder->CreateGEP(stackElementTy, sa.data, idx3, "elem3_ptr");
		// Load all three and rotate: a b c -> b c a
		llvm::Value* elem1 = builder->CreateLoad(stackElementTy, elem1Ptr, "elem1");
		llvm::Value* elem2 = builder->CreateLoad(stackElementTy, elem2Ptr, "elem2");
		llvm::Value* elem3 = builder->CreateLoad(stackElementTy, elem3Ptr, "elem3");
		builder->CreateStore(elem2, elem1Ptr);
		builder->CreateStore(elem3, elem2Ptr);
		builder->CreateStore(elem1, elem3Ptr);
	}

	llvm::Value* LlvmGenerator::Impl::generateInlinePopInt(llvm::Value* ctx) {
		// Inline pop for integer-only functions - returns the popped i64 value
		auto sa = getStackAccess(ctx);
		// Load top element value: data[size - 1].value
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value = builder->CreateLoad(int64Ty, valuePtr, "pop_value");
		// Decrement size
		llvm::Value* newSize = builder->CreateSub(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
		return value;
	}

	void LlvmGenerator::Impl::generateInlinePopIntToStorage(llvm::Value* ctx, llvm::Value* dst) {
		// Inline pop for integer-only functions - stores directly to a qd_stack_element_t alloca
		auto sa = getStackAccess(ctx);
		// Load top element value: data[size - 1].value
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value = builder->CreateLoad(int64Ty, valuePtr, "pop_value");
		// Store to destination (qd_stack_element_t: { i64 value, i32 type, i1 tainted })
		llvm::Value* dstValuePtr = builder->CreateStructGEP(stackElementTy, dst, 0, "dst_value_ptr");
		builder->CreateStore(value, dstValuePtr);
		llvm::Value* dstTypePtr = builder->CreateStructGEP(stackElementTy, dst, 1, "dst_type_ptr");
		builder->CreateStore(builder->getInt32(0), dstTypePtr);
		llvm::Value* dstTaintedPtr = builder->CreateStructGEP(stackElementTy, dst, 2, "dst_tainted_ptr");
		builder->CreateStore(builder->getInt1(false), dstTaintedPtr);
		// Decrement size
		llvm::Value* newSize = builder->CreateSub(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
	}

} // namespace Qd
