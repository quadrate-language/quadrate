#include "generator_impl.h"
#include <chrono>
#include <dlfcn.h>
#include <quadrate/qc/ast_node_type_alias.h>
#include <thread>

// Platform abstractions
extern "C" {
#include "src/platform/exe_path_platform.h"
}

// Check if a library import is a known Quadrate stdlib module (uses usr_ name mangling).
// This covers the renamed stdlib libraries (librt.a, libfmt.a, etc.) that no longer
// have the libqd* prefix. External modules installed via quadpm still use libqd* prefix
// and are handled separately.
static bool isStdlibImport(const std::string& library) {
	static const char* const stdlibLibs[] = {
			"librt.a",
			"libfmt.a",
			"libio.a",
			"libmath.a",
			"libmem.a",
			"libnet.a",
			"libos.a",
			"libsignal.a",
			"libstrings.a",
			"libstrconv.a",
			"libtime.a",
			"libthread.a",
			"libtesting.a",
			"libtty.a",
			"libbits.a",
			"libhttp.a",
			"libtls.a",
			"liblog.a",
	};
	for (const char* name : stdlibLibs) {
		if (library == name) {
			return true;
		}
	}
	return false;
}

namespace Qd {

	llvm::Value* LlvmGenerator::Impl::getOrCreateGlobalString(const std::string& str) {
		auto it = globalStringCache.find(str);
		if (it != globalStringCache.end()) {
			return it->second;
		}
		llvm::Value* globalStr = builder->CreateGlobalString(str);
		globalStringCache[str] = globalStr;
		return globalStr;
	}

	void LlvmGenerator::Impl::createForwardingWrapperBody(llvm::Function* wrapperFn, llvm::Function* targetFn) {
		auto entryBB = llvm::BasicBlock::Create(*context, "entry", wrapperFn);
		builder->SetInsertPoint(entryBB);
		auto ctx = wrapperFn->arg_begin();
		auto result = builder->CreateCall(targetFn, {ctx});
		builder->CreateRet(result);
	}

	void LlvmGenerator::Impl::setupRuntimeDeclarations() {
		// Cached primitive types - initialize all before use
		ptrTy = llvm::PointerType::getUnqual(*context);
		int64Ty = builder->getInt64Ty();
		int32Ty = builder->getInt32Ty();
		contextPtrTy = ptrTy; // alias for contextPtrTy

		// Use builder directly for struct creation to avoid any initialization ordering issues
		auto i32t = builder->getInt32Ty();
		auto i64t = builder->getInt64Ty();

		// exec_result is just an i32 (0 = success, non-zero = error)
		execResultTy = i32t;

		// qd_stack_element_t layout: { union(i64, double, ptr, ptr), i32 type, i8 is_error_tainted }
		stackElementTy = llvm::StructType::create(*context, {i64t, i32t, builder->getInt8Ty()}, "qd_stack_element_t");

		// Context layout: {qd_stack* st, int64_t error_code, char* error_msg, int argc, char** argv, char*
		// program_name}
		contextStructTy = llvm::StructType::create(*context, {ptrTy, i64t, ptrTy, i32t, ptrTy, ptrTy}, "qd_context_t");

		// Closure layout: {int64_t magic, ptr fn, ptr env, int64_t capture_count}
		closureStructTy = llvm::StructType::create(*context, {i64t, ptrTy, ptrTy, i64t}, "qd_closure_t");

		// Stack layout: {ptr data, int64_t capacity, int64_t size}
		stackStructTy = llvm::StructType::create(*context, {ptrTy, i64t, i64t}, "qd_stack_t");

		// Common function type signatures
		auto ctxToResultTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		auto ctxToVoidTy = llvm::FunctionType::get(builder->getVoidTy(), {contextPtrTy}, false);
		auto ptrToVoidTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
		auto ptrToPtrTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		auto ctxPtrToResultTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, ptrTy}, false);

		// Helper to declare external functions
		auto declareFn = [this](llvm::FunctionType* ty, const char* name) {
			auto fn = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, name, *module);
			// Runtime functions don't throw C++ exceptions
			fn->addFnAttr(llvm::Attribute::NoUnwind);
			return fn;
		};

		// Context management: (i64) -> ctx, (ctx) -> void, (ctx) -> ctx
		auto i64ToCtxTy = llvm::FunctionType::get(contextPtrTy, {int64Ty}, false);
		createContextFn = declareFn(i64ToCtxTy, "qd_create_context");
		freeContextFn = declareFn(ctxToVoidTy, "qd_free_context");
		cloneContextFn = declareFn(ptrToPtrTy, "qd_clone_context");

		// Push functions: (ctx, value) -> result
		auto ctxI64ToResultTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, int64Ty}, false);
		auto ctxF64ToResultTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, builder->getDoubleTy()}, false);
		pushIntFn = declareFn(ctxI64ToResultTy, "qd_push_i");
		pushFloatFn = declareFn(ctxF64ToResultTy, "qd_push_f");
		pushStrFn = declareFn(ctxPtrToResultTy, "qd_push_s");
		pushStrRefFn = declareFn(ctxPtrToResultTy, "qd_push_s_ref");
		pushPtrFn = declareFn(ctxPtrToResultTy, "qd_push_p");

		// Runtime operations: (ctx) -> result
		callFn = declareFn(ctxToResultTy, "qd_call");
		printsFn = declareFn(ctxToResultTy, "qd_prints");
		nlFn = declareFn(ctxToResultTy, "qd_nl");
		addFn = declareFn(ctxToResultTy, "qd_add");
		subFn = declareFn(ctxToResultTy, "qd_sub");
		mulFn = declareFn(ctxToResultTy, "qd_mul");
		andFn = declareFn(ctxToResultTy, "qd_and");
		orFn = declareFn(ctxToResultTy, "qd_or");
		xorFn = declareFn(ctxToResultTy, "qd_xor");
		notFn = declareFn(ctxToResultTy, "qd_not");
		shlFn = declareFn(ctxToResultTy, "qd_shl");
		shrFn = declareFn(ctxToResultTy, "qd_shr");

		// Call stack management
		auto pushCallFnTy = llvm::FunctionType::get(builder->getVoidTy(), {contextPtrTy, ptrTy, ptrTy, int64Ty}, false);
		pushCallFn = declareFn(pushCallFnTy, "qd_push_call");
		popCallFn = declareFn(ctxToVoidTy, "qd_pop_call");

		// Stack checking
		auto checkStackFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {contextPtrTy, int64Ty, ptrTy, ptrTy}, false);
		checkStackFn = declareFn(checkStackFnTy, "qd_check_stack");

		// Stack operations
		auto stackPopFnTy = llvm::FunctionType::get(int32Ty, {ptrTy, ptrTy}, false);
		auto ptrToI64Ty = llvm::FunctionType::get(int64Ty, {ptrTy}, false);
		stackPopFn = declareFn(stackPopFnTy, "qd_stack_pop");
		stackSizeFn = declareFn(ptrToI64Ty, "qd_stack_size");

		// C library functions
		auto i64ToPtrTy = llvm::FunctionType::get(ptrTy, {int64Ty}, false);
		strdupFn = declareFn(ptrToPtrTy, "strdup");
		mallocFn = declareFn(i64ToPtrTy, "malloc");
		freeFn = declareFn(ptrToVoidTy, "free");

		// Struct/pointer management
		auto allocFnTy = llvm::FunctionType::get(ptrTy, {int64Ty, ptrTy}, false);
		qdStructAllocFn = declareFn(allocFnTy, "qd_struct_alloc");
		qdStructReleaseFn = declareFn(ptrToVoidTy, "qd_struct_release");
		qdStructRetainFn = declareFn(ptrToPtrTy, "qd_struct_retain");
		qdPtrReleaseFn = declareFn(ptrToVoidTy, "qd_ptr_release");
		qdPtrRetainFn = declareFn(ptrToPtrTy, "qd_ptr_retain");

		// Error handling functions
		auto ptrPtrToVoidTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy, ptrTy}, false);
		// Use _exit(1) instead of abort() — abort() raises SIGABRT which
		// triggers Haiku's debug_server, hanging or interfering with output.
		auto i32ToVoidTy = llvm::FunctionType::get(builder->getVoidTy(), {int32Ty}, false);
		exitFn = declareFn(i32ToVoidTy, "_exit");
		printStackTraceFn = declareFn(ctxToVoidTy, "qd_print_stack_trace");
		printErrorMsgFn = declareFn(ptrPtrToVoidTy, "qd_print_error_msg");

		// String functions
		qdStringReleaseFn = declareFn(ptrToVoidTy, "qd_string_release");
		qdStringDataFn = declareFn(ptrToPtrTy, "qd_string_data");

		// Initialize debug info if enabled
		if (debugInfoEnabled) {
			debugBuilder = std::make_unique<llvm::DIBuilder>(*module);

			// Extract directory and filename from source path
			// Convert to absolute path for better debugger compatibility
			std::filesystem::path srcPath(sourceFileName);
			std::filesystem::path absPath = std::filesystem::absolute(srcPath);
			std::string directory = absPath.parent_path().string();
			std::string filename = absPath.filename().string();
			if (directory.empty()) {
				directory = ".";
			}

			// Create debug file with absolute path
			debugFile = debugBuilder->createFile(filename, directory);

			// Create compile unit
			compileUnit = debugBuilder->createCompileUnit(
					llvm::dwarf::DW_LANG_C99, // Use C99 as the base language (closest to stack machine)
					debugFile,
					"quadc", // Producer
					false,	 // isOptimized (set to false for debugging)
					"",		 // Flags
					0		 // Runtime version
			);

			// Set module flags for debug info
			module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
			module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);

			// Initialize scope stack with compile unit
			debugScopeStack.push_back(compileUnit);

			// Create DIFile objects for all modules
			for (const auto& pair : moduleSourceFiles) {
				const std::string& moduleName = pair.first;
				const std::string& moduleSourcePath = pair.second;

				std::filesystem::path moduleSrcPath(moduleSourcePath);
				std::filesystem::path moduleAbsPath = std::filesystem::absolute(moduleSrcPath);
				std::string moduleDirectory = moduleAbsPath.parent_path().string();
				std::string moduleFilename = moduleAbsPath.filename().string();
				if (moduleDirectory.empty()) {
					moduleDirectory = ".";
				}

				llvm::DIFile* moduleDIFile = debugBuilder->createFile(moduleFilename, moduleDirectory);
				moduleDebugFiles[moduleName] = moduleDIFile;
			}

			// Add main module to debug files map
			moduleDebugFiles["main"] = debugFile;

			// Create debug type info for runtime structures so GDB can inspect them
			// This is needed for JIT-compiled code where GDB can't access libqdrt's debug symbols

			// Basic types - store in member variables for use in local variable debug info
			auto int64Type = debugBuilder->createBasicType("int64_t", 64, llvm::dwarf::DW_ATE_signed);
			auto doubleType = debugBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
			auto boolType = debugBuilder->createBasicType("bool", 8, llvm::dwarf::DW_ATE_boolean);
			auto charType = debugBuilder->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);
			auto sizeType = debugBuilder->createBasicType("size_t", 64, llvm::dwarf::DW_ATE_unsigned);
			auto charPtrType = debugBuilder->createPointerType(charType, 64);
			auto voidPtrType = debugBuilder->createPointerType(nullptr, 64);

			// Store basic types for use in local variable debug info
			int64DebugType = int64Type;
			floatDebugType = doubleType;
			stringDebugType = charPtrType;

			// qd_stack_type enum (just use int32 for simplicity)
			auto stackTypeEnum = debugBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);

			// qd_stack_value union (inside qd_stack_element_t)
			llvm::SmallVector<llvm::Metadata*, 4> unionFields;
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "i", debugFile, 54, 64, 64, 0, llvm::DINode::FlagZero, int64Type));
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "f", debugFile, 55, 64, 64, 0, llvm::DINode::FlagZero, doubleType));
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "p", debugFile, 56, 64, 64, 0, llvm::DINode::FlagZero, voidPtrType));
			unionFields.push_back(debugBuilder->createMemberType(
					compileUnit, "s", debugFile, 57, 64, 64, 0, llvm::DINode::FlagZero, charPtrType));
			auto valueUnionType = debugBuilder->createUnionType(compileUnit, "qd_stack_value", debugFile, 53, 64, 64,
					llvm::DINode::FlagZero, debugBuilder->getOrCreateArray(unionFields));

			// qd_stack_element_t structure
			llvm::SmallVector<llvm::Metadata*, 3> elementFields;
			elementFields.push_back(debugBuilder->createMemberType(
					compileUnit, "value", debugFile, 58, 64, 64, 0, llvm::DINode::FlagZero, valueUnionType));
			elementFields.push_back(debugBuilder->createMemberType(
					compileUnit, "type", debugFile, 60, 32, 32, 64, llvm::DINode::FlagZero, stackTypeEnum));
			elementFields.push_back(debugBuilder->createMemberType(
					compileUnit, "is_error_tainted", debugFile, 61, 8, 8, 96, llvm::DINode::FlagZero, boolType));
			auto elementType = debugBuilder->createStructType(compileUnit, "qd_stack_element_t", debugFile, 52, 128, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(elementFields));
			stackElementDebugType = elementType; // Store for later use
			auto elementPtrType = debugBuilder->createPointerType(elementType, 64);

			// qd_stack structure
			llvm::SmallVector<llvm::Metadata*, 3> stackFields;
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "data", debugFile, 71, 64, 64, 0, llvm::DINode::FlagZero, elementPtrType));
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "capacity", debugFile, 72, 64, 64, 64, llvm::DINode::FlagZero, sizeType));
			stackFields.push_back(debugBuilder->createMemberType(
					compileUnit, "size", debugFile, 73, 64, 64, 128, llvm::DINode::FlagZero, sizeType));
			auto stackStructType = debugBuilder->createStructType(compileUnit, "qd_stack", debugFile, 70, 192, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(stackFields));
			auto stackPtrType = debugBuilder->createPointerType(stackStructType, 64);

			// qd_context structure
			llvm::SmallVector<llvm::Metadata*, 8> contextFields;
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "st", debugFile, 45, 64, 64, 0, llvm::DINode::FlagZero, stackPtrType));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "error_code", debugFile, 46, 64, 64, 64, llvm::DINode::FlagZero, int64Type));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "error_msg", debugFile, 47, 64, 64, 128, llvm::DINode::FlagZero, charPtrType));
			contextFields.push_back(debugBuilder->createMemberType(compileUnit, "argc", debugFile, 48, 32, 32, 192,
					llvm::DINode::FlagZero, debugBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed)));
			auto charPtrPtrType = debugBuilder->createPointerType(charPtrType, 64);
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "argv", debugFile, 49, 64, 64, 256, llvm::DINode::FlagZero, charPtrPtrType));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "program_name", debugFile, 50, 64, 64, 320, llvm::DINode::FlagZero, charPtrType));
			// call_stack is const char* [256] at offset 48 bytes (384 bits)
			auto callStackArrayType = debugBuilder->createArrayType(16384, // 256 * 64 bits
					64,													   // alignment
					charPtrType, debugBuilder->getOrCreateArray({debugBuilder->getOrCreateSubrange(0, 256)}));
			contextFields.push_back(debugBuilder->createMemberType(compileUnit, "call_stack", debugFile, 53, 16384, 64,
					384, llvm::DINode::FlagZero, callStackArrayType));
			contextFields.push_back(debugBuilder->createMemberType(
					compileUnit, "call_stack_depth", debugFile, 54, 64, 64, 16768, llvm::DINode::FlagZero, sizeType));

			auto contextStructType = debugBuilder->createStructType(compileUnit, "qd_context", debugFile, 44, 16832, 64,
					llvm::DINode::FlagZero, nullptr, debugBuilder->getOrCreateArray(contextFields));
			contextDebugType = debugBuilder->createPointerType(contextStructType, 64);
		}
	}

	// Analyze if function body uses only integer types
	// Returns true if the body contains no strings, floats, or module calls that might return non-integers
	// Collect all function calls from an AST node (for reachability analysis)
	void LlvmGenerator::Impl::collectCalledFunctions(
			IAstNode* node, const std::string& currentModule, std::set<std::string>& calledFunctions) {
		if (!node) {
			return;
		}

		// Check for scoped identifier (module::function call)
		if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			auto* scoped = static_cast<AstNodeScopedIdentifier*>(node);
			std::string qualifiedName = scoped->scope() + "::" + scoped->name();
			calledFunctions.insert(qualifiedName);
		}

		// Check for identifier (local function call or module function)
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			auto* ident = static_cast<AstNodeIdentifier*>(node);
			const std::string& name = ident->name();
			// Could be a local function or module function - add both possibilities
			calledFunctions.insert(name); // local
			if (!currentModule.empty()) {
				calledFunctions.insert(currentModule + "::" + name); // module-qualified
			}
		}

		// Check for instruction (could be a function call)
		if (node->type() == IAstNode::Type::INSTRUCTION) {
			auto* inst = static_cast<AstNodeInstruction*>(node);
			const std::string& name = inst->name();
			// Add as potential function call
			calledFunctions.insert(name);
			if (!currentModule.empty()) {
				calledFunctions.insert(currentModule + "::" + name);
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			collectCalledFunctions(child, currentModule, calledFunctions);
		}
	}

	// Compute all functions reachable from main
	void LlvmGenerator::Impl::computeReachableFunctions(IAstNode* mainRoot, std::set<std::string>& reachable) {
		// Start with functions called from main
		std::set<std::string> worklist;

		// Collect direct calls from main file
		for (auto* child : mainRoot->children()) {
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				// Mark main file functions as reachable
				std::string funcName = funcNode->name();
				reachable.insert(funcName);
				reachable.insert(mainModuleName + "::" + funcName);

				// Collect calls from function body
				if (funcNode->body()) {
					collectCalledFunctions(funcNode->body(), mainModuleName, worklist);
				}
			}
			if (auto testNode = dynamic_cast<AstNodeTest*>(child)) {
				// Test functions are reachable in test mode
				if (testNode->body()) {
					collectCalledFunctions(testNode->body(), mainModuleName, worklist);
				}
			}
		}

		// Process worklist until no new functions are found
		while (!worklist.empty()) {
			std::set<std::string> newWorklist;

			for (const std::string& funcName : worklist) {
				if (reachable.count(funcName)) {
					continue; // Already processed
				}
				reachable.insert(funcName);

				// Find the function definition in module ASTs
				for (const auto& modulePair : moduleASTs) {
					const std::string& moduleName = modulePair.first;
					IAstNode* moduleRoot = modulePair.second;
					if (!moduleRoot) {
						continue;
					}

					for (auto* child : moduleRoot->children()) {
						if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
							std::string qualifiedName = moduleName + "::" + funcNode->name();
							std::string methodName;
							if (funcNode->hasReceiver()) {
								methodName = moduleName + "::" + funcNode->receiverType() + "::" + funcNode->name();
							}

							// Check if this function matches what we're looking for
							if (funcName == qualifiedName || funcName == funcNode->name() || funcName == methodName ||
									(!methodName.empty() &&
											funcName == funcNode->receiverType() + "::" + funcNode->name())) {
								// Found the function - collect its calls
								if (funcNode->body()) {
									collectCalledFunctions(funcNode->body(), moduleName, newWorklist);
								}
							}
						}
					}
				}
			}

			worklist = std::move(newWorklist);
		}
	}

	void LlvmGenerator::Impl::collectLocalNames(IAstNode* node, std::set<std::string>& names) {
		if (!node) {
			return;
		}
		if (node->type() == IAstNode::Type::LOCAL) {
			auto* local = static_cast<AstNodeLocal*>(node);
			for (const auto& n : local->names()) {
				if (n != "_") {
					names.insert(n);
				}
			}
		}
		for (auto* child : node->children()) {
			collectLocalNames(child, names);
		}
	}

	bool LlvmGenerator::Impl::analyzeIsBodyNativeEligible(
			IAstNode* node, const std::set<std::string>& localNames, bool allowFloat) {
		if (!node) {
			return true;
		}

		// Check this node's type
		switch (node->type()) {
		case IAstNode::Type::LITERAL: {
			auto* lit = static_cast<AstNodeLiteral*>(node);
			// INTEGER literals always allowed; FLOAT only when allowFloat is true
			if (lit->literalType() != AstNodeLiteral::LiteralType::INTEGER &&
					lit->literalType() != AstNodeLiteral::LiteralType::BOOL &&
					(!allowFloat || lit->literalType() != AstNodeLiteral::LiteralType::FLOAT)) {
				return false;
			}
			break;
		}
		case IAstNode::Type::INSTRUCTION: {
			auto* inst = static_cast<AstNodeInstruction*>(node);
			const std::string& name = inst->name();
			// Reject instructions that work with non-integer types (strings, arrays, pointers)
			// Also reject dynamic stack ops (pick, roll) that can't be statically handled
			// But skip the check if the instruction name is actually a local variable reference
			// (the parser emits Instruction nodes for names like "len" even when they refer to locals)
			// Note: "cast" is allowed when allowFloat is true for compile-time stack i64<->f64 conversions
			if (localNames.find(name) == localNames.end()) {
				if ((!allowFloat && name == "cast") || name == "err" || name == "read" || name == "getenv" ||
						name == "make" || name == "set" || name == "nth" || name == "len" || name == "push_back" ||
						name == "pop_back" || name == "panic" || name == "sizeof" || name == "type" || name == "call" ||
						name == "pick" || name == "roll" || name == "depth" || name == "clear" || name == "within" ||
						name == "swap2" || name == "over2" || name == "nipd" || name == "swapd" || name == "dupd" ||
						name == "overd" || name == "free" || name == "peek") {
					return false;
				}
			}
			break;
		}
		case IAstNode::Type::CONTINUE_STATEMENT:
			break;
		case IAstNode::Type::DEFER_STATEMENT:
			// Defer blocks execute at function return, which conflicts with the
			// compile-time stack approach. Reject for now.
			return false;
		case IAstNode::Type::CTX_STATEMENT:
			// Ctx blocks clone the context and expect results on the runtime stack,
			// which conflicts with the compile-time stack approach.
			return false;
		case IAstNode::Type::SCOPED_IDENTIFIER: {
			auto* scoped = static_cast<AstNodeScopedIdentifier*>(node);
			std::string fullName = scoped->scope() + "::" + scoped->name();
			// Allow if a native bridge exists for this scoped function
			if (nativeFunctions.find(fullName) == nativeFunctions.end()) {
				return false;
			}
			break;
		}
		case IAstNode::Type::ANONYMOUS_FUNCTION:
			// Anonymous functions/closures are complex, reject for now
			return false;
		case IAstNode::Type::ARRAY_LITERAL:
		case IAstNode::Type::STRUCT_CONSTRUCTION:
		case IAstNode::Type::FIELD_ACCESS:
		case IAstNode::Type::FIELD_SET:
			// Arrays, structs, and field operations use pointers
			return false;
		default:
			break;
		}

		// Recursively check children
		for (auto* child : node->children()) {
			if (!analyzeIsBodyNativeEligible(child, localNames, allowFloat)) {
				return false;
			}
		}

		return true;
	}

	// Check if all user function calls in a body have native versions.
	// Used to verify a native function won't call non-native user functions.
	bool LlvmGenerator::Impl::analyzeCalleesAllNative(IAstNode* node) {
		if (!node) {
			return true;
		}
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			auto* ident = static_cast<AstNodeIdentifier*>(node);
			const std::string& name = ident->name();
			// Check if this identifier is a user function call
			std::string lookupName = name;
			if (currentModulePrefix != "main" && userFunctions.find(lookupName) == userFunctions.end()) {
				lookupName = currentModulePrefix + "::" + name;
			}
			auto it = userFunctions.find(lookupName);
			if (it != userFunctions.end()) {
				// It's a user function call - check if it has a native version
				if (nativeFunctions.find(lookupName) == nativeFunctions.end()) {
					return false;
				}
			}
		}
		if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			auto* scoped = static_cast<AstNodeScopedIdentifier*>(node);
			std::string fullName = scoped->scope() + "::" + scoped->name();
			// Imported C functions with native bridges are fine
			if (nativeFunctions.find(fullName) == nativeFunctions.end()) {
				return false;
			}
		}
		for (auto* child : node->children()) {
			if (!analyzeCalleesAllNative(child)) {
				return false;
			}
		}
		return true;
	}

	bool LlvmGenerator::Impl::generateFunction(
			AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix) {
		// Clear local variables for this function
		localVariables.clear();
		localVariableStructTypes.clear();
		stackAllocatedStructLocals.clear();
		localArrayVariables.clear();
		lastStructConstructed.clear();
		lastStructWasConstructedInPlace = false;
		lastFieldAccessResultType.clear();
		heapAllocatedCaptures.clear();
		heapCapturePointers.clear();
		indirectLocalVariables.clear();
		closureVariables.clear();
		localVariableTypeHints.clear();

		// Populate type hints from function input parameters
		// This allows debug info to show proper types for locals
		for (const auto& paramNode : funcNode->inputParameters()) {
			if (const auto* param = dynamic_cast<const AstNodeParameter*>(paramNode.get())) {
				const std::string& paramName = param->name();
				const std::string& typeStr = param->typeString();
				if (!paramName.empty() && !typeStr.empty()) {
					localVariableTypeHints[paramName] = typeStr;
				}
			}
		}

		// Check if function returns a pointer (structs must be heap-allocated in such functions)
		// Note: Quadrate allows implicit returns (values left on stack), so we check both:
		// Always use heap allocation for structs to ensure proper cleanup via destructors.
		// Stack allocation was tried but causes memory leaks because stack-allocated structs
		// aren't registered and qd_struct_release can't clean up their string fields.
		currentFunctionReturnsPtr = true;

		// Set current module prefix for intra-module function calls
		currentModulePrefix = namePrefix;

		// Get the correct DIFile for this module
		llvm::DIFile* funcDebugFile = debugFile; // Default to main file
		if (debugInfoEnabled && debugBuilder) {
			auto it = moduleDebugFiles.find(namePrefix);
			if (it != moduleDebugFiles.end()) {
				funcDebugFile = it->second;
			}
		}

		llvm::Function* fn = nullptr;
		currentFunctionIsMain = isMain;

		if (isMain) {
			// Create main function: i32 @main(i32 %argc, i8** %argv)
			auto mainFnTy = llvm::FunctionType::get(int32Ty, {int32Ty, ptrTy}, false);
			fn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);

			// Add debug info for main function
			if (debugInfoEnabled && debugBuilder) {
				auto funcType = debugBuilder->createSubroutineType(debugBuilder->getOrCreateTypeArray({}));
				auto subprogram = debugBuilder->createFunction(compileUnit, // Scope
						funcNode->name(),									// Name
						"main",												// Linkage name
						funcDebugFile,										// File
						static_cast<unsigned>(funcNode->line()),			// Line number
						funcType,											// Type
						static_cast<unsigned>(funcNode->line()),			// Scope line
						llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
				fn->setSubprogram(subprogram);
				debugScopeStack.push_back(subprogram);

				// Emit function start location
				auto loc = llvm::DILocation::get(*context, static_cast<unsigned>(funcNode->line()), 0, subprogram);
				builder->SetCurrentDebugLocation(loc);
			}

			// Create entry basic block
			auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
			builder->SetInsertPoint(entryBB);

			// Create Quadrate context
			auto stackSizeVal = builder->getInt64(stackSize);
			auto ctx = builder->CreateCall(createContextFn, {stackSizeVal}, "ctx");

			// Create alloca for ctx so debugger can reliably access it
			llvm::AllocaInst* ctxAlloca = builder->CreateAlloca(ctx->getType(), nullptr, "ctx.addr");
			builder->CreateStore(ctx, ctxAlloca);

			// Store argc and argv in the context
			// Context layout: {qd_stack*, int64_t error_code, char* error_msg, int argc, char** argv, ...}
			auto argcArg = fn->getArg(0); // i32 argc
			auto argvArg = fn->getArg(1); // i8** argv

			// Store argc (field index 3)
			auto argcPtr = builder->CreateStructGEP(contextStructTy, ctx, 3, "argc.ptr");
			builder->CreateStore(argcArg, argcPtr);

			// Store argv (field index 4)
			auto argvPtr = builder->CreateStructGEP(contextStructTy, ctx, 4, "argv.ptr");
			builder->CreateStore(argvArg, argvPtr);

			// Add debug info for ctx local variable in main
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && contextDebugType) {
				// contextDebugType is already qd_context* (pointer), so use it directly
				auto ctxPtrType = contextDebugType;

				// Create local variable for ctx
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (main function)
						"ctx",															 // Name
						funcDebugFile,													 // File
						static_cast<unsigned>(funcNode->line()),						 // Line
						ctxPtrType,														 // Type
						true															 // Always preserve
				);

				// Insert declare to make it visible in debugger (use alloca, not SSA value)
				debugBuilder->insertDeclare(ctxAlloca,	  // Storage (alloca, not SSA)
						localVar,						  // Variable
						debugBuilder->createExpression(), // Expression
						llvm::DILocation::get(
								*context, static_cast<unsigned>(funcNode->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}

			// Push "main::main" onto call stack for debugging
			std::string fullFuncName = namePrefix + "::" + funcNode->name();
			auto funcNameStr = builder->CreateGlobalString(fullFuncName);
			std::string sourceFile;
			auto srcIt = moduleSourceFiles.find(namePrefix);
			if (srcIt != moduleSourceFiles.end()) {
				sourceFile = srcIt->second;
			}
			// Use cached source file string
			auto sourceFileStr = getOrCreateGlobalString(sourceFile);
			auto lineNum = builder->getInt64(funcNode->line());
			builder->CreateCall(pushCallFn, {ctx, funcNameStr, sourceFileStr, lineNum});

			// Detect if main function body only uses integers (for type specialization)
			currentFunctionIsIntegerOnly = true;
			std::set<std::string> mainLocalNames;
			collectLocalNames(funcNode, mainLocalNames);
			for (auto* child : funcNode->children()) {
				if (!analyzeIsBodyNativeEligible(child, mainLocalNames, false)) {
					currentFunctionIsIntegerOnly = false;
					break;
				}
			}

			// Create return basic block for defer execution
			auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);

			// Initialize defer scope stack for this function
			deferScopeStack.clear();
			pushDeferScope();

			// Scan for captured variables to enable escaped closures
			// Variables captured by closures need heap allocation so they survive function return
			auto body = funcNode->body();
			if (body) {
				collectAllCapturesFromAST(body, heapAllocatedCaptures);
			}

			// Generate function body
			if (body) {
				generateNode(body, ctx);
			}

			// Branch to return block if no terminator
			if (!builder->GetInsertBlock()->getTerminator()) {
				builder->CreateBr(returnBB);
			}

			// Generate return block - this is where function-level defers execute
			builder->SetInsertPoint(returnBB);

			// Execute function-level defer scope
			executeDeferScope(ctx);

			// Clean up local variables (free strings)
			generateLocalCleanup();

			// Pop from call stack
			builder->CreateCall(popCallFn, {ctx});

			// Free context
			builder->CreateCall(freeContextFn, {ctx});

			// Return 0
			builder->CreateRet(builder->getInt32(0));

			// Pop debug scope for main function
			if (debugInfoEnabled && !debugScopeStack.empty()) {
				debugScopeStack.pop_back();
			}
		} else {
			// User-defined function: int usr_<prefix>_<name>(qd_context* ctx)
			// For methods: int usr_<prefix>_<ReceiverType>_<name>(qd_context* ctx)
			std::string fnName;
			if (funcNode->hasReceiver()) {
				fnName = "usr_" + namePrefix + "_" + funcNode->receiverType() + "_" + funcNode->name();
			} else {
				fnName = "usr_" + namePrefix + "_" + funcNode->name();
			}
			// Check if function was already declared in pre-pass (for forward references)
			fn = module->getFunction(fnName);
			if (!fn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				// Use InternalLinkage for user functions unless in export mode (shared library compilation)
				// InternalLinkage allows LLVM to eliminate unused functions via GlobalDCE
				auto linkage = exportMode ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
				fn = llvm::Function::Create(fnTy, linkage, fnName, *module);
				fn->addParamAttr(0, llvm::Attribute::NonNull);
				fn->addParamAttr(0, llvm::Attribute::NoAlias);
				fn->addFnAttr(llvm::Attribute::NoUnwind);
				if (funcNode->isInline()) {
					fn->addFnAttr(llvm::Attribute::AlwaysInline);
				}
			}

			// Add debug info for user function
			if (debugInfoEnabled && debugBuilder) {
				auto funcType = debugBuilder->createSubroutineType(debugBuilder->getOrCreateTypeArray({}));
				auto subprogram = debugBuilder->createFunction(compileUnit, // Scope
						funcNode->name(),									// Name
						fnName.c_str(),										// Linkage name
						funcDebugFile,										// File
						static_cast<unsigned>(funcNode->line()),			// Line number
						funcType,											// Type
						static_cast<unsigned>(funcNode->line()),			// Scope line
						llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
				fn->setSubprogram(subprogram);
				debugScopeStack.push_back(subprogram);

				// Emit function start location
				auto loc = llvm::DILocation::get(*context, static_cast<unsigned>(funcNode->line()), 0, subprogram);
				builder->SetCurrentDebugLocation(loc);
			}

			// Register the function with appropriate scope
			// For methods, register with mangled name: module::ReceiverType::methodName
			std::string registerName;
			if (funcNode->hasReceiver()) {
				// Qualify struct type with module prefix for methods from modules
				std::string qualifiedReceiverType = funcNode->receiverType();
				if (namePrefix != "main" && qualifiedReceiverType.find("::") == std::string::npos) {
					qualifiedReceiverType = namePrefix + "::" + qualifiedReceiverType;
				}
				registerName = qualifiedReceiverType + "::" + funcNode->name();

				// Also register with unqualified struct type for intra-module method calls
				std::string unqualifiedRegisterName = funcNode->receiverType() + "::" + funcNode->name();
				if (unqualifiedRegisterName != registerName) {
					userFunctions[unqualifiedRegisterName] = fn;
					fallibleFunctions[unqualifiedRegisterName] = funcNode->throws();
				}
			} else {
				registerName = (namePrefix == "main") ? funcNode->name() : (namePrefix + "::" + funcNode->name());

				// For merged modules (local file imports), also register with unqualified name
				// This allows calling functions without the module prefix
				if (namePrefix != "main" && mergedModules.count(namePrefix) > 0) {
					userFunctions[funcNode->name()] = fn;
					fallibleFunctions[funcNode->name()] = funcNode->throws();
				}
			}
			userFunctions[registerName] = fn;
			fallibleFunctions[registerName] = funcNode->throws();

			// Create basic blocks
			auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
			auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);

			builder->SetInsertPoint(entryBB);

			// Get context parameter
			llvm::Value* ctx = fn->getArg(0);
			ctx->setName("ctx");

			// Create alloca for ctx parameter for debug info
			// Store the parameter value to it so debugger can always find it
			llvm::AllocaInst* ctxAlloca = builder->CreateAlloca(ctx->getType(), nullptr, "ctx.addr");
			builder->CreateStore(ctx, ctxAlloca);

			// Add debug info for ctx parameter (treat as local variable, not parameter)
			if (debugInfoEnabled && debugBuilder && !debugScopeStack.empty() && contextDebugType) {
				// contextDebugType is already qd_context* (pointer), so use it directly
				auto ctxPtrType = contextDebugType;

				// Create local variable for ctx (not a parameter variable)
				// This prevents LLVM from tracking the parameter value flow
				auto localVar = debugBuilder->createAutoVariable(debugScopeStack.back(), // Scope (current function)
						"ctx",															 // Name
						funcDebugFile,													 // File
						static_cast<unsigned>(funcNode->line()),						 // Line
						ctxPtrType,														 // Type
						true															 // Always preserve
				);

				// Insert declare on the alloca so debugger can always find it
				// Use DW_OP_deref since the alloca is a pointer to where the value is stored
				// No deref needed - ctxAlloca stores the pointer value directly
				debugBuilder->insertDeclare(ctxAlloca,	  // Storage (the alloca)
						localVar,						  // Variable
						debugBuilder->createExpression(), // Empty expression (no deref)
						llvm::DILocation::get(
								*context, static_cast<unsigned>(funcNode->line()), 0, debugScopeStack.back()),
						builder->GetInsertBlock());
			}

			// Set the return target for this function
			currentFunctionReturnBlock = returnBB;
			currentFunctionIsFallible = funcNode->throws();

			// Detect if function only uses numeric types (for type specialization)
			// Do this BEFORE generating call tracking so we can skip it for numeric-only functions
			// A function is "integer-only" if all typed parameters are i64
			// AND the body contains no floats, strings, or module calls.
			// This enables fast inline integer operations (no type checking needed).
			currentFunctionIsIntegerOnly = true;
			for (const auto& param : funcNode->inputParameters()) {
				if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param.get())) {
					const std::string& typeStr = paramNode->typeString();
					if (!typeStr.empty() && typeStr != "i64" && typeStr != "int" && typeStr != "int64" &&
							typeStr != "i") {
						currentFunctionIsIntegerOnly = false;
						break;
					}
				}
			}
			if (currentFunctionIsIntegerOnly) {
				for (const auto& param : funcNode->outputParameters()) {
					if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param.get())) {
						const std::string& typeStr = paramNode->typeString();
						if (!typeStr.empty() && typeStr != "i64" && typeStr != "int" && typeStr != "int64" &&
								typeStr != "i") {
							currentFunctionIsIntegerOnly = false;
							break;
						}
					}
				}
			}

			// Also check the function body for non-numeric types (strings, module calls)
			if (currentFunctionIsIntegerOnly) {
				// Scan the function body to ensure it only uses numerics
				std::set<std::string> funcLocalNames;
				collectLocalNames(funcNode, funcLocalNames);
				for (auto* child : funcNode->children()) {
					if (!analyzeIsBodyNativeEligible(child, funcLocalNames, false)) {
						currentFunctionIsIntegerOnly = false;
						break;
					}
				}
			}

			// Check if this function has a native version (compile-time stack optimization)
			auto nativeIt = nativeFunctions.find(registerName);
			// Verify all callee user functions also have native versions
			bool calleesAllNative = true;
			if (nativeIt != nativeFunctions.end()) {
				auto body = funcNode->body();
				if (body) {
					for (auto* child : body->children()) {
						if (!analyzeCalleesAllNative(child)) {
							calleesAllNative = false;
							break;
						}
					}
				}
			}
			// If callees aren't all native, remove the native function declaration
			if (nativeIt != nativeFunctions.end() && !calleesAllNative) {
				auto* erasedFn = nativeIt->second;
				erasedFn->eraseFromParent();
				nativeFunctions.erase(registerName);
				nativeFuncInfo.erase(registerName);
				// Also remove any other name that points to the same erased function
				// (main-file functions are registered under both qualified and unqualified names)
				auto unqualIt = nativeFunctions.find(funcNode->name());
				if (unqualIt != nativeFunctions.end() && unqualIt->second == erasedFn) {
					nativeFunctions.erase(unqualIt);
					nativeFuncInfo.erase(funcNode->name());
				}
				if (namePrefix != "main" && mergedModules.count(namePrefix) > 0) {
					nativeFunctions.erase(funcNode->name());
					nativeFuncInfo.erase(funcNode->name());
				}
				nativeIt = nativeFunctions.end(); // Invalidate iterator
				currentFunctionIsIntegerOnly = false;
			}
			if (nativeIt != nativeFunctions.end() && !debugInfoEnabled && nativeIt->second->empty()) {
				// === NATIVE CALLING CONVENTION PATH ===
				// Generate two functions:
				// 1. Native function: direct i64 params/return, compile-time stack
				// 2. Stack wrapper: pops from runtime stack, calls native, pushes result
				// Skip if native function already has a body (merged/local modules)

				llvm::Function* nativeFn = nativeIt->second;
				auto& info = nativeFuncInfo[registerName];

				// --- Generate native function body ---
				auto nativeEntryBB = llvm::BasicBlock::Create(*context, "entry", nativeFn);
				builder->SetInsertPoint(nativeEntryBB);

				// Save and reset state for native body generation
				auto savedLocalVars = localVariables;
				auto savedLocalVarStructTypes = localVariableStructTypes;
				auto savedStackAllocStructLocals = stackAllocatedStructLocals;
				auto savedLocalArrayVars = localArrayVariables;
				auto savedNativeLocalVars = nativeLocalVariables;
				auto savedReturnBlock = currentFunctionReturnBlock;
				auto savedIsFallible = currentFunctionIsFallible;
				auto savedIsIntegerOnly = currentFunctionIsIntegerOnly;
				auto savedIteratorVars = iteratorVars;

				localVariables.clear();
				localVariableStructTypes.clear();
				stackAllocatedStructLocals.clear();
				localArrayVariables.clear();
				nativeLocalVariables.clear();
				iteratorVars.clear();
				currentFunctionIsIntegerOnly = true;
				currentFunctionIsFallible = false;

				// Get ctx parameter (arg 0)
				llvm::Value* nativeCtx = nativeFn->getArg(0);
				nativeCtx->setName("ctx");

				// Set up compile-time stack with function parameters
				useCompileTimeStack = true;
				compileTimeStack.clear();
				for (size_t i = 0; i < info.inputCount; i++) {
					llvm::Value* param = nativeFn->getArg(static_cast<unsigned>(i + 1));
					// Name the parameter after the input parameter name
					if (i < funcNode->inputParameters().size()) {
						auto* paramNode = static_cast<AstNodeParameter*>(funcNode->inputParameters()[i].get());
						param->setName(paramNode->name());
					}
					compileTimeStack.push_back(param);
				}

				// Auto-bind named input parameters as local variables (native path)
				// For native functions, params are LLVM function args on the compileTimeStack.
				// Bind named params to nativeLocalVariables so they can be referenced by name.
				// Values stay on the compileTimeStack too (native codegen uses them positionally).
				{
					const auto& inputs = funcNode->inputParameters();
					for (size_t i = 0; i < inputs.size() && i < compileTimeStack.size(); i++) {
						const auto* paramNode = static_cast<const AstNodeParameter*>(inputs[i].get());
						if (!paramNode->hasName()) {
							continue;
						}
						const std::string& paramName = paramNode->name();
						llvm::Value* val = compileTimeStack[i];

						llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
						llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
						auto* alloca = tmpBuilder.CreateAlloca(val->getType(), nullptr, paramName);
						nativeLocalVariables[paramName] = alloca;
						builder->CreateStore(val, alloca);
					}
				}

				// Create return value alloca in entry block (LLVM mem2reg will promote to SSA)
				nativeReturnAlloca = nullptr;
				if (info.outputCount == 1) {
					llvm::Type* retTy = (info.outputType == NativeParamType::F64) ? builder->getDoubleTy() : int64Ty;
					nativeReturnAlloca = builder->CreateAlloca(retTy, nullptr, "retval");
					llvm::Value* retInit =
							retTy->isDoubleTy()
									? static_cast<llvm::Value*>(llvm::ConstantFP::get(builder->getDoubleTy(), 0.0))
									: static_cast<llvm::Value*>(builder->getInt64(0));
					builder->CreateStore(retInit, nativeReturnAlloca);
				}

				// Create return block for native function
				auto nativeReturnBB = llvm::BasicBlock::Create(*context, "return", nativeFn);
				currentFunctionReturnBlock = nativeReturnBB;

				// Initialize defer scope
				deferScopeStack.clear();
				pushDeferScope();

				// Generate body using compile-time stack
				auto body = funcNode->body();
				if (body) {
					generateNode(body, nativeCtx);
				}

				// Before branching to return, store the return value
				llvm::BasicBlock* nativeBodyBlock = builder->GetInsertBlock();
				if (nativeBodyBlock != nullptr && nativeBodyBlock->getTerminator() == nullptr) {
					if (info.outputCount == 1 && !compileTimeStack.empty()) {
						builder->CreateStore(compileTimeStack.back(), nativeReturnAlloca);
					}
					builder->CreateBr(nativeReturnBB);
				}

				// Generate return block
				builder->SetInsertPoint(nativeReturnBB);

				if (info.outputCount == 1) {
					llvm::Value* retVal = builder->CreateLoad(
							nativeReturnAlloca->getAllocatedType(), nativeReturnAlloca, "retval_load");
					builder->CreateRet(retVal);
				} else {
					builder->CreateRetVoid();
				}

				// Clean up native mode
				useCompileTimeStack = false;
				compileTimeStack.clear();
				nativeReturnAlloca = nullptr;

				// Restore state
				localVariables = savedLocalVars;
				localVariableStructTypes = savedLocalVarStructTypes;
				stackAllocatedStructLocals = savedStackAllocStructLocals;
				localArrayVariables = savedLocalArrayVars;
				nativeLocalVariables = savedNativeLocalVars;
				currentFunctionReturnBlock = savedReturnBlock;
				currentFunctionIsFallible = savedIsFallible;
				currentFunctionIsIntegerOnly = savedIsIntegerOnly;
				iteratorVars = savedIteratorVars;

				// --- Generate stack wrapper body ---
				// The wrapper pops args from runtime stack, calls native, pushes result
				builder->SetInsertPoint(entryBB);

				// Pop arguments from runtime stack (in reverse order)
				std::vector<llvm::Value*> nativeArgs;
				nativeArgs.push_back(ctx); // First arg is always ctx
				// Pop N args (they come off stack in reverse parameter order)
				std::vector<llvm::Value*> poppedArgs;
				for (size_t i = 0; i < info.inputCount; i++) {
					// Pop in reverse order, so pop index is (inputCount - 1 - i)
					size_t popIdx = info.inputCount - 1 - i;
					if (popIdx < info.inputTypes.size() && info.inputTypes[popIdx] == NativeParamType::F64) {
						poppedArgs.push_back(generateInlinePopFloat(ctx));
					} else {
						poppedArgs.push_back(generateInlinePopInt(ctx));
					}
				}
				// Reverse to get correct parameter order
				for (auto it = poppedArgs.rbegin(); it != poppedArgs.rend(); ++it) {
					nativeArgs.push_back(*it);
				}

				// Call native function and push result to runtime stack
				if (info.outputCount == 1) {
					llvm::Value* nativeResult = builder->CreateCall(nativeFn, nativeArgs, "result");
					if (info.outputType == NativeParamType::F64) {
						generateInlinePushFloatValue(ctx, nativeResult);
					} else {
						generateInlinePushIntValue(ctx, nativeResult);
					}
				} else {
					builder->CreateCall(nativeFn, nativeArgs);
				}

				// Branch to return
				builder->CreateBr(returnBB);

				// Generate return block for wrapper
				builder->SetInsertPoint(returnBB);
				builder->CreateRet(builder->getInt32(0));

				// Clear function context
				currentFunctionReturnBlock = nullptr;
				currentFunctionIsFallible = false;
				currentFunctionIsIntegerOnly = false;

				// Pop debug scope
				if (debugInfoEnabled && !debugScopeStack.empty()) {
					debugScopeStack.pop_back();
				}
			} else {
				// === NORMAL (NON-NATIVE) PATH ===

				// If this function was considered integer-only but ended up here
				// (e.g., fallible function, or callees not all native), clear the flag
				// so auto-binding, call tracking, and type checking are not skipped.
				currentFunctionIsIntegerOnly = false;

				// Push function name onto call stack for debugging
				std::string fullFuncName = namePrefix + "::" + funcNode->name();
				llvm::Value* funcNameStr = nullptr;
				if (!currentFunctionIsIntegerOnly) {
					funcNameStr = builder->CreateGlobalString(fullFuncName);
					std::string sourceFile;
					auto srcIt = moduleSourceFiles.find(namePrefix);
					if (srcIt != moduleSourceFiles.end()) {
						sourceFile = srcIt->second;
					}
					// Use cached source file string since all functions in a module share the same path
					auto sourceFileStr = getOrCreateGlobalString(sourceFile);
					auto lineNum = builder->getInt64(funcNode->line());
					builder->CreateCall(pushCallFn, {ctx, funcNameStr, sourceFileStr, lineNum});
				}

				// Coverage instrumentation: emit qd_coverage_mark(idx) at the
				// start of every user function body. The matching
				// qd_coverage_register call is emitted in the test runner main
				// in the same order, so compile-time idx = runtime idx.
				// Only the main module's functions are tracked, since stdlib
				// functions don't have meaningful "covered by test" semantics.
				if (coverageMode && testMode && namePrefix == mainModuleName) {
					uint32_t covIdx = static_cast<uint32_t>(coverageFunctionNames.size());
					coverageFunctionNames.push_back(fullFuncName);
					auto markFnTy = llvm::FunctionType::get(builder->getVoidTy(), {int32Ty}, false);
					auto markFn = module->getOrInsertFunction("qd_coverage_mark", markFnTy);
					builder->CreateCall(markFn, {builder->getInt32(covIdx)});
				}

				// Generate type check for input parameters
				// Skip for integer-only functions - semantic validator has already verified types
				if (!funcNode->inputParameters().empty() && !currentFunctionIsIntegerOnly) {
					// Create array of types
					std::vector<llvm::Constant*> typeValues;
					for (const auto& paramNode : funcNode->inputParameters()) {
						AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
						std::string typeStr = param->typeString();
						uint32_t typeValue;
						if (typeStr.empty()) {
							typeValue = 2; // QD_STACK_TYPE_PTR - untyped
						} else if (typeStr == "i") {
							typeValue = 0; // QD_STACK_TYPE_INT
						} else if (typeStr == "f") {
							typeValue = 1; // QD_STACK_TYPE_FLOAT
						} else if (typeStr == "s") {
							typeValue = 3; // QD_STACK_TYPE_STR
						} else if (typeStr == "p") {
							typeValue = 2; // QD_STACK_TYPE_PTR
						} else {
							typeValue = 2; // QD_STACK_TYPE_PTR - unknown type
						}
						typeValues.push_back(builder->getInt32(typeValue));
					}

					// Create global array constant
					auto arrayType = llvm::ArrayType::get(int32Ty, typeValues.size());
					auto arrayInit = llvm::ConstantArray::get(arrayType, typeValues);
					auto globalArray = new llvm::GlobalVariable(
							*module, arrayType, true, llvm::GlobalValue::PrivateLinkage, arrayInit, "input_types");

					// Call qd_check_stack(ctx, count, types, func_name)
					auto arrayPtr = builder->CreateBitCast(globalArray, ptrTy);
					builder->CreateCall(checkStackFn,
							{ctx, builder->getInt64(funcNode->inputParameters().size()), arrayPtr, funcNameStr});
				}

				// Initialize defer scope stack for this function
				deferScopeStack.clear();
				pushDeferScope();

				// Pre-register struct-typed parameters so they get released at cleanup
				// Parameters with struct type annotations will be stored via `-> name` in the body
				for (const auto& paramNode : funcNode->inputParameters()) {
					if (const auto* param = dynamic_cast<const AstNodeParameter*>(paramNode.get())) {
						const std::string& typeStr = param->typeString();
						// Check if this is a struct type (not a primitive type)
						if (!typeStr.empty() && typeStr != "i" && typeStr != "i64" && typeStr != "int" &&
								typeStr != "int64" && typeStr != "f" && typeStr != "f64" && typeStr != "float" &&
								typeStr != "s" && typeStr != "str" && typeStr != "string" && typeStr != "p" &&
								typeStr != "ptr" && typeStr != "pointer") {
							// This is a struct type - mark the parameter name as a struct local
							// so it gets released at function cleanup (use full qualified name)
							if (!param->name().empty()) {
								localVariableStructTypes[param->name()] = extractStructName(typeStr);
							}
						}
					}
				}

				// For methods, pop the receiver from the stack and bind it as a local variable
				// The receiver is implicitly passed and must be bound before the body executes
				if (funcNode->hasReceiver()) {
					const std::string& receiverName = funcNode->receiverName();
					const std::string& receiverType = funcNode->receiverType();

					// Create alloca for the receiver variable
					llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
					llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
					llvm::AllocaInst* receiverAlloca = tmpBuilder.CreateAlloca(stackElementTy, nullptr, receiverName);

					// Initialize type field
					llvm::Value* typePtr =
							tmpBuilder.CreateStructGEP(stackElementTy, receiverAlloca, 1, receiverName + "_init_type");
					tmpBuilder.CreateStore(tmpBuilder.getInt32(static_cast<uint32_t>(-1)), typePtr);

					// Store in local variables map
					localVariables[receiverName] = receiverAlloca;
					// Qualify receiver type with module prefix if not already qualified
					std::string qualifiedReceiverType = receiverType;
					if (receiverType.find("::") == std::string::npos && currentModulePrefix != "main") {
						qualifiedReceiverType = currentModulePrefix + "::" + receiverType;
					}
					localVariableStructTypes[receiverName] = qualifiedReceiverType;

					// Pop the receiver from the stack
					llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
					llvm::Value* stackPtr = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");
					builder->CreateCall(stackPopFn, {stackPtr, receiverAlloca});
				}

				// Scan for captured variables to enable escaped closures
				// Variables captured by closures need heap allocation so they survive function return
				auto body = funcNode->body();
				if (body) {
					collectAllCapturesFromAST(body, heapAllocatedCaptures);
				}

				// Auto-bind named input parameters as local variables
				// Pop from stack in reverse order (stack is LIFO: last param on top)
				// Only auto-bind when ALL input params are named (mixed named/unnamed
				// would require complex stack reordering)
				if (!currentFunctionIsIntegerOnly) {
					const auto& inputs = funcNode->inputParameters();
					bool allNamed = true;
					for (size_t i = 0; i < inputs.size(); i++) {
						if (!static_cast<const AstNodeParameter*>(inputs[i].get())->hasName()) {
							allNamed = false;
							break;
						}
					}
					for (int paramIdx = static_cast<int>(inputs.size()) - 1; allNamed && paramIdx >= 0; paramIdx--) {
						const auto* param =
								static_cast<const AstNodeParameter*>(inputs[static_cast<size_t>(paramIdx)].get());
						const std::string& paramName = param->name();

						// Create alloca in entry block
						llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
						llvm::IRBuilder<> tmpBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
						llvm::AllocaInst* paramAlloca = tmpBuilder.CreateAlloca(stackElementTy, nullptr, paramName);

						// Initialize type field to -1 (uninitialized marker)
						llvm::Value* typePtr =
								tmpBuilder.CreateStructGEP(stackElementTy, paramAlloca, 1, paramName + "_init_type");
						tmpBuilder.CreateStore(tmpBuilder.getInt32(static_cast<uint32_t>(-1)), typePtr);

						// Store in local variables map
						localVariables[paramName] = paramAlloca;

						// Track struct type if applicable
						const std::string& typeStr = param->typeString();
						if (!typeStr.empty() && typeStr != "i" && typeStr != "i64" && typeStr != "int" &&
								typeStr != "int64" && typeStr != "f" && typeStr != "f64" && typeStr != "float" &&
								typeStr != "s" && typeStr != "str" && typeStr != "string" && typeStr != "p" &&
								typeStr != "ptr" && typeStr != "pointer") {
							localVariableStructTypes[paramName] = extractStructName(typeStr);
						}

						// Pop from runtime stack
						llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
						llvm::Value* stackPtr = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");
						builder->CreateCall(stackPopFn, {stackPtr, paramAlloca});
					}
				}

				// Generate function body
				if (body) {
					generateNode(body, ctx);
				}

				// Clear return target (but save integer-only flag for return block)
				currentFunctionReturnBlock = nullptr;
				currentFunctionIsFallible = false;
				bool wasIntegerOnly = currentFunctionIsIntegerOnly;
				currentFunctionIsIntegerOnly = false;

				// If the block doesn't end with a terminator, branch to return block
				llvm::BasicBlock* funcBodyBlock = builder->GetInsertBlock();
				if (funcBodyBlock != nullptr && funcBodyBlock->getTerminator() == nullptr) {
					builder->CreateBr(returnBB);
				}

				// Generate return block - this is where function-level defers execute
				builder->SetInsertPoint(returnBB);

				// Execute function-level defer scope
				executeDeferScope(ctx);

				// Clean up local variables (free strings)
				generateLocalCleanup();

				// Pop function from call stack before returning
				// Skip for integer-only functions (we didn't push in that case)
				if (!wasIntegerOnly) {
					builder->CreateCall(popCallFn, {ctx});
				}

				// Return success
				builder->CreateRet(builder->getInt32(0));

				// Pop debug scope for user function
				if (debugInfoEnabled && !debugScopeStack.empty()) {
					debugScopeStack.pop_back();
				}

			} // end else (non-native path)
		}

		return true;
	}

	bool LlvmGenerator::Impl::declareFunction(AstNodeFunctionDeclaration* funcNode, const std::string& namePrefix) {
		// Create LLVM function declaration and register in userFunctions
		// This is a pre-pass to ensure all functions are known before generating bodies

		std::string fnName;
		if (funcNode->hasReceiver()) {
			fnName = "usr_" + namePrefix + "_" + funcNode->receiverType() + "_" + funcNode->name();
		} else {
			fnName = "usr_" + namePrefix + "_" + funcNode->name();
		}

		// Check if function was already declared
		llvm::Function* fn = module->getFunction(fnName);
		if (!fn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			auto linkage = exportMode ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
			fn = llvm::Function::Create(fnTy, linkage, fnName, *module);
			fn->addParamAttr(0, llvm::Attribute::NonNull);
			fn->addParamAttr(0, llvm::Attribute::NoAlias);
			fn->addFnAttr(llvm::Attribute::NoUnwind);
			if (funcNode->isInline()) {
				fn->addFnAttr(llvm::Attribute::AlwaysInline);
			}
		}

		// Register the function with appropriate scope
		std::string registerName;
		if (funcNode->hasReceiver()) {
			std::string qualifiedReceiverType = funcNode->receiverType();
			if (namePrefix != "main" && qualifiedReceiverType.find("::") == std::string::npos) {
				qualifiedReceiverType = namePrefix + "::" + qualifiedReceiverType;
			}
			registerName = qualifiedReceiverType + "::" + funcNode->name();

			// Also register with unqualified struct type for intra-module method calls
			std::string unqualifiedRegisterName = funcNode->receiverType() + "::" + funcNode->name();
			if (unqualifiedRegisterName != registerName) {
				userFunctions[unqualifiedRegisterName] = fn;
				fallibleFunctions[unqualifiedRegisterName] = funcNode->throws();
			}

			// Store receiver position (number of non-receiver args) for fallback method resolution
			size_t receiverPos = funcNode->inputParameters().size();
			methodReceiverPosition[registerName] = receiverPos;
			if (unqualifiedRegisterName != registerName) {
				methodReceiverPosition[unqualifiedRegisterName] = receiverPos;
			}
		} else {
			registerName = (namePrefix == "main") ? funcNode->name() : (namePrefix + "::" + funcNode->name());

			// For merged modules (local file imports), also register with unqualified name
			// This allows calling functions without the module prefix
			if (namePrefix != "main" && mergedModules.count(namePrefix) > 0) {
				userFunctions[funcNode->name()] = fn;
				fallibleFunctions[funcNode->name()] = funcNode->throws();
			}
		}
		userFunctions[registerName] = fn;
		fallibleFunctions[registerName] = funcNode->throws();

		// Check if this function qualifies for native calling convention
		// Requirements: non-receiver, non-fallible, all params i64/f64, body is native-eligible
		if (!funcNode->hasReceiver() && !funcNode->throws()) {
			bool allParamsNumeric = true;
			std::vector<NativeParamType> inputTypes;
			for (const auto& param : funcNode->inputParameters()) {
				if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param.get())) {
					const std::string& typeStr = paramNode->typeString();
					if (typeStr == "f64" || typeStr == "float" || typeStr == "float64" || typeStr == "f") {
						inputTypes.push_back(NativeParamType::F64);
					} else if (typeStr.empty() || typeStr == "i64" || typeStr == "int" || typeStr == "int64" ||
							   typeStr == "i") {
						inputTypes.push_back(NativeParamType::I64);
					} else {
						allParamsNumeric = false;
						break;
					}
				}
			}
			NativeParamType outputType = NativeParamType::I64;
			if (allParamsNumeric) {
				for (const auto& param : funcNode->outputParameters()) {
					if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param.get())) {
						const std::string& typeStr = paramNode->typeString();
						if (typeStr == "f64" || typeStr == "float" || typeStr == "float64" || typeStr == "f") {
							outputType = NativeParamType::F64;
						} else if (!typeStr.empty() && typeStr != "i64" && typeStr != "int" && typeStr != "int64" &&
								   typeStr != "i") {
							allParamsNumeric = false;
							break;
						}
					}
				}
			}
			if (allParamsNumeric) {
				// Check body is native-eligible
				std::set<std::string> funcLocalNames;
				collectLocalNames(funcNode, funcLocalNames);
				bool bodyOk = true;
				for (auto* child : funcNode->children()) {
					if (!analyzeIsBodyNativeEligible(child, funcLocalNames)) {
						bodyOk = false;
						break;
					}
				}
				if (bodyOk) {
					size_t inputCount = funcNode->inputParameters().size();
					size_t outputCount = funcNode->outputParameters().size();
					// Require at least 1 typed input parameter and at most 1 output
					if (inputCount > 0 && outputCount <= 1) {
						std::string nativeName = fnName + "_native";

						// Check if native function already exists (can happen for merged/local modules)
						llvm::Function* nativeFn = module->getFunction(nativeName);
						if (!nativeFn) {
							auto* doubleTy = builder->getDoubleTy();
							std::vector<llvm::Type*> nativeParams;
							nativeParams.push_back(contextPtrTy); // ctx still needed for print/etc
							for (size_t i = 0; i < inputCount; i++) {
								nativeParams.push_back(inputTypes[i] == NativeParamType::F64 ? doubleTy : int64Ty);
							}
							llvm::Type* nativeRetTy =
									(outputCount == 0) ? builder->getVoidTy()
													   : (outputType == NativeParamType::F64 ? doubleTy : int64Ty);
							auto nativeFnTy = llvm::FunctionType::get(nativeRetTy, nativeParams, false);
							auto linkage =
									exportMode ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
							nativeFn = llvm::Function::Create(nativeFnTy, linkage, nativeName, *module);
							nativeFn->addParamAttr(0, llvm::Attribute::NonNull);
							nativeFn->addParamAttr(0, llvm::Attribute::NoAlias);
							nativeFn->addFnAttr(llvm::Attribute::NoUnwind);
							if (funcNode->isInline()) {
								nativeFn->addFnAttr(llvm::Attribute::AlwaysInline);
							}
						}

						// Register native function by all names the stack-based version is registered under
						nativeFunctions[registerName] = nativeFn;
						nativeFuncInfo[registerName] = {inputCount, outputCount, inputTypes, outputType};

						// Also register with unqualified name for merged modules
						if (namePrefix != "main" && mergedModules.count(namePrefix) > 0) {
							nativeFunctions[funcNode->name()] = nativeFn;
							nativeFuncInfo[funcNode->name()] = {inputCount, outputCount, inputTypes, outputType};
						}
					}
				}
			}
		}

		return true;
	}

	bool LlvmGenerator::Impl::generateFunctionBody(
			AstNodeFunctionDeclaration* funcNode, bool isMain, const std::string& namePrefix) {
		// Generate the function body - function should already be declared in pre-pass
		// This just delegates to generateFunction, which will find the already-declared function
		return generateFunction(funcNode, isMain, namePrefix);
	}

	bool LlvmGenerator::Impl::generateTest(AstNodeTest* testNode, const std::string& namePrefix) {
		// Clear local variables for this test
		localVariables.clear();
		localVariableStructTypes.clear();
		stackAllocatedStructLocals.clear();
		localArrayVariables.clear();
		lastStructConstructed.clear();
		lastStructWasConstructedInPlace = false;
		lastFieldAccessResultType.clear();

		currentFunctionReturnsPtr = true;
		currentModulePrefix = namePrefix;

		// Generate test function name: test_<sanitized_name>
		std::string sanitizedName = testNode->name();
		for (char& c : sanitizedName) {
			if (!std::isalnum(c)) {
				c = '_';
			}
		}
		std::string funcName = "test_" + namePrefix + "_" + sanitizedName;

		// Create function type: int function(qd_context*)
		auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
		auto fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage, funcName, *module);

		// Track the test function
		userFunctions[funcName] = fn;

		// Create entry block
		auto entryBB = llvm::BasicBlock::Create(*context, "entry", fn);
		builder->SetInsertPoint(entryBB);

		// Get context parameter
		auto ctx = fn->arg_begin();
		ctx->setName("ctx");

		// Create error tracking alloca for this test
		testErrorAlloca = builder->CreateAlloca(int32Ty, nullptr, "test_error");
		builder->CreateStore(builder->getInt32(0), testErrorAlloca);

		// Initialize defer scope for this test
		deferScopeStack.clear();
		deferScopeStack.push_back({});

		// Create return block
		auto returnBB = llvm::BasicBlock::Create(*context, "return", fn);
		currentFunctionReturnBlock = returnBB;

		// Push test function onto call stack
		std::string testDisplayName = namePrefix + "::test(" + testNode->name() + ")";
		auto funcNameStr = builder->CreateGlobalString(testDisplayName);
		std::string sourceFile;
		auto srcIt = moduleSourceFiles.find(namePrefix);
		if (srcIt != moduleSourceFiles.end()) {
			sourceFile = srcIt->second;
		}
		// Use cached source file string
		auto sourceFileStr = getOrCreateGlobalString(sourceFile);
		auto lineNum = builder->getInt64(testNode->line());
		builder->CreateCall(pushCallFn, {ctx, funcNameStr, sourceFileStr, lineNum});

		// Generate test body
		auto body = testNode->body();
		if (body) {
			generateNode(body, ctx);
		}

		// Branch to return block if no terminator
		llvm::BasicBlock* testBodyBlock = builder->GetInsertBlock();
		if (testBodyBlock != nullptr && testBodyBlock->getTerminator() == nullptr) {
			builder->CreateBr(returnBB);
		}

		// Generate return block
		builder->SetInsertPoint(returnBB);

		// Execute defer scope
		executeDeferScope(ctx);

		// Clean up local variables
		generateLocalCleanup();

		// Pop from call stack
		builder->CreateCall(popCallFn, {ctx});

		// Return the accumulated error status
		auto finalError = builder->CreateLoad(int32Ty, testErrorAlloca, "final_error");
		builder->CreateRet(finalError);

		currentFunctionReturnBlock = nullptr;
		testErrorAlloca = nullptr; // Clear test context

		return true;
	}

	bool LlvmGenerator::Impl::generateTestRunner(
			const std::vector<std::pair<std::string, std::string>>& testNamesWithDisplay) {
		// Create main function: i32 @main(i32 %argc, i8** %argv)
		auto mainFnTy = llvm::FunctionType::get(int32Ty, {int32Ty, ptrTy}, false);
		auto mainFn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);

		auto entryBB = llvm::BasicBlock::Create(*context, "entry", mainFn);
		builder->SetInsertPoint(entryBB);

		// Check NO_COLOR environment variable
		auto getenvFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		auto getenvFn = module->getOrInsertFunction("getenv", getenvFnTy);
		auto noColorStr = builder->CreateGlobalString("NO_COLOR", "no_color_env");
		auto noColorVal = builder->CreateCall(getenvFn, {noColorStr}, "no_color");
		auto noColorNull = builder->CreateICmpEQ(noColorVal, llvm::ConstantPointerNull::get(ptrTy), "no_color_null");
		// useColor = (getenv("NO_COLOR") == NULL)
		auto useColor = noColorNull;

		// Create Quadrate context
		auto stackSizeVal = builder->getInt64(stackSize);
		auto ctx = builder->CreateCall(createContextFn, {stackSizeVal}, "ctx");

		// Create printf function for direct output
		auto printfFnTy = llvm::FunctionType::get(int32Ty, {ptrTy}, true);
		auto printfFn = module->getOrInsertFunction("printf", printfFnTy);

		// Coverage instrumentation: register every user function with the
		// runtime tracker, in the same order they were assigned compile-time
		// coverage indices, so registration idx == compile-time idx.
		if (coverageMode) {
			auto regFnTy = llvm::FunctionType::get(int32Ty, {ptrTy}, false);
			auto regFn = module->getOrInsertFunction("qd_coverage_register", regFnTy);
			for (const auto& name : coverageFunctionNames) {
				auto nameStr = builder->CreateGlobalString(name);
				builder->CreateCall(regFn, {nameStr});
			}
		}

		// Counters for passed/failed tests
		auto passedCountAlloca = builder->CreateAlloca(int32Ty, nullptr, "passed_count");
		auto failedCountAlloca = builder->CreateAlloca(int32Ty, nullptr, "failed_count");
		builder->CreateStore(builder->getInt32(0), passedCountAlloca);
		builder->CreateStore(builder->getInt32(0), failedCountAlloca);

		// Print header using printf directly
		auto headerStr = builder->CreateGlobalString(
				"Running " + std::to_string(testNamesWithDisplay.size()) + " tests...\n", "test_header");
		builder->CreateCall(printfFn, {headerStr});

		// Run each test
		for (const auto& [funcName, displayName] : testNamesWithDisplay) {
			// Get the test function
			auto testFn = userFunctions[funcName];
			if (!testFn) {
				continue;
			}

			// Call the test function
			auto errorCode = builder->CreateCall(testFn, {ctx}, "error_code");

			// Check if test passed (only 0 = success)
			auto isSuccess = builder->CreateICmpULT(errorCode, builder->getInt32(1), "is_success");

			auto successBB = llvm::BasicBlock::Create(*context, "test_success", mainFn);
			auto failBB = llvm::BasicBlock::Create(*context, "test_fail", mainFn);
			auto contBB = llvm::BasicBlock::Create(*context, "test_continue", mainFn);

			builder->CreateCondBr(isSuccess, successBB, failBB);

			// Success block
			builder->SetInsertPoint(successBB);
			auto passedCount = builder->CreateLoad(int32Ty, passedCountAlloca, "passed");
			auto newPassed = builder->CreateAdd(passedCount, builder->getInt32(1), "new_passed");
			builder->CreateStore(newPassed, passedCountAlloca);

			// Select colored or plain pass message
			auto passStrColor =
					builder->CreateGlobalString("  \x1b[32m\xe2\x9c\x93\x1b[0m " + displayName + "\n", "pass_color");
			auto passStrPlain = builder->CreateGlobalString("  \xe2\x9c\x93 " + displayName + "\n", "pass_plain");
			auto passStr = builder->CreateSelect(useColor, passStrColor, passStrPlain, "pass_msg");
			builder->CreateCall(printfFn, {passStr});
			builder->CreateBr(contBB);

			// Fail block
			builder->SetInsertPoint(failBB);
			auto failedCount = builder->CreateLoad(int32Ty, failedCountAlloca, "failed");
			auto newFailed = builder->CreateAdd(failedCount, builder->getInt32(1), "new_failed");
			builder->CreateStore(newFailed, failedCountAlloca);

			// Select colored or plain fail message
			auto failStrColor =
					builder->CreateGlobalString("  \x1b[31m\xe2\x9c\x97\x1b[0m " + displayName + "\n", "fail_color");
			auto failStrPlain = builder->CreateGlobalString("  \xe2\x9c\x97 " + displayName + "\n", "fail_plain");
			auto failStr = builder->CreateSelect(useColor, failStrColor, failStrPlain, "fail_msg");
			builder->CreateCall(printfFn, {failStr});
			builder->CreateBr(contBB);

			// Continue to next test
			builder->SetInsertPoint(contBB);
		}

		// Print summary using printf with colors
		auto finalPassed = builder->CreateLoad(int32Ty, passedCountAlloca, "final_passed");
		auto finalFailed = builder->CreateLoad(int32Ty, failedCountAlloca, "final_failed");

		// Select colored or plain summary
		auto summaryFmtColor =
				builder->CreateGlobalString("\n\x1b[32m%d passed\x1b[0m, \x1b[31m%d failed\x1b[0m\n", "summary_color");
		auto summaryFmtPlain = builder->CreateGlobalString("\n%d passed, %d failed\n", "summary_plain");
		auto summaryFmt = builder->CreateSelect(useColor, summaryFmtColor, summaryFmtPlain, "summary_fmt");
		builder->CreateCall(printfFn, {summaryFmt, finalPassed, finalFailed});

		// Coverage report after the test summary, before context teardown.
		if (coverageMode) {
			auto reportFnTy = llvm::FunctionType::get(builder->getVoidTy(), {int32Ty}, false);
			auto reportFn = module->getOrInsertFunction("qd_coverage_report", reportFnTy);
			auto useColorI32 = builder->CreateZExt(useColor, int32Ty, "use_color_i32");
			builder->CreateCall(reportFn, {useColorI32});
		}

		// Free context
		builder->CreateCall(freeContextFn, {ctx});

		// Return 0 if all passed, 1 if any failed
		auto hasFailures = builder->CreateICmpNE(finalFailed, builder->getInt32(0), "has_failures");
		auto exitCode = builder->CreateSelect(hasFailures, builder->getInt32(1), builder->getInt32(0), "exit_code");
		builder->CreateRet(exitCode);

		return true;
	}

	bool LlvmGenerator::Impl::generateProgram(IAstNode* root) {
		// Timing helper - only active when QUADC_TIMING is set
		static bool timing = std::getenv("QUADC_TIMING") != nullptr;
		auto timeLast = std::chrono::steady_clock::now();
		auto printTiming = [&](const char* label) {
			if (timing) {
				auto now = std::chrono::steady_clock::now();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeLast).count();
				std::cerr << "[TIMING]   irGeneration/" << label << ": " << ms << "ms" << std::endl;
				timeLast = now;
			}
		};

		if (!root) {
			std::cerr << "Error: Root node is null" << std::endl;
			return false;
		}

		// Set target triple and data layout early so codegen generates correct
		// struct layouts and alignments for the target (especially i386 vs x86_64).
		// writeObject() sets these again with the full target machine; this is for
		// correct IR generation before that point.
		if (!targetTriple.empty()) {
			llvm::InitializeAllTargetInfos();
			llvm::InitializeAllTargets();
			llvm::InitializeAllTargetMCs();
			std::string tripleErr;
			llvm::Triple triple(targetTriple);
#if LLVM_VERSION_MAJOR >= 21
			auto* tgt = llvm::TargetRegistry::lookupTarget(triple, tripleErr);
#elif LLVM_VERSION_MAJOR >= 20
			auto* tgt = llvm::TargetRegistry::lookupTarget(triple.getTriple(), tripleErr);
#else
			auto* tgt = llvm::TargetRegistry::lookupTarget(targetTriple, tripleErr);
#endif
			if (tgt) {
#if LLVM_VERSION_MAJOR >= 21
				auto tm = std::unique_ptr<llvm::TargetMachine>(tgt->createTargetMachine(
						triple, "generic", "", llvm::TargetOptions(), std::optional<llvm::Reloc::Model>()));
#else
				auto tm = std::unique_ptr<llvm::TargetMachine>(tgt->createTargetMachine(
						triple.getTriple(), "generic", "", llvm::TargetOptions(), std::optional<llvm::Reloc::Model>()));
#endif
				if (tm) {
					auto dl = tm->createDataLayout();
					// On i686 (and other 32-bit x86), the System V ABI uses 4-byte
					// alignment for int64_t/uint64_t. LLVM's default DataLayout for
					// i686 doesn't include i64:32:64, causing LLVM struct layouts to
					// disagree with C struct layouts. Patch the DataLayout to match.
					if (triple.getArch() == llvm::Triple::x86) {
						std::string dlStr = dl.getStringRepresentation();
						if (dlStr.find("i64:") == std::string::npos) {
							dlStr += "-i64:32:64";
							dl = llvm::DataLayout(dlStr);
						}
					}
					module->setDataLayout(dl);
				}
			}
#if LLVM_VERSION_MAJOR >= 20
			module->setTargetTriple(triple);
#else
			module->setTargetTriple(targetTriple);
#endif
		}

		setupRuntimeDeclarations();
		printTiming("setupRuntimeDeclarations");

		// Process import statements from all modules
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (auto* child : moduleRoot->children()) {
				if (auto importNode = dynamic_cast<AstNodeImport*>(child)) {
					// Process import: create external function declarations
					const std::string& namespaceName = importNode->namespaceName();
					const std::string& library = importNode->library();

					// Track library for linking
					importedLibraries.insert(library);

					for (const auto& func : importNode->functions()) {
						// Determine mangled name based on library
						std::string mangledName;
						if (library == "libstdqd.so") {
							// For libstdqd, use qd_stdqd_ prefix (matches C implementation)
							mangledName = "qd_stdqd_" + func->name;
						} else {
							// Check for stdlib imports: libqd* (quadpm modules) or known renamed stdlib
							if ((library.rfind("libqd", 0) == 0 && library.find(".a") != std::string::npos) ||
									isStdlibImport(library)) {
								mangledName = "usr_" + namespaceName + "_" + func->name;
							} else {
								mangledName = func->name;
							}
						}

						// Check if function already exists
						llvm::Function* fn = module->getFunction(mangledName);
						if (!fn) {
							// Create function type: int function(qd_context*)
							auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
							fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, *module);
						}

						// Register fallibility for this imported function
						std::string fullName = namespaceName + "::" + func->name;
						fallibleFunctions[fullName] = func->throws;
						// Mark as imported C function (handles its own success status)
						importedCFunctions.insert(fullName);

						// Track return struct type for imported functions
						for (const auto& outParam : func->outputParameters) {
							const std::string& typeStr = outParam->typeString();
							if (looksLikeStructType(typeStr)) {
								// Qualify with module name if needed
								std::string qualifiedType = extractStructName(typeStr);
								if (qualifiedType.find("::") == std::string::npos) {
									qualifiedType = moduleName + "::" + qualifiedType;
								}
								functionReturnStructType[fullName] = qualifiedType;
								break; // Use first struct-typed output
							}
						}

						// Also register this function in userFunctions with the scoped name
						// so that namespace::function calls work
						std::string scopedName = "usr_" + namespaceName + "_" + func->name;
						if (scopedName != mangledName && !module->getFunction(scopedName)) {
							// Create alias with usr_ prefix that calls the actual function
							auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
							auto aliasFn =
									llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, scopedName, *module);
							createForwardingWrapperBody(aliasFn, fn);
						}

						// If this is a public imported function, make it available for cross-module access
						if (func->isPublic) {
							// Register for cross-module access as module::funcname
							std::string crossModuleName = moduleName + "::" + func->name;
							CrossModuleImportInfo info;
							info.library = library;
							info.cFunctionName = mangledName;
							info.throws = func->throws;
							crossModuleImportedFunctions[crossModuleName] = info;

							// Also register fallibility under the module name
							fallibleFunctions[crossModuleName] = func->throws;
							importedCFunctions.insert(crossModuleName);

							// Track return struct type for cross-module access
							for (const auto& outParam : func->outputParameters) {
								const std::string& typeStr = outParam->typeString();
								if (looksLikeStructType(typeStr)) {
									// Qualify with module name if needed
									std::string qualifiedType = extractStructName(typeStr);
									if (qualifiedType.find("::") == std::string::npos) {
										qualifiedType = moduleName + "::" + qualifiedType;
									}
									functionReturnStructType[crossModuleName] = qualifiedType;
									break;
								}
							}

							// Create wrapper function accessible as usr_modulename_funcname
							std::string moduleScoped = "usr_" + moduleName + "_" + func->name;
							if (!module->getFunction(moduleScoped)) {
								auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
								auto wrapperFn = llvm::Function::Create(
										fnTy, llvm::Function::ExternalLinkage, moduleScoped, *module);
								createForwardingWrapperBody(wrapperFn, fn);
							}
						}

						// Create native bridge wrapper for qualifying imported C functions.
						// This allows functions calling these imports to use native calling convention.
						if (!func->throws) {
							bool allNumeric = true;
							std::vector<NativeParamType> bridgeInputTypes;
							for (const auto& param : func->inputParameters) {
								const std::string& typeStr = param->typeString();
								if (typeStr == "f64" || typeStr == "float" || typeStr == "float64" || typeStr == "f") {
									bridgeInputTypes.push_back(NativeParamType::F64);
								} else if (typeStr.empty() || typeStr == "i64" || typeStr == "int" ||
										   typeStr == "int64" || typeStr == "i") {
									bridgeInputTypes.push_back(NativeParamType::I64);
								} else {
									allNumeric = false;
									break;
								}
							}
							NativeParamType bridgeOutputType = NativeParamType::I64;
							size_t bridgeOutputCount = func->outputParameters.size();
							if (allNumeric && bridgeOutputCount <= 1) {
								for (const auto& param : func->outputParameters) {
									const std::string& typeStr = param->typeString();
									if (typeStr == "f64" || typeStr == "float" || typeStr == "float64" ||
											typeStr == "f") {
										bridgeOutputType = NativeParamType::F64;
									} else if (!typeStr.empty() && typeStr != "i64" && typeStr != "int" &&
											   typeStr != "int64" && typeStr != "i") {
										allNumeric = false;
										break;
									}
								}
							}
							if (allNumeric && bridgeInputTypes.size() > 0 && bridgeOutputCount <= 1) {
								size_t bridgeInputCount = bridgeInputTypes.size();
								std::string nativeName = mangledName + "_native";

								// Build native function type: (ctx, typed_params...) -> typed_result
								auto* doubleTy = builder->getDoubleTy();
								std::vector<llvm::Type*> nativeParams;
								nativeParams.push_back(contextPtrTy);
								for (size_t i = 0; i < bridgeInputCount; i++) {
									nativeParams.push_back(
											bridgeInputTypes[i] == NativeParamType::F64 ? doubleTy : int64Ty);
								}
								llvm::Type* nativeRetTy =
										(bridgeOutputCount == 0)
												? builder->getVoidTy()
												: (bridgeOutputType == NativeParamType::F64 ? doubleTy : int64Ty);
								auto nativeFnTy = llvm::FunctionType::get(nativeRetTy, nativeParams, false);
								auto nativeFn = llvm::Function::Create(
										nativeFnTy, llvm::Function::InternalLinkage, nativeName, *module);
								nativeFn->addParamAttr(0, llvm::Attribute::NonNull);
								nativeFn->addParamAttr(0, llvm::Attribute::NoAlias);
								nativeFn->addFnAttr(llvm::Attribute::NoUnwind);

								// Generate bridge body: push args to runtime stack, call C func, pop result
								auto bridgeEntryBB = llvm::BasicBlock::Create(*context, "entry", nativeFn);
								auto savedInsertPoint = builder->GetInsertBlock();
								auto savedInsertPos = builder->GetInsertPoint();
								builder->SetInsertPoint(bridgeEntryBB);

								auto bridgeCtx = nativeFn->getArg(0);
								// Push each typed arg to the runtime stack
								for (size_t i = 0; i < bridgeInputCount; i++) {
									auto arg = nativeFn->getArg(static_cast<unsigned>(i + 1));
									if (bridgeInputTypes[i] == NativeParamType::F64) {
										generateInlinePushFloatValue(bridgeCtx, arg);
									} else {
										generateInlinePushIntValue(bridgeCtx, arg);
									}
								}
								// Call the C function
								builder->CreateCall(fn, {bridgeCtx});
								// Pop result if any
								if (bridgeOutputCount == 1) {
									llvm::Value* result;
									if (bridgeOutputType == NativeParamType::F64) {
										result = generateInlinePopFloat(bridgeCtx);
									} else {
										result = generateInlinePopInt(bridgeCtx);
									}
									builder->CreateRet(result);
								} else {
									builder->CreateRetVoid();
								}

								// Restore insert point
								if (savedInsertPoint) {
									builder->SetInsertPoint(savedInsertPoint, savedInsertPos);
								}

								// Register under the namespace::name key
								std::string bridgeFullName = namespaceName + "::" + func->name;
								nativeFunctions[bridgeFullName] = nativeFn;
								nativeFuncInfo[bridgeFullName] = {
										bridgeInputCount, bridgeOutputCount, bridgeInputTypes, bridgeOutputType};

								// Also register under moduleName::name for cross-module access
								if (func->isPublic && moduleName != namespaceName) {
									std::string crossName = moduleName + "::" + func->name;
									nativeFunctions[crossName] = nativeFn;
									nativeFuncInfo[crossName] = {
											bridgeInputCount, bridgeOutputCount, bridgeInputTypes, bridgeOutputType};
								}
							}
						}
					}
				}
			}
		}

		printTiming("processModuleImports");

		// Collect constants from all modules
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (auto* child : moduleRoot->children()) {
				if (auto constNode = dynamic_cast<AstNodeConstant*>(child)) {
					// Store constant with scope::name key
					std::string fullName = moduleName + "::" + constNode->name();
					moduleConstants[fullName] = constNode->value();
					// Also store without scope for module-internal access
					moduleConstants[constNode->name()] = constNode->value();
				}
			}
		}

		// Collect enum variants from all modules
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}
			for (auto* child : moduleRoot->children()) {
				if (auto enumNode = dynamic_cast<AstNodeEnumDeclaration*>(child)) {
					for (const auto& variant : enumNode->variants()) {
						std::string val = std::to_string(variant.value);
						std::string enumScoped = enumNode->name() + "::" + variant.name;
						moduleConstants[enumScoped] = val;
						moduleConstants[moduleName + "::" + enumScoped] = val;
					}
				}
			}
		}

		// Collect constants from main file
		for (auto* child : root->children()) {
			if (auto constNode = dynamic_cast<AstNodeConstant*>(child)) {
				// Store constant with just the name (no scope prefix for main file)
				moduleConstants[constNode->name()] = constNode->value();
			}
			if (auto enumNode = dynamic_cast<AstNodeEnumDeclaration*>(child)) {
				for (const auto& variant : enumNode->variants()) {
					std::string enumScoped = enumNode->name() + "::" + variant.name;
					moduleConstants[enumScoped] = std::to_string(variant.value);
				}
			}
		}

		printTiming("collectConstants");

		// Collect module-level mutable vars (`var name:type = literal`) from
		// the main file AND every imported module (for `use "file.qd"` style
		// local merges, the imports show up as separate module ASTs). Emit
		// each as an LLVM global variable with the parsed initializer.
		// Reads/writes are lowered in generateIdentifier and
		// generateLocalOne respectively.
		std::vector<IAstNode*> allRootsForGlobals;
		allRootsForGlobals.push_back(root);
		for (const auto& modulePair : moduleASTs) {
			if (modulePair.second) {
				allRootsForGlobals.push_back(modulePair.second);
			}
		}
		for (auto* rootIt : allRootsForGlobals) {
			for (auto* child : rootIt->children()) {
				auto* varNode = dynamic_cast<AstNodeGlobalVar*>(child);
				if (!varNode) {
					continue;
				}

				// Skip if already collected from a different root.
				if (moduleGlobalVars.count(varNode->name())) {
					continue;
				}

				const std::string& name = varNode->name();
				const std::string& typeName = varNode->typeName();
				const std::string value = varNode->value();

				llvm::Type* ty = nullptr;
				llvm::Constant* init = nullptr;

				if (typeName == "f64") {
					ty = builder->getDoubleTy();
					double d = 0.0;
					if (!value.empty()) {
						char* endp = nullptr;
						d = std::strtod(value.c_str(), &endp);
						(void)endp;
					}
					init = llvm::ConstantFP::get(ty, d);
				} else if (typeName == "str") {
					// String globals always start as null. Refcounted string
					// machinery requires a heap-allocated header that can't
					// be expressed as an LLVM static initializer; if users
					// want content, they assign it at runtime. Usually
					// globals this big are ptrs anyway.
					ty = ptrTy;
					init = llvm::ConstantPointerNull::get(ptrTy);
				} else if (typeName == "ptr") {
					ty = ptrTy;
					init = llvm::ConstantPointerNull::get(ptrTy);
				} else {
					// Integer types (i8/i16/i32/i64/u8/u16/u32/u64). Stored in
					// an LLVM i64 — sized-int semantics live at memory-access
					// sites, not on module globals (which aren't part of any
					// packed record).
					ty = int64Ty;
					int64_t iv = 0;
					safeParseInt64(value, iv);
					init = builder->getInt64(static_cast<uint64_t>(iv));
				}

				auto* gv = new llvm::GlobalVariable(*module, ty,
						/*isConstant=*/false, llvm::GlobalValue::InternalLinkage, init, "qd_global_" + name);
				moduleGlobalVars[name] = gv;
				moduleGlobalVarTypes[name] = typeName;
			}
		}
		printTiming("collectGlobalVars");

		// Process struct declarations from all modules
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (auto* child : moduleRoot->children()) {
				if (auto typeAlias = dynamic_cast<AstNodeTypeAlias*>(child)) {
					typeAliases[typeAlias->name()] = typeAlias->targetType();
				}
				if (auto structNode = dynamic_cast<AstNodeStructDeclaration*>(child)) {
					processStructDeclaration(structNode, moduleName);
				}
			}
		}

		// Collect type aliases from main file
		for (auto* child : root->children()) {
			if (auto typeAlias = dynamic_cast<AstNodeTypeAlias*>(child)) {
				typeAliases[typeAlias->name()] = typeAlias->targetType();
			}
		}

		// Process struct declarations from main file (no module name)
		for (auto* child : root->children()) {
			if (auto structNode = dynamic_cast<AstNodeStructDeclaration*>(child)) {
				processStructDeclaration(structNode, "");
			}
		}

		// Generate destructor functions for all struct types (after all structs are known)
		generateStructDestructors();
		printTiming("processStructs");

		// Process import statements from main file
		for (auto* child : root->children()) {
			if (auto importNode = dynamic_cast<AstNodeImport*>(child)) {
				const std::string& namespaceName = importNode->namespaceName();
				const std::string& library = importNode->library();

				// Track library for linking
				importedLibraries.insert(library);

				for (const auto& func : importNode->functions()) {
					std::string mangledName;
					if (library == "libstdqd.so") {
						mangledName = "qd_stdqd_" + func->name;
					} else {
						// Check for stdlib imports: libqd* (quadpm modules) or known renamed stdlib
						if ((library.rfind("libqd", 0) == 0 && library.find(".a") != std::string::npos) ||
								isStdlibImport(library)) {
							mangledName = "usr_" + namespaceName + "_" + func->name;
						} else {
							mangledName = func->name;
						}
					}

					if (module->getFunction(mangledName)) {
						continue;
					}

					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
					auto fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, mangledName, *module);

					// Register fallibility for this imported function
					std::string fullName = namespaceName + "::" + func->name;
					fallibleFunctions[fullName] = func->throws;
					// Mark as imported C function (handles its own success status)
					importedCFunctions.insert(fullName);

					std::string scopedName = "usr_" + namespaceName + "_" + func->name;
					if (scopedName != mangledName && !module->getFunction(scopedName)) {
						auto aliasFn =
								llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, scopedName, *module);
						createForwardingWrapperBody(aliasFn, fn);
					}
				}
			}
		}

		// Determine if we should generate a C main() entry point
		// Check if there's a 'main' function in the root AND the module name looks like standalone
		// (not a module name like "repl_0" which starts with "repl_")
		bool hasMainFunction = false;
		for (auto* child : root->children()) {
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				if (funcNode->name() == "main") {
					hasMainFunction = true;
					break;
				}
			}
		}
		// Only generate C main for standalone mode (main module is "main" or a file path)
		// but NOT for REPL modules (which have names like "repl_0", "repl_1", etc.).
		// In freestanding mode we never generate a C main; the user provides
		// their own entry (typically `_start`) and we emit a thin shim below.
		bool isReplModule = (mainModuleName.find("repl_") == 0);
		bool generateCMain = hasMainFunction && !isReplModule && !freestandingMode;

		// Pre-pass: declare all user-defined functions from main file (for forward references)
		// This ensures functions can call each other regardless of definition order
		for (auto* child : root->children()) {
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				// Skip main function declaration in standalone mode - it will be the C main
				if (generateCMain && funcNode->name() == "main") {
					continue;
				}
				// Create function declaration with proper module prefix
				// For methods, use mangled name: ReceiverType_methodName
				std::string fnName;
				std::string registerName;
				if (funcNode->hasReceiver()) {
					fnName = "usr_" + mainModuleName + "_" + funcNode->receiverType() + "_" + funcNode->name();
					registerName = funcNode->receiverType() + "::" + funcNode->name();
				} else {
					fnName = "usr_" + mainModuleName + "_" + funcNode->name();
					registerName = funcNode->name();
				}
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				// Use InternalLinkage for user functions unless in export mode (shared library compilation)
				// InternalLinkage allows LLVM to eliminate unused functions via GlobalDCE
				// ExternalLinkage for export mode and for pub functions in freestanding mode
				// (pub freestanding functions may be called from assembly/C)
				bool needsExternal = exportMode || (freestandingMode && funcNode->isPublic());
				auto linkage = needsExternal ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
				auto fn = llvm::Function::Create(fnTy, linkage, fnName, *module);
				fn->addParamAttr(0, llvm::Attribute::NonNull);
				fn->addParamAttr(0, llvm::Attribute::NoAlias);
				fn->addFnAttr(llvm::Attribute::NoUnwind);
				if (funcNode->isInline()) {
					fn->addFnAttr(llvm::Attribute::AlwaysInline);
				}
				// Register the function for forward reference lookup
				userFunctions[registerName] = fn;
				fallibleFunctions[registerName] = funcNode->throws();
				// Track return struct type if output parameter is a struct
				const auto& outputs = funcNode->outputParameters();
				bool foundExplicitStructType = false;
				for (const auto& outParam : outputs) {
					if (auto* param = dynamic_cast<AstNodeParameter*>(outParam.get())) {
						const std::string& typeStr = param->typeString();
						if (looksLikeStructType(typeStr)) {
							// Store the unqualified struct name for lookup
							functionReturnStructType[registerName] = extractStructName(typeStr);
							foundExplicitStructType = true;
							break; // Use first struct-typed output
						}
					}
				}
				// If return type is ptr but body constructs a struct, infer the type
				if (!foundExplicitStructType && funcNode->body()) {
					std::string inferredType = findLastStructConstruction(funcNode->body());
					if (!inferredType.empty()) {
						functionReturnStructType[registerName] = inferredType;
					}
				}

				// Check if this function qualifies for native calling convention
				if (!funcNode->hasReceiver() && !funcNode->throws()) {
					bool allParamsNumeric = true;
					std::vector<NativeParamType> inputTypes;
					for (const auto& param : funcNode->inputParameters()) {
						if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param.get())) {
							const std::string& typeStr = paramNode->typeString();
							if (typeStr == "f64" || typeStr == "float" || typeStr == "float64" || typeStr == "f") {
								inputTypes.push_back(NativeParamType::F64);
							} else if (typeStr.empty() || typeStr == "i64" || typeStr == "int" || typeStr == "int64" ||
									   typeStr == "i") {
								inputTypes.push_back(NativeParamType::I64);
							} else {
								allParamsNumeric = false;
								break;
							}
						}
					}
					NativeParamType outputType = NativeParamType::I64;
					if (allParamsNumeric) {
						for (const auto& param : funcNode->outputParameters()) {
							if (const auto* paramNode = dynamic_cast<const AstNodeParameter*>(param.get())) {
								const std::string& typeStr = paramNode->typeString();
								if (typeStr == "f64" || typeStr == "float" || typeStr == "float64" || typeStr == "f") {
									outputType = NativeParamType::F64;
								} else if (!typeStr.empty() && typeStr != "i64" && typeStr != "int" &&
										   typeStr != "int64" && typeStr != "i") {
									allParamsNumeric = false;
									break;
								}
							}
						}
					}
					if (allParamsNumeric) {
						std::set<std::string> funcLocalNames;
						collectLocalNames(funcNode, funcLocalNames);
						bool bodyOk = true;
						for (auto* child2 : funcNode->children()) {
							if (!analyzeIsBodyNativeEligible(child2, funcLocalNames)) {
								bodyOk = false;
								break;
							}
						}
						if (bodyOk) {
							size_t inputCount = funcNode->inputParameters().size();
							size_t outputCount = funcNode->outputParameters().size();
							if (inputCount > 0 && outputCount <= 1) {
								std::string nativeName = fnName + "_native";
								llvm::Function* nativeFn = module->getFunction(nativeName);
								if (!nativeFn) {
									auto* doubleTy = builder->getDoubleTy();
									std::vector<llvm::Type*> nativeParams;
									nativeParams.push_back(contextPtrTy);
									for (size_t i = 0; i < inputCount; i++) {
										nativeParams.push_back(
												inputTypes[i] == NativeParamType::F64 ? doubleTy : int64Ty);
									}
									llvm::Type* nativeRetTy =
											(outputCount == 0)
													? builder->getVoidTy()
													: (outputType == NativeParamType::F64 ? doubleTy : int64Ty);
									auto nativeFnTy = llvm::FunctionType::get(nativeRetTy, nativeParams, false);
									nativeFn = llvm::Function::Create(nativeFnTy, linkage, nativeName, *module);
									nativeFn->addParamAttr(0, llvm::Attribute::NonNull);
									nativeFn->addParamAttr(0, llvm::Attribute::NoAlias);
									nativeFn->addFnAttr(llvm::Attribute::NoUnwind);
									if (funcNode->isInline()) {
										nativeFn->addFnAttr(llvm::Attribute::AlwaysInline);
									}
								}
								nativeFunctions[registerName] = nativeFn;
								nativeFuncInfo[registerName] = {inputCount, outputCount, inputTypes, outputType};
								// Also register with qualified name (mainModuleName::funcName)
								// since generateFunction uses that as registerName
								std::string qualifiedName = mainModuleName + "::" + funcNode->name();
								nativeFunctions[qualifiedName] = nativeFn;
								nativeFuncInfo[qualifiedName] = {inputCount, outputCount, inputTypes, outputType};
							}
						}
					}
				}
			}
		}

		printTiming("declareMainFunctions");

		// Compute reachable functions to skip generating IR for unused module functions
		if (useReachabilityAnalysis) {
			reachableFunctions.clear();
			computeReachableFunctions(root, reachableFunctions);
		}
		printTiming("computeReachability");

		// First pass: DECLARE all functions from all loaded modules
		// This ensures all functions are registered in userFunctions before any bodies are generated.
		// This is crucial for merged modules where cross-module calls need to resolve correctly.
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			for (auto* child : moduleRoot->children()) {
				if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
					// Track return struct type for module functions (same logic as main file)
					// For methods, use format: moduleName::ReceiverType::methodName
					std::string qualifiedName;
					if (funcNode->hasReceiver()) {
						qualifiedName = moduleName + "::" + funcNode->receiverType() + "::" + funcNode->name();
					} else {
						qualifiedName = moduleName + "::" + funcNode->name();
					}
					const auto& outputs = funcNode->outputParameters();
					bool foundExplicitStructType = false;
					for (const auto& outParam : outputs) {
						if (auto* param = dynamic_cast<AstNodeParameter*>(outParam.get())) {
							const std::string& typeStr = param->typeString();
							if (looksLikeStructType(typeStr)) {
								functionReturnStructType[qualifiedName] = extractStructName(typeStr);
								foundExplicitStructType = true;
								break;
							}
						}
					}
					// If return type is ptr but body constructs a struct, infer the type
					if (!foundExplicitStructType && funcNode->body()) {
						std::string inferredType = findLastStructConstruction(funcNode->body());
						if (!inferredType.empty()) {
							functionReturnStructType[qualifiedName] = inferredType;
						}
					}

					// For merged modules, also register return struct type with unqualified name
					// This allows sibling function calls to properly track struct return types
					if (!funcNode->hasReceiver() && mergedModules.count(moduleName) > 0) {
						auto retIt = functionReturnStructType.find(qualifiedName);
						if (retIt != functionReturnStructType.end()) {
							functionReturnStructType[funcNode->name()] = retIt->second;
						}
					}

					// Skip declaring unreachable functions
					if (useReachabilityAnalysis) {
						bool isReachable =
								reachableFunctions.count(qualifiedName) || reachableFunctions.count(funcNode->name()) ||
								(funcNode->hasReceiver() &&
										(reachableFunctions.count(funcNode->receiverType() + "::" + funcNode->name()) ||
												reachableFunctions.count(moduleName + "::" + funcNode->name())));
						if (!isReachable) {
							continue;
						}
					}

					// Declare the function (creates LLVM function and registers in userFunctions)
					if (!declareFunction(funcNode, moduleName)) {
						return false;
					}
				}
			}
		}
		printTiming("declareModuleFunctions");

		// Second pass: GENERATE function bodies from all loaded modules
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) {
				continue;
			}

			// Set current module name for struct lookups during code generation
			currentModuleName = moduleName;

			for (auto* child : moduleRoot->children()) {
				if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
					std::string qualifiedName;
					if (funcNode->hasReceiver()) {
						qualifiedName = moduleName + "::" + funcNode->receiverType() + "::" + funcNode->name();
					} else {
						qualifiedName = moduleName + "::" + funcNode->name();
					}

					// Skip generating bodies for unreachable functions
					if (useReachabilityAnalysis) {
						bool isReachable =
								reachableFunctions.count(qualifiedName) || reachableFunctions.count(funcNode->name()) ||
								(funcNode->hasReceiver() &&
										(reachableFunctions.count(funcNode->receiverType() + "::" + funcNode->name()) ||
												reachableFunctions.count(moduleName + "::" + funcNode->name())));
						if (!isReachable) {
							continue;
						}
					}

					// Generate module function body with module name as prefix
					if (!generateFunctionBody(funcNode, false, moduleName)) {
						currentModuleName = ""; // Reset on error
						return false;
					}
				}
			}
		}

		// Reset module name for main file generation
		currentModuleName = "";
		printTiming("generateModuleFunctions");

		// Second pass: generate all user-defined functions from main file
		for (auto* child : root->children()) {
			if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
				// Skip main function in standalone mode - handle separately
				if (generateCMain && funcNode->name() == "main" && !testMode) {
					continue;
				}
				// Generate as regular user function with module name prefix
				if (!generateFunction(funcNode, false, mainModuleName)) {
					return false;
				}
			}
		}

		printTiming("generateMainFunctions");

		// Third pass: generate test functions (in test mode)
		if (testMode) {
			collectedTestNames.clear();

			// Generate test functions from imported modules (not the main module)
			for (const auto& modulePair : moduleASTs) {
				const std::string& moduleName = modulePair.first;
				IAstNode* moduleRoot = modulePair.second;
				if (!moduleRoot) {
					continue;
				}
				// Skip the main module - it's handled separately below
				if (moduleName == mainModuleName) {
					continue;
				}

				for (auto* child : moduleRoot->children()) {
					if (auto testNode = dynamic_cast<AstNodeTest*>(child)) {
						if (!generateTest(testNode, moduleName)) {
							return false;
						}
						// Sanitize test name for function lookup
						std::string sanitizedName = testNode->name();
						for (char& c : sanitizedName) {
							if (!std::isalnum(c)) {
								c = '_';
							}
						}
						std::string funcName = "test_" + moduleName + "_" + sanitizedName;
						// Display name: module::test_name
						std::string displayName = moduleName + "::" + testNode->name();
						collectedTestNames.push_back({funcName, displayName});
					}
				}
			}

			// Generate test functions from main file
			for (auto* child : root->children()) {
				if (auto testNode = dynamic_cast<AstNodeTest*>(child)) {
					if (!generateTest(testNode, mainModuleName)) {
						return false;
					}
					std::string sanitizedName = testNode->name();
					for (char& c : sanitizedName) {
						if (!std::isalnum(c)) {
							c = '_';
						}
					}
					std::string funcName = "test_" + mainModuleName + "_" + sanitizedName;
					// For main module, extract just the filename for display
					std::string shortName = mainModuleName;
					auto lastSlash = shortName.rfind('/');
					if (lastSlash != std::string::npos) {
						shortName = shortName.substr(lastSlash + 1);
					}
					std::string displayName = shortName + "::" + testNode->name();
					collectedTestNames.push_back({funcName, displayName});
				}
			}

			// Generate test runner
			if (!generateTestRunner(collectedTestNames)) {
				return false;
			}
		}

		// Fourth pass: generate C main function (only in standalone mode, not in test mode)
		if (generateCMain && !testMode) {
			for (auto* child : root->children()) {
				if (auto funcNode = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
					if (funcNode->name() == "main") {
						if (!generateFunction(funcNode, true)) {
							return false;
						}
					}
				}
			}
		}

		// Freestanding mode: emit a C-callable `_start` shim that calls the
		// user's `pub fn _start( -- )` (or `pub fn main( -- )` as a fallback)
		// using a runtime-provided static context. The freestanding runtime
		// (libqdrt-freestanding.a) provides:
		//   extern qd_context qd_freestanding_ctx;   // statically allocated
		//   extern void qd_freestanding_halt(void);  // weak; user override
		if (freestandingMode && !testMode) {
			// Find the user entry function: prefer "_start", fall back to "main".
			AstNodeFunctionDeclaration* entryFn = nullptr;
			for (auto* child : root->children()) {
				if (auto fn = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
					if (fn->name() == "_start") {
						entryFn = fn;
						break;
					}
				}
			}
			if (!entryFn) {
				for (auto* child : root->children()) {
					if (auto fn = dynamic_cast<AstNodeFunctionDeclaration*>(child)) {
						if (fn->name() == "main") {
							entryFn = fn;
							break;
						}
					}
				}
			}
			if (!entryFn) {
				std::cerr << "Error: --freestanding requires a 'pub fn _start( -- )' "
							 "or 'pub fn main( -- )' in the main module\n";
				return false;
			}

			// Look up the already-generated user function: usr_<mainModuleName>__start (or _main).
			std::string userFnName = "usr_" + mainModuleName + "_" + entryFn->name();
			llvm::Function* userFn = module->getFunction(userFnName);
			if (!userFn) {
				std::cerr << "Error: --freestanding could not find generated entry function '" << userFnName << "'\n";
				return false;
			}
			// Promote to external linkage so the linker can keep it.
			userFn->setLinkage(llvm::Function::ExternalLinkage);

			// Declare the freestanding runtime's static context as an external
			// global. The runtime side is just `qd_context qd_freestanding_ctx;`.
			auto ctxGlobal = new llvm::GlobalVariable(*module, contextStructTy,
					/*isConstant=*/false, llvm::GlobalValue::ExternalLinkage,
					/*Initializer=*/nullptr, "qd_freestanding_ctx");

			// Declare the halt hook (weak so user can override).
			auto haltFnTy = llvm::FunctionType::get(builder->getVoidTy(), {}, false);
			auto haltFn = module->getOrInsertFunction("qd_freestanding_halt", haltFnTy);

			// Emit `void _start(void)`.
			auto startFnTy = llvm::FunctionType::get(builder->getVoidTy(), {}, false);
			auto startFn = llvm::Function::Create(startFnTy, llvm::Function::ExternalLinkage, "_start", *module);
			auto startBB = llvm::BasicBlock::Create(*context, "entry", startFn);
			builder->SetInsertPoint(startBB);
			builder->CreateCall(userFn, {ctxGlobal});
			builder->CreateCall(haltFn);
			builder->CreateUnreachable();
		}

		printTiming("generateCMain");

		// Verify module
		// Finalize debug info
		if (debugInfoEnabled && debugBuilder) {
			debugBuilder->finalize();
		}

		std::string errorMsg;
		llvm::raw_string_ostream errorStream(errorMsg);
		if (llvm::verifyModule(*module, &errorStream)) {
			std::cerr << "LLVM module verification failed:\n" << errorMsg << std::endl;
			return false;
		}
		printTiming("verifyModule");

		return !compilationFailed;
	}

	LlvmGenerator::LlvmGenerator() : mImpl(nullptr) {
	}

	LlvmGenerator::~LlvmGenerator() = default;

	void LlvmGenerator::setDebugInfo(bool enabled) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->debugInfoEnabled = enabled;
	}

	void LlvmGenerator::setOptimizationLevel(int level) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		// Clamp level to 0-3
		if (level < 0) {
			level = 0;
		}
		if (level > 3) {
			level = 3;
		}
		mImpl->optimizationLevel = level;
	}

	void LlvmGenerator::setExportMode(bool enabled) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->exportMode = enabled;
	}

	void LlvmGenerator::addLibrarySearchPath(const std::string& path) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->librarySearchPaths.push_back(path);
	}

	void LlvmGenerator::setStackSize(size_t size) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->stackSize = size;
	}

	void LlvmGenerator::setTestMode(bool enabled) {
		if (!mImpl) {
			// Create implementation with a temporary module name - will be recreated in generate()
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->testMode = enabled;
	}

	void LlvmGenerator::setCoverageMode(bool enabled) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->coverageMode = enabled;
	}

	void LlvmGenerator::setFreestandingMode(bool enabled) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->freestandingMode = enabled;
	}

	void LlvmGenerator::setTargetTriple(const std::string& triple) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>("temp");
		}
		mImpl->targetTriple = triple;
	}

	bool LlvmGenerator::generate(IAstNode* root, const std::string& moduleName) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>(moduleName);
		}
		// Store source filename for debug info
		// If moduleName looks like a file path (contains / or ends with .qd), use it directly
		// Otherwise append .qd extension
		if (moduleName.find('/') != std::string::npos || moduleName.find('\\') != std::string::npos ||
				(moduleName.size() > 3 && moduleName.substr(moduleName.size() - 3) == ".qd")) {
			mImpl->sourceFileName = moduleName;
		} else {
			mImpl->sourceFileName = moduleName + ".qd";
		}
		// Store the main module name
		mImpl->mainModuleName = moduleName;
		// Add main source file to moduleSourceFiles for stack trace info
		mImpl->moduleSourceFiles["main"] = mImpl->sourceFileName;
		// Also add with the actual module name for user function lookups
		mImpl->moduleSourceFiles[moduleName] = mImpl->sourceFileName;
		return mImpl->generateProgram(root);
	}

	void LlvmGenerator::addModuleAST(const std::string& moduleName, IAstNode* moduleRoot,
			const std::string& sourceFileName, bool mergeIntoMain) {
		if (!mImpl) {
			mImpl = std::make_unique<Impl>("quadrate_module");
		}
		mImpl->moduleASTs.push_back({moduleName, moduleRoot});
		if (!sourceFileName.empty()) {
			mImpl->moduleSourceFiles[moduleName] = sourceFileName;
		}
		if (mergeIntoMain) {
			mImpl->mergedModules.insert(moduleName);
		}
	}

	std::string LlvmGenerator::getIRString() const {
		if (!mImpl || !mImpl->module) {
			return "";
		}

		std::string str;
		llvm::raw_string_ostream os(str);
		mImpl->module->print(os, nullptr);
		return str;
	}

	bool LlvmGenerator::writeIR(const std::string& filename) {
		if (!mImpl || !mImpl->module) {
			return false;
		}

		std::error_code ec;
		llvm::raw_fd_ostream os(filename, ec);
		if (ec) {
			std::cerr << "Error opening file: " << ec.message() << std::endl;
			return false;
		}

		mImpl->module->print(os, nullptr);
		return true;
	}

	bool LlvmGenerator::writeObject(const std::string& filename) {
		// Timing helper - only active when QUADC_TIMING is set
		static bool timing = std::getenv("QUADC_TIMING") != nullptr;
		auto timeLast = std::chrono::steady_clock::now();
		auto printTiming = [&](const char* label) {
			if (timing) {
				auto now = std::chrono::steady_clock::now();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeLast).count();
				std::cerr << "[TIMING]   writeObject/" << label << ": " << ms << "ms" << std::endl;
				timeLast = now;
			}
		};

		if (!mImpl || !mImpl->module) {
			return false;
		}

		// Initialize targets
		llvm::InitializeAllTargetInfos();
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		llvm::InitializeAllAsmPrinters();
		printTiming("initTargets");

		// Use custom target triple if set, otherwise use host default
		std::string targetTripleStr;
		if (!mImpl->targetTriple.empty()) {
			targetTripleStr = mImpl->targetTriple;
		} else {
			targetTripleStr = llvm::sys::getDefaultTargetTriple();
		}
		llvm::Triple targetTriple(targetTripleStr);
// LLVM 20+ changed API to accept Triple objects instead of strings
#if LLVM_VERSION_MAJOR >= 20
		mImpl->module->setTargetTriple(targetTriple);
#else
		mImpl->module->setTargetTriple(targetTriple.getTriple());
#endif

		std::string error;
#if LLVM_VERSION_MAJOR >= 21
		auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
#else
		auto target = llvm::TargetRegistry::lookupTarget(targetTripleStr, error);
#endif
		if (!target) {
			std::cerr << "Error: " << error << std::endl;
			return false;
		}

		// Use host CPU for native compilation, "generic" for cross-compilation
		std::string cpu;
		if (!mImpl->targetTriple.empty()) {
			cpu = "generic";
		} else {
			cpu = std::string(llvm::sys::getHostCPUName());
		}
		auto features = "";
		llvm::TargetOptions opt;

		// Map optimization level to LLVM CodeGenOptLevel
		// Using None for -O0 significantly speeds up code generation
		llvm::CodeGenOptLevel codeGenOptLevel;
		switch (mImpl->optimizationLevel) {
		case 0:
			codeGenOptLevel = llvm::CodeGenOptLevel::None;
			break;
		case 1:
			codeGenOptLevel = llvm::CodeGenOptLevel::Less;
			break;
		case 2:
			codeGenOptLevel = llvm::CodeGenOptLevel::Default;
			break;
		default:
			codeGenOptLevel = llvm::CodeGenOptLevel::Aggressive;
			break;
		}

// LLVM 20+ changed API to accept Triple objects instead of strings
#if LLVM_VERSION_MAJOR >= 20
		std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(targetTriple, cpu, features, opt,
				std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_), std::nullopt, codeGenOptLevel));
#else
		std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(targetTriple.getTriple(), cpu,
				features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_), std::nullopt, codeGenOptLevel));
#endif

		mImpl->module->setDataLayout(targetMachine->createDataLayout());
		printTiming("createTargetMachine");

		// Always run GlobalDCE to eliminate unused internal functions (even at -O0)
		// This significantly reduces code generation time by not compiling dead code
		if (!mImpl->exportMode) {
			llvm::LoopAnalysisManager lam;
			llvm::FunctionAnalysisManager fam;
			llvm::CGSCCAnalysisManager cgam;
			llvm::ModuleAnalysisManager mam;
			llvm::PassBuilder pb(targetMachine.get());
			pb.registerModuleAnalyses(mam);
			pb.registerCGSCCAnalyses(cgam);
			pb.registerFunctionAnalyses(fam);
			pb.registerLoopAnalyses(lam);
			pb.crossRegisterProxies(lam, fam, cgam, mam);

			llvm::ModulePassManager npm;
			npm.addPass(llvm::AlwaysInlinerPass());
			// Only run DCE if not freestanding — freestanding pub functions may be
			// called from assembly/C and must not be removed.
			if (!mImpl->freestandingMode) {
				npm.addPass(llvm::GlobalDCEPass());
			}
			npm.run(*mImpl->module, mam);
		}
		printTiming("alwaysInlineAndDCE");

		// Run optimization passes if optimization level > 0
		if (mImpl->optimizationLevel > 0) {
			// Use new PassBuilder API (replaces legacy FunctionPassManager)
			// This gives us the full standard pipeline: inlining, vectorization,
			// LICM, GVN, SROA, loop unroll, SLP vectorize, etc.
			llvm::LoopAnalysisManager optLam;
			llvm::FunctionAnalysisManager optFam;
			llvm::CGSCCAnalysisManager optCgam;
			llvm::ModuleAnalysisManager optMam;

			llvm::PassBuilder optPb(targetMachine.get());
			optPb.registerModuleAnalyses(optMam);
			optPb.registerCGSCCAnalyses(optCgam);
			optPb.registerFunctionAnalyses(optFam);
			optPb.registerLoopAnalyses(optLam);
			optPb.crossRegisterProxies(optLam, optFam, optCgam, optMam);

			llvm::OptimizationLevel optLevel;
			switch (mImpl->optimizationLevel) {
			case 1:
				optLevel = llvm::OptimizationLevel::O1;
				break;
			case 2:
				optLevel = llvm::OptimizationLevel::O2;
				break;
			default:
				optLevel = llvm::OptimizationLevel::O3;
				break;
			}

			llvm::ModulePassManager optMpm = optPb.buildPerModuleDefaultPipeline(optLevel);
			optMpm.run(*mImpl->module, optMam);
			printTiming("optimizationPasses");
		}

		std::error_code ec;
		llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
		if (ec) {
			std::cerr << "Could not open file: " << ec.message() << std::endl;
			return false;
		}

		llvm::legacy::PassManager pass;
		if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
			std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
			return false;
		}
		printTiming("setupCodeGen");

		pass.run(*mImpl->module);
		printTiming("codeGen");

		dest.flush();

		return true;
	}

	bool LlvmGenerator::writeExecutable(const std::string& filename) {
		// Timing helper - only active when QUADC_TIMING is set
		static bool timing = std::getenv("QUADC_TIMING") != nullptr;
		auto start = std::chrono::steady_clock::now();

		// Generate object file first
		std::string objFile = filename + ".o";
		if (!writeObject(objFile)) {
			return false;
		}

		if (timing) {
			auto objEnd = std::chrono::steady_clock::now();
			auto objMs = std::chrono::duration_cast<std::chrono::milliseconds>(objEnd - start).count();
			std::cerr << "[TIMING] writeObject: " << objMs << "ms" << std::endl;
		}

		// Determine library directory
		std::string libDir;

		// Check QUADRATE_LIBDIR environment variable first
		if (const char* quadrateLibDir = std::getenv("QUADRATE_LIBDIR")) {
			std::filesystem::path libPath(quadrateLibDir);
			// Convert to absolute path if relative
			if (libPath.is_relative()) {
				libPath = std::filesystem::absolute(libPath);
			}
			if (std::filesystem::exists(libPath)) {
				libDir = libPath.string();
			}
		}
		// Check ./dist/lib (development build) - use absolute path
		if (libDir.empty()) {
			std::filesystem::path distLib = std::filesystem::absolute("./dist/lib");
			if (std::filesystem::exists(distLib)) {
				libDir = distLib.string();
			}
		}
		// Check relative to executable (installed binaries)
		if (libDir.empty()) {
			char exePathBuf[4096];
			int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
			if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
				std::error_code ec;
				std::filesystem::path exePath = std::filesystem::canonical(exePathBuf, ec);
				if (!ec) {
					std::filesystem::path exeDir = exePath.parent_path();
					std::filesystem::path installedLib = exeDir / ".." / "lib";
					if (std::filesystem::exists(installedLib)) {
						libDir = installedLib.string();
					}
				}
			}
		}
		// Check ~/.local/lib (user installation)
		if (libDir.empty()) {
			if (const char* home = std::getenv("HOME")) {
				std::filesystem::path localLib = std::filesystem::path(home) / ".local" / "lib";
				if (std::filesystem::exists(localLib)) {
					libDir = localLib.string();
				}
			}
		}
		// Check system library path
		if (libDir.empty()) {
			if (std::filesystem::exists("/usr/lib")) {
				libDir = "/usr/lib";
			}
		}

		// Standard library subdirectory (quadrate/) where static libs live
		std::string stdlibDir = libDir + "/quadrate";

		// Build library flags - link static libraries directly
		// Check for nested structure (build directory) first, then flat structure (dist/lib/quadrate/)

		// Track which libraries have been processed to avoid duplicates and circular dependencies
		std::set<std::string> processedLibraries;
		std::string allDepsFlags;

		// Helper function to recursively process a library and its .deps file
		std::function<void(const std::string&)> processLibraryDeps;
		processLibraryDeps = [&](const std::string& libPath) {
			// Skip if already processed
			if (processedLibraries.count(libPath)) {
				return;
			}
			processedLibraries.insert(libPath);

			// Check for .deps file
			std::string depsFile = libPath;
			if (depsFile.size() > 2 && depsFile.substr(depsFile.size() - 2) == ".a") {
				depsFile = depsFile.substr(0, depsFile.size() - 2) + ".deps";
			}

			if (std::filesystem::exists(depsFile)) {
				std::ifstream deps(depsFile);
				if (deps.is_open()) {
					std::string depLine;
					while (std::getline(deps, depLine)) {
						// Trim whitespace
						depLine.erase(0, depLine.find_first_not_of(" \t\r\n"));
						depLine.erase(depLine.find_last_not_of(" \t\r\n") + 1);
						if (!depLine.empty()) {
							// Expand ${VAR} environment variables
							size_t pos = 0;
							while ((pos = depLine.find("${", pos)) != std::string::npos) {
								size_t endPos = depLine.find("}", pos);
								if (endPos != std::string::npos) {
									std::string varName = depLine.substr(pos + 2, endPos - pos - 2);
									const char* varValue = std::getenv(varName.c_str());
									std::string replacement = varValue ? varValue : "";
									depLine.replace(pos, endPos - pos + 1, replacement);
									pos += replacement.length();
								} else {
									break;
								}
							}

							// Check if it's an absolute path to a .a file (transitive native dependency)
							if (depLine[0] == '/' && depLine.size() > 2 && depLine.substr(depLine.size() - 2) == ".a") {
								std::string resolvedDep = depLine;
								if (!std::filesystem::exists(depLine)) {
									// Absolute path doesn't exist — try stdlib libDir
									// Extract base name: "/path/to/libnet_static.a" -> "net"
									std::string fn = std::filesystem::path(depLine).filename().string();
									std::string bn = fn;
									if (bn.rfind("lib", 0) == 0) {
										bn = bn.substr(3);
									}
									if (bn.size() > 2 && bn.substr(bn.size() - 2) == ".a") {
										bn = bn.substr(0, bn.size() - 2);
									}
									if (bn.size() > 7 && bn.substr(bn.size() - 7) == "_static") {
										bn = bn.substr(0, bn.size() - 7);
									}
									std::string stdlibPath = stdlibDir + "/lib" + bn + ".a";
									if (std::filesystem::exists(stdlibPath)) {
										resolvedDep = stdlibPath;
									} else {
										// Try original filename in libDir
										stdlibPath = stdlibDir + "/" + fn;
										if (std::filesystem::exists(stdlibPath)) {
											resolvedDep = stdlibPath;
										}
									}
								}
								allDepsFlags += " " + resolvedDep;
								processLibraryDeps(resolvedDep);
							}
							// For -l<name> entries, try to resolve to full path if it's a Quadrate library
							else if (depLine.rfind("-l", 0) == 0 && depLine.size() > 2) {
								std::string depLibName = depLine.substr(2);
								std::string depLibFile = "lib" + depLibName + ".a";

								// Check flat path (e.g., dist/lib/quadrate/libtls.a)
								std::string flatDepLib = stdlibDir + "/" + depLibFile;
								// Check nested path: directory name matches library name
								// (e.g., build dir: lib/tls/libtls.a for -ltls)
								std::string nestedDepLib = libDir + "/" + depLibName + "/" + depLibFile;

								if (std::filesystem::exists(flatDepLib)) {
									allDepsFlags += " " + flatDepLib;
									processLibraryDeps(flatDepLib);
								} else if (std::filesystem::exists(nestedDepLib)) {
									allDepsFlags += " " + nestedDepLib;
									processLibraryDeps(nestedDepLib);
								} else {
									// System library, use -l flag as-is
									allDepsFlags += " " + depLine;
								}
							} else {
								allDepsFlags += " " + depLine;
							}
						}
					}
				}
			}
		};

		std::string qdrtStaticPath;
		// Try quadrate/ subdirectory first, then build dir nested structure, then legacy flat
		std::string quadratePath = stdlibDir + "/librt.a";
		std::string nestedPath = libDir + "/rt/librt_static.a";
		std::string flatPath = libDir + "/librt.a";
		std::string flatPathLegacy = libDir + "/librt_static.a";

		if (std::filesystem::exists(quadratePath)) {
			qdrtStaticPath = quadratePath;
		} else if (std::filesystem::exists(flatPath)) {
			qdrtStaticPath = flatPath;
		} else if (std::filesystem::exists(nestedPath)) {
			qdrtStaticPath = nestedPath;
		} else if (std::filesystem::exists(flatPathLegacy)) {
			qdrtStaticPath = flatPathLegacy;
		} else {
			// Fallback to flat path (will error later if doesn't exist)
			qdrtStaticPath = flatPath;
		}

		std::string libraryFlags = qdrtStaticPath;

		// Track resolved static library paths for use in lld command
		std::vector<std::string> resolvedStaticLibs;

		// Add imported libraries
		for (const auto& library : mImpl->importedLibraries) {
			// Check if it's already a .a file (static library)
			if (library.size() >= 2 && library.substr(library.size() - 2) == ".a") {
				// It's a static library, link it directly
				std::string foundLibPath;

				// First, check in additional library search paths (third-party packages)
				for (const auto& searchPath : mImpl->librarySearchPaths) {
					std::string candidatePath = searchPath + "/" + library;
					if (std::filesystem::exists(candidatePath)) {
						foundLibPath = candidatePath;
						break;
					}
				}

				// If not found in search paths, check main libDir
				if (foundLibPath.empty()) {
					std::string flatLib = stdlibDir + "/" + library;

					// Extract library name for nested search
					// Examples: "libmath.a" -> "math", "librt_static.a" -> "rt"
					std::string libBaseName = library;
					if (libBaseName.rfind("lib", 0) == 0) {
						libBaseName = libBaseName.substr(3); // Remove "lib" prefix
					}
					// Remove ".a" suffix first
					if (libBaseName.size() > 2 && libBaseName.substr(libBaseName.size() - 2) == ".a") {
						libBaseName = libBaseName.substr(0, libBaseName.size() - 2);
					}
					// Remove "_static" suffix if present
					if (libBaseName.size() > 7 && libBaseName.substr(libBaseName.size() - 7) == "_static") {
						libBaseName = libBaseName.substr(0, libBaseName.size() - 7);
					}
					std::string nestedLib = libDir + "/" + libBaseName + "/" + library;

					if (std::filesystem::exists(flatLib)) {
						foundLibPath = flatLib;
					} else if (std::filesystem::exists(nestedLib)) {
						foundLibPath = nestedLib;
					} else {
						// Try libDir directly (user libraries in dist/lib/)
						std::string libDirLib = libDir + "/" + library;
						if (std::filesystem::exists(libDirLib)) {
							foundLibPath = libDirLib;
						} else {
							// Try bare path (fallback)
							foundLibPath = library;
						}
					}
				}

				// Add the library and recursively process its .deps file for transitive dependencies
				libraryFlags += " " + foundLibPath;
				resolvedStaticLibs.push_back(foundLibPath);
				processLibraryDeps(foundLibPath);
			} else {
				// Handle .so libraries (dynamic linking)
				std::string libName = library;

				// Remove "lib" prefix and ".so" suffix to get library name
				if (libName.rfind("lib", 0) == 0) {
					libName = libName.substr(3);
				}
				if (libName.size() >= 3 && libName.substr(libName.size() - 3) == ".so") {
					libName = libName.substr(0, libName.size() - 3);
				}

				libraryFlags += " -l" + libName;
			}
		}

		// Add all stdlib native libraries so external modules can resolve stdlib symbols
		if (!stdlibDir.empty() && std::filesystem::exists(stdlibDir)) {
			for (const auto& entry : std::filesystem::directory_iterator(stdlibDir)) {
				if (!entry.is_regular_file()) {
					continue;
				}
				std::string fn = entry.path().filename().string();
				if ((fn.rfind("libqd", 0) == 0 || isStdlibImport(fn)) && fn.size() > 2 &&
						fn.substr(fn.size() - 2) == ".a" && fn != "libqd.a") {
					std::string libPath = entry.path().string();
					if (!processedLibraries.count(libPath)) {
						allDepsFlags += " " + libPath;
						processLibraryDeps(libPath);
					}
				}
			}
		}

		// Add standard system libraries
		// Note: C11 threads don't need -lpthread, but we need -lstdc++ for C++ filesystem code
		libraryFlags += " -lm -lstdc++";

		// Add transitive dependencies from .deps files
		// Use --start-group/--end-group to resolve circular dependencies between static libraries
		if (!allDepsFlags.empty()) {
			libraryFlags += " -Wl,--start-group" + allDepsFlags + " -Wl,--end-group";
		}

		// Note: We don't add -L flags for module lib directories because:
		// 1. Static libraries are already linked by full path
		// 2. Deps use system libraries via -l flags and shouldn't be shadowed by module libs
		// (e.g., a module named "glut" would have libglut.so which would shadow system -lglut)

		// Try to use ld.lld directly for faster linking
		// Fall back to clang if lld fails or is not available
		int result = -1;
		bool usedLld = false;

		// Check if ld.lld is available and we're not cross-compiling
		if (mImpl->targetTriple.empty()) {
			// Check for lld availability
			if (system("which ld.lld >/dev/null 2>&1") == 0) {
				// Try direct lld linking - significantly faster than going through clang
				// Find CRT files using standard locations
				std::string crtDir;
				std::string gccCrtDir;

				// Common CRT locations on Linux
				if (std::filesystem::exists("/usr/lib/crt1.o")) {
					crtDir = "/usr/lib";
				} else if (std::filesystem::exists("/usr/lib64/crt1.o")) {
					crtDir = "/usr/lib64";
				} else if (std::filesystem::exists("/usr/lib/x86_64-linux-gnu/crt1.o")) {
					crtDir = "/usr/lib/x86_64-linux-gnu";
				}

				// Find GCC CRT files (crtbegin.o, crtend.o) - try common GCC paths
				for (const auto& path : {"/usr/lib64/gcc/x86_64-pc-linux-gnu", "/usr/lib/gcc/x86_64-pc-linux-gnu",
							 "/usr/lib/gcc/x86_64-linux-gnu"}) {
					if (std::filesystem::exists(path)) {
						// Find the latest GCC version directory
						for (const auto& entry : std::filesystem::directory_iterator(path)) {
							if (entry.is_directory()) {
								std::string checkPath = entry.path().string() + "/crtbeginS.o";
								if (std::filesystem::exists(checkPath)) {
									gccCrtDir = entry.path().string();
									break;
								}
							}
						}
						if (!gccCrtDir.empty()) {
							break;
						}
					}
				}

				if (!crtDir.empty() && !gccCrtDir.empty()) {
					// Build lld command with CRT files
					std::string lldCmd = "ld.lld --hash-style=gnu --build-id --eh-frame-hdr -m elf_x86_64 -pie "
										 "-dynamic-linker /lib64/ld-linux-x86-64.so.2 "
										 "--gc-sections -o " +
										 filename + " " + crtDir + "/Scrt1.o " + crtDir + "/crti.o " + gccCrtDir +
										 "/crtbeginS.o " + "-L" + gccCrtDir + " -L" + crtDir + " " + objFile + " " +
										 qdrtStaticPath;

					// Add resolved static libraries (already have full paths)
					for (const auto& libPath : resolvedStaticLibs) {
						lldCmd += " " + libPath;
					}

					// Add dynamic libraries (.so)
					for (const auto& lib : mImpl->importedLibraries) {
						if (lib.size() >= 3 && lib.substr(lib.size() - 3) == ".so") {
							std::string libName = lib;
							if (libName.rfind("lib", 0) == 0) {
								libName = libName.substr(3);
							}
							libName = libName.substr(0, libName.size() - 3);
							lldCmd += " -l" + libName;
						}
					}

					// Add standard libraries and CRT files
					lldCmd += " -lm -lstdc++ -lgcc --as-needed -lgcc_s --no-as-needed -lc -lgcc --as-needed -lgcc_s "
							  "--no-as-needed " +
							  gccCrtDir + "/crtendS.o " + crtDir + "/crtn.o";

					// Add transitive dependencies
					if (!allDepsFlags.empty()) {
						lldCmd += " --start-group" + allDepsFlags + " --end-group";
					}

					// Suppress error output (errors will fall back to clang)
					lldCmd += " 2>/dev/null";

					if (timing) {
						auto lldStart = std::chrono::steady_clock::now();
						result = system(lldCmd.c_str());
						auto lldEnd = std::chrono::steady_clock::now();
						auto lldMs = std::chrono::duration_cast<std::chrono::milliseconds>(lldEnd - lldStart).count();
						std::cerr << "[TIMING] system(ld.lld): " << lldMs << "ms" << std::endl;
					} else {
						result = system(lldCmd.c_str());
					}

					if (result == 0) {
						usedLld = true;
					}
				}
			}
		}

		// Fall back to clang if lld wasn't used or failed
		if (!usedLld) {
			std::string linkCmd = "clang -Wl,--gc-sections -o " + filename + " " + objFile + " " + libraryFlags;

			if (timing) {
				auto linkStart = std::chrono::steady_clock::now();
				result = system(linkCmd.c_str());
				auto linkEnd = std::chrono::steady_clock::now();
				auto linkMs = std::chrono::duration_cast<std::chrono::milliseconds>(linkEnd - linkStart).count();
				std::cerr << "[TIMING] system(clang): " << linkMs << "ms" << std::endl;
			} else {
				result = system(linkCmd.c_str());
			}
		}

		// Clean up object file
		std::remove(objFile.c_str());

		return result == 0;
	}

	int LlvmGenerator::runJIT() {
		static bool timing = std::getenv("QUADC_TIMING") != nullptr;
		auto timeStart = std::chrono::steady_clock::now();
		auto printTiming = [&](const char* label) {
			if (timing) {
				auto now = std::chrono::steady_clock::now();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeStart).count();
				std::cerr << "[TIMING]   jit/" << label << ": " << ms << "ms" << std::endl;
				timeStart = now;
			}
		};

		if (!mImpl->module) {
			std::cerr << "Error: No module generated. Call generate() first." << std::endl;
			return -1;
		}

		// Initialize native target for JIT
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		printTiming("initTargets");

		// Load runtime library so JIT can resolve symbols
		// The runtime symbols need to be available in the current process
		void* rtHandle = dlopen("libqdrt.so", RTLD_NOW | RTLD_GLOBAL);
		if (!rtHandle) {
			// Try with full path from QUADRATE_LIBDIR
			const char* libDir = std::getenv("QUADRATE_LIBDIR");
			if (libDir) {
				std::string fullPath = std::string(libDir) + "/libqdrt.so";
				rtHandle = dlopen(fullPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
			}
			if (!rtHandle) {
				// Try relative to executable
				char exePathBuf[4096];
				int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
				if (len > 0) {
					std::filesystem::path exePath(exePathBuf);
					std::filesystem::path libPath = exePath.parent_path() / ".." / "lib" / "libqdrt.so";
					rtHandle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
				}
			}
			if (!rtHandle) {
				std::cerr << "Error: Could not load libqdrt.so for JIT execution: " << dlerror() << std::endl;
				return -1;
			}
		}
		printTiming("loadRuntime");

		// Create LLJIT instance with concurrent compilation and no optimization
		// JIT mode is for quick testing, so skip optimizations for faster compilation
		auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
		if (!jtmb) {
			std::cerr << "Error: Failed to detect host for JIT: " << toString(jtmb.takeError()) << std::endl;
			dlclose(rtHandle);
			return -1;
		}
		jtmb->setCodeGenOptLevel(llvm::CodeGenOptLevel::None);

		auto jitExpected = llvm::orc::LLJITBuilder()
								   .setJITTargetMachineBuilder(std::move(*jtmb))
								   .setNumCompileThreads(std::thread::hardware_concurrency())
								   .create();
		if (!jitExpected) {
			std::cerr << "Error: Failed to create JIT: " << toString(jitExpected.takeError()) << std::endl;
			dlclose(rtHandle);
			return -1;
		}
		auto jit = std::move(*jitExpected);
		printTiming("createJIT");

		// Add a generator for resolving symbols from the current process (including libqdrt)
		auto& mainJD = jit->getMainJITDylib();
		auto processSymbolsGenerator =
				llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(jit->getDataLayout().getGlobalPrefix());
		if (!processSymbolsGenerator) {
			std::cerr << "Error: Failed to create process symbol generator: "
					  << toString(processSymbolsGenerator.takeError()) << std::endl;
			dlclose(rtHandle);
			return -1;
		}
		mainJD.addGenerator(std::move(*processSymbolsGenerator));
		printTiming("setupSymbols");

		// Move the module to a ThreadSafeModule
		// We need to transfer ownership, so create new context/module
		auto tsMod = llvm::orc::ThreadSafeModule(std::move(mImpl->module), std::move(mImpl->context));

		// Add the module to the JIT
		if (auto err = jit->addIRModule(std::move(tsMod))) {
			std::cerr << "Error: Failed to add module to JIT: " << toString(std::move(err)) << std::endl;
			dlclose(rtHandle);
			return -1;
		}
		printTiming("addModule");

		// Look up the main function
		auto mainSymbol = jit->lookup("main");
		if (!mainSymbol) {
			std::string errMsg = toString(mainSymbol.takeError());
			// Check if this is a symbol resolution failure (missing stdlib symbols)
			// The error contains "materialize" when symbols can't be resolved
			if (errMsg.find("materialize") != std::string::npos || errMsg.find("usr_") != std::string::npos) {
				std::cerr << "\nJIT error: Program uses stdlib functions not available for JIT execution.\n";
				std::cerr << "Use --no-jit flag for programs using math, str, io, or other stdlib modules.\n";
			} else {
				std::cerr << "Error: Could not find 'main' function: " << errMsg << std::endl;
			}
			dlclose(rtHandle);
			return -1;
		}
		printTiming("lookupMain");

		// Cast to function pointer and call with argc/argv
		// The generated main expects: int main(int argc, char** argv)
		auto mainFn = mainSymbol->toPtr<int(int, char**)>();
		printTiming("prepareCall");

		// Set up minimal argc/argv (program name only, no user arguments)
		const char* programName = "quadc";
		char* argv[] = {const_cast<char*>(programName), nullptr};
		int result = mainFn(1, argv);
		printTiming("execute");

		dlclose(rtHandle);
		return result;
	}

} // namespace Qd
