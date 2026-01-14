#include "generator_impl.h"

namespace Qd {

	LlvmGenerator::Impl::TypeAwareOpContext LlvmGenerator::Impl::setupTypeAwareOp(
			llvm::Value* ctx, const char* opName) {
		TypeAwareOpContext toc;
		auto sa = getStackAccess(ctx);
		toc.sizePtr = sa.sizePtr;
		toc.size = sa.size;

		// Get pointers to top two elements
		llvm::Value* idx1 = builder->CreateSub(toc.size, builder->getInt64(2), "idx1");
		toc.elem1Ptr = builder->CreateGEP(stackElementTy, sa.data, idx1, "elem1_ptr");
		llvm::Value* idx2 = builder->CreateSub(toc.size, builder->getInt64(1), "idx2");
		toc.elem2Ptr = builder->CreateGEP(stackElementTy, sa.data, idx2, "elem2_ptr");

		// Load types of both elements
		llvm::Value* type1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 1, "type1_ptr");
		llvm::Value* type1 = builder->CreateLoad(builder->getInt32Ty(), type1Ptr, "type1");

		llvm::Value* type2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 1, "type2_ptr");
		llvm::Value* type2 = builder->CreateLoad(builder->getInt32Ty(), type2Ptr, "type2");

		// Check if both are integers (QD_STACK_TYPE_INT = 0)
		llvm::Value* type1IsInt = builder->CreateICmpEQ(type1, builder->getInt32(0), "type1_is_int");
		llvm::Value* type2IsInt = builder->CreateICmpEQ(type2, builder->getInt32(0), "type2_is_int");
		llvm::Value* bothInts = builder->CreateAnd(type1IsInt, type2IsInt, "both_ints");

		// Create basic blocks
		std::string fastName = std::string("fast_int_") + opName;
		std::string slowName = std::string("slow_") + opName;
		std::string endName = std::string(opName) + "_end";
		toc.fastPath = llvm::BasicBlock::Create(*context, fastName, builder->GetInsertBlock()->getParent());
		toc.slowPath = llvm::BasicBlock::Create(*context, slowName, builder->GetInsertBlock()->getParent());
		toc.endBlock = llvm::BasicBlock::Create(*context, endName, builder->GetInsertBlock()->getParent());

		builder->CreateCondBr(bothInts, toc.fastPath, toc.slowPath);
		builder->SetInsertPoint(toc.fastPath);

		return toc;
	}

	void LlvmGenerator::Impl::finishTypeAwareOp(
			const TypeAwareOpContext& toc, llvm::Value* result, llvm::Value* resultPtr) {
		builder->CreateStore(result, resultPtr);
		llvm::Value* newSize = builder->CreateSub(toc.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, toc.sizePtr);
		builder->CreateBr(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareAdd(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "add");

		// Fast path: load values and add
		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateNSWAdd(value1, value2, "add_result");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		// Slow path
		builder->SetInsertPoint(toc.slowPath);
		builder->CreateCall(addFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareSub(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "sub");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateNSWSub(value1, value2, "sub_result");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		builder->CreateCall(subFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareMul(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "mul");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* result = builder->CreateNSWMul(value1, value2, "mul_result");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		builder->CreateCall(mulFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareLt(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "lt");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmpSLT(value1, value2, "lt_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* ltFn = module->getFunction("qd_lt");
		if (!ltFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			ltFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_lt", *module);
		}
		builder->CreateCall(ltFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareGt(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "gt");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmpSGT(value1, value2, "gt_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* gtFn = module->getFunction("qd_gt");
		if (!gtFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			gtFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_gt", *module);
		}
		builder->CreateCall(gtFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareEq(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "eq");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmpEQ(value1, value2, "eq_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* eqFn = module->getFunction("qd_eq");
		if (!eqFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			eqFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_eq", *module);
		}
		builder->CreateCall(eqFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareNeq(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "neq");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmpNE(value1, value2, "neq_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* neqFn = module->getFunction("qd_neq");
		if (!neqFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			neqFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_neq", *module);
		}
		builder->CreateCall(neqFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareLte(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "lte");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmpSLE(value1, value2, "lte_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* lteFn = module->getFunction("qd_lte");
		if (!lteFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			lteFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_lte", *module);
		}
		builder->CreateCall(lteFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareGte(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "gte");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		llvm::Value* cmpResult = builder->CreateICmpSGE(value1, value2, "gte_result");
		llvm::Value* result = builder->CreateZExt(cmpResult, builder->getInt64Ty(), "result_i64");
		finishTypeAwareOp(toc, result, value1iPtrCast);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* gteFn = module->getFunction("qd_gte");
		if (!gteFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			gteFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_gte", *module);
		}
		builder->CreateCall(gteFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareDiv(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "div");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Check for division by zero - jump to slow path if zero
		llvm::Value* isZero = builder->CreateICmpEQ(value2, builder->getInt64(0), "divisor_is_zero");
		llvm::BasicBlock* divOkBlock =
				llvm::BasicBlock::Create(*context, "div_ok", builder->GetInsertBlock()->getParent());
		builder->CreateCondBr(isZero, toc.slowPath, divOkBlock);

		builder->SetInsertPoint(divOkBlock);
		llvm::Value* result = builder->CreateSDiv(value1, value2, "div_result");
		builder->CreateStore(result, value1iPtrCast);
		llvm::Value* newSize = builder->CreateSub(toc.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, toc.sizePtr);
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* divFn = module->getFunction("qd_div");
		if (!divFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			divFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_div", *module);
		}
		builder->CreateCall(divFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

	void LlvmGenerator::Impl::generateTypeAwareMod(llvm::Value* ctx) {
		auto toc = setupTypeAwareOp(ctx, "mod");

		llvm::Value* value1Ptr = builder->CreateStructGEP(stackElementTy, toc.elem1Ptr, 0, "value1_ptr");
		llvm::Value* value1iPtrCast = builder->CreateBitCast(value1Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value1 = builder->CreateLoad(builder->getInt64Ty(), value1iPtrCast, "value1");

		llvm::Value* value2Ptr = builder->CreateStructGEP(stackElementTy, toc.elem2Ptr, 0, "value2_ptr");
		llvm::Value* value2iPtrCast = builder->CreateBitCast(value2Ptr, llvm::PointerType::get(*context, 0));
		llvm::Value* value2 = builder->CreateLoad(builder->getInt64Ty(), value2iPtrCast, "value2");

		// Check for modulo by zero - jump to slow path if zero
		llvm::Value* isZero = builder->CreateICmpEQ(value2, builder->getInt64(0), "divisor_is_zero");
		llvm::BasicBlock* modOkBlock =
				llvm::BasicBlock::Create(*context, "mod_ok", builder->GetInsertBlock()->getParent());
		builder->CreateCondBr(isZero, toc.slowPath, modOkBlock);

		builder->SetInsertPoint(modOkBlock);
		llvm::Value* result = builder->CreateSRem(value1, value2, "mod_result");
		builder->CreateStore(result, value1iPtrCast);
		llvm::Value* newSize = builder->CreateSub(toc.size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, toc.sizePtr);
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.slowPath);
		llvm::Function* modFn = module->getFunction("qd_mod");
		if (!modFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			modFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_mod", *module);
		}
		builder->CreateCall(modFn, {ctx});
		builder->CreateBr(toc.endBlock);

		builder->SetInsertPoint(toc.endBlock);
	}

} // namespace Qd
