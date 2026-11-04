#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::emitFatalError(llvm::Value* ctx, const char* message) {
		// Use write(2, msg, len) instead of fprintf(stderr, ...) because the
		// stderr symbol is not a portable global — on Haiku, musl, macOS, etc.
		// it is a macro, not a linker symbol, so loading it would crash.
		auto writeFn = module->getOrInsertFunction(
				"write", llvm::FunctionType::get(int64Ty, {int32Ty, ptrTy, int64Ty}, false));
		std::string detail = message;
		for (const char* prefix : {"Fatal error in ", "Fatal error: "}) {
			if (detail.rfind(prefix, 0) == 0) {
				detail = detail.substr(strlen(prefix));
				break;
			}
		}
		while (!detail.empty() && detail.back() == '\n') {
			detail.pop_back();
		}
		builder->CreateCall(
				fatalRecoverFn, {ctx, llvm::ConstantPointerNull::get(ptrTy), builder->CreateGlobalString(detail)});
		// What the program printed came before the failure, so it goes out first. A freestanding
		// program has no stdio to flush.
		if (!freestandingMode) {
			auto fflushFn = module->getOrInsertFunction("fflush", llvm::FunctionType::get(int32Ty, {ptrTy}, false));
			builder->CreateCall(fflushFn, {llvm::ConstantPointerNull::get(ptrTy)});
		}
		auto errorMsg = builder->CreateGlobalString(message);
		builder->CreateCall(writeFn, {builder->getInt32(2), errorMsg, builder->getInt64(strlen(message))});
		builder->CreateCall(printStackTraceFn, {ctx});
		builder->CreateCall(exitFn, {});
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
		generateInlinePushTaggedValue(ctx, value, builder->getInt32(0), "pushv");
	}

	// The body of the inline pushes. `bits` goes into the element's union field
	// and `typeTag` into its type field; the tag is an ordinary value, so a `for`
	// iterator whose type is only known at run time can select it.
	void LlvmGenerator::Impl::generateInlinePushTaggedValue(
			llvm::Value* ctx, llvm::Value* bits, llvm::Value* typeTag, const char* label) {
		llvm::Value* stPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(ptrTy, stPtr, "st");

		llvm::Value* sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(int64Ty, sizePtr, "size");

		// Skip overflow check for integer-only non-main functions (predictable stack usage)
		if (!currentFunctionIsIntegerOnly || currentFunctionIsMain) {
			// Check for overflow
			llvm::Value* capacityPtr = builder->CreateStructGEP(stackStructTy, st, 1, "capacity_ptr");
			llvm::Value* capacity = builder->CreateLoad(int64Ty, capacityPtr, "capacity");
			llvm::Value* hasSpace = builder->CreateICmpULT(size, capacity, "has_space");

			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* overflowBB =
					llvm::BasicBlock::Create(*context, std::string(label) + ".overflow", currentFn);
			llvm::BasicBlock* pushBB = llvm::BasicBlock::Create(*context, std::string(label) + ".do", currentFn);
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
		builder->CreateStore(bits, valuePtr);

		llvm::Value* typePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 1, "type_ptr");
		builder->CreateStore(typeTag, typePtr);

		// Set is_error_tainted to false
		llvm::Value* taintedPtr = builder->CreateStructGEP(stackElementTy, elemPtr, 2, "tainted_ptr");
		builder->CreateStore(builder->getInt1(false), taintedPtr);

		// Increment size
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::emitDivisorZeroCheck(llvm::Value* ctx, llvm::Value* divisor, const char* opName) {
		llvm::Value* isZero = builder->CreateICmpEQ(divisor, builder->getInt64(0), "divisor_is_zero");
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* zeroBB = llvm::BasicBlock::Create(*context, std::string(opName) + ".divzero", currentFn);
		llvm::BasicBlock* okBB = llvm::BasicBlock::Create(*context, std::string(opName) + ".ok", currentFn);
		builder->CreateCondBr(isZero, zeroBB, okBB);
		builder->SetInsertPoint(zeroBB);
		std::string msg = std::string("Fatal error in ") + opName + ": Division by zero\n";
		emitFatalError(ctx, msg.c_str());
		builder->SetInsertPoint(okBB);
	}

	llvm::Value* LlvmGenerator::Impl::emitWrappingSDiv(llvm::Value* a, llvm::Value* b) {
		llvm::Value* isNegOne =
				builder->CreateICmpEQ(b, llvm::ConstantInt::getSigned(builder->getInt64Ty(), -1), "divisor_is_neg1");
		llvm::Value* safeB = builder->CreateSelect(isNegOne, builder->getInt64(1), b, "safe_divisor");
		llvm::Value* quotient = builder->CreateSDiv(a, safeB, "div");
		llvm::Value* negated = builder->CreateSub(builder->getInt64(0), a, "div_neg");
		return builder->CreateSelect(isNegOne, negated, quotient, "div_result");
	}

	llvm::Value* LlvmGenerator::Impl::emitWrappingSRem(llvm::Value* a, llvm::Value* b) {
		llvm::Value* isNegOne =
				builder->CreateICmpEQ(b, llvm::ConstantInt::getSigned(builder->getInt64Ty(), -1), "divisor_is_neg1");
		llvm::Value* safeB = builder->CreateSelect(isNegOne, builder->getInt64(1), b, "safe_divisor");
		llvm::Value* remainder = builder->CreateSRem(a, safeB, "mod");
		return builder->CreateSelect(isNegOne, builder->getInt64(0), remainder, "mod_result");
	}

	llvm::Value* LlvmGenerator::Impl::emitCheckedShift(
			llvm::Value* ctx, llvm::Value* value, llvm::Value* count, bool left, const char* opName) {
		llvm::Value* inRange = builder->CreateICmpULT(count, builder->getInt64(64), "shift_count_ok");
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* badBB = llvm::BasicBlock::Create(*context, std::string(opName) + ".badcount", currentFn);
		llvm::BasicBlock* okBB = llvm::BasicBlock::Create(*context, std::string(opName) + ".ok", currentFn);
		builder->CreateCondBr(inRange, okBB, badBB);
		builder->SetInsertPoint(badBB);
		std::string msg = std::string("Fatal error in ") + opName + ": Shift count out of range (must be 0-63)\n";
		emitFatalError(ctx, msg.c_str());
		builder->SetInsertPoint(okBB);
		return left ? builder->CreateShl(value, count, "shl") : builder->CreateLShr(value, count, "shr");
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

		// Guard against modulo-by-zero. The type-aware path diverts to the
		// runtime qd_mod, which reports a clean error; in an integer-only
		// function we emit the same fatal error inline rather than letting SRem
		// trap with SIGFPE.
		llvm::Value* isZero = builder->CreateICmpEQ(boc.value2, builder->getInt64(0), "divisor_is_zero");
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* divZeroBB = llvm::BasicBlock::Create(*context, "mod.divzero", currentFn);
		llvm::BasicBlock* modOkBB = llvm::BasicBlock::Create(*context, "mod.ok", currentFn);
		builder->CreateCondBr(isZero, divZeroBB, modOkBB);

		builder->SetInsertPoint(divZeroBB);
		// Same wording as the runtime's QDRT_FATAL path (runtime_ops.c), so the
		// message does not depend on whether this op was inlined.
		emitFatalError(ctx, "Fatal error in mod: Division by zero\n");

		builder->SetInsertPoint(modOkBB);
		llvm::Value* result = emitWrappingSRem(boc.value1, boc.value2);
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

	// Logical AND and OR: ( a:int b:int -- result:int ). Every non-zero value counts as true,
	// so both operands are narrowed to 0/1 first -- `2 1 and` is 1, where the bitwise
	// `bits::and` makes it 0.
	void LlvmGenerator::Impl::generateInlineLogicalAnd(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* a = builder->CreateICmpNE(boc.value1, builder->getInt64(0), "a_true");
		llvm::Value* b = builder->CreateICmpNE(boc.value2, builder->getInt64(0), "b_true");
		llvm::Value* result = builder->CreateZExt(builder->CreateAnd(a, b, "land"), int64Ty, "land_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineLogicalOr(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* a = builder->CreateICmpNE(boc.value1, builder->getInt64(0), "a_true");
		llvm::Value* b = builder->CreateICmpNE(boc.value2, builder->getInt64(0), "b_true");
		llvm::Value* result = builder->CreateZExt(builder->CreateOr(a, b, "lor"), int64Ty, "lor_result");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineLogicalNot(llvm::Value* ctx) {
		// Inline implementation of logical NOT: ( a:int -- result:int )
		// Distinct from `not`, which is bitwise: 0 -> 1, anything else -> 0.
		auto sa = getStackAccess(ctx);
		llvm::Value* idx = builder->CreateSub(sa.size, builder->getInt64(1), "idx");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, sa.data, idx, "elem_ptr");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valuePtrCast = builder->CreateBitCast(valuePtr, ptrTy);
		llvm::Value* value = builder->CreateLoad(int64Ty, valuePtrCast, "value");
		llvm::Value* isZero = builder->CreateICmpEQ(value, builder->getInt64(0), "is_zero");
		llvm::Value* result = builder->CreateZExt(isZero, int64Ty, "lnot_result");
		builder->CreateStore(result, valuePtrCast);
	}

	void LlvmGenerator::Impl::generateInlineIsNonZero(llvm::Value* ctx) {
		// ( a:int -- flag:int ): 1 for anything but 0, the inverse of generateInlineLogicalNot
		auto sa = getStackAccess(ctx);
		llvm::Value* idx = builder->CreateSub(sa.size, builder->getInt64(1), "idx");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, sa.data, idx, "elem_ptr");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valuePtrCast = builder->CreateBitCast(valuePtr, ptrTy);
		llvm::Value* value = builder->CreateLoad(int64Ty, valuePtrCast, "value");
		llvm::Value* isNonZero = builder->CreateICmpNE(value, builder->getInt64(0), "is_nonzero");
		builder->CreateStore(builder->CreateZExt(isNonZero, int64Ty, "nonzero_result"), valuePtrCast);
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
		llvm::Value* result = emitCheckedShift(ctx, boc.value1, boc.value2, true, "bits::shl");
		finishBinaryOp(boc, result);
	}

	void LlvmGenerator::Impl::generateInlineBitRshift(llvm::Value* ctx) {
		auto boc = setupBinaryOp(ctx);
		llvm::Value* result = emitCheckedShift(ctx, boc.value1, boc.value2, false, "bits::shr");
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

	llvm::Value* LlvmGenerator::Impl::loadSlotValue(llvm::Value* valuePtr, const std::string& name) {
		return builder->CreateAlignedLoad(slotValueTy, valuePtr, llvm::Align(8), name);
	}

	void LlvmGenerator::Impl::storeSlotValue(llvm::Value* value, llvm::Value* valuePtr) {
		builder->CreateAlignedStore(value, valuePtr, llvm::Align(8));
	}

	void LlvmGenerator::Impl::pushD128At(llvm::Value* ctx, llvm::Value* valuePtr, const std::string& name) {
		llvm::Value* words = loadSlotValue(valuePtr, name + "_d128");
		builder->CreateCall(pushD128BitsFn, {ctx, builder->CreateExtractValue(words, 0, name + "_d128_low"),
													builder->CreateExtractValue(words, 1, name + "_d128_high")});
	}

	bool LlvmGenerator::Impl::d128LiteralWords(const std::string& text, uint64_t& low, uint64_t& high) {
		std::string digits;
		qd_d128_t value;
		if (readD128Literal(text, digits) != std::errc() || !qd_d128_parse(digits.c_str(), &value)) {
			return false;
		}
		low = value.w[0];
		high = value.w[1];
		return true;
	}

	void LlvmGenerator::Impl::addD128PushCase(llvm::SwitchInst* sw, llvm::Value* valuePtr, llvm::Value* ctx,
			llvm::BasicBlock* endBlock, const std::string& name) {
		llvm::IRBuilderBase::InsertPointGuard guard(*builder);
		llvm::BasicBlock* block =
				llvm::BasicBlock::Create(*context, name + "_push_d128", builder->GetInsertBlock()->getParent());
		sw->addCase(builder->getInt32(kStackTypeD128), block);
		builder->SetInsertPoint(block);
		pushD128At(ctx, valuePtr, name);
		builder->CreateBr(endBlock);
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

	llvm::Value* LlvmGenerator::Impl::generateInlinePopFloat(llvm::Value* ctx) {
		// Inline pop for float values - returns the popped f64 value
		// The value union field is at offset 0 (same layout for i64 and f64)
		auto sa = getStackAccess(ctx);
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "pop_fvalue");
		// Decrement size
		llvm::Value* newSize = builder->CreateSub(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
		return value;
	}

	void LlvmGenerator::Impl::generateInlinePushFloatValue(llvm::Value* ctx, llvm::Value* value) {
		// Inline implementation of qd_push_f for runtime float values. The union
		// field holds either kind, so this is the integer push under another tag.
		generateInlinePushTaggedValue(ctx, value, builder->getInt32(1), "pushf");
	}

	void LlvmGenerator::Impl::generateInlinePopIntToStorage(llvm::Value* ctx, llvm::Value* dst) {
		// Inline pop for numeric functions - stores directly to a qd_stack_element_t alloca
		auto sa = getStackAccess(ctx);
		// Load top element: data[size - 1]
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value = loadSlotValue(valuePtr, "pop_value");
		llvm::Value* srcTypePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 1, "src_type_ptr");
		llvm::Value* srcType = builder->CreateLoad(int32Ty, srcTypePtr, "src_type");
		// Store to destination (qd_stack_element_t: { [2 x i64] value, i32 type, i8 tainted, i8 ptr_kind })
		llvm::Value* dstValuePtr = builder->CreateStructGEP(stackElementTy, dst, 0, "dst_value_ptr");
		storeSlotValue(value, dstValuePtr);
		llvm::Value* dstTypePtr = builder->CreateStructGEP(stackElementTy, dst, 1, "dst_type_ptr");
		builder->CreateStore(srcType, dstTypePtr);
		llvm::Value* dstTaintedPtr = builder->CreateStructGEP(stackElementTy, dst, 2, "dst_tainted_ptr");
		builder->CreateStore(builder->getInt1(false), dstTaintedPtr);
		llvm::Value* srcKind = builder->CreateLoad(builder->getInt8Ty(),
				builder->CreateStructGEP(stackElementTy, topElemPtr, 3, "src_kind_ptr"), "src_kind");
		builder->CreateStore(srcKind, builder->CreateStructGEP(stackElementTy, dst, 3, "dst_kind_ptr"));
		// Decrement size
		llvm::Value* newSize = builder->CreateSub(sa.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sa.sizePtr);
	}

	namespace {
		// What each inline mem:: word reads or writes
		struct MemAccess {
			unsigned bits; // 64 for f64
			bool isFloat;
			bool isStore;
			bool isSigned; // a load's extension
		};

		const MemAccess* memAccess(const std::string& name) {
			static const std::map<std::string, MemAccess> accesses = {
					{"mem::get_byte", {8, false, false, false}},
					{"mem::get_u8", {8, false, false, false}},
					{"mem::get_i8", {8, false, false, true}},
					{"mem::get_u16", {16, false, false, false}},
					{"mem::get_i16", {16, false, false, true}},
					{"mem::get_u32", {32, false, false, false}},
					{"mem::get_i32", {32, false, false, true}},
					{"mem::get_i64", {64, false, false, true}},
					{"mem::get_f64", {64, true, false, false}},
					{"mem::set_byte", {8, false, true, false}},
					{"mem::set_u8", {8, false, true, false}},
					{"mem::set_i8", {8, false, true, false}},
					{"mem::set_u16", {16, false, true, false}},
					{"mem::set_i16", {16, false, true, false}},
					{"mem::set_u32", {32, false, true, false}},
					{"mem::set_i32", {32, false, true, false}},
					{"mem::set_i64", {64, false, true, false}},
					{"mem::set_f64", {64, true, true, false}},
			};
			auto it = accesses.find(name);
			return it == accesses.end() ? nullptr : &it->second;
		}
	}

	bool LlvmGenerator::Impl::isInlineMemWord(const std::string& name) {
		return memAccess(name) != nullptr;
	}

	bool LlvmGenerator::Impl::bodyPrintsOrCasts(IAstNode* node) {
		if (!node) {
			return false;
		}
		if (node->type() == IAstNode::Type::INSTRUCTION) {
			const std::string& name = static_cast<AstNodeInstruction*>(node)->name();
			if (name == "print" || name == "cast") {
				return true;
			}
		}
		for (auto* child : node->children()) {
			if (bodyPrintsOrCasts(child)) {
				return true;
			}
		}
		return false;
	}

	bool LlvmGenerator::Impl::bodyReadsElements(IAstNode* node) {
		if (!node) {
			return false;
		}
		if (node->type() == IAstNode::Type::INSTRUCTION) {
			const std::string& name = static_cast<AstNodeInstruction*>(node)->name();
			if (name == "len" || name == "nth") {
				return true;
			}
		}
		for (auto* child : node->children()) {
			if (bodyReadsElements(child)) {
				return true;
			}
		}
		return false;
	}

	bool LlvmGenerator::Impl::bodyStringSafe(IAstNode* node) {
		if (!node) {
			return true;
		}
		switch (node->type()) {
		case IAstNode::Type::INSTRUCTION: {
			auto* inst = static_cast<AstNodeInstruction*>(node);
			static const std::set<std::string> MOVES = {"dup", "dup2", "drop", "drop2", "swap", "over", "rot", "rrot",
					"nip", "tuck", "pick", "rot4", "rrot4", "len", "nth"};
			if (!MOVES.count(inst->name()) && inst->stringOperands() != AstNodeInstruction::StringOperands::None) {
				return false;
			}
			break;
		}
		case IAstNode::Type::LOCAL:
			// A store into a global would keep the address past the call
			for (const std::string& name : static_cast<AstNodeLocal*>(node)->names()) {
				if (findGlobalVar(name)) {
					return false;
				}
			}
			break;
		case IAstNode::Type::IDENTIFIER: {
			const GlobalVar* global = findGlobalVar(static_cast<AstNodeIdentifier*>(node)->name());
			if (global && valueKind(global->type) != ValueKind::Int && valueKind(global->type) != ValueKind::Float) {
				return false;
			}
			break;
		}
		default:
			break;
		}
		for (auto* child : node->children()) {
			if (!bodyStringSafe(child)) {
				return false;
			}
		}
		return true;
	}

	llvm::Value* LlvmGenerator::Impl::generateInlinePopPtr(llvm::Value* ctx, llvm::Value*& isRaw) {
		auto sa = getStackAccess(ctx);
		llvm::Value* topIdx = builder->CreateSub(sa.size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, sa.data, topIdx, "top_elem");
		llvm::Value* value = builder->CreateLoad(
				int64Ty, builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr"), "pop_address");
		llvm::Value* kind = builder->CreateLoad(
				builder->getInt8Ty(), builder->CreateStructGEP(stackElementTy, topElemPtr, 3, "kind_ptr"), "pop_kind");
		isRaw = builder->CreateICmpEQ(kind, builder->getInt8(1), "pop_raw"); // QD_PTR_RAW
		builder->CreateStore(builder->CreateSub(sa.size, builder->getInt64(1), "new_size"), sa.sizePtr);
		return value;
	}

	// The slot's reference, given up once the native version is done with the address, as the
	// stack version's consuming it would have. A string's slot owns one (isRaw null); a `ptr` is
	// raw memory and holds none.
	void LlvmGenerator::Impl::releasePoppedPtr(llvm::Value* address, llvm::Value* isRaw) {
		if (!isRaw) {
			builder->CreateCall(qdStringReleaseFn, {builder->CreateIntToPtr(address, ptrTy, "release_str")});
		}
	}

	// mem::get_* ( address:ptr offset:i64 -- value ) and mem::set_* ( value address:ptr offset:i64 -- )
	// as one load or store. The C words move every argument through qd_args, which is most of
	// what a byte read costs; here the slots are read where they lie. Only the case the words
	// were written for is taken -- a non-null pointer, an integer offset, a value of the type
	// stored -- and anything else calls the C word, which raises as it always has.
	bool LlvmGenerator::Impl::generateInlineMemCall(const std::string& callee, llvm::Function* word, llvm::Value* ctx) {
		const MemAccess* found = memAccess(callee);
		if (!found) {
			return false;
		}
		const MemAccess& access = *found;
		const uint64_t operands = access.isStore ? 3 : 2;
		llvm::Type* memTy = access.isFloat ? builder->getDoubleTy() : builder->getIntNTy(access.bits);

		// In a native version the address is an i64 on the compile-time stack, and so are the
		// offset and the value; a null address raises as the C word does
		if (useCompileTimeStack) {
			if (compileTimeStack.size() < operands) {
				return false;
			}
			llvm::Value* offset = asNativeType(compileTimeStack.back(), NativeParamType::I64);
			compileTimeStack.pop_back();
			llvm::Value* address = asNativeType(compileTimeStack.back(), NativeParamType::I64);
			compileTimeStack.pop_back();
			llvm::Value* value = nullptr;
			if (access.isStore) {
				value = asNativeType(
						compileTimeStack.back(), access.isFloat ? NativeParamType::F64 : NativeParamType::I64);
				compileTimeStack.pop_back();
			}
			llvm::Function* fn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* nullBlock = llvm::BasicBlock::Create(*context, "mem_null", fn);
			llvm::BasicBlock* okBlock = llvm::BasicBlock::Create(*context, "mem_ok", fn);
			builder->CreateCondBr(builder->CreateICmpEQ(address, builder->getInt64(0)), nullBlock, okBlock);
			builder->SetInsertPoint(nullBlock);
			llvm::FunctionCallee raise = module->getOrInsertFunction("qd_fatal_raise",
					llvm::FunctionType::get(builder->getVoidTy(), {contextPtrTy, ptrTy, ptrTy}, true));
			builder->CreateCall(raise, {ctx, getOrCreateGlobalString(callee), getOrCreateGlobalString("Null pointer")});
			builder->CreateUnreachable();
			builder->SetInsertPoint(okBlock);
			llvm::Value* at =
					builder->CreateIntToPtr(builder->CreateAdd(address, offset, "mem_at_int"), ptrTy, "mem_at");
			if (access.isStore) {
				if (!access.isFloat && access.bits < 64) {
					value = builder->CreateTrunc(value, memTy, "mem_narrow");
				}
				builder->CreateAlignedStore(value, at, llvm::Align(1));
			} else {
				llvm::Value* loaded = builder->CreateAlignedLoad(memTy, at, llvm::Align(1), "mem_loaded");
				if (!access.isFloat && access.bits < 64) {
					loaded = access.isSigned ? builder->CreateSExt(loaded, int64Ty, "mem_wide")
											 : builder->CreateZExt(loaded, int64Ty, "mem_wide");
				}
				compileTimeStack.push_back(loaded);
			}
			return true;
		}

		auto sa = getStackAccess(ctx);
		auto slot = [&](uint64_t fromTop, const char* name) {
			llvm::Value* idx = builder->CreateSub(sa.size, builder->getInt64(fromTop), std::string(name) + "_idx");
			return builder->CreateGEP(stackElementTy, sa.data, idx, std::string(name) + "_slot");
		};
		auto typeOf = [&](llvm::Value* elem, const char* name) {
			return builder->CreateLoad(int32Ty,
					builder->CreateStructGEP(stackElementTy, elem, 1, std::string(name) + "_type_ptr"),
					std::string(name) + "_type");
		};
		auto valueAt = [&](llvm::Value* elem) { return builder->CreateStructGEP(stackElementTy, elem, 0, "mem_vp"); };

		llvm::Function* fn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* checkBlock = llvm::BasicBlock::Create(*context, "mem_check", fn);
		llvm::BasicBlock* fastBlock = llvm::BasicBlock::Create(*context, "mem_fast", fn);
		llvm::BasicBlock* slowBlock = llvm::BasicBlock::Create(*context, "mem_slow", fn);
		llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context, "mem_done", fn);

		// Enough values to look at, before any slot is read
		builder->CreateCondBr(
				builder->CreateICmpUGE(sa.size, builder->getInt64(operands), "mem_enough"), checkBlock, slowBlock);

		builder->SetInsertPoint(checkBlock);
		llvm::Value* offsetSlot = slot(1, "mem_offset");
		llvm::Value* addressSlot = slot(2, "mem_address");
		llvm::Value* address = builder->CreateLoad(ptrTy, valueAt(addressSlot), "mem_address");
		llvm::Value* ok = builder->CreateICmpEQ(typeOf(offsetSlot, "mem_offset"), builder->getInt32(0)); // INT
		ok = builder->CreateAnd(
				ok, builder->CreateICmpEQ(typeOf(addressSlot, "mem_address"), builder->getInt32(2))); // PTR
		ok = builder->CreateAnd(ok, builder->CreateIsNotNull(address, "mem_not_null"));
		llvm::Value* valueSlot = nullptr;
		if (access.isStore) {
			valueSlot = slot(3, "mem_value");
			ok = builder->CreateAnd(ok, builder->CreateICmpEQ(typeOf(valueSlot, "mem_value"),
												builder->getInt32(access.isFloat ? 1 : 0))); // FLOAT : INT
		}
		builder->CreateCondBr(ok, fastBlock, slowBlock);

		builder->SetInsertPoint(fastBlock);
		llvm::Value* offset = builder->CreateLoad(int64Ty, valueAt(offsetSlot), "mem_offset");
		llvm::Value* at = builder->CreateGEP(builder->getInt8Ty(), address, offset, "mem_at");
		if (access.isStore) {
			llvm::Value* value = builder->CreateLoad(
					access.isFloat ? builder->getDoubleTy() : int64Ty, valueAt(valueSlot), "mem_value");
			if (!access.isFloat && access.bits < 64) {
				value = builder->CreateTrunc(value, memTy, "mem_narrow");
			}
			builder->CreateAlignedStore(value, at, llvm::Align(1));
		} else {
			llvm::Value* value = builder->CreateAlignedLoad(memTy, at, llvm::Align(1), "mem_loaded");
			if (!access.isFloat && access.bits < 64) {
				value = access.isSigned ? builder->CreateSExt(value, int64Ty, "mem_wide")
										: builder->CreateZExt(value, int64Ty, "mem_wide");
			}
			// The result takes the address's slot; the pointer's reference goes, as qd_args lets it
			builder->CreateStore(value, valueAt(addressSlot));
			builder->CreateStore(builder->getInt32(access.isFloat ? 1 : 0),
					builder->CreateStructGEP(stackElementTy, addressSlot, 1, "mem_result_type"));
			builder->CreateStore(
					builder->getInt8(0), builder->CreateStructGEP(stackElementTy, addressSlot, 2, "mem_result_taint"));
		}
		builder->CreateStore(builder->CreateSub(sa.size, builder->getInt64(access.isStore ? 3 : 1)), sa.sizePtr);
		builder->CreateBr(endBlock);

		builder->SetInsertPoint(slowBlock);
		builder->CreateCall(word, {ctx});
		builder->CreateBr(endBlock);

		builder->SetInsertPoint(endBlock);
		return true;
	}

} // namespace Qd
