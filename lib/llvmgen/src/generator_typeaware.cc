#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::generateTypeAwareAdd(llvm::Value* ctx) {
		// Type-aware inline add: checks types at runtime and uses fast path for integers
		// If both operands are integers: inline add
		// Otherwise: call qd_add() which handles all type combinations

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

		// Get pointers to top two elements
		llvm::Value* idx1 = builder->CreateSub(size, builder->getInt64(2), "idx1");
		llvm::Value* elem1Ptr = builder->CreateGEP(stackElementTy, data, idx1, "elem1_ptr");

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		// Load types of both elements (field 1 of qd_stack_element_t)
		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		// Check if both are integers (QD_STACK_TYPE_INT = 0)
		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		// Create basic blocks for fast path (inline) and slow path (runtime call)
		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_add", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_add", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "add_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		// Fast path: inline integer add
		builder->SetInsertPoint(fastPath);
		{
			// Load integer values (field 0, accessed as i64)
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform addition (nsw = no signed wrap)
			llvm::Value* result = builder->CreateNSWAdd(value1, value2, "add_result");

			// Store result at elem1 (reuse first position)
			builder->CreateStore(result, value1iPtrCast);

			// Decrement size by 1 (net: pop 2, push 1)
			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		// Slow path: call qd_add() runtime function
		builder->SetInsertPoint(slowPath);
		{
			// addFn is now a member variable, initialized in setupRuntimeDeclarations
			builder->CreateCall(addFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareSub(llvm::Value* ctx) {
		// Similar to generateTypeAwareAdd, but with subtraction

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_sub", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_sub", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "sub_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* result = builder->CreateNSWSub(value1, value2, "sub_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			// subFn is now a member variable, initialized in setupRuntimeDeclarations
			builder->CreateCall(subFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareMul(llvm::Value* ctx) {
		// Similar to generateTypeAwareAdd, but with multiplication

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_mul", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_mul", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "mul_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* result = builder->CreateNSWMul(value1, value2, "mul_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			// mulFn is now a member variable, initialized in setupRuntimeDeclarations
			builder->CreateCall(mulFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareLt(llvm::Value* ctx) {
		// Type-aware less than: checks types and uses fast path for integers

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_lt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_lt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "lt_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform comparison: value1 < value2
			llvm::Value* cmpResult = builder->CreateICmpSLT(value1, value2, "lt_result");
			// Convert bool to i64 (0 or 1)
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* ltFn = module->getFunction("qd_lt");
			if (!ltFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				ltFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_lt", *module);
			}
			builder->CreateCall(ltFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareGt(llvm::Value* ctx) {
		// Type-aware greater than: similar to lt

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_gt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_gt", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "gt_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform comparison: value1 > value2
			llvm::Value* cmpResult = builder->CreateICmpSGT(value1, value2, "gt_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* gtFn = module->getFunction("qd_gt");
			if (!gtFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				gtFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_gt", *module);
			}
			builder->CreateCall(gtFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareEq(llvm::Value* ctx) {
		// Type-aware equality: checks types and uses fast path for integers

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_eq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_eq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "eq_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Perform comparison: value1 == value2
			llvm::Value* cmpResult = builder->CreateICmpEQ(value1, value2, "eq_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* eqFn = module->getFunction("qd_eq");
			if (!eqFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				eqFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_eq", *module);
			}
			builder->CreateCall(eqFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareNeq(llvm::Value* ctx) {
		// Type-aware not-equal: similar to eq but with !=

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_neq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_neq", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "neq_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* cmpResult = builder->CreateICmpNE(value1, value2, "neq_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* neqFn = module->getFunction("qd_neq");
			if (!neqFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				neqFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_neq", *module);
			}
			builder->CreateCall(neqFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareLte(llvm::Value* ctx) {
		// Type-aware less than or equal

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_lte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_lte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "lte_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* cmpResult = builder->CreateICmpSLE(value1, value2, "lte_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* lteFn = module->getFunction("qd_lte");
			if (!lteFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				lteFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_lte", *module);
			}
			builder->CreateCall(lteFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareGte(llvm::Value* ctx) {
		// Type-aware greater than or equal

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_gte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_gte", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "gte_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			llvm::Value* cmpResult = builder->CreateICmpSGE(value1, value2, "gte_result");
			llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");

			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* gteFn = module->getFunction("qd_gte");
			if (!gteFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				gteFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_gte", *module);
			}
			builder->CreateCall(gteFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareDiv(llvm::Value* ctx) {
		// Type-aware division

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_div", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_div", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "div_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Check for division by zero
			llvm::Value* isZero = builder->CreateICmpEQ(value2, builder->getInt64(0), "divisor_is_zero");
			llvm::BasicBlock* divOkBlock =
					llvm::BasicBlock::Create(*context, "div_ok", builder->GetInsertBlock()->getParent());

			// If zero, jump to slow path (which has error handling); otherwise proceed
			builder->CreateCondBr(isZero, slowPath, divOkBlock);

			// Continue division in divOkBlock
			builder->SetInsertPoint(divOkBlock);

			llvm::Value* result = builder->CreateSDiv(value1, value2, "div_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* divFn = module->getFunction("qd_div");
			if (!divFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				divFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_div", *module);
			}
			builder->CreateCall(divFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareMod(llvm::Value* ctx) {
		// Type-aware modulo

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

		llvm::Value* idx2 = builder->CreateSub(size, builder->getInt64(1), "idx2");
		llvm::Value* elem2Ptr = builder->CreateGEP(stackElementTy, data, idx2, "elem2_ptr");

		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		llvm::BasicBlock* fastPath =
				llvm::BasicBlock::Create(*context, "fast_int_mod", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* slowPath =
				llvm::BasicBlock::Create(*context, "slow_mod", builder->GetInsertBlock()->getParent());
		llvm::BasicBlock* endBlock =
				llvm::BasicBlock::Create(*context, "mod_end", builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, fastPath, slowPath);

		builder->SetInsertPoint(fastPath);
		{
			llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, elem1Ptr, 0, "value1_ptr");
			llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

			llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, elem2Ptr, 0, "value2_ptr");
			llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
			llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

			// Check for division by zero
			llvm::Value* isZero = builder->CreateICmpEQ(value2, builder->getInt64(0), "divisor_is_zero");
			llvm::BasicBlock* modOkBlock =
					llvm::BasicBlock::Create(*context, "mod_ok", builder->GetInsertBlock()->getParent());

			// If zero, jump to slow path (which has error handling); otherwise proceed
			builder->CreateCondBr(isZero, slowPath, modOkBlock);

			// Continue modulo in modOkBlock
			builder->SetInsertPoint(modOkBlock);

			llvm::Value* result = builder->CreateSRem(value1, value2, "mod_result");
			builder->CreateStore(result, value1iPtrCast);

			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(slowPath);
		{
			llvm::Function* modFn = module->getFunction("qd_mod");
			if (!modFn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				modFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_mod", *module);
			}
			builder->CreateCall(modFn, {ctx});
			builder->CreateBr(endBlock);
		}

		builder->SetInsertPoint(endBlock);
	}

} // namespace Qd
