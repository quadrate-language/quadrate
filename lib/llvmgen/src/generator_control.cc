#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::generateIf(AstNodeIfStatement* ifStmt, llvm::Value* ctx) {
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

		// Convert to condition value
		auto condValue = builder->CreateTrunc(value64, int32Ty, "cond");

		// Check if condition is non-zero
		auto isTrue = builder->CreateICmpNE(condValue, builder->getInt32(0), "is_true");

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

		// Fast path for integer-only functions: inline pops, no type checking
		if (currentFunctionIsIntegerOnly) {
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
		loopStack.push_back({loopExitBB, loopIncBB});

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

		// Loop increment (nsw enables loop optimizations like strength reduction)
		builder->SetInsertPoint(loopIncBB);
		auto nextIter = builder->CreateNSWAdd(iterVar, stepValue, "next_i");
		iterVar->addIncoming(nextIter, loopIncBB);
		builder->CreateBr(loopHeaderBB);

		// Continue after loop
		builder->SetInsertPoint(loopExitBB);
	}

	void LlvmGenerator::Impl::generateWhile(AstNodeWhileStatement* whileStmt, llvm::Value* ctx) {
		// Get current function
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();

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

		// Convert to condition value
		auto condValue = builder->CreateTrunc(value64, int32Ty, "cond");

		// Check if condition is non-zero
		auto isTrue = builder->CreateICmpNE(condValue, builder->getInt32(0), "is_true");

		// Branch based on condition with weights (loop body is likely)
		llvm::MDBuilder mdBuilder(*context);
		llvm::MDNode* branchWeights = mdBuilder.createBranchWeights(1000, 1);
		auto* br = builder->CreateCondBr(isTrue, whileBodyBB, whileExitBB);
		br->setMetadata(llvm::LLVMContext::MD_prof, branchWeights);

		// While body
		builder->SetInsertPoint(whileBodyBB);

		// Push loop context for break/continue
		// break jumps to whileExitBB, continue jumps back to whileCondBB
		loopStack.push_back({whileExitBB, whileCondBB});

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

		// Create basic blocks
		llvm::BasicBlock* loopBodyBB = llvm::BasicBlock::Create(*context, "loop.body", currentFn);
		llvm::BasicBlock* loopExitBB = llvm::BasicBlock::Create(*context, "loop.exit", currentFn);

		// Jump to loop body
		builder->CreateBr(loopBodyBB);

		// Loop body
		builder->SetInsertPoint(loopBodyBB);

		// Push loop context for break/continue
		loopStack.push_back({loopExitBB, loopBodyBB});

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
				builder->CreateBr(loopStack.back().continueTarget);
			}
			break;
		case IAstNode::Type::RETURN_STATEMENT:
			// Return from current function
			if (currentFunctionReturnBlock) {
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
