#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::generateInlinePushInt(llvm::Value* ctx, int64_t value) {
		// Delegate to generateInlinePushIntValue with a constant
		generateInlinePushIntValue(ctx, builder->getInt64(static_cast<uint64_t>(value)));
	}

	void LlvmGenerator::Impl::generateInlinePushIntValue(llvm::Value* ctx, llvm::Value* value) {
		// Inline implementation of qd_push_i for runtime integer values
		// Same as generateInlinePushInt but takes llvm::Value* instead of int64_t

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(*context,
				{
						llvm::PointerType::get(*context, 0), // data
						builder->getInt64Ty(),				 // capacity
						builder->getInt64Ty()				 // size
				},
				false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Skip overflow check for integer-only functions (predictable stack usage)
		if (!currentFunctionIsIntegerOnly) {
			// Check for overflow
			llvm::Value* capacityPtr = builder->CreateStructGEP(stackTy, st, 1, "capacity_ptr");
			llvm::Value* capacity = builder->CreateLoad(builder->getInt64Ty(), capacityPtr, "capacity");
			llvm::Value* hasSpace = builder->CreateICmpULT(size, capacity, "has_space");

			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* overflowBB = llvm::BasicBlock::Create(*context, "pushv.overflow", currentFn);
			llvm::BasicBlock* pushBB = llvm::BasicBlock::Create(*context, "pushv.do", currentFn);
			builder->CreateCondBr(hasSpace, pushBB, overflowBB);

			// Generate overflow error
			builder->SetInsertPoint(overflowBB);
			auto fprintfFn = module->getOrInsertFunction("fprintf",
					llvm::FunctionType::get(builder->getInt32Ty(),
							{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
			auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
			auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
			auto errorMsg =
					builder->CreateGlobalString("Fatal error: Stack overflow (use -s to increase stack size)\n");
			builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
			auto printStackTraceFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
			builder->CreateCall(printStackTraceFn, {ctx});
			auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
			builder->CreateCall(abortFn, {});
			builder->CreateUnreachable();

			// Do the push
			builder->SetInsertPoint(pushBB);
		}

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, size, "elem_ptr");

		// Store runtime value
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valueiPtr = builder->CreateBitCast(valuePtr, llvm::PointerType::get(*context, 0));
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
		// Inline implementation of integer add: ( a:int b:int -- result:int )
		// Pops two integers from stack, adds them, pushes result
		// Assumes both operands are integers (no type checking for performance)

		// Get ctx->st
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		// Get stack structure
		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		// Get st->size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Get st->data
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load first operand: data[size - 2]
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		// Load second operand: data[size - 1]
		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform addition (nsw = no signed wrap, enables better optimization)
		llvm::Value* result = builder->CreateNSWAdd(value1, value2, "add_result");

		// Store result at data[size - 2]
		builder->CreateStore(result, value1iPtrCast);

		// Update size: size - 1 (net effect: pop 2, push 1)
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntSub(llvm::Value* ctx) {
		// Inline implementation of integer subtract: ( a:int b:int -- result:int )
		// Same as add, but with subtraction

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform subtraction (nsw = no signed wrap, enables better optimization)
		llvm::Value* result = builder->CreateNSWSub(value1, value2, "sub_result");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntMul(llvm::Value* ctx) {
		// Inline implementation of integer multiply: ( a:int b:int -- result:int )
		// Same pattern as add/sub, but with multiplication

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform multiplication (nsw = no signed wrap, enables better optimization)
		llvm::Value* result = builder->CreateNSWMul(value1, value2, "mul_result");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntMod(llvm::Value* ctx) {
		// Inline implementation of integer modulo: ( a:int b:int -- result:int )
		// Assumes both operands are integers (no type checking for performance)

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Perform modulo (signed remainder)
		llvm::Value* result = builder->CreateSRem(value1, value2, "mod_result");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineIntCompare(
			llvm::Value* ctx, llvm::CmpInst::Predicate pred, const char* resultName) {
		// Inline implementation of integer comparison: ( a:int b:int -- result:int )
		// Pops two integers, compares them with the given predicate, pushes 0 or 1

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmp(pred, value1, value2, resultName);
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
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
		// Inline implementation of bitwise AND: ( a:int b:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateAnd(value1, value2, "and_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitOr(llvm::Value* ctx) {
		// Inline implementation of bitwise OR: ( a:int b:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateOr(value1, value2, "or_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitXor(llvm::Value* ctx) {
		// Inline implementation of bitwise XOR: ( a:int b:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateXor(value1, value2, "xor_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitNot(llvm::Value* ctx) {
		// Inline implementation of bitwise NOT: ( a:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		llvm::Value* idx = builder->CreateSub(size, builder->getInt64(1), "idx");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, idx, "elem_ptr");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");
		llvm::Value* valuePtrCast = builder->CreateBitCast(valuePtr, llvm::PointerType::get(*context, 0));
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), valuePtrCast, "value");

		llvm::Value* result = builder->CreateNot(value, "not_result");
		builder->CreateStore(result, valuePtrCast);
		// Stack size unchanged for unary operation
	}

	void LlvmGenerator::Impl::generateInlineBitLshift(llvm::Value* ctx) {
		// Inline implementation of left shift: ( value:int shift:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load value (first operand at size-2)
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value");

		// Load shift amount (second operand at size-1)
		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* shift = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "shift");

		llvm::Value* result = builder->CreateShl(value, shift, "lshift_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineBitRshift(llvm::Value* ctx) {
		// Inline implementation of logical right shift: ( value:int shift:int -- result:int )
		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load value (first operand at size-2)
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value");

		// Load shift amount (second operand at size-1)
		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");
		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* shift = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "shift");

		// Use logical shift right (LShr) for unsigned behavior
		llvm::Value* result = builder->CreateLShr(value, shift, "rshift_result");
		builder->CreateStore(result, value1iPtrCast);

		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineDup(llvm::Value* ctx) {
		// Inline dup: duplicate top stack element
		// Simple operation, no type checking needed

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointer to top element (size - 1)
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");

		// Get pointer to new element (size)
		llvm::Value* newElemPtr = builder->CreateGEP(stackElementTy, data, size, "new_elem");

		// Copy entire element (value union, type, is_error_tainted)
		// Element size is 16 bytes (8 for union, 4 for type, 1 for bool + padding)
		llvm::Value* topValue = builder->CreateLoad(stackElementTy, topElemPtr, "top_value");
		builder->CreateStore(topValue, newElemPtr);

		// Increment size
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineSwap(llvm::Value* ctx) {
		// Inline swap: swap top two stack elements
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Check for stack underflow (need at least 2 elements)
		llvm::Value* hasEnough = builder->CreateICmpUGE(size, builder->getInt64(2), "has_enough");
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "swap.underflow", currentFn);
		llvm::BasicBlock* swapBB = llvm::BasicBlock::Create(*context, "swap.do", currentFn);
		builder->CreateCondBr(hasEnough, swapBB, underflowBB);

		// Generate underflow error
		builder->SetInsertPoint(underflowBB);
		auto fprintfFn = module->getOrInsertFunction("fprintf",
				llvm::FunctionType::get(builder->getInt32Ty(),
						{llvm::PointerType::getUnqual(*context), llvm::PointerType::getUnqual(*context)}, true));
		auto stderrGlobal = module->getOrInsertGlobal("stderr", llvm::PointerType::getUnqual(*context));
		auto stderrVal = builder->CreateLoad(llvm::PointerType::getUnqual(*context), stderrGlobal, "stderr");
		auto errorMsg = builder->CreateGlobalString("Fatal error in swap: Stack underflow (requires 2 elements)\n");
		builder->CreateCall(fprintfFn, {stderrVal, errorMsg});
		auto printStackTraceFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
		auto printStackTraceFn = module->getOrInsertFunction("qd_print_stack_trace", printStackTraceFnTy);
		builder->CreateCall(printStackTraceFn, {ctx});
		auto abortFn = module->getOrInsertFunction("abort", llvm::FunctionType::get(builder->getVoidTy(), false));
		builder->CreateCall(abortFn, {});
		builder->CreateUnreachable();

		// Do the swap
		builder->SetInsertPoint(swapBB);
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointers to top two elements
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		// Load both elements
		llvm::Value* elem1 = builder->CreateLoad(stackElementTy, elem1Ptr, "elem1");
		llvm::Value* elem2 = builder->CreateLoad(stackElementTy, elem2Ptr, "elem2");

		// Store them swapped
		builder->CreateStore(elem2, elem1Ptr);
		builder->CreateStore(elem1, elem2Ptr);
	}

	void LlvmGenerator::Impl::generateInlineDrop(llvm::Value* ctx) {
		// Inline drop: remove top stack element

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Simply decrement size
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineOver(llvm::Value* ctx) {
		// Inline over: copy second element to top ( a b -- a b a )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get second from top (size - 2)
		llvm::Value* secondIdx = builder->CreateSub(size, builder->getInt64(2), "second_idx");
		llvm::Value* secondElemPtr = builder->CreateGEP(stackElementTy, data, secondIdx, "second_elem");

		// Get new top position (size)
		llvm::Value* newElemPtr = builder->CreateGEP(stackElementTy, data, size, "new_elem");

		// Copy second element to new top
		llvm::Value* secondValue = builder->CreateLoad(stackElementTy, secondElemPtr, "second_value");
		builder->CreateStore(secondValue, newElemPtr);

		// Increment size
		llvm::Value* newSize = builder->CreateAdd(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

	void LlvmGenerator::Impl::generateInlineRot(llvm::Value* ctx) {
		// Inline rot: rotate top three elements ( a b c -- b c a )

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Get pointers to top three elements
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(3), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(2), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* idx3 = builder->CreateSub(size, builder->getInt64(1), "idx3");
		llvm::Value* elem3Ptr = builder->CreateGEP(stackElementTy, data, idx3, "elem3_ptr");

		// Load all three
		llvm::Value* elem1 = builder->CreateLoad(stackElementTy, elem1Ptr, "elem1");
		llvm::Value* elem2 = builder->CreateLoad(stackElementTy, elem2Ptr, "elem2");
		llvm::Value* elem3 = builder->CreateLoad(stackElementTy, elem3Ptr, "elem3");

		// Rotate: a b c -> b c a
		builder->CreateStore(elem2, elem1Ptr);
		builder->CreateStore(elem3, elem2Ptr);
		builder->CreateStore(elem1, elem3Ptr);
	}

	llvm::Value* LlvmGenerator::Impl::generateInlinePopInt(llvm::Value* ctx) {
		// Inline pop for integer-only functions - returns the popped i64 value
		// Assumes stack is not empty (caller must ensure this in integer-only context)

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		// Get current size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Get data pointer
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load top element value: data[size - 1].value
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "pop_value");

		// Decrement size
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		return value;
	}

	void LlvmGenerator::Impl::generateInlinePopIntToStorage(llvm::Value* ctx, llvm::Value* dst) {
		// Inline pop for integer-only functions - stores directly to a qd_stack_element_t alloca
		// Assumes stack is not empty (caller must ensure this in integer-only context)

		llvm::Type* contextTy = llvm::StructType::get(*context, {llvm::PointerType::get(*context, 0)}, false);
		llvm::Value* stPtr = builder->CreateStructGEP(contextTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(llvm::PointerType::get(*context, 0), stPtr, "st");

		llvm::Type* stackTy = llvm::StructType::get(
				*context, {llvm::PointerType::get(*context, 0), builder->getInt64Ty(), builder->getInt64Ty()}, false);

		// Get current size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(builder->getInt64Ty(), sizePtr, "size");

		// Get data pointer
		llvm::Value* dataPtr = builder->CreateStructGEP(stackTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(llvm::PointerType::get(*context, 0), dataPtr, "data");

		// Load top element value: data[size - 1].value
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value = builder->CreateLoad(builder->getInt64Ty(), valuePtr, "pop_value");

		// Store to destination (qd_stack_element_t: { i64 value, i32 type, i1 tainted })
		llvm::Value* dstValuePtr = builder->CreateStructGEP(stackElementTy, dst, 0, "dst_value_ptr");
		builder->CreateStore(value, dstValuePtr);

		// Set type to INT (0)
		llvm::Value* dstTypePtr = builder->CreateStructGEP(stackElementTy, dst, 1, "dst_type_ptr");
		builder->CreateStore(builder->getInt32(0), dstTypePtr);

		// Set tainted to false
		llvm::Value* dstTaintedPtr = builder->CreateStructGEP(stackElementTy, dst, 2, "dst_tainted_ptr");
		builder->CreateStore(builder->getInt1(false), dstTaintedPtr);

		// Decrement size
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);
	}

} // namespace Qd
