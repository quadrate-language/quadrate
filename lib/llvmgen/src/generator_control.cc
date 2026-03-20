#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx) {
		// Compile-time stack path
		if (useCompileTimeStack) {
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

			llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "if.then", currentFn);
			llvm::BasicBlock* elseBB =
					ifStmt->elseBody() ? llvm::BasicBlock::Create(*context, "if.else", currentFn) : nullptr;
			llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "if.merge", currentFn);

			// Pop condition from compile-time stack
			llvm::Value* condition = compileTimeStack.back();
			compileTimeStack.pop_back();
			auto isTrue = builder->CreateICmpNE(condition, builder->getInt64(0), "is_true");

			// Remember the block where the branch is emitted (needed as else predecessor when no else block)
			llvm::BasicBlock* condBlock = builder->GetInsertBlock();

			if (elseBB) {
				builder->CreateCondBr(isTrue, thenBB, elseBB);
			} else {
				builder->CreateCondBr(isTrue, thenBB, mergeBB);
			}

			// Save compile-time stack state before then block
			auto savedStack = compileTimeStack;

			// Generate then block
			builder->SetInsertPoint(thenBB);
			if (ifStmt->thenBody()) {
				generateNode(ifStmt->thenBody(), ctx);
			}
			auto thenStack = compileTimeStack;
			llvm::BasicBlock* thenExitBlock = builder->GetInsertBlock();
			bool thenTerminated = (thenExitBlock != nullptr && thenExitBlock->getTerminator() != nullptr);
			if (!thenTerminated) {
				builder->CreateBr(mergeBB);
			}

			// Generate else block
			llvm::BasicBlock* elseExitBlock = condBlock; // Default: condition block (no else)
			auto elseStack = savedStack;				 // Default: unchanged if no else
			bool elseTerminated = false;
			if (elseBB) {
				compileTimeStack = savedStack;
				builder->SetInsertPoint(elseBB);
				if (ifStmt->elseBody()) {
					generateNode(ifStmt->elseBody(), ctx);
				}
				elseStack = compileTimeStack;
				elseExitBlock = builder->GetInsertBlock();
				elseTerminated = (elseExitBlock != nullptr && elseExitBlock->getTerminator() != nullptr);
				if (!elseTerminated) {
					builder->CreateBr(mergeBB);
				}
			}

			// Merge block - create PHI nodes for differing stack values
			builder->SetInsertPoint(mergeBB);

			if (thenTerminated && elseTerminated) {
				// Both branches terminated - merge block is unreachable
				compileTimeStack = thenStack;
			} else if (thenTerminated) {
				// Only then terminated - use else values
				compileTimeStack = elseStack;
			} else if (elseTerminated) {
				// Only else terminated - use then values
				compileTimeStack = thenStack;
			} else {
				// Both branches reach merge - need PHI nodes
				llvm::BasicBlock* thenPred = thenExitBlock;
				llvm::BasicBlock* elsePred = elseExitBlock;

				// Merge stack values with PHI nodes
				size_t mergeSize = std::min(thenStack.size(), elseStack.size());
				compileTimeStack.clear();
				for (size_t i = 0; i < mergeSize; i++) {
					if (thenStack[i] == elseStack[i]) {
						compileTimeStack.push_back(thenStack[i]);
					} else {
						llvm::Type* stkType = thenStack[i]->getType();
						auto* phi = builder->CreatePHI(stkType, 2, "merge");
						phi->addIncoming(thenStack[i], thenPred);
						phi->addIncoming(elseStack[i], elsePred);
						compileTimeStack.push_back(phi);
					}
				}
			}

			return;
		}

		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Create basic blocks
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "if.underflow", currentFn);
		llvm::BasicBlock* popBB = llvm::BasicBlock::Create(*context, "if.pop", currentFn);
		llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "if.then", currentFn);
		llvm::BasicBlock* elseBB =
				ifStmt->elseBody() ? llvm::BasicBlock::Create(*context, "if.else", currentFn) : nullptr;
		llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "if.merge", currentFn);

		// Inline pop for condition - direct stack access for performance
		// ctx is a pointer to a struct with first field being qd_stack* st
		llvm::Value* stPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(ptrTy, stPtr, "st");

		llvm::Value* sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(int64Ty, sizePtr, "size");

		// Check for stack underflow before popping
		llvm::Value* isEmpty = builder->CreateICmpEQ(size, builder->getInt64(0), "is_empty");
		builder->CreateCondBr(isEmpty, underflowBB, popBB);

		// Generate underflow error block
		builder->SetInsertPoint(underflowBB);
		emitFatalError(ctx, "Fatal error in if: Stack underflow (requires 1 value for condition)\n");

		// Continue with normal pop in popBB
		builder->SetInsertPoint(popBB);

		llvm::Value* dataPtr = builder->CreateStructGEP(stackStructTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(ptrTy, dataPtr, "data");

		// Access top element value directly: data[size-1].value
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value64 = builder->CreateLoad(int64Ty, valuePtr, "value64");

		// Decrement size (inline pop)
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		// Check if condition is non-zero (compare full i64, not truncated i32)
		auto isTrue = builder->CreateICmpNE(value64, builder->getInt64(0), "is_true");

		// Branch based on condition
		if (elseBB) {
			builder->CreateCondBr(isTrue, thenBB, elseBB);
		} else {
			builder->CreateCondBr(isTrue, thenBB, mergeBB);
		}

		// Generate then block
		builder->SetInsertPoint(thenBB);
		if (ifStmt->thenBody()) {
			generateNode(ifStmt->thenBody(), ctx);
		}
		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* thenBlock = builder->GetInsertBlock();
		if (thenBlock != nullptr && thenBlock->getTerminator() == nullptr) {
			builder->CreateBr(mergeBB);
		}

		// Generate else block if present
		if (elseBB) {
			builder->SetInsertPoint(elseBB);
			if (ifStmt->elseBody()) {
				generateNode(ifStmt->elseBody(), ctx);
			}
			// Only add branch if block doesn't already have a terminator
			llvm::BasicBlock* elseBlock = builder->GetInsertBlock();
			if (elseBlock != nullptr && elseBlock->getTerminator() == nullptr) {
				builder->CreateBr(mergeBB);
			}
		}

		// Continue in merge block
		builder->SetInsertPoint(mergeBB);
	}

	void LlvmGenerator::Impl::generateFor(AstNodeForStatement* forStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		llvm::Value* startValue;
		llvm::Value* endValue;
		llvm::Value* stepValue;

		// Compile-time stack path: pop start/end/step from compile-time stack
		if (useCompileTimeStack) {
			stepValue = compileTimeStack.back();
			compileTimeStack.pop_back();
			endValue = compileTimeStack.back();
			compileTimeStack.pop_back();
			startValue = compileTimeStack.back();
			compileTimeStack.pop_back();
		} else if (currentFunctionIsIntegerOnly) {
			// Fast path for integer-only functions: inline pops, no type checking
			// Pop start, end, step directly as integers (in reverse order: step, end, start)
			stepValue = generateInlinePopInt(ctx);
			endValue = generateInlinePopInt(ctx);
			startValue = generateInlinePopInt(ctx);
		} else {
			// Pop start, end, step from stack (in reverse order: step, end, start)
			auto stackFieldPtr = builder->CreateStructGEP(llvm::StructType::get(*context,
																  {
																		  ptrTy,   // qd_stack* st
																		  int64Ty, // int64_t error_code
																		  ptrTy,   // char* error_msg
																		  int32Ty, // int argc
																		  ptrTy,   // char** argv
																		  ptrTy	   // char* program_name
																  }),
					ctx, 0, "st_ptr");
			auto stack = builder->CreateLoad(ptrTy, stackFieldPtr, "st");

			// Create allocas in entry block to avoid stack growth in nested loops
			llvm::BasicBlock& entryBlock = currentFn->getEntryBlock();
			llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.getFirstInsertionPt());
			auto stepElemPtr = entryBuilder.CreateAlloca(stackElementTy, nullptr, "step_elem");
			auto endElemPtr = entryBuilder.CreateAlloca(stackElementTy, nullptr, "end_elem");
			auto startElemPtr = entryBuilder.CreateAlloca(stackElementTy, nullptr, "start_elem");

			builder->CreateCall(stackPopFn, {stack, stepElemPtr});
			builder->CreateCall(stackPopFn, {stack, endElemPtr});
			builder->CreateCall(stackPopFn, {stack, startElemPtr});

			// Extract values (stackElementTy layout: { i64 value, i32 type, i1 is_error_tainted })
			// The i64 field is a union that holds either int or float bits
			// Type field: 0=INT, 1=FLOAT, 2=PTR, 3=STR

			// Check the type of start element to determine if we're using int or float loop
			auto startTypePtr = builder->CreateStructGEP(stackElementTy, startElemPtr, 1, "start_type_ptr");
			auto startType = builder->CreateLoad(int32Ty, startTypePtr, "start_type");
			auto isFloatLoop = builder->CreateICmpEQ(startType, builder->getInt32(1), "is_float_loop");

			// Extract start value
			auto startValuePtr = builder->CreateStructGEP(stackElementTy, startElemPtr, 0, "start_value_ptr");
			auto startBits = builder->CreateLoad(int64Ty, startValuePtr, "start_bits");

			// Convert start based on type
			auto startAsFloat = builder->CreateBitCast(startBits, builder->getDoubleTy(), "start_as_float");
			auto startFloatToInt = builder->CreateFPToSI(startAsFloat, int64Ty, "start_float_to_int");
			startValue = builder->CreateSelect(isFloatLoop, startFloatToInt, startBits, "start");

			// Extract end value
			auto endValuePtr = builder->CreateStructGEP(stackElementTy, endElemPtr, 0, "end_value_ptr");
			auto endBits = builder->CreateLoad(int64Ty, endValuePtr, "end_bits");

			auto endAsFloat = builder->CreateBitCast(endBits, builder->getDoubleTy(), "end_as_float");
			auto endFloatToInt = builder->CreateFPToSI(endAsFloat, int64Ty, "end_float_to_int");
			endValue = builder->CreateSelect(isFloatLoop, endFloatToInt, endBits, "end");

			// Extract step value
			auto stepValuePtr = builder->CreateStructGEP(stackElementTy, stepElemPtr, 0, "step_value_ptr");
			auto stepBits = builder->CreateLoad(int64Ty, stepValuePtr, "step_bits");

			auto stepAsFloat = builder->CreateBitCast(stepBits, builder->getDoubleTy(), "step_as_float");
			auto stepFloatToInt = builder->CreateFPToSI(stepAsFloat, int64Ty, "step_float_to_int");
			stepValue = builder->CreateSelect(isFloatLoop, stepFloatToInt, stepBits, "step");
		}

		// Compile-time stack path for the entire for loop structure
		if (useCompileTimeStack) {
			// Save pre-loop stack state (after start/end/step have been popped)
			auto preLoopStack = compileTimeStack;

			// Create basic blocks
			llvm::BasicBlock* loopHeaderBB = llvm::BasicBlock::Create(*context, "for.header", currentFn);
			llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "for.body", currentFn);
			llvm::BasicBlock* loopIncBB = llvm::BasicBlock::Create(*context, "for.inc", currentFn);
			llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "for.exit", currentFn);

			llvm::BasicBlock* preBB = builder->GetInsertBlock();
			builder->CreateBr(loopHeaderBB);

			// Loop header: create iterator PHI + stack PHIs
			builder->SetInsertPoint(loopHeaderBB);
			llvm::PHINode* iterVar = builder->CreatePHI(startValue->getType(), 2, "i");
			iterVar->addIncoming(startValue, preBB);

			// Create PHI nodes for each compile-time stack position (typed)
			std::vector<llvm::PHINode*> stackPHIs;
			compileTimeStack.clear();
			for (size_t i = 0; i < preLoopStack.size(); i++) {
				llvm::Type* stkType = preLoopStack[i]->getType();
				auto* phi = builder->CreatePHI(stkType, 2, "stk");
				phi->addIncoming(preLoopStack[i], preBB);
				stackPHIs.push_back(phi);
				compileTimeStack.push_back(phi);
			}

			// Loop condition (type-aware for int/float iterators)
			llvm::Value* stepIsNegative;
			llvm::Value* condPositive;
			llvm::Value* condNegative;
			if (startValue->getType()->isDoubleTy()) {
				stepIsNegative = builder->CreateFCmpOLT(
						stepValue, llvm::ConstantFP::get(builder->getDoubleTy(), 0.0), "step_neg");
				condPositive = builder->CreateFCmpOLT(iterVar, endValue, "cmp_pos");
				condNegative = builder->CreateFCmpOGT(iterVar, endValue, "cmp_neg");
			} else {
				stepIsNegative = builder->CreateICmpSLT(stepValue, builder->getInt64(0), "step_neg");
				condPositive = builder->CreateICmpSLT(iterVar, endValue, "cmp_pos");
				condNegative = builder->CreateICmpSGT(iterVar, endValue, "cmp_neg");
			}
			auto cond = builder->CreateSelect(stepIsNegative, condNegative, condPositive, "cmp");

			llvm::MDBuilder mdBuilder(*context);
			llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
			auto* br = builder->CreateCondBr(cond, loopBodyBB, loopExitBB);
			br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

			// Loop body
			builder->SetInsertPoint(loopBodyBB);

			// Push loop context for break/continue
			loopStack.push_back({loopExitBB, loopIncBB, {}, {}});

			// Register iterator variable
			const std::string& iterName = forStmt->iteratorName();
			llvm::Value* prevIterVar = nullptr;
			auto prevIt = iteratorVars.find(iterName);
			if (prevIt != iteratorVars.end()) {
				prevIterVar = prevIt->second;
			}
			iteratorVars[iterName] = iterVar;

			if (forStmt->body()) {
				generateNode(forStmt->body(), ctx);
			}

			// Restore iterator
			if (prevIterVar) {
				iteratorVars[iterName] = prevIterVar;
			} else {
				iteratorVars.erase(iterName);
			}

			// Capture body-end state before branching to inc
			auto bodyEndStack = compileTimeStack;
			llvm::BasicBlock* bodyEndBlock = builder->GetInsertBlock();
			bool bodyFallsThrough = (bodyEndBlock != nullptr && bodyEndBlock->getTerminator() == nullptr);
			if (bodyFallsThrough) {
				builder->CreateBr(loopIncBB);
			}

			// Collect break/continue infos before popping loop context
			auto continueInfos = std::move(loopStack.back().continueInfos);
			auto breakInfos = std::move(loopStack.back().breakInfos);
			loopStack.pop_back();

			// Loop increment
			builder->SetInsertPoint(loopIncBB);

			// Merge compile-time stack from normal body end + continue states
			std::vector<llvm::Value*> incStack;
			unsigned numIncPreds = (bodyFallsThrough ? 1 : 0) + static_cast<unsigned>(continueInfos.size());
			for (size_t i = 0; i < stackPHIs.size(); i++) {
				llvm::Type* phiTy = stackPHIs[i]->getType();
				auto defaultVal = phiTy->isDoubleTy() ? static_cast<llvm::Value*>(llvm::ConstantFP::get(phiTy, 0.0))
													  : static_cast<llvm::Value*>(builder->getInt64(0));

				if (continueInfos.empty()) {
					// No continues - use body-end value directly (original path)
					incStack.push_back((i < bodyEndStack.size()) ? bodyEndStack[i] : defaultVal);
				} else if (numIncPreds == 1) {
					// Single predecessor - no PHI needed
					if (bodyFallsThrough) {
						incStack.push_back((i < bodyEndStack.size()) ? bodyEndStack[i] : defaultVal);
					} else {
						incStack.push_back(
								(i < continueInfos[0].stackState.size()) ? continueInfos[0].stackState[i] : defaultVal);
					}
				} else {
					// Multiple predecessors - merge with PHI
					auto* phi = builder->CreatePHI(phiTy, numIncPreds, "cont.merge");
					if (bodyFallsThrough) {
						phi->addIncoming((i < bodyEndStack.size()) ? bodyEndStack[i] : defaultVal, bodyEndBlock);
					}
					for (const auto& ci : continueInfos) {
						phi->addIncoming((i < ci.stackState.size()) ? ci.stackState[i] : defaultVal, ci.fromBlock);
					}
					incStack.push_back(phi);
				}
			}

			llvm::Value* nextIter;
			if (iterVar->getType()->isDoubleTy()) {
				nextIter = builder->CreateFAdd(iterVar, stepValue, "next_i");
			} else {
				nextIter = builder->CreateAdd(iterVar, stepValue, "next_i");
			}
			iterVar->addIncoming(nextIter, loopIncBB);

			// Wire stack PHI back-edges from inc block
			for (size_t i = 0; i < stackPHIs.size(); i++) {
				stackPHIs[i]->addIncoming(incStack[i], loopIncBB);
			}
			builder->CreateBr(loopHeaderBB);

			// Exit block
			builder->SetInsertPoint(loopExitBB);

			// Merge compile-time stack: header PHI values + break states
			if (breakInfos.empty()) {
				// No breaks - stack is just the header PHI values
				compileTimeStack.clear();
				for (auto* phi : stackPHIs) {
					compileTimeStack.push_back(phi);
				}
			} else {
				// Merge header values (from normal exit) with break states
				compileTimeStack.clear();
				for (size_t i = 0; i < stackPHIs.size(); i++) {
					bool allSame = true;
					for (const auto& bi : breakInfos) {
						if (i < bi.stackState.size() && bi.stackState[i] != stackPHIs[i]) {
							allSame = false;
							break;
						}
					}
					if (allSame) {
						compileTimeStack.push_back(stackPHIs[i]);
					} else {
						llvm::Type* phiTy = stackPHIs[i]->getType();
						auto* exitPhi =
								builder->CreatePHI(phiTy, static_cast<unsigned>(1 + breakInfos.size()), "exit.stk");
						exitPhi->addIncoming(stackPHIs[i], loopHeaderBB);
						for (const auto& bi : breakInfos) {
							llvm::Value* val;
							if (i < bi.stackState.size()) {
								val = bi.stackState[i];
							} else {
								val = phiTy->isDoubleTy() ? static_cast<llvm::Value*>(llvm::ConstantFP::get(phiTy, 0.0))
														  : builder->getInt64(0);
							}
							exitPhi->addIncoming(val, bi.fromBlock);
						}
						compileTimeStack.push_back(exitPhi);
					}
				}
			}

			return;
		}

		// Create basic blocks
		llvm::BasicBlock* loopHeaderBB = llvm::BasicBlock::Create(*context, "for.header", currentFn);
		llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "for.body", currentFn);
		llvm::BasicBlock* loopIncBB = llvm::BasicBlock::Create(*context, "for.inc", currentFn);
		llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "for.exit", currentFn);

		// Remember the predecessor block for PHI node
		llvm::BasicBlock* preBB = builder->GetInsertBlock();

		// Jump to loop header
		builder->CreateBr(loopHeaderBB);

		// Loop header: check condition
		builder->SetInsertPoint(loopHeaderBB);
		llvm::PHINode* iterVar = builder->CreatePHI(int64Ty, 2, "i");
		iterVar->addIncoming(startValue, preBB);

		// Check if step is negative to determine comparison direction
		auto stepIsNegative = builder->CreateICmpSLT(stepValue, builder->getInt64(0), "step_neg");
		auto condPositive = builder->CreateICmpSLT(iterVar, endValue, "cmp_pos");
		auto condNegative = builder->CreateICmpSGT(iterVar, endValue, "cmp_neg");
		auto cond = builder->CreateSelect(stepIsNegative, condNegative, condPositive, "cmp");

		// Add branch weights: loop body is likely (1000), exit is unlikely (1)
		llvm::MDBuilder mdBuilder(*context);
		llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
		auto* br = builder->CreateCondBr(cond, loopBodyBB, loopExitBB);
		br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

		// Loop body
		builder->SetInsertPoint(loopBodyBB);

		// Push loop context for break/continue
		loopStack.push_back({loopExitBB, loopIncBB, {}, {}});

		// Push defer scope for this loop - defers will be generated at end of iteration
		pushDeferScope();

		// Register the iterator variable with its name
		const std::string& iterName = forStmt->iteratorName();
		llvm::Value* prevIterVar = nullptr;
		auto prevIt = iteratorVars.find(iterName);
		if (prevIt != iteratorVars.end()) {
			prevIterVar = prevIt->second; // Save previous value for restoration
		}
		iteratorVars[iterName] = iterVar;

		if (forStmt->body()) {
			generateNode(forStmt->body(), ctx);
		}

		// Restore previous iterator value (for nested loops with same name)
		if (prevIterVar) {
			iteratorVars[iterName] = prevIterVar;
		} else {
			iteratorVars.erase(iterName);
		}

		// Generate defer execution code at end of loop body
		// This code will execute at runtime for each iteration
		if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
			auto& currentScope = deferScopeStack.back();
			// Generate IR to execute defers in REVERSE order (LIFO)
			for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				// Generate defer body inline
				for (auto* child : deferNode->children()) {
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (auto* innerChild : child->children()) {
							generateNode(innerChild, ctx);
						}
					} else {
						generateNode(child, ctx);
					}
				}
			}
		}

		// Pop defer scope (compilation-time cleanup)
		popDeferScope();

		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* loopBodyBlock = builder->GetInsertBlock();
		if (loopBodyBlock != nullptr && loopBodyBlock->getTerminator() == nullptr) {
			builder->CreateBr(loopIncBB);
		}

		loopStack.pop_back();

		// Loop increment (use plain add to avoid UB on overflow near INT64_MAX)
		builder->SetInsertPoint(loopIncBB);
		auto nextIter = builder->CreateAdd(iterVar, stepValue, "next_i");
		iterVar->addIncoming(nextIter, loopIncBB);
		builder->CreateBr(loopHeaderBB);

		// Continue after loop
		builder->SetInsertPoint(loopExitBB);
	}

	void LlvmGenerator::Impl::generateWhile(AstNodeWhileStatement* whileStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Compile-time stack path
		if (useCompileTimeStack) {
			// The condition is on top of the compile-time stack
			llvm::Value* initialCond = compileTimeStack.back();
			compileTimeStack.pop_back();
			auto preLoopStack = compileTimeStack;

			llvm::BasicBlock* whileCondBB = llvm::BasicBlock::Create(*context, "while.cond", currentFn);
			llvm::BasicBlock* whileBodyBB = llvm::BasicBlock::Create(*context, "while.body", currentFn);
			llvm::BasicBlock* whileExitBB = llvm::BasicBlock::Create(*context, "while.exit", currentFn);

			llvm::BasicBlock* preBB = builder->GetInsertBlock();
			builder->CreateBr(whileCondBB);

			// Condition block with PHIs
			builder->SetInsertPoint(whileCondBB);
			llvm::PHINode* condPhi = builder->CreatePHI(int64Ty, 2, "while.cond.phi");
			condPhi->addIncoming(initialCond, preBB);

			std::vector<llvm::PHINode*> stackPHIs;
			compileTimeStack.clear();
			for (size_t i = 0; i < preLoopStack.size(); i++) {
				llvm::Type* phiTy = preLoopStack[i]->getType();
				auto* phi = builder->CreatePHI(phiTy, 2, "while.stk");
				phi->addIncoming(preLoopStack[i], preBB);
				stackPHIs.push_back(phi);
				compileTimeStack.push_back(phi);
			}

			auto isTrue = builder->CreateICmpNE(condPhi, builder->getInt64(0), "is_true");
			llvm::MDBuilder mdBuilder(*context);
			llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
			auto* br = builder->CreateCondBr(isTrue, whileBodyBB, whileExitBB);
			br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

			// While body
			builder->SetInsertPoint(whileBodyBB);
			loopStack.push_back({whileExitBB, whileCondBB, {}, {}});

			if (whileStmt->body()) {
				generateNode(whileStmt->body(), ctx);
			}

			// Body should push new condition on stack
			llvm::Value* newCond = compileTimeStack.back();
			compileTimeStack.pop_back();
			auto bodyEndStack = compileTimeStack;

			llvm::BasicBlock* bodyEndBlock = builder->GetInsertBlock();
			bool bodyFallsThrough = (bodyEndBlock != nullptr && bodyEndBlock->getTerminator() == nullptr);
			if (bodyFallsThrough) {
				builder->CreateBr(whileCondBB);
			}

			// Wire PHI back-edges from normal body end
			if (bodyFallsThrough) {
				condPhi->addIncoming(newCond, bodyEndBlock);
				for (size_t i = 0; i < stackPHIs.size(); i++) {
					llvm::Value* val;
					if (i < bodyEndStack.size()) {
						val = bodyEndStack[i];
					} else {
						llvm::Type* stkTy = stackPHIs[i]->getType();
						val = stkTy->isDoubleTy()
									  ? static_cast<llvm::Value*>(llvm::ConstantFP::get(builder->getDoubleTy(), 0.0))
									  : static_cast<llvm::Value*>(builder->getInt64(0));
					}
					stackPHIs[i]->addIncoming(val, bodyEndBlock);
				}
			}

			// Wire PHI back-edges from continue states
			auto continueInfos = std::move(loopStack.back().continueInfos);
			for (const auto& ci : continueInfos) {
				// Continue in while loop: top of stack is new condition
				if (!ci.stackState.empty()) {
					condPhi->addIncoming(ci.stackState.back(), ci.fromBlock);
					for (size_t i = 0; i < stackPHIs.size(); i++) {
						llvm::Type* stkTy = stackPHIs[i]->getType();
						auto defaultVal = stkTy->isDoubleTy()
												  ? static_cast<llvm::Value*>(llvm::ConstantFP::get(stkTy, 0.0))
												  : static_cast<llvm::Value*>(builder->getInt64(0));
						stackPHIs[i]->addIncoming(
								(i < ci.stackState.size() - 1) ? ci.stackState[i] : defaultVal, ci.fromBlock);
					}
				}
			}

			auto breakInfos = std::move(loopStack.back().breakInfos);
			loopStack.pop_back();

			// Exit block
			builder->SetInsertPoint(whileExitBB);

			if (breakInfos.empty()) {
				compileTimeStack.clear();
				for (auto* phi : stackPHIs) {
					compileTimeStack.push_back(phi);
				}
			} else {
				compileTimeStack.clear();
				for (size_t i = 0; i < stackPHIs.size(); i++) {
					bool allSame = true;
					for (const auto& bi : breakInfos) {
						if (i < bi.stackState.size() && bi.stackState[i] != stackPHIs[i]) {
							allSame = false;
							break;
						}
					}
					if (allSame) {
						compileTimeStack.push_back(stackPHIs[i]);
					} else {
						llvm::Type* phiTy = stackPHIs[i]->getType();
						auto* exitPhi = builder->CreatePHI(
								phiTy, static_cast<unsigned>(1 + breakInfos.size()), "while.exit.stk");
						exitPhi->addIncoming(stackPHIs[i], whileCondBB);
						for (const auto& bi : breakInfos) {
							llvm::Value* val;
							if (i < bi.stackState.size()) {
								val = bi.stackState[i];
							} else {
								val = phiTy->isDoubleTy() ? static_cast<llvm::Value*>(
																	llvm::ConstantFP::get(builder->getDoubleTy(), 0.0))
														  : static_cast<llvm::Value*>(builder->getInt64(0));
							}
							exitPhi->addIncoming(val, bi.fromBlock);
						}
						compileTimeStack.push_back(exitPhi);
					}
				}
			}

			return;
		}

		// Create basic blocks
		llvm::BasicBlock* whileCondBB = llvm::BasicBlock::Create(*context, "while.cond", currentFn);
		llvm::BasicBlock* underflowBB = llvm::BasicBlock::Create(*context, "while.underflow", currentFn);
		llvm::BasicBlock* popBB = llvm::BasicBlock::Create(*context, "while.pop", currentFn);
		llvm::BasicBlock* whileBodyBB = llvm::BasicBlock::Create(*context, "while.body", currentFn);
		llvm::BasicBlock* whileExitBB = llvm::BasicBlock::Create(*context, "while.exit", currentFn);

		// Jump to condition check
		builder->CreateBr(whileCondBB);

		// Condition block - pop value and check
		builder->SetInsertPoint(whileCondBB);

		// Inline pop for condition - direct stack access for performance
		// ctx is a pointer to a struct with first field being qd_stack* st
		llvm::Value* stPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(ptrTy, stPtr, "st");

		llvm::Value* sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(int64Ty, sizePtr, "size");

		// Check for stack underflow before popping
		llvm::Value* isEmpty = builder->CreateICmpEQ(size, builder->getInt64(0), "is_empty");
		builder->CreateCondBr(isEmpty, underflowBB, popBB);

		// Generate underflow error block
		builder->SetInsertPoint(underflowBB);
		emitFatalError(ctx, "Fatal error in while: Stack underflow (requires 1 value for condition)\n");

		// Continue with normal pop in popBB
		builder->SetInsertPoint(popBB);

		llvm::Value* dataPtr = builder->CreateStructGEP(stackStructTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(ptrTy, dataPtr, "data");

		// Access top element value directly: data[size-1].value
		llvm::Value* topIdx = builder->CreateSub(size, builder->getInt64(1), "top_idx");
		llvm::Value* topElemPtr = builder->CreateGEP(stackElementTy, data, topIdx, "top_elem");
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, topElemPtr, 0, "value_ptr");
		llvm::Value* value64 = builder->CreateLoad(int64Ty, valuePtr, "value64");

		// Decrement size (inline pop)
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		// Check if condition is non-zero (compare full i64, not truncated i32)
		auto isTrue = builder->CreateICmpNE(value64, builder->getInt64(0), "is_true");

		// Branch based on condition with weights (loop body is likely)
		llvm::MDBuilder mdBuilder(*context);
		llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
		auto* br = builder->CreateCondBr(isTrue, whileBodyBB, whileExitBB);
		br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

		// While body
		builder->SetInsertPoint(whileBodyBB);

		// Push loop context for break/continue
		// break jumps to whileExitBB, continue jumps back to whileCondBB
		loopStack.push_back({whileExitBB, whileCondBB, {}, {}});

		// Push defer scope for this loop iteration
		pushDeferScope();

		if (whileStmt->body()) {
			generateNode(whileStmt->body(), ctx);
		}

		// Generate defer execution code at end of loop body
		if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
			auto& currentScope = deferScopeStack.back();
			for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				for (auto* child : deferNode->children()) {
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (auto* innerChild : child->children()) {
							generateNode(innerChild, ctx);
						}
					} else {
						generateNode(child, ctx);
					}
				}
			}
		}

		// Pop defer scope (compilation-time cleanup)
		popDeferScope();

		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* whileBlock = builder->GetInsertBlock();
		if (whileBlock != nullptr && whileBlock->getTerminator() == nullptr) {
			builder->CreateBr(whileCondBB); // Loop back to condition
		}

		loopStack.pop_back();

		// Continue after loop (reached when condition is false or via break)
		builder->SetInsertPoint(whileExitBB);
	}

	void LlvmGenerator::Impl::generateLoop(AstNodeLoopStatement* loopStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

		// Compile-time stack path
		if (useCompileTimeStack) {
			auto preLoopStack = compileTimeStack;

			llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "loop.body", currentFn);
			llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "loop.exit", currentFn);

			llvm::BasicBlock* preBB = builder->GetInsertBlock();
			builder->CreateBr(loopBodyBB);

			// Loop body with stack PHIs
			builder->SetInsertPoint(loopBodyBB);
			std::vector<llvm::PHINode*> stackPHIs;
			compileTimeStack.clear();
			for (size_t i = 0; i < preLoopStack.size(); i++) {
				llvm::Type* phiTy = preLoopStack[i]->getType();
				auto* phi = builder->CreatePHI(phiTy, 2, "loop.stk");
				phi->addIncoming(preLoopStack[i], preBB);
				stackPHIs.push_back(phi);
				compileTimeStack.push_back(phi);
			}

			loopStack.push_back({loopExitBB, loopBodyBB, {}, {}});

			if (loopStmt->body()) {
				generateNode(loopStmt->body(), ctx);
			}

			auto bodyEndStack = compileTimeStack;
			llvm::BasicBlock* bodyEndBlock = builder->GetInsertBlock();
			if (bodyEndBlock != nullptr && bodyEndBlock->getTerminator() == nullptr) {
				builder->CreateBr(loopBodyBB);
				// Wire stack PHI back-edges
				for (size_t i = 0; i < stackPHIs.size(); i++) {
					llvm::Value* val;
					if (i < bodyEndStack.size()) {
						val = bodyEndStack[i];
					} else {
						llvm::Type* stkTy = stackPHIs[i]->getType();
						val = stkTy->isDoubleTy()
									  ? static_cast<llvm::Value*>(llvm::ConstantFP::get(builder->getDoubleTy(), 0.0))
									  : static_cast<llvm::Value*>(builder->getInt64(0));
					}
					stackPHIs[i]->addIncoming(val, bodyEndBlock);
				}
			}

			// Wire stack PHI back-edges from continue states
			auto continueInfos = std::move(loopStack.back().continueInfos);
			for (const auto& ci : continueInfos) {
				for (size_t i = 0; i < stackPHIs.size(); i++) {
					llvm::Type* stkTy = stackPHIs[i]->getType();
					auto defaultVal = stkTy->isDoubleTy() ? static_cast<llvm::Value*>(llvm::ConstantFP::get(stkTy, 0.0))
														  : static_cast<llvm::Value*>(builder->getInt64(0));
					stackPHIs[i]->addIncoming((i < ci.stackState.size()) ? ci.stackState[i] : defaultVal, ci.fromBlock);
				}
			}

			auto breakInfos = std::move(loopStack.back().breakInfos);
			loopStack.pop_back();

			// Exit block - only reached via break
			builder->SetInsertPoint(loopExitBB);

			if (breakInfos.empty()) {
				// No breaks - loop is infinite with no exit; stack is meaningless
				compileTimeStack.clear();
				for (auto* phi : stackPHIs) {
					compileTimeStack.push_back(phi);
				}
			} else if (breakInfos.size() == 1) {
				// Single break - use its stack state directly
				compileTimeStack = breakInfos[0].stackState;
			} else {
				// Multiple breaks - merge with PHI nodes
				compileTimeStack.clear();
				size_t maxSize = 0;
				for (const auto& bi : breakInfos) {
					maxSize = std::max(maxSize, bi.stackState.size());
				}
				for (size_t i = 0; i < maxSize; i++) {
					bool allSame = true;
					llvm::Value* firstVal =
							(i < breakInfos[0].stackState.size()) ? breakInfos[0].stackState[i] : nullptr;
					for (size_t j = 1; j < breakInfos.size(); j++) {
						llvm::Value* val =
								(i < breakInfos[j].stackState.size()) ? breakInfos[j].stackState[i] : nullptr;
						if (val != firstVal) {
							allSame = false;
							break;
						}
					}
					if (allSame && firstVal) {
						compileTimeStack.push_back(firstVal);
					} else {
						llvm::Type* phiTy = (i < breakInfos[0].stackState.size() && breakInfos[0].stackState[i])
													? breakInfos[0].stackState[i]->getType()
													: int64Ty;
						auto* exitPhi =
								builder->CreatePHI(phiTy, static_cast<unsigned>(breakInfos.size()), "loop.exit.stk");
						for (const auto& bi : breakInfos) {
							llvm::Value* val;
							if (i < bi.stackState.size()) {
								val = bi.stackState[i];
							} else {
								val = phiTy->isDoubleTy() ? static_cast<llvm::Value*>(
																	llvm::ConstantFP::get(builder->getDoubleTy(), 0.0))
														  : static_cast<llvm::Value*>(builder->getInt64(0));
							}
							exitPhi->addIncoming(val, bi.fromBlock);
						}
						compileTimeStack.push_back(exitPhi);
					}
				}
			}

			return;
		}

		// Create basic blocks
		llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "loop.body", currentFn);
		llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "loop.exit", currentFn);

		// Jump to loop body
		builder->CreateBr(loopBodyBB);

		// Loop body
		builder->SetInsertPoint(loopBodyBB);

		// Push loop context for break/continue
		loopStack.push_back({loopExitBB, loopBodyBB, {}, {}});

		// Push defer scope for this loop iteration
		pushDeferScope();

		if (loopStmt->body()) {
			generateNode(loopStmt->body(), ctx);
		}

		// Generate defer execution code at end of loop body
		if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
			auto& currentScope = deferScopeStack.back();
			for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
				AstNodeDefer* deferNode = *it;
				for (auto* child : deferNode->children()) {
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (auto* innerChild : child->children()) {
							generateNode(innerChild, ctx);
						}
					} else {
						generateNode(child, ctx);
					}
				}
			}
		}

		// Pop defer scope (compilation-time cleanup)
		popDeferScope();

		// Only add branch if block doesn't already have a terminator
		llvm::BasicBlock* loopBlock = builder->GetInsertBlock();
		if (loopBlock != nullptr && loopBlock->getTerminator() == nullptr) {
			builder->CreateBr(loopBodyBB); // Loop forever
		}

		loopStack.pop_back();

		// Continue after loop (only reached via break)
		builder->SetInsertPoint(loopExitBB);
	}

	void LlvmGenerator::Impl::generateCtxBlock(AstNodeCtx* ctxNode, llvm::Value* ctx) {
		// Clone the parent context
		auto clonedCtx = builder->CreateCall(cloneContextFn, {ctx}, "cloned_ctx");

		// Execute the block with the cloned context
		for (auto* child : ctxNode->children()) {
			generateNode(child, clonedCtx);
		}

		// Get the stack from cloned context
		auto stackFieldPtr = builder->CreateStructGEP(llvm::StructType::get(*context,
															  {
																	  ptrTy,   // qd_stack* st
																	  int64Ty, // int64_t error_code
																	  ptrTy,   // char* error_msg
																	  int32Ty, // int argc
																	  ptrTy,   // char** argv
																	  ptrTy	   // char* program_name
															  }),
				clonedCtx, 0, "cloned_st_ptr");
		auto clonedStack = builder->CreateLoad(ptrTy, stackFieldPtr, "cloned_st");

		// Pop exactly one value from the cloned stack
		auto resultElemPtr = builder->CreateAlloca(stackElementTy, nullptr, "ctx_result_elem");
		builder->CreateCall(stackPopFn, {clonedStack, resultElemPtr});

		// Get the result value and type
		auto resultValuePtr = builder->CreateStructGEP(stackElementTy, resultElemPtr, 0, "result_value_ptr");
		auto resultValue = builder->CreateLoad(int64Ty, resultValuePtr, "result_value");
		auto resultTypePtr = builder->CreateStructGEP(stackElementTy, resultElemPtr, 1, "result_type_ptr");
		auto resultType = builder->CreateLoad(int32Ty, resultTypePtr, "result_type");

		// Push the result to the parent context based on its type BEFORE freeing cloned context
		// This is critical for strings: qd_push_s will duplicate the string, so we need
		// the original string to still be valid when we call it
		// Type field: 0=INT, 1=FLOAT, 2=PTR, 3=STR
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock* pushIntBB = llvm::BasicBlock::Create(*context, "ctx.push_int", currentFn);
		llvm::BasicBlock* pushFloatBB = llvm::BasicBlock::Create(*context, "ctx.push_float", currentFn);
		llvm::BasicBlock* pushPtrBB = llvm::BasicBlock::Create(*context, "ctx.push_ptr", currentFn);
		llvm::BasicBlock* pushStrBB = llvm::BasicBlock::Create(*context, "ctx.push_str", currentFn);
		llvm::BasicBlock* pushDoneBB = llvm::BasicBlock::Create(*context, "ctx.push_done", currentFn);

		// Switch on type
		auto switchInst = builder->CreateSwitch(resultType, pushDoneBB, 4);
		switchInst->addCase(builder->getInt32(0), pushIntBB);	// INT
		switchInst->addCase(builder->getInt32(1), pushFloatBB); // FLOAT
		switchInst->addCase(builder->getInt32(2), pushPtrBB);	// PTR
		switchInst->addCase(builder->getInt32(3), pushStrBB);	// STR

		// Push INT
		builder->SetInsertPoint(pushIntBB);
		builder->CreateCall(pushIntFn, {ctx, resultValue});
		builder->CreateBr(pushDoneBB);

		// Push FLOAT
		builder->SetInsertPoint(pushFloatBB);
		auto floatValue = builder->CreateBitCast(resultValue, builder->getDoubleTy(), "float_value");
		builder->CreateCall(pushFloatFn, {ctx, floatValue});
		builder->CreateBr(pushDoneBB);

		// Push PTR
		builder->SetInsertPoint(pushPtrBB);
		auto ptrValue = builder->CreateIntToPtr(resultValue, ptrTy, "ptr_value");
		builder->CreateCall(pushPtrFn, {ctx, ptrValue});
		builder->CreateBr(pushDoneBB);

		// Push STR
		builder->SetInsertPoint(pushStrBB);
		auto strPtr = builder->CreateIntToPtr(resultValue, ptrTy, "str_ptr");
		// Call qd_string_data to get const char* from qd_string_t*
		auto strData = builder->CreateCall(qdStringDataFn, {strPtr}, "str_data");
		builder->CreateCall(pushStrFn, {ctx, strData});
		// Release the string reference from cloned context (qd_push_s has created a new copy)
		builder->CreateCall(qdStringReleaseFn, {strPtr});
		builder->CreateBr(pushDoneBB);

		// Free the cloned context AFTER pushing (qd_push_s has now duplicated the string)
		builder->SetInsertPoint(pushDoneBB);
		builder->CreateCall(freeContextFn, {clonedCtx});

		// Continue after push
		builder->SetInsertPoint(pushDoneBB);
	}

	void LlvmGenerator::Impl::generateNode(IAstNode* node, llvm::Value* ctx) {
		if (!node) {
			return;
		}

		// Set debug location for this node
		if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty()) {
			unsigned line = static_cast<unsigned>(node->line());
			unsigned column = static_cast<unsigned>(node->column());
			if (line > 0) {
				auto debugLoc = llvm::DILocation::get(*context, line, column, debugScopeStack.back());
				builder->SetCurrentDebugLocation(debugLoc);
			}
		}

		auto nodeType = node->type();

		switch (nodeType) {
		case IAstNode::Type::LITERAL:
			generateLiteral(static_cast<AstNodeLiteral*>(node), ctx);
			break;
		case IAstNode::Type::INSTRUCTION:
			generateInstruction(static_cast<AstNodeInstruction*>(node), ctx);
			break;
		case IAstNode::Type::LOCAL:
			generateLocal(static_cast<AstNodeLocal*>(node), ctx);
			break;
		case IAstNode::Type::IF_STATEMENT:
			generateIf(static_cast<AstNodeIfStatement*>(node), ctx);
			break;
		case IAstNode::Type::FOR_STATEMENT:
			generateFor(static_cast<AstNodeForStatement*>(node), ctx);
			break;
		case IAstNode::Type::WHILE_STATEMENT:
			generateWhile(static_cast<AstNodeWhileStatement*>(node), ctx);
			break;
		case IAstNode::Type::LOOP_STATEMENT:
			generateLoop(static_cast<AstNodeLoopStatement*>(node), ctx);
			break;
		case IAstNode::Type::SWITCH_STATEMENT:
			generateSwitchStatement(static_cast<AstNodeSwitchStatement*>(node), ctx);
			break;
		case IAstNode::Type::BREAK_STATEMENT:
			// Execute defer scope before breaking from current loop
			if (!loopStack.empty()) {
				// Generate IR to execute defers before breaking
				if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
					auto& currentScope = deferScopeStack.back();
					for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
						AstNodeDefer* deferNode = *it;
						for (auto* child : deferNode->children()) {
							if (child && child->type() == IAstNode::Type::BLOCK) {
								for (auto* innerChild : child->children()) {
									generateNode(innerChild, ctx);
								}
							} else {
								generateNode(child, ctx);
							}
						}
					}
				}
				// Record break state for compile-time stack merging at loop exit
				if (useCompileTimeStack) {
					loopStack.back().breakInfos.push_back({builder->GetInsertBlock(), compileTimeStack});
				}
				builder->CreateBr(loopStack.back().breakTarget);
			}
			break;
		case IAstNode::Type::CONTINUE_STATEMENT:
			// Execute defer scope before continuing to next iteration
			if (!loopStack.empty()) {
				// Generate IR to execute defers before continuing
				if (!deferScopeStack.empty() && !deferScopeStack.back().empty()) {
					auto& currentScope = deferScopeStack.back();
					for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
						AstNodeDefer* deferNode = *it;
						for (auto* child : deferNode->children()) {
							if (child && child->type() == IAstNode::Type::BLOCK) {
								for (auto* innerChild : child->children()) {
									generateNode(innerChild, ctx);
								}
							} else {
								generateNode(child, ctx);
							}
						}
					}
				}
				// Record continue state for compile-time stack merging
				if (useCompileTimeStack) {
					loopStack.back().continueInfos.push_back({builder->GetInsertBlock(), compileTimeStack});
				}
				builder->CreateBr(loopStack.back().continueTarget);
			}
			break;
		case IAstNode::Type::RETURN_STATEMENT:
			// Return from current function — defers are executed in the return block itself
			if (currentFunctionReturnBlock) {
				// For native functions, store return value before branching
				if (useCompileTimeStack && nativeReturnAlloca && !compileTimeStack.empty()) {
					builder->CreateStore(compileTimeStack.back(), nativeReturnAlloca);
				}
				builder->CreateBr(currentFunctionReturnBlock);
			}
			break;
		case IAstNode::Type::DEFER_STATEMENT:
			// Collect defer statement for later execution at scope end
			if (deferScopeStack.empty()) {
				pushDeferScope();
			}
			deferScopeStack.back().push_back(static_cast<AstNodeDefer*>(node));
			// Don't generate code now - will be generated at scope end
			break;
		case IAstNode::Type::CTX_STATEMENT:
			generateCtxBlock(static_cast<AstNodeCtx*>(node), ctx);
			break;
		case IAstNode::Type::IDENTIFIER:
			generateIdentifier(static_cast<AstNodeIdentifier*>(node), ctx);
			break;
		case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
			generateFunctionPointer(static_cast<AstNodeFunctionPointerReference*>(node), ctx);
			break;
		case IAstNode::Type::ANONYMOUS_FUNCTION:
			generateAnonymousFunction(static_cast<AstNodeAnonymousFunction*>(node), ctx);
			break;
		case IAstNode::Type::BLOCK:
			// For blocks, just recursively generate all children
			for (auto* child : node->children()) {
				generateNode(child, ctx);
				// Stop if we've added a terminator (return, break, continue)
				llvm::BasicBlock* currentBlock = builder->GetInsertBlock();
				if (currentBlock != nullptr && currentBlock->getTerminator() != nullptr) {
					break;
				}
			}
			break;
		case IAstNode::Type::FUNCTION_DECLARATION:
			// Skip - functions are handled at the top level
			break;
		case IAstNode::Type::TEST_DECLARATION:
			// Skip - tests are handled at the top level
			break;
		case IAstNode::Type::SCOPED_IDENTIFIER:
			generateScopedIdentifier(static_cast<AstNodeScopedIdentifier*>(node), ctx);
			break;
		case IAstNode::Type::USE_STATEMENT:
			// Use statements are handled during program generation, not during execution
			break;
		case IAstNode::Type::FIELD_ACCESS:
			generateFieldAccess(static_cast<AstNodeFieldAccess*>(node), ctx);
			break;
		case IAstNode::Type::FIELD_SET:
			generateFieldSet(static_cast<AstNodeFieldSet*>(node), ctx);
			break;
		case IAstNode::Type::STRUCT_DECLARATION:
			// Struct declarations are processed during program generation, not during execution
			break;
		case IAstNode::Type::STRUCT_CONSTRUCTION: {
			// Struct construction with named fields: StructName { field1: expr1 field2: expr2 }
			AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(node);
			const std::string& structName = construct->structName();
			const auto& fieldInits = construct->fieldInits();

			// Special handling for anonymous error literal: error { code = X message = Y }
			// This just pushes message then code onto the stack (for panic to consume)
			if (structName == "__error__") {
				// Find message and code initializers
				const StructFieldInit* messageInit = nullptr;
				const StructFieldInit* codeInit = nullptr;
				for (const auto& init : fieldInits) {
					if (init.fieldName == "message") {
						messageInit = &init;
					} else if (init.fieldName == "code") {
						codeInit = &init;
					}
				}

				// Generate code for message first (goes on stack first)
				if (messageInit) {
					for (IAstNode* valueNode : messageInit->valueNodes) {
						generateNode(valueNode, ctx);
					}
				}

				// Then generate code for code (goes on top of stack)
				if (codeInit) {
					for (IAstNode* valueNode : codeInit->valueNodes) {
						generateNode(valueNode, ctx);
					}
				}
				break;
			}

			// Get struct layout to know field order (use helper to handle qualified names)
			const StructLayout* layoutPtr = findStructDefinition(structName);
			if (layoutPtr == nullptr) {
				std::cerr << "Error: Unknown struct type in construction: " << structName << std::endl;
				break;
			}

			const StructLayout& layout = *layoutPtr;

			// Build a map from field name to initializer
			std::unordered_map<std::string, const StructFieldInit*> initMap;
			for (const auto& init : fieldInits) {
				initMap[init.fieldName] = &init;
			}

			// Generate code for field initializers in struct definition order
			// (generateStructConstruction pops them in reverse order)
			for (const auto& field : layout.fields) {
				auto initIt = initMap.find(field.name);
				if (initIt != initMap.end()) {
					// Generate code for the field initializer expression
					for (IAstNode* valueNode : initIt->second->valueNodes) {
						generateNode(valueNode, ctx);
					}
				} else if (!field.defaultValue.empty()) {
					// Use default value for this field
					for (IAstNode* defaultNode : field.defaultValue) {
						generateNode(defaultNode, ctx);
					}
				} else {
					// Missing field with no default - should have been caught by semantic validator
					std::cerr << "Error: Missing field initializer for '" << field.name << "' in struct " << structName
							  << std::endl;
				}
			}

			// Now construct the struct (pops values from stack)
			// Use the layout name (which is qualified for module structs)
			generateStructConstruction(layout.name, ctx);
			break;
		}
		case IAstNode::Type::ARRAY_LITERAL:
			generateArrayLiteral(static_cast<AstNodeArrayLiteral*>(node), ctx);
			break;
		case IAstNode::Type::ARRAY_INDEX:
			// Array indexing is handled via 'nth' instruction
			break;
		default:
			// Ignore other node types for now
			break;
		}
	}

} // namespace Qd
