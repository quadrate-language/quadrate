// Instruction generation for LLVM code generator
// Extracted from generator_nodes.cc for maintainability

#include "generator_impl.h"

namespace Qd {

	void LlvmGenerator::Impl::generateInstruction(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& name = inst->name();

		// A local that shares a builtin's name is the local
		if (generateLocalPush(name, ctx)) {
			return;
		}
		if (useCompileTimeStack && generateCompileTimeInstruction(inst, ctx)) {
			return;
		}
		if (inst->isMethodCall() && generateInstructionMethodCall(inst, ctx)) {
			return;
		}
		if (name == "cast" && inst->hasTypeParam()) {
			generateCast(inst, ctx);
			return;
		}
		if (name == "sizeof" && inst->hasTypeParam()) {
			// A struct is its inline size; anything else is what a field of that type holds
			const StructLayout* layout = findStructDefinition(inst->typeParam());
			const size_t size = layout != nullptr ? layout->totalSize : getTypeSize(inst->typeParam());
			generateInlinePushInt(ctx, static_cast<int64_t>(size));
			return;
		}

		// The two forms of an operator: inlined on i64 values in a function known to hold only
		// those, and type-aware otherwise. `runtime` is the runtime function to call instead:
		// for an operator with no type-aware form, and under `-g` when `debugCall` says so.
		struct RuntimeOperator {
			const char* name;
			void (Impl::*integerOnly)(llvm::Value*);
			void (Impl::*typeAware)(llvm::Value*);
			const char* runtime;
			bool debugCall;
		};

		static const RuntimeOperator RUNTIME_OPERATORS[] = {
				{"+", &Impl::generateInlineIntAdd, &Impl::generateTypeAwareAdd, "qd_add", true},
				{"add", &Impl::generateInlineIntAdd, &Impl::generateTypeAwareAdd, "qd_add", true},
				{"-", &Impl::generateInlineIntSub, &Impl::generateTypeAwareSub, "qd_sub", true},
				{"sub", &Impl::generateInlineIntSub, &Impl::generateTypeAwareSub, "qd_sub", true},
				{"*", &Impl::generateInlineIntMul, &Impl::generateTypeAwareMul, "qd_mul", true},
				{"mul", &Impl::generateInlineIntMul, &Impl::generateTypeAwareMul, "qd_mul", true},
				{"/", nullptr, &Impl::generateTypeAwareDiv, nullptr, false},
				{"div", nullptr, &Impl::generateTypeAwareDiv, nullptr, false},
				{"%", &Impl::generateInlineIntMod, &Impl::generateTypeAwareMod, nullptr, false},
				{"mod", &Impl::generateInlineIntMod, &Impl::generateTypeAwareMod, nullptr, false},
				{"<", &Impl::generateInlineIntLt, &Impl::generateTypeAwareLt, nullptr, false},
				{"lt", &Impl::generateInlineIntLt, &Impl::generateTypeAwareLt, nullptr, false},
				{">", &Impl::generateInlineIntGt, &Impl::generateTypeAwareGt, nullptr, false},
				{"gt", &Impl::generateInlineIntGt, &Impl::generateTypeAwareGt, nullptr, false},
				{"==", &Impl::generateInlineIntEq, &Impl::generateTypeAwareEq, nullptr, false},
				{"eq", &Impl::generateInlineIntEq, &Impl::generateTypeAwareEq, nullptr, false},
				{"!=", &Impl::generateInlineIntNeq, &Impl::generateTypeAwareNeq, nullptr, false},
				{"neq", &Impl::generateInlineIntNeq, &Impl::generateTypeAwareNeq, nullptr, false},
				{"<=", &Impl::generateInlineIntLte, &Impl::generateTypeAwareLte, nullptr, false},
				{"lte", &Impl::generateInlineIntLte, &Impl::generateTypeAwareLte, nullptr, false},
				{">=", &Impl::generateInlineIntGte, &Impl::generateTypeAwareGte, nullptr, false},
				{"gte", &Impl::generateInlineIntGte, &Impl::generateTypeAwareGte, nullptr, false},
				{"and", &Impl::generateInlineLogicalAnd, nullptr, "qd_land", false},
				{"or", &Impl::generateInlineLogicalOr, nullptr, "qd_lor", false},
				{"not", &Impl::generateInlineLogicalNot, nullptr, "qd_lnot", false},
				{"__and", &Impl::generateInlineBitAnd, nullptr, "qd_and", false},
				{"__or", &Impl::generateInlineBitOr, nullptr, "qd_or", false},
				{"__xor", &Impl::generateInlineBitXor, nullptr, "qd_xor", false},
				{"__not", &Impl::generateInlineBitNot, nullptr, "qd_not", false},
				{"shl", &Impl::generateInlineBitLshift, nullptr, "qd_shl", false},
				{"shr", &Impl::generateInlineBitRshift, nullptr, "qd_shr", false},
		};

		auto pop = [this, ctx]() { return generateInlinePopInt(ctx); };
		auto push = [this, ctx](llvm::Value* value) { generateInlinePushIntValue(ctx, value); };
		if (generateMachineInstruction(name, pop, push)) {
			return;
		}

		for (const RuntimeOperator& op : RUNTIME_OPERATORS) {
			if (name != op.name) {
				continue;
			}
			if (debugInfoEnabled && op.debugCall) {
				builder->CreateCall(declareRuntimeFunction(op.runtime), {ctx});
			} else if (currentFunctionIsIntegerOnly && op.integerOnly != nullptr) {
				(this->*op.integerOnly)(ctx);
			} else if (op.typeAware != nullptr) {
				(this->*op.typeAware)(ctx);
			} else {
				builder->CreateCall(declareRuntimeFunction(op.runtime), {ctx});
			}
			return;
		}

		if (name == "swap") {
			// Moves elements and nothing else, so no reference counting is involved
			generateInlineSwap(ctx);
		} else if (name == "dup") {
			// Strings and pointers are reference counted, which only the runtime's dup handles
			if (currentFunctionIsIntegerOnly) {
				generateInlineDup(ctx);
			} else {
				builder->CreateCall(declareRuntimeFunction("qd_dup"), {ctx});
			}
		} else if (name == "clone") {
			generateClone(inst, ctx);
		} else {
			generateRuntimeInstruction(inst, ctx);
		}
	}

	// The runtime function `name`, int name(qd_context*), declared on first use
	llvm::Function* LlvmGenerator::Impl::declareRuntimeFunction(const char* name) {
		llvm::Function* fn = module->getFunction(name);
		if (!fn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, name, *module);
		}
		return fn;
	}

	// A method that shares a builtin's name, which the validator resolved. False when it named
	// no receiver type, and the name is the builtin after all.
	bool LlvmGenerator::Impl::generateInstructionMethodCall(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& name = inst->name();
		const std::string& receiverType = inst->receiverType();
		if (!receiverType.empty()) {
			// Generate method call. The identifier path uses the userFunctions map
			// which is populated with both qualified and unqualified method keys
			// (see generator.cc:812-817). We use the same lookup so method calls
			// that were parsed as AstNodeInstruction (because their name collides
			// with a builtin instruction, e.g. "dup", "over", "len") resolve
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
				return true;
			}

			// Casts, and the checks an `any` argument needs, before the receiver moves
			generateCastInstructions(inst->parameterCasts(), methodKey, true, ctx);
			const bool fallible = inst->calleeFallible();
			if (fallible) {
				generateClearErrorState(ctx);
			}

			generateReceiverRoll(inst->methodReceiverPositionFromTop(), ctx);

			builder->CreateCall(methodFn, {ctx});
			if (fallible) {
				generateFallibleCallEpilogue(ctx, name.c_str(), inst, inst->abortOnError(), inst->propagateOnError());
			}
			return true;
		}
		return false;
	}

	// `cast<T>`
	void LlvmGenerator::Impl::generateCast(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& typeParam = inst->typeParam();

		// Compile-time stack path for numeric casts
		if (useCompileTimeStack) {
			llvm::Value* val = compileTimeStack.back();
			compileTimeStack.pop_back();
			if (typeParam == "f64") {
				if (!val->getType()->isDoubleTy()) {
					val = builder->CreateSIToFP(val, builder->getDoubleTy(), "cast_f64");
				}
			} else if (typeParam == "i64") {
				if (val->getType()->isDoubleTy()) {
					val = builder->CreateFPToSI(val, int64Ty, "cast_i64");
				}
			}
			compileTimeStack.push_back(val);
			return;
		}

		// The validator lets through only i64, f64, d128, str and ptr
		std::string fnName = "qd_casts";
		switch (valueKind(typeParam)) {
		case ValueKind::Int:
			fnName = "qd_casti";
			break;
		case ValueKind::Float:
			fnName = "qd_castf";
			break;
		case ValueKind::D128:
			fnName = "qd_castd";
			break;
		case ValueKind::Ptr:
			fnName = "qd_castp";
			break;
		case ValueKind::Str:
		case ValueKind::Any:
			break;
		}

		llvm::Function* castFn = module->getFunction(fnName);
		if (!castFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			castFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
		}
		builder->CreateCall(castFn, {ctx});
		return;
	}

	// `clone`: a shallow copy of a struct
	void LlvmGenerator::Impl::generateClone(AstNodeInstruction* inst, llvm::Value* ctx) {
		// The struct type is not on the stack -- what is popped is a bare pointer -- so the
		// validator recorded it on the node and it picks the cloner out here. An empty one
		// means the validator already reported that this is not a struct.
		const std::string& structName = inst->resolvedType();
		llvm::Function* cloner = structName.empty() ? nullptr : findStructCloner(structName);
		if (cloner == nullptr) {
			// Never silently: leaving the value alone makes `a clone -> b` bind the original,
			// so the two names share a struct the program was told they would not. An empty
			// name means the validator already reported that this is not a struct.
			if (!structName.empty()) {
				std::cerr << "Error: no cloner for struct type: " << structName << std::endl;
			}
			return;
		}

		llvm::Function* qdPopPFn = module->getFunction("qd_pop_p");
		if (!qdPopPFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, ptrTy}, false);
			qdPopPFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_pop_p", *module);
		}
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::IRBuilder<> entryBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
		llvm::Value* slot = entryBuilder.CreateAlloca(ptrTy, nullptr, "clone_src");
		builder->CreateCall(qdPopPFn, {ctx, slot});
		llvm::Value* src = builder->CreateLoad(ptrTy, slot, "clone_src_ptr");
		llvm::Value* copy = builder->CreateCall(cloner, {src}, "clone_result");
		// The stack held a reference to the original and this consumed it, the same way
		// printing a value does. Pushing a local retains it, so without this the original
		// ends the scope one owner up and is never freed -- which is what a struct with no
		// refcounted fields at all still leaked.
		builder->CreateCall(qdPtrReleaseFn, {src});
		builder->CreateCall(pushPtrFn, {ctx, copy});

		// The copy is the same struct type as the original, so whatever follows -- `<<field`,
		// `-> name`, a method call -- resolves against it the way it would after `nth`.
		lastFieldAccessResultType = structName;
		lastStructConstructed = structName;
		lastPushedWasArray = false;
		return;
	}

	// Any other builtin: a call to its runtime function
	void LlvmGenerator::Impl::generateRuntimeInstruction(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& name = inst->name();
		// Map instruction name to runtime function name
		// `++` and `--` are inc and dec; every operator with a symbol of its own is handled above
		const std::string fnName = name == "++" ? "qd_inc" : name == "--" ? "qd_dec" : "qd_" + name;

		llvm::Function* runtimeFn = declareRuntimeFunction(fnName.c_str());

		// `call` on a fallible function pointer is a fallible call site, and gets the same
		// epilogue a fallible call by name gets: stale error state cleared first, so the check
		// after it is about this call. Without it `call!` ran on and the program carried on
		// past a panic with nothing on the stack -- true for a pointer to a fallible named
		// function long before a lambda could be marked `!`.
		const bool fallibleCall = name == "call" && inst->calleeFallible();
		if (fallibleCall) {
			generateClearErrorState(ctx);
		}

		builder->CreateCall(runtimeFn, {ctx});

		if (fallibleCall) {
			generateFallibleCallEpilogue(ctx, "call", inst, inst->abortOnError(), inst->propagateOnError());
		}

		// Special handling for 'panic' instruction in fallible functions
		// After calling qd_panic, we need to return immediately to prevent further execution
		if (name == "panic" && currentFunctionIsFallible && currentFunctionReturnBlock) {
			builder->CreateBr(currentFunctionReturnBlock);
		}

		// `append` leaves the array it appended to
		if (name == "append") {
			lastPushedWasArray = true;
		}

		// `arr i nth` leaves an element, and its type is what a following `<<field` or
		// `-> name` reads. Nothing here can work it out -- by the time `nth` runs the array
		// is two pushes back and which name put it there is not recoverable -- so the
		// validator records it on the node.
		//
		// Without it, a field access on the element fell through to the by-name search over
		// every struct in the module: it took whichever one declares a field of that name
		// and computed the offset from *its* layout. With `struct Aaa { p:f64  x:f64 }`
		// alongside `struct Bbb { x:f64  y:f64 }`, `c 0 nth <<x` on a `[]Bbb` parameter read
		// Aaa's offset for `x` and printed the value of Bbb's `y` -- silently, and decided
		// by map order, so adding a struct could change an unrelated function's answer.
		if (name == "nth") {
			const std::string& element = inst->resolvedType();
			if (!element.empty()) {
				lastFieldAccessResultType = isKnownStruct(element) ? element : std::string();
				lastStructConstructed = element;
				lastPushedWasArray = isArrayType(element);
			}
		}
	}

	// The instructions that lower to machine operations rather than a runtime call: raw memory
	// loads and stores, x86 port I/O and CPU control. Both stacks use them, so the operands come
	// from `pop` and a result goes to `push`. False when `name` is none of them.
	bool LlvmGenerator::Impl::generateMachineInstruction(const std::string& name,
			const std::function<llvm::Value*()>& pop, const std::function<void(llvm::Value*)>& push) {
		// addr offset value __st8/__st16/__st32/__st64
		if (name == "__st8" || name == "__st16" || name == "__st32" || name == "__st64") {
			unsigned bits = (name == "__st8") ? 8 : (name == "__st16") ? 16 : (name == "__st32") ? 32 : 64;
			llvm::Value* value = pop();
			llvm::Value* offset = pop();
			llvm::Value* addr = pop();
			llvm::Value* effective = builder->CreateAdd(addr, offset);
			llvm::Value* ptr = builder->CreateIntToPtr(effective, ptrTy);
			llvm::Type* storeTy = llvm::Type::getIntNTy(*context, bits);
			llvm::Value* truncated = (bits == 64) ? value : builder->CreateTrunc(value, storeTy);
			builder->CreateAlignedStore(truncated, ptr, llvm::Align(1));
			return true;
		}
		// addr offset __ld8/__ld16/__ld32/__ld64 -- value, zero-extended to i64
		if (name == "__ld8" || name == "__ld16" || name == "__ld32" || name == "__ld64") {
			unsigned bits = (name == "__ld8") ? 8 : (name == "__ld16") ? 16 : (name == "__ld32") ? 32 : 64;
			llvm::Value* offset = pop();
			llvm::Value* addr = pop();
			llvm::Value* effective = builder->CreateAdd(addr, offset);
			llvm::Value* ptr = builder->CreateIntToPtr(effective, ptrTy);
			llvm::Type* loadTy = llvm::Type::getIntNTy(*context, bits);
			llvm::Value* loaded = builder->CreateAlignedLoad(loadTy, ptr, llvm::Align(1));
			push((bits == 64) ? loaded : builder->CreateZExt(loaded, int64Ty));
			return true;
		}

		const bool portOut = name == "__port_out8" || name == "__port_out16" || name == "__port_out32";
		const bool portIn = name == "__port_in8" || name == "__port_in16" || name == "__port_in32";
		const bool cpuControl = name == "__cli" || name == "__sti" || name == "__hlt";
		if (!portOut && !portIn && !cpuControl) {
			return false;
		}
		llvm::Triple triple(module->getTargetTriple());
		if (triple.getArch() != llvm::Triple::x86 && triple.getArch() != llvm::Triple::x86_64) {
			llvm::errs() << "Error: '" << name << "' requires an x86 target\n";
			compilationFailed = true;
			return true;
		}
		if (cpuControl) {
			const char* mnemonic = name.c_str() + 2; // strip the __ prefix
			auto* asmFnTy = llvm::FunctionType::get(builder->getVoidTy(), {}, false);
			builder->CreateCall(llvm::InlineAsm::get(asmFnTy, mnemonic, "", /*hasSideEffects=*/true), {});
			return true;
		}
		const char last = name.back();
		const unsigned bits = last == '8' ? 8 : last == '6' ? 16 : 32;
		llvm::Type* int16Ty = llvm::Type::getInt16Ty(*context);
		llvm::Type* valTy = llvm::Type::getIntNTy(*context, bits);
		if (portOut) {
			// port value __port_out8/__port_out16/__port_out32
			llvm::Value* value = pop();
			llvm::Value* port = pop();
			llvm::Value* truncPort = builder->CreateTrunc(port, int16Ty, "port16");
			llvm::Value* truncVal = builder->CreateTrunc(value, valTy, "val");
			const char* asmStr = (bits == 8) ? "outb %al, %dx" : (bits == 16) ? "outw %ax, %dx" : "outl %eax, %dx";
			const char* regStr = (bits == 8)	? "{al},{dx},~{dirflag},~{fpsr},~{flags}"
								 : (bits == 16) ? "{ax},{dx},~{dirflag},~{fpsr},~{flags}"
												: "{eax},{dx},~{dirflag},~{fpsr},~{flags}";
			auto* asmFnTy = llvm::FunctionType::get(builder->getVoidTy(), {valTy, int16Ty}, false);
			builder->CreateCall(
					llvm::InlineAsm::get(asmFnTy, asmStr, regStr, /*hasSideEffects=*/true), {truncVal, truncPort});
			return true;
		}
		// port __port_in8/__port_in16/__port_in32 -- value
		llvm::Value* port = pop();
		llvm::Value* truncPort = builder->CreateTrunc(port, int16Ty, "port16");
		const char* asmStr = (bits == 8) ? "inb %dx, %al" : (bits == 16) ? "inw %dx, %ax" : "inl %dx, %eax";
		const char* regStr = (bits == 8)	? "={al},{dx},~{dirflag},~{fpsr},~{flags}"
							 : (bits == 16) ? "={ax},{dx},~{dirflag},~{fpsr},~{flags}"
											: "={eax},{dx},~{dirflag},~{fpsr},~{flags}";
		auto* asmFnTy = llvm::FunctionType::get(valTy, {int16Ty}, false);
		llvm::Value* result = builder->CreateCall(
				llvm::InlineAsm::get(asmFnTy, asmStr, regStr, /*hasSideEffects=*/true), {truncPort});
		push(builder->CreateZExt(result, int64Ty, "zext"));
		return true;
	}

	// The compile-time stack's form of an instruction, inside a native function. False when it
	// has none, and the instruction goes to the runtime stack. What it covers is what
	// isCompileTimeStackInstruction lists.
	bool LlvmGenerator::Impl::generateCompileTimeInstruction(AstNodeInstruction* inst, llvm::Value* ctx) {
		const std::string& name = inst->name();

		// Arithmetic: an i64 operand meeting an f64 one is promoted to f64
		struct Arithmetic {
			const char* name;
			const char* alias;
			char op;
		};

		static const Arithmetic ARITHMETIC[] = {
				{"+", "add", '+'}, {"-", "sub", '-'}, {"*", "mul", '*'}, {"/", "div", '/'}, {"%", "mod", '%'}};
		for (const Arithmetic& arith : ARITHMETIC) {
			if (name != arith.name && name != arith.alias) {
				continue;
			}
			auto [a, b] = popPromotedOperands();
			if (a->getType()->isDoubleTy()) {
				switch (arith.op) {
				case '+':
					compileTimeStack.push_back(builder->CreateFAdd(a, b, "fadd"));
					break;
				case '-':
					compileTimeStack.push_back(builder->CreateFSub(a, b, "fsub"));
					break;
				case '*':
					compileTimeStack.push_back(builder->CreateFMul(a, b, "fmul"));
					break;
				case '/':
					compileTimeStack.push_back(builder->CreateFDiv(a, b, "fdiv"));
					break;
				default:
					compileTimeStack.push_back(builder->CreateFRem(a, b, "fmod"));
					break;
				}
				return true;
			}
			switch (arith.op) {
			case '+':
				compileTimeStack.push_back(builder->CreateNSWAdd(a, b, "add"));
				break;
			case '-':
				compileTimeStack.push_back(builder->CreateNSWSub(a, b, "sub"));
				break;
			case '*':
				compileTimeStack.push_back(builder->CreateNSWMul(a, b, "mul"));
				break;
			case '/':
				emitDivisorZeroCheck(ctx, b, "div");
				compileTimeStack.push_back(emitWrappingSDiv(a, b));
				break;
			default:
				emitDivisorZeroCheck(ctx, b, "mod");
				compileTimeStack.push_back(emitWrappingSRem(a, b));
				break;
			}
			return true;
		}

		// Comparisons, promoted the same way, always leave an i64
		struct Comparison {
			const char* name;
			const char* alias;
			llvm::CmpInst::Predicate floatPredicate;
			llvm::CmpInst::Predicate intPredicate;
			const char* label;
		};

		static const Comparison COMPARISONS[] = {
				{"<", "lt", llvm::CmpInst::FCMP_OLT, llvm::CmpInst::ICMP_SLT, "lt"},
				{">", "gt", llvm::CmpInst::FCMP_OGT, llvm::CmpInst::ICMP_SGT, "gt"},
				{"==", "eq", llvm::CmpInst::FCMP_OEQ, llvm::CmpInst::ICMP_EQ, "eq"},
				{"!=", "neq", llvm::CmpInst::FCMP_ONE, llvm::CmpInst::ICMP_NE, "neq"},
				{"<=", "lte", llvm::CmpInst::FCMP_OLE, llvm::CmpInst::ICMP_SLE, "lte"},
				{">=", "gte", llvm::CmpInst::FCMP_OGE, llvm::CmpInst::ICMP_SGE, "gte"},
		};
		for (const Comparison& cmp : COMPARISONS) {
			if (name != cmp.name && name != cmp.alias) {
				continue;
			}
			auto [a, b] = popPromotedOperands();
			const std::string label = cmp.label;
			llvm::Value* result = a->getType()->isDoubleTy()
										  ? builder->CreateFCmp(cmp.floatPredicate, a, b, "f" + label)
										  : builder->CreateICmp(cmp.intPredicate, a, b, label);
			compileTimeStack.push_back(builder->CreateZExt(result, int64Ty, label + "_i64"));
			return true;
		}

		// Logical operations: every non-zero value counts as true, so both operands are
		// narrowed to 0/1 before combining. The bitwise forms are the __ ones below.
		if (name == "and" || name == "or") {
			llvm::Value* b = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* aTrue = builder->CreateICmpNE(a, builder->getInt64(0), "a_true");
			llvm::Value* bTrue = builder->CreateICmpNE(b, builder->getInt64(0), "b_true");
			llvm::Value* r =
					name == "and" ? builder->CreateAnd(aTrue, bTrue, "land") : builder->CreateOr(aTrue, bTrue, "lor");
			compileTimeStack.push_back(builder->CreateZExt(r, int64Ty, name.c_str()));
			return true;
		}
		if (name == "not") {
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* isZero = builder->CreateICmpEQ(a, builder->getInt64(0), "is_zero");
			compileTimeStack.push_back(builder->CreateZExt(isZero, int64Ty, "not"));
			return true;
		}
		// Bitwise operations (integer-only, no float changes needed)
		if (name == "__and" || name == "__or" || name == "__xor") {
			llvm::Value* b = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* r = name == "__and"  ? builder->CreateAnd(a, b, "and")
							 : name == "__or" ? builder->CreateOr(a, b, "or")
											  : builder->CreateXor(a, b, "xor");
			compileTimeStack.push_back(r);
			return true;
		}
		if (name == "__not") {
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			compileTimeStack.push_back(builder->CreateNot(a, "not"));
			return true;
		}
		if (name == "shl") {
			llvm::Value* b = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			compileTimeStack.push_back(emitCheckedShift(ctx, a, b, true, "shl"));
			return true;
		}
		if (name == "shr") {
			llvm::Value* b = compileTimeStack.back();
			compileTimeStack.pop_back();
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			// Logical, not arithmetic. `shr` treats its operand as unsigned everywhere else
			// -- generateInlineBitRshift uses CreateLShr, and both runtimes cast to uint64_t
			// before shifting. Using CreateAShr here made the same `v shr n` expression yield
			// a different result depending on whether it happened to be folded.
			compileTimeStack.push_back(emitCheckedShift(ctx, a, b, false, "shr"));
			return true;
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
			return true;
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
			return true;
		}
		if (name == "neg") {
			llvm::Value* a = compileTimeStack.back();
			compileTimeStack.pop_back();
			if (a->getType()->isDoubleTy()) {
				compileTimeStack.push_back(builder->CreateFNeg(a, "fneg"));
			} else {
				compileTimeStack.push_back(builder->CreateNeg(a, "neg"));
			}
			return true;
		}
		// Stack operations
		if (name == "dup") {
			compileTimeStack.push_back(compileTimeStack.back());
			return true;
		}
		if (name == "dup2") {
			size_t sz = compileTimeStack.size();
			compileTimeStack.push_back(compileTimeStack[sz - 2]);
			compileTimeStack.push_back(compileTimeStack[sz - 1]);
			return true;
		}
		if (name == "swap") {
			size_t sz = compileTimeStack.size();
			std::swap(compileTimeStack[sz - 1], compileTimeStack[sz - 2]);
			return true;
		}
		if (name == "drop") {
			compileTimeStack.pop_back();
			return true;
		}
		if (name == "over") {
			size_t sz = compileTimeStack.size();
			compileTimeStack.push_back(compileTimeStack[sz - 2]);
			return true;
		}
		if (name == "rot") {
			size_t sz = compileTimeStack.size();
			llvm::Value* a = compileTimeStack[sz - 3];
			compileTimeStack[sz - 3] = compileTimeStack[sz - 2];
			compileTimeStack[sz - 2] = compileTimeStack[sz - 1];
			compileTimeStack[sz - 1] = a;
			return true;
		}
		if (name == "nip") {
			size_t sz = compileTimeStack.size();
			compileTimeStack[sz - 2] = compileTimeStack[sz - 1];
			compileTimeStack.pop_back();
			return true;
		}
		if (name == "pick") {
			size_t sz = compileTimeStack.size();
			compileTimeStack.push_back(compileTimeStack[sz - 3]);
			return true;
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
			return true;
		}
		if (name == "prints") {
			builder->CreateCall(printsFn, {ctx});
			return true;
		}
		if (name == "nl") {
			builder->CreateCall(nlFn, {ctx});
			return true;
		}

		auto pop = [this]() {
			llvm::Value* value = compileTimeStack.back();
			compileTimeStack.pop_back();
			return value;
		};
		auto push = [this](llvm::Value* value) { compileTimeStack.push_back(value); };
		return generateMachineInstruction(name, pop, push);
	}

	// The top two compile-time values, a below b, with an i64 promoted to f64 when the other is one
	std::pair<llvm::Value*, llvm::Value*> LlvmGenerator::Impl::popPromotedOperands() {
		llvm::Value* b = compileTimeStack.back();
		compileTimeStack.pop_back();
		llvm::Value* a = compileTimeStack.back();
		compileTimeStack.pop_back();
		const bool aIsDouble = a->getType()->isDoubleTy();
		const bool bIsDouble = b->getType()->isDoubleTy();
		if (aIsDouble && !bIsDouble) {
			b = builder->CreateSIToFP(b, builder->getDoubleTy(), "promo_b");
		} else if (!aIsDouble && bIsDouble) {
			a = builder->CreateSIToFP(a, builder->getDoubleTy(), "promo_a");
		}
		return {a, b};
	}

} // namespace Qd
