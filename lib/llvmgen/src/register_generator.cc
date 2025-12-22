// Register-based LLVM code generator implementation

#include <llvmgen/register_generator.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Qd {

	RegisterGenerator::RegisterGenerator() {
		context = std::make_unique<llvm::LLVMContext>();
		module = std::make_unique<llvm::Module>("quadrate", *context);
		builder = std::make_unique<llvm::IRBuilder<>>(*context);
	}

	RegisterGenerator::~RegisterGenerator() {
		if (debugBuilder) {
			debugBuilder->finalize();
		}
	}

	void RegisterGenerator::setupTargetTriple() {
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmParser();
		llvm::InitializeNativeTargetAsmPrinter();

		auto targetTriple = llvm::sys::getDefaultTargetTriple();
		module->setTargetTriple(llvm::Triple(targetTriple));

		std::string error;
		auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
		if (!target) {
			std::cerr << "Error: " << error << "\n";
			return;
		}

		auto cpu = "generic";
		auto features = "";
		llvm::TargetOptions opt;
		auto rm = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
		auto targetMachine = target->createTargetMachine(llvm::Triple(targetTriple), cpu, features, opt, rm);
		module->setDataLayout(targetMachine->createDataLayout());
	}

	void RegisterGenerator::setupRuntimeDeclarations() {
		auto ptrTy = llvm::PointerType::getUnqual(*context);
		auto voidTy = builder->getVoidTy();
		auto i64Ty = builder->getInt64Ty();
		auto i32Ty = builder->getInt32Ty();

		// malloc(size_t) -> void*
		auto mallocFnTy = llvm::FunctionType::get(ptrTy, {i64Ty}, false);
		mallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);

		// free(void*)
		auto freeFnTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
		freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);

		// strdup(const char*) -> char*
		auto strdupFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		strdupFn = llvm::Function::Create(strdupFnTy, llvm::Function::ExternalLinkage, "strdup", *module);

		// printf(const char*, ...) -> int
		auto printfFnTy = llvm::FunctionType::get(i32Ty, {ptrTy}, true);
		auto printfFn = llvm::Function::Create(printfFnTy, llvm::Function::ExternalLinkage, "printf", *module);
		(void)printfFn;

		// puts(const char*) -> int
		auto putsFnTy = llvm::FunctionType::get(i32Ty, {ptrTy}, false);
		auto putsFn = llvm::Function::Create(putsFnTy, llvm::Function::ExternalLinkage, "puts", *module);
		(void)putsFn;

		// qd_string_create(const char*) -> qd_string_t*
		auto qdStringCreateFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		llvm::Function::Create(qdStringCreateFnTy, llvm::Function::ExternalLinkage, "qd_string_create", *module);

		// qd_string_retain(qd_string_t*) -> qd_string_t*
		auto qdStringRetainFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		qdStringRetainFn =
				llvm::Function::Create(qdStringRetainFnTy, llvm::Function::ExternalLinkage, "qd_string_retain", *module);

		// qd_string_release(qd_string_t*)
		auto qdStringReleaseFnTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
		qdStringReleaseFn = llvm::Function::Create(
				qdStringReleaseFnTy, llvm::Function::ExternalLinkage, "qd_string_release", *module);

		// qd_string_data(qd_string_t*) -> const char*
		auto qdStringDataFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		llvm::Function::Create(qdStringDataFnTy, llvm::Function::ExternalLinkage, "qd_string_data", *module);

		// qd_struct_alloc(size_t, destructor_fn) -> void*
		auto qdStructAllocFnTy = llvm::FunctionType::get(ptrTy, {i64Ty, ptrTy}, false);
		qdStructAllocFn =
				llvm::Function::Create(qdStructAllocFnTy, llvm::Function::ExternalLinkage, "qd_struct_alloc", *module);

		// qd_struct_retain(void*) -> void*
		auto qdStructRetainFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
		qdStructRetainFn =
				llvm::Function::Create(qdStructRetainFnTy, llvm::Function::ExternalLinkage, "qd_struct_retain", *module);

		// qd_struct_release(void*)
		auto qdStructReleaseFnTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
		qdStructReleaseFn = llvm::Function::Create(
				qdStructReleaseFnTy, llvm::Function::ExternalLinkage, "qd_struct_release", *module);

		// Array functions
		// qd_array_create(size_t capacity, int32_t elemType) -> qd_array_t*
		auto qdArrayCreateFnTy = llvm::FunctionType::get(ptrTy, {i64Ty, i32Ty}, false);
		llvm::Function::Create(qdArrayCreateFnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);

		// qd_array_push_int(qd_array_t*, int64_t) -> int
		auto qdArrayPushIntFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, i64Ty}, false);
		llvm::Function::Create(qdArrayPushIntFnTy, llvm::Function::ExternalLinkage, "qd_array_push_int", *module);

		// qd_array_push_float(qd_array_t*, double) -> int
		auto qdArrayPushFloatFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, builder->getDoubleTy()}, false);
		llvm::Function::Create(qdArrayPushFloatFnTy, llvm::Function::ExternalLinkage, "qd_array_push_float", *module);

		// qd_array_push_ptr(qd_array_t*, void*) -> int
		auto qdArrayPushPtrFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, ptrTy}, false);
		llvm::Function::Create(qdArrayPushPtrFnTy, llvm::Function::ExternalLinkage, "qd_array_push_ptr", *module);

		// qd_array_release(qd_array_t*)
		auto qdArrayReleaseFnTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
		llvm::Function::Create(qdArrayReleaseFnTy, llvm::Function::ExternalLinkage, "qd_array_release", *module);

		// qd_array_length(qd_array_t*) -> size_t
		auto qdArrayLengthFnTy = llvm::FunctionType::get(i64Ty, {ptrTy}, false);
		llvm::Function::Create(qdArrayLengthFnTy, llvm::Function::ExternalLinkage, "qd_array_length", *module);

		// qd_array_get_int(qd_array_t*, size_t, int64_t*) -> int
		auto qdArrayGetIntFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, i64Ty, ptrTy}, false);
		llvm::Function::Create(qdArrayGetIntFnTy, llvm::Function::ExternalLinkage, "qd_array_get_int", *module);

		// qd_array_get_float(qd_array_t*, size_t, double*) -> int
		auto qdArrayGetFloatFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, i64Ty, ptrTy}, false);
		llvm::Function::Create(qdArrayGetFloatFnTy, llvm::Function::ExternalLinkage, "qd_array_get_float", *module);

		// qd_array_get_ptr(qd_array_t*, size_t, void**) -> int
		auto qdArrayGetPtrFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, i64Ty, ptrTy}, false);
		llvm::Function::Create(qdArrayGetPtrFnTy, llvm::Function::ExternalLinkage, "qd_array_get_ptr", *module);

		// qd_array_retain(qd_array_t*)
		auto qdArrayRetainFnTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
		llvm::Function::Create(qdArrayRetainFnTy, llvm::Function::ExternalLinkage, "qd_array_retain", *module);

		// String functions
		// qd_string_concat_smart(qd_string_t*, qd_string_t*) -> qd_string_t*
		auto qdStringConcatSmartFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false);
		llvm::Function::Create(
				qdStringConcatSmartFnTy, llvm::Function::ExternalLinkage, "qd_string_concat_smart", *module);

		// qd_string_length(qd_string_t*) -> size_t
		auto qdStringLengthFnTy = llvm::FunctionType::get(i64Ty, {ptrTy}, false);
		llvm::Function::Create(qdStringLengthFnTy, llvm::Function::ExternalLinkage, "qd_string_length", *module);

		// qd_string_from_int(int64_t) -> qd_string_t*
		auto qdStringFromIntFnTy = llvm::FunctionType::get(ptrTy, {i64Ty}, false);
		llvm::Function::Create(qdStringFromIntFnTy, llvm::Function::ExternalLinkage, "qd_string_from_int", *module);

		// qd_string_from_double(double) -> qd_string_t*
		auto qdStringFromDoubleFnTy = llvm::FunctionType::get(ptrTy, {builder->getDoubleTy()}, false);
		llvm::Function::Create(
				qdStringFromDoubleFnTy, llvm::Function::ExternalLinkage, "qd_string_from_double", *module);

		// qd_string_create_with_length(const char*, size_t) -> qd_string_t*
		auto qdStringCreateWithLengthFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, i64Ty}, false);
		llvm::Function::Create(
				qdStringCreateWithLengthFnTy, llvm::Function::ExternalLinkage, "qd_string_create_with_length", *module);

		// C string functions
		// strlen(const char*) -> size_t
		auto strlenFnTy = llvm::FunctionType::get(i64Ty, {ptrTy}, false);
		llvm::Function::Create(strlenFnTy, llvm::Function::ExternalLinkage, "strlen", *module);

		// strstr(const char*, const char*) -> char*
		auto strstrFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false);
		llvm::Function::Create(strstrFnTy, llvm::Function::ExternalLinkage, "strstr", *module);

		// strncmp(const char*, const char*, size_t) -> int
		auto strncmpFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, ptrTy, i64Ty}, false);
		llvm::Function::Create(strncmpFnTy, llvm::Function::ExternalLinkage, "strncmp", *module);

		// strcmp(const char*, const char*) -> int
		auto strcmpFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, ptrTy}, false);
		llvm::Function::Create(strcmpFnTy, llvm::Function::ExternalLinkage, "strcmp", *module);

		// strchr(const char*, int) -> char*
		auto strchrFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, i32Ty}, false);
		llvm::Function::Create(strchrFnTy, llvm::Function::ExternalLinkage, "strchr", *module);

		// memcpy(void*, const void*, size_t) -> void*
		auto memcpyFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy, i64Ty}, false);
		llvm::Function::Create(memcpyFnTy, llvm::Function::ExternalLinkage, "memcpy", *module);

		// memset(void*, int, size_t) -> void*
		auto memsetFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, i32Ty, i64Ty}, false);
		llvm::Function::Create(memsetFnTy, llvm::Function::ExternalLinkage, "memset", *module);

		// snprintf(char*, size_t, const char*, ...) -> int
		auto snprintfFnTy = llvm::FunctionType::get(i32Ty, {ptrTy, i64Ty, ptrTy}, true);
		llvm::Function::Create(snprintfFnTy, llvm::Function::ExternalLinkage, "snprintf", *module);

		// strtoll(const char*, char**, int) -> int64_t
		auto strtollFnTy = llvm::FunctionType::get(i64Ty, {ptrTy, ptrTy, i32Ty}, false);
		llvm::Function::Create(strtollFnTy, llvm::Function::ExternalLinkage, "strtoll", *module);

		// strtod(const char*, char**) -> double
		auto strtodFnTy = llvm::FunctionType::get(builder->getDoubleTy(), {ptrTy, ptrTy}, false);
		llvm::Function::Create(strtodFnTy, llvm::Function::ExternalLinkage, "strtod", *module);
	}

	bool RegisterGenerator::generate(IAstNode* root, const std::string& moduleName) {
		mainModuleName = moduleName;
		currentModulePrefix = moduleName;

		setupTargetTriple();
		setupRuntimeDeclarations();

		return generateProgram(root);
	}

	void RegisterGenerator::addModuleAST(const std::string& moduleName, IAstNode* moduleRoot,
										 const std::string& sourceFile) {
		moduleASTs.push_back({moduleName, moduleRoot});
		(void)sourceFile;
	}

	bool RegisterGenerator::generateProgram(IAstNode* root) {
		if (!root) {
			return false;
		}

		// First pass: collect struct definitions and function signatures
		for (size_t i = 0; i < root->childCount(); i++) {
			IAstNode* child = root->child(i);
			if (child->type() == IAstNode::Type::STRUCT_DECLARATION) {
				processStructDeclaration(static_cast<AstNodeStructDeclaration*>(child), currentModulePrefix);
			}
		}

		// Process module ASTs
		for (const auto& pair : moduleASTs) {
			const std::string& modName = pair.first;
			IAstNode* modRoot = pair.second;

			std::string savedPrefix = currentModulePrefix;
			currentModulePrefix = modName;

			for (size_t i = 0; i < modRoot->childCount(); i++) {
				IAstNode* child = modRoot->child(i);
				if (child->type() == IAstNode::Type::STRUCT_DECLARATION) {
					processStructDeclaration(static_cast<AstNodeStructDeclaration*>(child), modName);
				}
			}

			currentModulePrefix = savedPrefix;
		}

		// Generate struct destructors
		generateStructDestructors();

		// Process import statements from modules (before function declarations)
		for (const auto& modulePair : moduleASTs) {
			const std::string& moduleName = modulePair.first;
			IAstNode* moduleRoot = modulePair.second;
			if (!moduleRoot) continue;

			for (size_t i = 0; i < moduleRoot->childCount(); i++) {
				auto* child = moduleRoot->child(i);
				if (child->type() == IAstNode::Type::IMPORT_STATEMENT) {
					processImportStatement(static_cast<AstNodeImport*>(child), moduleName);
				}
			}
		}

		// Process import statements from main file
		for (size_t i = 0; i < root->childCount(); i++) {
			auto* child = root->child(i);
			if (child->type() == IAstNode::Type::IMPORT_STATEMENT) {
				processImportStatement(static_cast<AstNodeImport*>(child), currentModulePrefix);
			}
		}

		// Second pass: generate function declarations (for forward references)
		for (size_t i = 0; i < root->childCount(); i++) {
			IAstNode* child = root->child(i);
			if (child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
				auto* funcNode = static_cast<AstNodeFunctionDeclaration*>(child);
				std::string funcName = mangleName(funcNode->name(), currentModulePrefix);

				// Create function type based on signature
				std::vector<llvm::Type*> paramTypes;
				const auto& inputParams = funcNode->inputParameters();
				for (size_t j = 0; j < inputParams.size(); j++) {
					auto* param = static_cast<AstNodeParameter*>(inputParams[j]);
					ValueType vt = typeFromString(param->typeString());
					paramTypes.push_back(getLlvmType(vt));
				}

				// Determine return type
				const auto& outputParams = funcNode->outputParameters();
				llvm::Type* returnType = builder->getVoidTy();
				bool isFallible = funcNode->throws();

				if (isFallible) {
					// Fallible functions return struct {result..., errcode:i64}
					std::vector<llvm::Type*> returnTypes;
					for (size_t j = 0; j < outputParams.size(); j++) {
						auto* outParam = static_cast<AstNodeParameter*>(outputParams[j]);
						returnTypes.push_back(getLlvmType(typeFromString(outParam->typeString())));
					}
					returnTypes.push_back(builder->getInt64Ty()); // Error code
					returnType = llvm::StructType::get(*context, returnTypes);
				} else if (outputParams.size() == 1) {
					auto* outParam = static_cast<AstNodeParameter*>(outputParams[0]);
					returnType = getLlvmType(typeFromString(outParam->typeString()));
				} else if (outputParams.size() > 1) {
					// Multiple returns - use a struct type
					std::vector<llvm::Type*> returnTypes;
					for (size_t j = 0; j < outputParams.size(); j++) {
						auto* outParam = static_cast<AstNodeParameter*>(outputParams[j]);
						returnTypes.push_back(getLlvmType(typeFromString(outParam->typeString())));
					}
					returnType = llvm::StructType::get(*context, returnTypes);
				}

				auto* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
				auto* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, funcName, *module);
				userFunctions[funcName] = func;

				// Store signature info
				RegFunctionSignature sig;
				for (size_t j = 0; j < inputParams.size(); j++) {
					auto* param = static_cast<AstNodeParameter*>(inputParams[j]);
					sig.params.push_back({param->name(), typeFromString(param->typeString())});
				}
				for (size_t j = 0; j < outputParams.size(); j++) {
					auto* outParam = static_cast<AstNodeParameter*>(outputParams[j]);
					std::string typeName = outParam->typeString();
					ValueType vt = typeFromString(typeName);
					// For PTR types, the typeName is the struct type name (e.g., "uri::Url")
					std::string structTypeName = (vt == ValueType::PTR) ? typeName : "";
					sig.returns.push_back({vt, structTypeName});
				}
				sig.isFallible = funcNode->throws();
				functionSignatures[funcName] = sig;
				fallibleFunctions[funcName] = funcNode->throws();
			}
		}

		// Helper lambda to check if function has 'any' type parameters (now supported)
		[[maybe_unused]] auto hasAnyTypeParam = [](AstNodeFunctionDeclaration* funcNode) -> bool {
			for (size_t j = 0; j < funcNode->inputParameters().size(); j++) {
				auto* param = static_cast<AstNodeParameter*>(funcNode->inputParameters()[j]);
				if (param->typeString() == "any") return true;
			}
			for (size_t j = 0; j < funcNode->outputParameters().size(); j++) {
				auto* param = static_cast<AstNodeParameter*>(funcNode->outputParameters()[j]);
				if (param->typeString() == "any") return true;
			}
			return false;
		};

		// Generate module functions
		for (const auto& pair : moduleASTs) {
			const std::string& modName = pair.first;
			IAstNode* modRoot = pair.second;

			std::string savedPrefix = currentModulePrefix;
			currentModulePrefix = modName;

			for (size_t i = 0; i < modRoot->childCount(); i++) {
				IAstNode* child = modRoot->child(i);
				if (child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
					auto* funcNode = static_cast<AstNodeFunctionDeclaration*>(child);

					// 'any' type parameters are now supported (treated as i64)
					std::string funcName = mangleName(funcNode->name(), modName);

					// Create function type
					std::vector<llvm::Type*> paramTypes;
					const auto& inputParams = funcNode->inputParameters();
					for (size_t j = 0; j < inputParams.size(); j++) {
						auto* param = static_cast<AstNodeParameter*>(inputParams[j]);
						paramTypes.push_back(getLlvmType(typeFromString(param->typeString())));
					}

					llvm::Type* returnType = builder->getVoidTy();
					const auto& outputParams = funcNode->outputParameters();
					bool modFuncIsFallible = funcNode->throws();

					if (modFuncIsFallible) {
						// Fallible functions return struct {result..., errcode:i64}
						std::vector<llvm::Type*> returnTypes;
						for (size_t j = 0; j < outputParams.size(); j++) {
							auto* outParam = static_cast<AstNodeParameter*>(outputParams[j]);
							returnTypes.push_back(getLlvmType(typeFromString(outParam->typeString())));
						}
						returnTypes.push_back(builder->getInt64Ty()); // Error code
						returnType = llvm::StructType::get(*context, returnTypes);
					} else if (outputParams.size() == 1) {
						auto* outParam = static_cast<AstNodeParameter*>(outputParams[0]);
						returnType = getLlvmType(typeFromString(outParam->typeString()));
					} else if (outputParams.size() > 1) {
						// Multiple returns - use a struct type
						std::vector<llvm::Type*> returnTypes;
						for (size_t j = 0; j < outputParams.size(); j++) {
							auto* outParam = static_cast<AstNodeParameter*>(outputParams[j]);
							returnTypes.push_back(getLlvmType(typeFromString(outParam->typeString())));
						}
						returnType = llvm::StructType::get(*context, returnTypes);
					}

					auto* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
					auto* func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, funcName, *module);
					userFunctions[funcName] = func;

					RegFunctionSignature sig;
					for (size_t j = 0; j < inputParams.size(); j++) {
						auto* param = static_cast<AstNodeParameter*>(inputParams[j]);
						sig.params.push_back({param->name(), typeFromString(param->typeString())});
					}
					for (size_t j = 0; j < outputParams.size(); j++) {
						auto* outParam = static_cast<AstNodeParameter*>(outputParams[j]);
						std::string typeName = outParam->typeString();
						ValueType vt = typeFromString(typeName);
						// For PTR types, the typeName is the struct type name
						// Prefix with module name if not already qualified
						std::string structTypeName = "";
						if (vt == ValueType::PTR) {
							if (typeName.find("::") != std::string::npos) {
								structTypeName = typeName;
							} else {
								structTypeName = modName + "::" + typeName;
							}
						}
						sig.returns.push_back({vt, structTypeName});
					}
					sig.isFallible = funcNode->throws();
					functionSignatures[funcName] = sig;
				} else if (child->type() == IAstNode::Type::CONSTANT_DECLARATION) {
					// Collect module constants
					auto* constNode = static_cast<AstNodeConstant*>(child);
					std::string fullName = modName + "::" + constNode->name();
					moduleConstants[fullName] = constNode->value();
					// Also store without scope for module-internal access
					moduleConstants[constNode->name()] = constNode->value();
				}
			}

			currentModulePrefix = savedPrefix;
		}

		// Third pass: generate function bodies
		bool hasMain = false;
		for (size_t i = 0; i < root->childCount(); i++) {
			IAstNode* child = root->child(i);
			if (child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
				auto* funcNode = static_cast<AstNodeFunctionDeclaration*>(child);
				bool isMain = (funcNode->name() == "main");
				if (isMain) hasMain = true;
				if (!generateFunction(funcNode, isMain, currentModulePrefix)) {
					return false;
				}
			} else if (child->type() == IAstNode::Type::TEST_DECLARATION && testMode) {
				if (!generateTest(static_cast<AstNodeTest*>(child), currentModulePrefix)) {
					return false;
				}
			}
		}

		// Generate module function bodies
		for (const auto& pair : moduleASTs) {
			const std::string& modName = pair.first;
			IAstNode* modRoot = pair.second;

			std::string savedPrefix = currentModulePrefix;
			currentModulePrefix = modName;

			for (size_t i = 0; i < modRoot->childCount(); i++) {
				IAstNode* child = modRoot->child(i);
				if (child->type() == IAstNode::Type::FUNCTION_DECLARATION) {
					auto* funcNode = static_cast<AstNodeFunctionDeclaration*>(child);

					// Skip functions with 'any' type parameters (not yet supported)
					if (hasAnyTypeParam(funcNode)) {
						continue;
					}

					if (!generateFunction(funcNode, false, modName)) {
						currentModulePrefix = savedPrefix;
						return false;
					}
				}
			}

			currentModulePrefix = savedPrefix;
		}

		// Generate test runner if in test mode
		if (testMode && !collectedTestNames.empty()) {
			return generateTestRunner();
		}

		// Ensure we have a main function
		if (!hasMain && !testMode) {
			// Create a minimal main function
			auto* mainFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
			auto* mainFn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);
			auto* entry = llvm::BasicBlock::Create(*context, "entry", mainFn);
			builder->SetInsertPoint(entry);
			builder->CreateRet(builder->getInt32(0));
		}

		// Verify the module
		std::string errorInfo;
		llvm::raw_string_ostream errorStream(errorInfo);
		if (llvm::verifyModule(*module, &errorStream)) {
			std::cerr << "Module verification failed:\n" << errorInfo << "\n";
			return false;
		}

		return !compilationFailed;
	}

	bool RegisterGenerator::generateFunction(AstNodeFunctionDeclaration* funcNode, bool isMain,
											 const std::string& namePrefix) {
		std::string funcName = isMain ? "main" : mangleName(funcNode->name(), namePrefix);

		// Get the function (already declared)
		llvm::Function* func = nullptr;
		if (isMain) {
			// Create the actual main function with int return type
			auto* mainFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
			func = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);
		} else {
			func = userFunctions[funcName];
			if (!func) {
				reportError("Function not found: " + funcName, funcNode->line());
				return false;
			}
		}

		currentFunction = func;
		currentFunctionIsFallible = funcNode->throws();

		// Create entry block
		auto* entry = llvm::BasicBlock::Create(*context, "entry", func);
		builder->SetInsertPoint(entry);

		// Clear local variables
		localVariables.clear();
		valueStack.clear();

		// Push parameters onto the value stack (in reverse order so first param is deeper)
		// This matches Quadrate's stack-based calling convention where the body uses
		// "-> name" to pop values from the stack into named locals
		if (!isMain) {
			const auto& inputParams = funcNode->inputParameters();
			size_t argIdx = 0;
			for (auto& arg : func->args()) {
				if (argIdx < inputParams.size()) {
					auto* param = static_cast<AstNodeParameter*>(inputParams[argIdx]);
					ValueType vt = typeFromString(param->typeString());
					arg.setName(param->name());

					// For struct type parameters, determine the struct type name
					std::string structTypeName;
					if (vt == ValueType::PTR) {
						// Check if it's a known struct type
						std::string prefixedType = namePrefix + "::" + param->typeString();
						if (findStructDefinition(prefixedType)) {
							structTypeName = prefixedType;
						} else if (findStructDefinition(param->typeString())) {
							structTypeName = param->typeString();
						}
					}

					// Push the argument value onto the compile-time stack
					valueStack.push_back(TrackedValue(&arg, vt, structTypeName, false));
				}
				argIdx++;
			}
		}

		// Generate function body
		pushDeferScope();
		if (funcNode->body()) {
			generateNode(funcNode->body());
		}
		executeDeferScope();
		popDeferScope();

		// Handle return
		if (isMain) {
			// Main returns 0
			if (!builder->GetInsertBlock()->getTerminator()) {
				builder->CreateRet(builder->getInt32(0));
			}
		} else if (currentFunctionIsFallible) {
			// Fallible function: return struct {result..., errcode=1 (Ok)}
			if (!builder->GetInsertBlock()->getTerminator()) {
				auto* retType = llvm::cast<llvm::StructType>(func->getReturnType());
				llvm::Value* retVal = llvm::UndefValue::get(retType);

				// Collect return values from stack
				std::vector<llvm::Value*> returnValues;
				for (size_t i = 0; i < funcNode->outputParameters().size() && !valueStack.empty(); i++) {
					returnValues.push_back(valueStack.back().value);
					valueStack.pop_back();
				}
				// Reverse to get correct order
				std::reverse(returnValues.begin(), returnValues.end());

				// Pack result values
				for (size_t i = 0; i < returnValues.size(); i++) {
					retVal = builder->CreateInsertValue(retVal, returnValues[i], static_cast<unsigned>(i));
				}
				// Add Ok (1) error code as last element
				retVal = builder->CreateInsertValue(retVal, builder->getInt64(1),
					static_cast<unsigned>(funcNode->outputParameters().size()));
				builder->CreateRet(retVal);
			}
		} else {
			// Return value(s) from value stack
			if (!builder->GetInsertBlock()->getTerminator()) {
				if (funcNode->outputParameters().size() == 0) {
					builder->CreateRetVoid();
				} else if (funcNode->outputParameters().size() == 1) {
					if (!valueStack.empty()) {
						builder->CreateRet(valueStack.back().value);
						valueStack.pop_back();
					} else {
						// Return default value
						auto retType = func->getReturnType();
						if (retType->isIntegerTy()) {
							builder->CreateRet(builder->getInt64(0));
						} else if (retType->isDoubleTy()) {
							builder->CreateRet(llvm::ConstantFP::get(builder->getDoubleTy(), 0.0));
						} else if (retType->isPointerTy()) {
							builder->CreateRet(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context)));
						} else {
							builder->CreateRetVoid();
						}
					}
				} else {
					// Multiple return values - pack into struct
					std::vector<llvm::Value*> returnValues;
					for (size_t i = 0; i < funcNode->outputParameters().size() && !valueStack.empty(); i++) {
						returnValues.push_back(valueStack.back().value);
						valueStack.pop_back();
					}
					// Reverse to get correct order
					std::reverse(returnValues.begin(), returnValues.end());

					auto* retType = llvm::cast<llvm::StructType>(func->getReturnType());
					llvm::Value* retVal = llvm::UndefValue::get(retType);
					for (size_t i = 0; i < returnValues.size(); i++) {
						retVal = builder->CreateInsertValue(retVal, returnValues[i], static_cast<unsigned>(i));
					}
					builder->CreateRet(retVal);
				}
			}
		}

		currentFunction = nullptr;
		return true;
	}

	bool RegisterGenerator::generateTest(AstNodeTest* testNode, const std::string& namePrefix) {
		std::string testName = mangleName("test_" + testNode->name(), namePrefix);
		collectedTestNames.push_back({testName, testNode->name()});

		// Create test function
		auto* testFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
		auto* testFn = llvm::Function::Create(testFnTy, llvm::Function::InternalLinkage, testName, *module);

		currentFunction = testFn;

		auto* entry = llvm::BasicBlock::Create(*context, "entry", testFn);
		builder->SetInsertPoint(entry);

		localVariables.clear();
		valueStack.clear();

		pushDeferScope();
		if (testNode->body()) {
			generateNode(testNode->body());
		}
		executeDeferScope();
		popDeferScope();

		if (!builder->GetInsertBlock()->getTerminator()) {
			builder->CreateRet(builder->getInt32(0));
		}

		currentFunction = nullptr;
		return true;
	}

	bool RegisterGenerator::generateTestRunner() {
		// Create main function that runs all tests
		auto* mainFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
		auto* mainFn = llvm::Function::Create(mainFnTy, llvm::Function::ExternalLinkage, "main", *module);

		auto* entry = llvm::BasicBlock::Create(*context, "entry", mainFn);
		builder->SetInsertPoint(entry);

		// Call each test
		for (const auto& test : collectedTestNames) {
			auto* testFn = module->getFunction(test.first);
			if (testFn) {
				builder->CreateCall(testFn);
			}
		}

		builder->CreateRet(builder->getInt32(0));
		return true;
	}

	void RegisterGenerator::generateNode(IAstNode* node) {
		if (!node) return;

		switch (node->type()) {
			case IAstNode::Type::LITERAL:
				generateLiteral(static_cast<AstNodeLiteral*>(node));
				break;

			case IAstNode::Type::IDENTIFIER:
				generateIdentifier(static_cast<AstNodeIdentifier*>(node));
				break;

			case IAstNode::Type::INSTRUCTION:
				generateInstruction(static_cast<AstNodeInstruction*>(node));
				break;

			case IAstNode::Type::LOCAL:
				generateLocal(static_cast<AstNodeLocal*>(node));
				break;

			case IAstNode::Type::SCOPED_IDENTIFIER:
				generateScopedIdentifier(static_cast<AstNodeScopedIdentifier*>(node));
				break;

			case IAstNode::Type::IF_STATEMENT:
				generateIf(static_cast<AstNodeIfStatement*>(node));
				break;

			case IAstNode::Type::FOR_STATEMENT:
				generateFor(static_cast<AstNodeForStatement*>(node));
				break;

			case IAstNode::Type::WHILE_STATEMENT:
				generateWhile(static_cast<AstNodeWhileStatement*>(node));
				break;

			case IAstNode::Type::LOOP_STATEMENT:
				generateLoop(static_cast<AstNodeLoopStatement*>(node));
				break;

			case IAstNode::Type::SWITCH_STATEMENT:
				generateSwitch(static_cast<AstNodeSwitchStatement*>(node));
				break;

			case IAstNode::Type::CTX_STATEMENT:
				generateCtxBlock(static_cast<AstNodeCtx*>(node));
				break;

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
				generateFunctionPointer(static_cast<AstNodeFunctionPointerReference*>(node));
				break;

			case IAstNode::Type::ANONYMOUS_FUNCTION:
				generateAnonymousFunction(static_cast<AstNodeAnonymousFunction*>(node));
				break;

			case IAstNode::Type::STRUCT_CONSTRUCTION: {
				auto* structConstruction = static_cast<AstNodeStructConstruction*>(node);
				generateStructConstructionNamed(structConstruction);
				break;
			}

			case IAstNode::Type::FIELD_ACCESS:
				generateFieldAccess(static_cast<AstNodeFieldAccess*>(node));
				break;

			case IAstNode::Type::FIELD_SET:
				generateFieldSet(static_cast<AstNodeFieldSet*>(node));
				break;

			case IAstNode::Type::BREAK_STATEMENT:
				if (!loopStack.empty()) {
					executeDeferScope();
					// Store values back to allocas before breaking (same as continue)
					if (!loopStack.back().stackAllocas.empty()) {
						size_t minSize = std::min(valueStack.size(), loopStack.back().stackAllocas.size());
						for (size_t i = 0; i < minSize; i++) {
							builder->CreateStore(valueStack[i].value, loopStack.back().stackAllocas[i]);
						}
					}
					builder->CreateBr(loopStack.back().breakTarget);
				}
				break;

			case IAstNode::Type::CONTINUE_STATEMENT:
				if (!loopStack.empty()) {
					executeDeferScope();
					// For while loops, store the new condition before continuing
					if (loopStack.back().conditionAlloca && !valueStack.empty()) {
						builder->CreateStore(valueStack.back().value, loopStack.back().conditionAlloca);
						valueStack.pop_back();
					}
					// For loops with stack allocas, store values back before continuing
					if (!loopStack.back().stackAllocas.empty()) {
						size_t minSize = std::min(valueStack.size(), loopStack.back().stackAllocas.size());
						for (size_t i = 0; i < minSize; i++) {
							builder->CreateStore(valueStack[i].value, loopStack.back().stackAllocas[i]);
						}
					}
					builder->CreateBr(loopStack.back().continueTarget);
				}
				break;

			case IAstNode::Type::RETURN_STATEMENT: {
				executeDeferScope();
				if (!valueStack.empty() && currentFunction->getReturnType() != builder->getVoidTy()) {
					builder->CreateRet(valueStack.back().value);
					valueStack.pop_back();
				} else {
					if (currentFunction->getReturnType() == builder->getVoidTy()) {
						builder->CreateRetVoid();
					} else {
						// Return default
						builder->CreateRet(llvm::Constant::getNullValue(currentFunction->getReturnType()));
					}
				}
				break;
			}

			case IAstNode::Type::DEFER_STATEMENT:
				if (!deferScopeStack.empty()) {
					deferScopeStack.back().push_back(static_cast<AstNodeDefer*>(node));
				}
				break;

			case IAstNode::Type::ARRAY_LITERAL:
				generateArrayLiteral(static_cast<AstNodeArrayLiteral*>(node));
				break;

			case IAstNode::Type::BLOCK:
			case IAstNode::Type::PROGRAM:
				// Process children
				for (size_t i = 0; i < node->childCount(); i++) {
					generateNode(node->child(i));
					if (builder->GetInsertBlock()->getTerminator()) {
						break; // Stop after terminator
					}
				}
				break;

			default:
				// Process children for any other node types
				for (size_t i = 0; i < node->childCount(); i++) {
					generateNode(node->child(i));
				}
				break;
		}
	}

	void RegisterGenerator::generateLiteral(AstNodeLiteral* lit) {
		switch (lit->literalType()) {
			case AstNodeLiteral::LiteralType::INTEGER: {
				int64_t value = 0;
				auto [ptr, ec] = std::from_chars(lit->value().data(), lit->value().data() + lit->value().size(), value);
				if (ec == std::errc()) {
					valueStack.push_back(TrackedValue(builder->getInt64(static_cast<uint64_t>(value)), ValueType::INT));
				}
				break;
			}

			case AstNodeLiteral::LiteralType::FLOAT: {
				double value = std::stod(lit->value());
				valueStack.push_back(TrackedValue(llvm::ConstantFP::get(builder->getDoubleTy(), value), ValueType::FLOAT));
				break;
			}

			case AstNodeLiteral::LiteralType::STRING: {
				// Extract string content (remove surrounding quotes)
				std::string content = lit->value();
				if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
					content = content.substr(1, content.size() - 2);
				}

				// Process escape sequences
				std::string processed;
				for (size_t i = 0; i < content.size(); i++) {
					if (content[i] == '\\' && i + 1 < content.size()) {
						switch (content[i + 1]) {
						case 'n':
							processed += '\n';
							i++;
							break;
						case 't':
							processed += '\t';
							i++;
							break;
						case 'r':
							processed += '\r';
							i++;
							break;
						case '\\':
							processed += '\\';
							i++;
							break;
						case '"':
							processed += '"';
							i++;
							break;
						default:
							processed += content[i];
						}
					} else {
						processed += content[i];
					}
				}

				// Create a global string constant
				auto* strConst = builder->CreateGlobalString(processed, "str");

				// Create a qd_string from it
				auto* createFn = module->getFunction("qd_string_create");
				if (createFn) {
					auto* qdStr = builder->CreateCall(createFn, {strConst}, "qdstr");
					TrackedValue val(qdStr, ValueType::STRING, true);
					val.isStringLiteral = false; // It's a new allocation, needs release
					valueStack.push_back(val);
				} else {
					// Fallback: just use the raw string pointer
					valueStack.push_back(TrackedValue(strConst, ValueType::STRING, false));
				}
				break;
			}

		}
	}

	void RegisterGenerator::generateIdentifier(AstNodeIdentifier* ident) {
		const std::string& name = ident->name();

		// Check if it's a local variable
		auto it = localVariables.find(name);
		if (it != localVariables.end()) {
			auto* loaded = builder->CreateLoad(getLlvmType(it->second.type), it->second.alloca, name);
			valueStack.push_back(TrackedValue(loaded, it->second.type, it->second.structType, false, it->second.closureFuncType));
			return;
		}

		// Check if it's a captured variable (from closure)
		auto capIt = capturedVariableRefs.find(name);
		if (capIt != capturedVariableRefs.end()) {
			// Captured variables have extra indirection: ptrAlloca -> outerAlloca -> value
			auto* outerPtr = builder->CreateLoad(llvm::PointerType::getUnqual(*context), capIt->second, name + "_outer");
			// Look up the type from closureVariables or use a default
			auto typeIt = capturedVariableTypes.find(name);
			ValueType vt = typeIt != capturedVariableTypes.end() ? typeIt->second : ValueType::INT;
			auto* loaded = builder->CreateLoad(getLlvmType(vt), outerPtr, name);
			valueStack.push_back(TrackedValue(loaded, vt, "", false));
			return;
		}

		// Check if it's a struct constructor
		auto structIt = structDefinitions.find(name);
		if (structIt != structDefinitions.end()) {
			generateStructConstruction(name);
			return;
		}

		// Check if it's a module-prefixed struct
		std::string prefixedName = currentModulePrefix + "::" + name;
		structIt = structDefinitions.find(prefixedName);
		if (structIt != structDefinitions.end()) {
			generateStructConstruction(prefixedName);
			return;
		}

		// Check if it's a function call
		std::string funcName = mangleName(name, currentModulePrefix);
		auto funcIt = userFunctions.find(funcName);
		if (funcIt != userFunctions.end()) {
			auto* func = funcIt->second;
			auto sigIt = functionSignatures.find(funcName);

			// Pop arguments from value stack
			std::vector<llvm::Value*> args;
			if (sigIt != functionSignatures.end()) {
				for (size_t i = 0; i < sigIt->second.params.size() && !valueStack.empty(); i++) {
					args.insert(args.begin(), valueStack.back().value);
					valueStack.pop_back();
				}
			}

			auto* result = builder->CreateCall(func, args);

			// Push return value(s)
			if (sigIt != functionSignatures.end()) {
				bool funcIsFallible = sigIt->second.isFallible;

				if (funcIsFallible) {
					// Fallible function returns struct {result..., errcode}
					for (size_t i = 0; i < sigIt->second.returns.size(); i++) {
						auto* extracted = builder->CreateExtractValue(result, static_cast<unsigned>(i));
						const auto& retInfo = sigIt->second.returns[i];
						valueStack.push_back(TrackedValue(extracted, retInfo.first, retInfo.second, true));
					}
					// Extract and push error code
					auto* errCode = builder->CreateExtractValue(result,
						static_cast<unsigned>(sigIt->second.returns.size()));
					valueStack.push_back(TrackedValue(errCode, ValueType::INT, "", false));
				} else if (!sigIt->second.returns.empty()) {
					if (sigIt->second.returns.size() == 1) {
						const auto& retInfo = sigIt->second.returns[0];
						valueStack.push_back(TrackedValue(result, retInfo.first, retInfo.second, true));
					} else {
						// Multiple returns - extract from struct
						for (size_t i = 0; i < sigIt->second.returns.size(); i++) {
							auto* extracted = builder->CreateExtractValue(result, static_cast<unsigned>(i));
							const auto& retInfo = sigIt->second.returns[i];
							valueStack.push_back(TrackedValue(extracted, retInfo.first, retInfo.second, true));
						}
					}
				}
			}
			return;
		}

		// Unknown identifier - might be a constant
		auto constIt = moduleConstants.find(name);
		if (constIt != moduleConstants.end()) {
			// Parse the constant value
			int64_t value = 0;
			auto [ptr, ec] =
					std::from_chars(constIt->second.data(), constIt->second.data() + constIt->second.size(), value);
			if (ec == std::errc()) {
				valueStack.push_back(TrackedValue(builder->getInt64(static_cast<uint64_t>(value)), ValueType::INT));
				return;
			}
		}

		// Try as builtin instruction (parser sometimes classifies instructions as identifiers)
		if (tryGenerateBuiltinInstruction(name)) {
			return;
		}

		reportError("Unknown identifier: " + name, ident->line());
	}

	bool RegisterGenerator::tryGenerateBuiltinInstruction(const std::string& name) {
		// Math builtins that might be parsed as identifiers
		static const std::set<std::string> singleArgMath = {
			"sin", "cos", "tan", "asin", "acos", "atan",
			"sinh", "cosh", "tanh", "exp", "ln", "log", "log10", "log2",
			"sqrt", "cbrt", "ceil", "floor", "round", "trunc", "abs", "fabs", "sq"
		};
		static const std::set<std::string> twoArgMath = {"pow"};

		if (singleArgMath.count(name)) {
			if (valueStack.empty()) return false;
			auto val = valueStack.back();
			valueStack.pop_back();
			llvm::Value* floatVal = val.value;
			if (val.type == ValueType::INT) {
				floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
			}

			llvm::Value* result = nullptr;
			if (name == "sin") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sin, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "cos") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::cos, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "tan") {
				auto* sinFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sin, {builder->getDoubleTy()});
				auto* cosFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::cos, {builder->getDoubleTy()});
				auto* sinVal = builder->CreateCall(sinFn, {floatVal});
				auto* cosVal = builder->CreateCall(cosFn, {floatVal});
				result = builder->CreateFDiv(sinVal, cosVal);
			} else if (name == "exp") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::exp, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "ln" || name == "log") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::log, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "log10") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::log10, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "log2") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::log2, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "sqrt") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sqrt, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "ceil") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::ceil, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "floor") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::floor, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "round") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::round, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "trunc") {
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::trunc, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "abs" || name == "fabs") {
				if (val.type == ValueType::INT) {
					auto* neg = builder->CreateNeg(val.value);
					auto* isNeg = builder->CreateICmpSLT(val.value, builder->getInt64(0));
					result = builder->CreateSelect(isNeg, neg, val.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
					return true;
				}
				auto* fn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::fabs, {builder->getDoubleTy()});
				result = builder->CreateCall(fn, {floatVal});
			} else if (name == "sq") {
				// Square: x * x (preserves type)
				if (val.type == ValueType::INT) {
					result = builder->CreateMul(val.value, val.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
					return true;
				}
				result = builder->CreateFMul(val.value, val.value);
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				return true;
			} else {
				// Not implemented yet - put value back
				valueStack.push_back(val);
				return false;
			}
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			return true;
		}

		if (twoArgMath.count(name)) {
			if (valueStack.size() < 2) return false;
			auto exp = valueStack.back();
			valueStack.pop_back();
			auto base = valueStack.back();
			valueStack.pop_back();
			llvm::Value* baseFloat = base.value;
			llvm::Value* expFloat = exp.value;
			if (base.type == ValueType::INT) {
				baseFloat = builder->CreateSIToFP(base.value, builder->getDoubleTy());
			}
			if (exp.type == ValueType::INT) {
				expFloat = builder->CreateSIToFP(exp.value, builder->getDoubleTy());
			}
			auto* powFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::pow, {builder->getDoubleTy()});
			auto* result = builder->CreateCall(powFn, {baseFloat, expFloat});
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			return true;
		}

		return false;
	}

	void RegisterGenerator::generateInstruction(AstNodeInstruction* inst) {
		std::string name = inst->name();

		// Handle generic make<T> instruction
		std::string makeElementType;  // For struct arrays, stores the element struct type
		if (name == "make" && inst->hasTypeParam()) {
			const std::string& typeParam = inst->typeParam();
			// Map type param to appropriate make function
			if (typeParam == "i64" || typeParam == "i32" || typeParam == "i16" || typeParam == "i8" ||
				typeParam == "u64" || typeParam == "u32" || typeParam == "u16" || typeParam == "u8") {
				name = "makei";
			} else if (typeParam == "f64" || typeParam == "f32") {
				name = "makef";
			} else if (typeParam == "str" || typeParam == "string") {
				name = "makes";
			} else {
				// Struct type - use makep and store element type
				name = "makep";
				makeElementType = typeParam;
			}
		}

		// Arithmetic
		if (name == "add" || name == "+") {
			generateAdd();
		} else if (name == "sub" || name == "-") {
			generateSub();
		} else if (name == "mul" || name == "*") {
			generateMul();
		} else if (name == "div" || name == "/") {
			generateDiv();
		} else if (name == "mod" || name == "%") {
			generateMod();
		}
		// Comparisons
		else if (name == "lt" || name == "<") {
			generateLt();
		} else if (name == "gt" || name == ">") {
			generateGt();
		} else if (name == "eq" || name == "=" || name == "==") {
			generateEq();
		} else if (name == "neq" || name == "!=") {
			generateNeq();
		} else if (name == "lte" || name == "<=") {
			generateLte();
		} else if (name == "gte" || name == ">=") {
			generateGte();
		}
		// Bitwise
		else if (name == "and" || name == "&") {
			generateBitAnd();
		} else if (name == "or" || name == "|") {
			generateBitOr();
		} else if (name == "xor" || name == "^") {
			generateBitXor();
		} else if (name == "not" || name == "~") {
			generateBitNot();
		} else if (name == "shl" || name == "<<") {
			generateBitLshift();
		} else if (name == "shr" || name == ">>") {
			generateBitRshift();
		}
		// Stack operations
		else if (name == "dup") {
			generateDup();
		} else if (name == "drop") {
			generateDrop();
		} else if (name == "swap") {
			generateSwap();
		} else if (name == "over") {
			generateOver();
		} else if (name == "rot") {
			generateRot();
		} else if (name == "nip") {
			generateNip();
		} else if (name == "tuck") {
			generateTuck();
		}
		// Depth stack operations
		else if (name == "dupd") {
			// ( a b -- a a b ) - duplicate second element
			if (valueStack.size() >= 2) {
				auto b = valueStack.back();
				valueStack.pop_back();
				auto a = valueStack.back();
				// Stack is now [a], we want [a, a, b]
				valueStack.push_back(a);  // a, a
				valueStack.push_back(b);  // a, a, b
			}
		} else if (name == "swapd") {
			// ( a b c -- b a c ) - swap second and third elements
			if (valueStack.size() >= 3) {
				auto c = valueStack.back();
				valueStack.pop_back();
				auto b = valueStack.back();
				valueStack.pop_back();
				auto a = valueStack.back();
				valueStack.pop_back();
				valueStack.push_back(b);
				valueStack.push_back(a);
				valueStack.push_back(c);
			}
		} else if (name == "overd") {
			// ( a b c -- a b a c ) - copy third element over second
			if (valueStack.size() >= 3) {
				auto c = valueStack.back();
				valueStack.pop_back();
				auto b = valueStack.back();
				valueStack.pop_back();
				auto a = valueStack.back();
				// Stack is now [a], we want [a, b, a, c]
				valueStack.push_back(b);
				valueStack.push_back(a);
				valueStack.push_back(c);
			}
		} else if (name == "nipd") {
			// ( a b c -- a c ) - remove second element
			if (valueStack.size() >= 3) {
				auto c = valueStack.back();
				valueStack.pop_back();
				valueStack.pop_back();  // Remove b
				// a is still on stack
				valueStack.push_back(c);
			}
		}
		// Advanced stack operations
		else if (name == "pick") {
			// ( ... n -- ... elem ) - copy nth element to top (0-indexed from top)
			if (!valueStack.empty()) {
				auto nVal = valueStack.back();
				valueStack.pop_back();
				// n must be a constant integer at compile time for register-based
				if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(nVal.value)) {
					size_t n = static_cast<size_t>(constInt->getSExtValue());
					if (n < valueStack.size()) {
						// Index from top: stack[size - 1 - n]
						size_t idx = valueStack.size() - 1 - n;
						valueStack.push_back(valueStack[idx]);
					}
				}
			}
		} else if (name == "roll") {
			// ( ... n -- ... ) - rotate top n elements
			if (!valueStack.empty()) {
				auto nVal = valueStack.back();
				valueStack.pop_back();
				// n must be a constant integer at compile time for register-based
				if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(nVal.value)) {
					size_t n = static_cast<size_t>(constInt->getSExtValue());
					if (n > 0 && n <= valueStack.size()) {
						// Extract top n elements
						std::vector<TrackedValue> topN;
						for (size_t i = 0; i < n; i++) {
							topN.insert(topN.begin(), valueStack.back());
							valueStack.pop_back();
						}
						// Rotate: move first element to end
						if (!topN.empty()) {
							auto first = topN.front();
							topN.erase(topN.begin());
							topN.push_back(first);
						}
						// Push back
						for (auto& v : topN) {
							valueStack.push_back(v);
						}
					}
				}
			}
		} else if (name == "dup2") {
			// ( a b -- a b a b ) - duplicate top 2 elements
			if (valueStack.size() >= 2) {
				auto b = valueStack[valueStack.size() - 1];
				auto a = valueStack[valueStack.size() - 2];
				valueStack.push_back(a);
				valueStack.push_back(b);
			}
		} else if (name == "drop2") {
			// ( a b -- ) - remove top 2 elements
			if (valueStack.size() >= 2) {
				valueStack.pop_back();
				valueStack.pop_back();
			}
		} else if (name == "swap2") {
			// ( a b c d -- c d a b ) - swap top two pairs
			if (valueStack.size() >= 4) {
				auto d = valueStack.back();
				valueStack.pop_back();
				auto c = valueStack.back();
				valueStack.pop_back();
				auto b = valueStack.back();
				valueStack.pop_back();
				auto a = valueStack.back();
				valueStack.pop_back();
				valueStack.push_back(c);
				valueStack.push_back(d);
				valueStack.push_back(a);
				valueStack.push_back(b);
			}
		} else if (name == "over2") {
			// ( a b c d -- a b c d a b ) - copy second pair to top
			if (valueStack.size() >= 4) {
				auto a = valueStack[valueStack.size() - 4];
				auto b = valueStack[valueStack.size() - 3];
				valueStack.push_back(a);
				valueStack.push_back(b);
			}
		} else if (name == "depth") {
			// Push the current stack depth
			auto* depth = builder->getInt64(static_cast<uint64_t>(valueStack.size()));
			valueStack.push_back(TrackedValue(depth, ValueType::INT));
		} else if (name == "clear") {
			// Clear the entire stack
			valueStack.clear();
		} else if (name == "prints") {
			// Print all values on the stack (from bottom to top) separated by spaces
			auto* printfFn = module->getFunction("printf");
			if (printfFn) {
				for (size_t i = 0; i < valueStack.size(); i++) {
					if (i > 0) {
						// Print space separator
						auto* spaceFmt = builder->CreateGlobalString(" ");
						builder->CreateCall(printfFn, {spaceFmt});
					}
					const auto& val = valueStack[i];
					if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
						auto* fmt = builder->CreateGlobalString("%ld");
						builder->CreateCall(printfFn, {fmt, val.value});
					} else if (val.type == ValueType::FLOAT) {
						auto* fmt = builder->CreateGlobalString("%g");
						builder->CreateCall(printfFn, {fmt, val.value});
					} else if (val.type == ValueType::STRING) {
						auto* dataFn = module->getFunction("qd_string_data");
						auto* lenFn = module->getFunction("qd_string_length");
						auto* strchrFn = module->getFunction("strchr");
						if (dataFn && lenFn && strchrFn) {
							auto* strData = builder->CreateCall(dataFn, {val.value});
							// Smart quoting: only quote strings with whitespace
							auto* hasSpace = builder->CreateCall(strchrFn, {strData, builder->getInt32(' ')});
							auto* hasTab = builder->CreateCall(strchrFn, {strData, builder->getInt32('\t')});
							auto* hasNL = builder->CreateCall(strchrFn, {strData, builder->getInt32('\n')});
							auto* nullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0));
							auto* spaceFound = builder->CreateICmpNE(hasSpace, nullPtr);
							auto* tabFound = builder->CreateICmpNE(hasTab, nullPtr);
							auto* nlFound = builder->CreateICmpNE(hasNL, nullPtr);
							auto* hasWhitespace =
									builder->CreateOr(builder->CreateOr(spaceFound, tabFound), nlFound);

							auto* quotedBB = llvm::BasicBlock::Create(*context, "quoted", currentFunction);
							auto* unquotedBB = llvm::BasicBlock::Create(*context, "unquoted", currentFunction);
							auto* mergeBB = llvm::BasicBlock::Create(*context, "merge_quote", currentFunction);

							builder->CreateCondBr(hasWhitespace, quotedBB, unquotedBB);

							builder->SetInsertPoint(quotedBB);
							auto* quotedFmt = builder->CreateGlobalString("\"%s\"");
							builder->CreateCall(printfFn, {quotedFmt, strData});
							builder->CreateBr(mergeBB);

							builder->SetInsertPoint(unquotedBB);
							auto* unquotedFmt = builder->CreateGlobalString("%s");
							builder->CreateCall(printfFn, {unquotedFmt, strData});
							builder->CreateBr(mergeBB);

							builder->SetInsertPoint(mergeBB);
						}
					} else {
						auto* fmt = builder->CreateGlobalString("%p");
						builder->CreateCall(printfFn, {fmt, val.value});
					}
				}
				// Print newline at end
				auto* nlFmt = builder->CreateGlobalString("\n");
				builder->CreateCall(printfFn, {nlFmt});
			}
		}
		// I/O
		else if (name == "print") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				auto* printfFn = module->getFunction("printf");
				if (printfFn) {
					// Check for runtime-typed value from nested array access
					if (val.structType == "runtime_element" && runtimeTypeAlloca) {
						// Load the runtime type
						auto* rtType = builder->CreateLoad(builder->getInt32Ty(), runtimeTypeAlloca, "rt_type_val");

						// Create blocks for each type
						auto* printIntBlock = llvm::BasicBlock::Create(*context, "print_rt_int", currentFunction);
						auto* printFloatBlock = llvm::BasicBlock::Create(*context, "print_rt_float", currentFunction);
						auto* printStrBlock = llvm::BasicBlock::Create(*context, "print_rt_str", currentFunction);
						auto* printPtrBlock = llvm::BasicBlock::Create(*context, "print_rt_ptr", currentFunction);
						auto* printMergeBlock = llvm::BasicBlock::Create(*context, "print_rt_merge", currentFunction);

						// Switch on runtime type
						auto* switchInst = builder->CreateSwitch(rtType, printIntBlock, 3);
						switchInst->addCase(builder->getInt32(1), printFloatBlock);
						switchInst->addCase(builder->getInt32(2), printStrBlock);
						switchInst->addCase(builder->getInt32(3), printPtrBlock);

						// Print INT (type=0)
						builder->SetInsertPoint(printIntBlock);
						{
							auto* intVal = builder->CreateLoad(builder->getInt64Ty(), runtimeIntAlloca, "rt_int_val");
							auto* fmt = builder->CreateGlobalString("%ld");
							builder->CreateCall(printfFn, {fmt, intVal});
						}
						builder->CreateBr(printMergeBlock);

						// Print FLOAT (type=1)
						builder->SetInsertPoint(printFloatBlock);
						{
							auto* floatVal = builder->CreateLoad(builder->getDoubleTy(), runtimeFloatAlloca, "rt_float_val");
							auto* fmt = builder->CreateGlobalString("%g");
							builder->CreateCall(printfFn, {fmt, floatVal});
						}
						builder->CreateBr(printMergeBlock);

						// Print STR (type=2)
						builder->SetInsertPoint(printStrBlock);
						{
							auto* ptrTy = llvm::PointerType::getUnqual(*context);
							auto* strPtr = builder->CreateLoad(ptrTy, runtimePtrAlloca, "rt_str_val");
							auto* dataFn = module->getFunction("qd_string_data");
							if (dataFn) {
								auto* strData = builder->CreateCall(dataFn, {strPtr});
								auto* fmt = builder->CreateGlobalString("%s");
								builder->CreateCall(printfFn, {fmt, strData});
							}
						}
						builder->CreateBr(printMergeBlock);

						// Print PTR (type=3)
						builder->SetInsertPoint(printPtrBlock);
						{
							auto* ptrTy = llvm::PointerType::getUnqual(*context);
							auto* ptrVal = builder->CreateLoad(ptrTy, runtimePtrAlloca, "rt_ptr_val");
							auto* fmt = builder->CreateGlobalString("%p");
							builder->CreateCall(printfFn, {fmt, ptrVal});
						}
						builder->CreateBr(printMergeBlock);

						builder->SetInsertPoint(printMergeBlock);
					} else if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
						auto* fmt = builder->CreateGlobalString("%ld");
						builder->CreateCall(printfFn, {fmt, val.value});
					} else if (val.type == ValueType::FLOAT) {
						auto* fmt = builder->CreateGlobalString("%g");
						builder->CreateCall(printfFn, {fmt, val.value});
					} else if (val.type == ValueType::STRING) {
						// Get the string data
						auto* dataFn = module->getFunction("qd_string_data");
						if (dataFn) {
							auto* strData = builder->CreateCall(dataFn, {val.value});
							auto* fmt = builder->CreateGlobalString("%s");
							builder->CreateCall(printfFn, {fmt, strData});
						}
						// Release the string if needed
						if (val.needsRelease) {
							releaseValue(val);
						}
					} else {
						auto* fmt = builder->CreateGlobalString("%p");
						builder->CreateCall(printfFn, {fmt, val.value});
					}
				}
			}
		} else if (name == "nl") {
			auto* putcharFn = module->getFunction("putchar");
			if (!putcharFn) {
				auto* putcharFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {builder->getInt32Ty()}, false);
				putcharFn = llvm::Function::Create(putcharFnTy, llvm::Function::ExternalLinkage, "putchar", *module);
			}
			builder->CreateCall(putcharFn, {builder->getInt32('\n')});
		} else if (name == "printv") {
			// Print value with type prefix (e.g., "int:42")
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				auto* printfFn = module->getFunction("printf");
				if (printfFn) {
					if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
						auto* fmt = builder->CreateGlobalString("int:%ld\n");
						builder->CreateCall(printfFn, {fmt, val.value});
					} else if (val.type == ValueType::FLOAT) {
						auto* fmt = builder->CreateGlobalString("float:%g\n");
						builder->CreateCall(printfFn, {fmt, val.value});
					} else if (val.type == ValueType::STRING) {
						auto* dataFn = module->getFunction("qd_string_data");
						if (dataFn) {
							auto* strData = builder->CreateCall(dataFn, {val.value});
							auto* fmt = builder->CreateGlobalString("str:%s\n");
							builder->CreateCall(printfFn, {fmt, strData});
						}
						if (val.needsRelease) {
							releaseValue(val);
						}
					} else {
						auto* fmt = builder->CreateGlobalString("ptr:%p\n");
						builder->CreateCall(printfFn, {fmt, val.value});
					}
				}
			}
		} else if (name == "call") {
			// Call a function pointer or closure from the stack
			if (!valueStack.empty()) {
				auto funcVal = valueStack.back();
				valueStack.pop_back();

				if (funcVal.type == ValueType::PTR && funcVal.value != nullptr) {
					// Determine function type: prefer closureFuncType from value, fall back to lastClosureFuncType
					llvm::FunctionType* closureFT = funcVal.closureFuncType ? funcVal.closureFuncType : lastClosureFuncType;

					// Check if this is a closure (marked by structType)
					if (funcVal.structType == "__closure__") {
						// This is a closure - extract function pointer and environment
						auto* closureStructTy = llvm::StructType::get(*context,
								{builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
										llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

						// Extract function pointer (at offset 1)
						llvm::Value* fnPtrSlot =
								builder->CreateStructGEP(closureStructTy, funcVal.value, 1, "fn_ptr_slot");
						llvm::Value* fnPtr =
								builder->CreateLoad(llvm::PointerType::getUnqual(*context), fnPtrSlot, "fn_ptr");

						// Extract environment pointer (at offset 2)
						llvm::Value* envPtrSlot =
								builder->CreateStructGEP(closureStructTy, funcVal.value, 2, "env_ptr_slot");
						llvm::Value* envPtr =
								builder->CreateLoad(llvm::PointerType::getUnqual(*context), envPtrSlot, "env_ptr");

						// Use the closure function type
						if (closureFT) {
							// Pop arguments from stack (excluding the hidden env parameter)
							size_t numUserParams = closureFT->getNumParams() - 1;  // -1 for env
							std::vector<llvm::Value*> args;
							args.push_back(envPtr);  // Environment is first arg

							for (size_t i = 0; i < numUserParams && !valueStack.empty(); i++) {
								auto arg = valueStack.back();
								valueStack.pop_back();

								// Cast if needed to match parameter type (offset by 1 for env)
								llvm::Type* expectedType =
										closureFT->getParamType(static_cast<unsigned>(numUserParams - i));
								llvm::Value* argVal = arg.value;
								if (arg.type == ValueType::INT && expectedType->isDoubleTy()) {
									argVal = builder->CreateSIToFP(arg.value, builder->getDoubleTy());
								} else if (arg.type == ValueType::FLOAT && expectedType->isIntegerTy(64)) {
									argVal = builder->CreateFPToSI(arg.value, builder->getInt64Ty());
								}
								args.insert(args.begin() + 1, argVal);  // Insert after env
							}

							// Call the closure
							auto* result = builder->CreateCall(closureFT, fnPtr, args);

							// Push return value(s) if not void
							if (!closureFT->getReturnType()->isVoidTy()) {
								if (closureFT->getReturnType()->isStructTy()) {
									// Multiple returns - unpack struct
									auto* structTy = llvm::cast<llvm::StructType>(closureFT->getReturnType());
									for (unsigned i = 0; i < structTy->getNumElements(); i++) {
										auto* elem = builder->CreateExtractValue(result, i);
										ValueType elemType = ValueType::INT;
										if (structTy->getElementType(i)->isDoubleTy()) {
											elemType = ValueType::FLOAT;
										} else if (structTy->getElementType(i)->isPointerTy()) {
											elemType = ValueType::PTR;
										}
										valueStack.push_back(TrackedValue(elem, elemType));
									}
								} else {
									ValueType retType = ValueType::INT;
									if (closureFT->getReturnType()->isDoubleTy()) {
										retType = ValueType::FLOAT;
									} else if (closureFT->getReturnType()->isPointerTy()) {
										retType = ValueType::PTR;
									}
									valueStack.push_back(TrackedValue(result, retType));
								}
							}
						}
					} else if (closureFT) {
						// Not marked as closure but has function type - use it for direct call
						size_t numParams = closureFT->getNumParams();

						// Pop arguments from stack (in reverse order)
						std::vector<llvm::Value*> args;
						for (size_t i = 0; i < numParams && !valueStack.empty(); i++) {
							auto arg = valueStack.back();
							valueStack.pop_back();

							// Cast if needed to match parameter type
							llvm::Type* expectedType =
									closureFT->getParamType(static_cast<unsigned>(numParams - 1 - i));
							llvm::Value* argVal = arg.value;
							if (arg.type == ValueType::INT && expectedType->isDoubleTy()) {
								argVal = builder->CreateSIToFP(arg.value, builder->getDoubleTy());
							} else if (arg.type == ValueType::FLOAT && expectedType->isIntegerTy(64)) {
								argVal = builder->CreateFPToSI(arg.value, builder->getInt64Ty());
							}
							args.insert(args.begin(), argVal);
						}

						// Call the function
						auto* result = builder->CreateCall(closureFT, funcVal.value, args);

						// Push return value(s) if not void
						if (!closureFT->getReturnType()->isVoidTy()) {
							if (closureFT->getReturnType()->isStructTy()) {
								auto* structTy = llvm::cast<llvm::StructType>(closureFT->getReturnType());
								for (unsigned i = 0; i < structTy->getNumElements(); i++) {
									auto* elem = builder->CreateExtractValue(result, i);
									ValueType elemType = ValueType::INT;
									if (structTy->getElementType(i)->isDoubleTy()) {
										elemType = ValueType::FLOAT;
									} else if (structTy->getElementType(i)->isPointerTy()) {
										elemType = ValueType::PTR;
									}
									valueStack.push_back(TrackedValue(elem, elemType));
								}
							} else {
								ValueType retType = ValueType::INT;
								if (closureFT->getReturnType()->isDoubleTy()) {
									retType = ValueType::FLOAT;
								} else if (closureFT->getReturnType()->isPointerTy()) {
									retType = ValueType::PTR;
								}
								valueStack.push_back(TrackedValue(result, retType));
							}
						}
					} else {
						// Regular function pointer - existing logic
						llvm::Function* func = llvm::dyn_cast<llvm::Function>(funcVal.value);
						llvm::FunctionType* funcType = nullptr;

						if (func) {
							funcType = func->getFunctionType();
						} else if (auto* load = llvm::dyn_cast<llvm::LoadInst>(funcVal.value)) {
							// Loaded from a variable - try to find original function
							if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(load->getPointerOperand())) {
								// Search for store to this alloca that stores a function
								for (auto* user : alloca->users()) {
									if (auto* store = llvm::dyn_cast<llvm::StoreInst>(user)) {
										if (auto* storedFunc =
														llvm::dyn_cast<llvm::Function>(store->getValueOperand())) {
											funcType = storedFunc->getFunctionType();
											break;
										}
									}
								}
							}
						}

						if (funcType) {
							size_t numParams = funcType->getNumParams();

							// Pop arguments from stack (in reverse order)
							std::vector<llvm::Value*> args;
							for (size_t i = 0; i < numParams && !valueStack.empty(); i++) {
								auto arg = valueStack.back();
								valueStack.pop_back();

								// Cast if needed to match parameter type
								llvm::Type* expectedType =
										funcType->getParamType(static_cast<unsigned>(numParams - 1 - i));
								llvm::Value* argVal = arg.value;
								if (arg.type == ValueType::INT && expectedType->isDoubleTy()) {
									argVal = builder->CreateSIToFP(arg.value, builder->getDoubleTy());
								} else if (arg.type == ValueType::FLOAT && expectedType->isIntegerTy(64)) {
									argVal = builder->CreateFPToSI(arg.value, builder->getInt64Ty());
								}
								args.insert(args.begin(), argVal);
							}

							// Call the function (use funcVal.value for indirect calls)
							auto* result = builder->CreateCall(funcType, funcVal.value, args);

							// Push return value(s) if not void
							if (!funcType->getReturnType()->isVoidTy()) {
								if (funcType->getReturnType()->isStructTy()) {
									// Multiple returns - unpack struct
									auto* structTy = llvm::cast<llvm::StructType>(funcType->getReturnType());
									for (unsigned i = 0; i < structTy->getNumElements(); i++) {
										auto* elem = builder->CreateExtractValue(result, i);
										ValueType elemType = ValueType::INT;
										if (structTy->getElementType(i)->isDoubleTy()) {
											elemType = ValueType::FLOAT;
										} else if (structTy->getElementType(i)->isPointerTy()) {
											elemType = ValueType::PTR;
										}
										valueStack.push_back(TrackedValue(elem, elemType));
									}
								} else {
									ValueType retType = ValueType::INT;
									if (funcType->getReturnType()->isDoubleTy()) {
										retType = ValueType::FLOAT;
									} else if (funcType->getReturnType()->isPointerTy()) {
										retType = ValueType::PTR;
									}
									valueStack.push_back(TrackedValue(result, retType));
								}
							}
						} else {
							// Can't determine function type - try calling as void()
							auto* voidFuncType = llvm::FunctionType::get(builder->getVoidTy(), {}, false);
							builder->CreateCall(voidFuncType, funcVal.value, {});
						}
					}
				}
			}
		}
		// Type conversions
		else if (name == "casti") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				llvm::Value* intVal = nullptr;
				if (val.type == ValueType::FLOAT) {
					intVal = builder->CreateFPToSI(val.value, builder->getInt64Ty());
				} else if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
					intVal = val.value;
				} else {
					intVal = builder->getInt64(0);
				}
				valueStack.push_back(TrackedValue(intVal, ValueType::INT));
			}
		} else if (name == "castf") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				llvm::Value* floatVal = nullptr;
				if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				} else if (val.type == ValueType::FLOAT) {
					floatVal = val.value;
				} else {
					floatVal = llvm::ConstantFP::get(builder->getDoubleTy(), 0.0);
				}
				valueStack.push_back(TrackedValue(floatVal, ValueType::FLOAT));
			}
		} else if (name == "castp") {
			// Cast to pointer - just mark as PTR type
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				valueStack.push_back(TrackedValue(val.value, ValueType::PTR, val.structType, false));
			}
		} else if (name == "casts") {
			// Cast to string - for non-strings, need to convert
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				if (val.type == ValueType::STRING) {
					valueStack.push_back(val);
				} else {
					// For now, just pass through - proper int/float to string would need sprintf
					valueStack.push_back(TrackedValue(val.value, ValueType::STRING));
				}
			}
		}
		// Handle generic cast<T> instruction
		else if (name == "cast" && inst->hasTypeParam()) {
			const std::string& typeParam = inst->typeParam();
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				if (typeParam == "i64" || typeParam == "i32" || typeParam == "i16" || typeParam == "i8" ||
					typeParam == "u64" || typeParam == "u32" || typeParam == "u16" || typeParam == "u8") {
					// Cast to integer
					llvm::Value* intVal = val.value;
					if (val.type == ValueType::FLOAT) {
						intVal = builder->CreateFPToSI(val.value, builder->getInt64Ty());
					} else if (val.type == ValueType::STRING) {
						// Parse string to integer using strtoll
						auto* dataFn = module->getFunction("qd_string_data");
						auto* strtollFn = module->getFunction("strtoll");
						if (dataFn && strtollFn) {
							auto* strData = builder->CreateCall(dataFn, {val.value}, "str_data");
							auto* ptrTy = llvm::PointerType::getUnqual(*context);
							auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
							intVal = builder->CreateCall(strtollFn, {strData, nullPtr, builder->getInt32(10)}, "parsed_int");
						}
						// Release the string if needed
						if (val.needsRelease && qdStringReleaseFn) {
							builder->CreateCall(qdStringReleaseFn, {val.value});
						}
					}
					valueStack.push_back(TrackedValue(intVal, ValueType::INT));
				} else if (typeParam == "f64" || typeParam == "f32") {
					// Cast to float
					llvm::Value* floatVal = val.value;
					if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
						floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
					} else if (val.type == ValueType::STRING) {
						// Parse string to float using strtod
						auto* dataFn = module->getFunction("qd_string_data");
						auto* strtodFn = module->getFunction("strtod");
						if (dataFn && strtodFn) {
							auto* strData = builder->CreateCall(dataFn, {val.value}, "str_data");
							auto* ptrTy = llvm::PointerType::getUnqual(*context);
							auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
							floatVal = builder->CreateCall(strtodFn, {strData, nullPtr}, "parsed_float");
						}
						// Release the string if needed
						if (val.needsRelease && qdStringReleaseFn) {
							builder->CreateCall(qdStringReleaseFn, {val.value});
						}
					}
					valueStack.push_back(TrackedValue(floatVal, ValueType::FLOAT));
				} else if (typeParam == "ptr") {
					// Cast to pointer - preserve any array type information
					// Special handling for runtime_element: load the actual pointer from alloca
					if (val.structType == "runtime_element" && runtimePtrAlloca) {
						auto* ptrTy = llvm::PointerType::getUnqual(*context);
						auto* actualPtr = builder->CreateLoad(ptrTy, runtimePtrAlloca, "rt_actual_ptr");
						valueStack.push_back(TrackedValue(actualPtr, ValueType::PTR, "", false));
					} else {
						valueStack.push_back(TrackedValue(val.value, ValueType::PTR, val.structType, false));
					}
				} else if (typeParam == "str" || typeParam == "string") {
					// Cast to string - actually convert the value
					if (val.type == ValueType::INT || val.type == ValueType::BOOL) {
						auto* fromIntFn = module->getFunction("qd_string_from_int");
						if (fromIntFn) {
							auto* strVal = builder->CreateCall(fromIntFn, {val.value}, "str_from_int");
							valueStack.push_back(TrackedValue(strVal, ValueType::STRING, "", true));
						} else {
							reportError("qd_string_from_int not found", 0);
						}
					} else if (val.type == ValueType::FLOAT) {
						auto* fromDoubleFn = module->getFunction("qd_string_from_double");
						if (fromDoubleFn) {
							auto* strVal = builder->CreateCall(fromDoubleFn, {val.value}, "str_from_float");
							valueStack.push_back(TrackedValue(strVal, ValueType::STRING, "", true));
						} else {
							reportError("qd_string_from_double not found", 0);
						}
					} else if (val.type == ValueType::STRING) {
						// Already a string, just keep it
						valueStack.push_back(TrackedValue(val.value, ValueType::STRING, "", val.needsRelease));
					} else {
						// PTR - can't convert to string directly
						reportError("Cannot cast pointer to string", 0);
					}
				} else {
					// Struct type or array type - use as structType marker
					// This allows cast<array_float> to mark a pointer as a float array
					valueStack.push_back(TrackedValue(val.value, ValueType::PTR, typeParam, false));
				}
			}
		}
		// Increment/Decrement
		else if (name == "inc" || name == "++") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				if (val.type == ValueType::INT) {
					auto* result = builder->CreateAdd(val.value, builder->getInt64(1));
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else if (val.type == ValueType::FLOAT) {
					auto* result = builder->CreateFAdd(val.value, llvm::ConstantFP::get(builder->getDoubleTy(), 1.0));
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		} else if (name == "dec" || name == "--") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				if (val.type == ValueType::INT) {
					auto* result = builder->CreateSub(val.value, builder->getInt64(1));
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else if (val.type == ValueType::FLOAT) {
					auto* result = builder->CreateFSub(val.value, llvm::ConstantFP::get(builder->getDoubleTy(), 1.0));
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		}
		// Negation
		else if (name == "neg") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				if (val.type == ValueType::INT) {
					auto* result = builder->CreateNeg(val.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else if (val.type == ValueType::FLOAT) {
					auto* result = builder->CreateFNeg(val.value);
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		}
		// Math functions
		else if (name == "abs") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				if (val.type == ValueType::INT) {
					// abs for integer: (x < 0) ? -x : x
					auto* isNeg = builder->CreateICmpSLT(val.value, builder->getInt64(0));
					auto* negated = builder->CreateNeg(val.value);
					auto* result = builder->CreateSelect(isNeg, negated, val.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else if (val.type == ValueType::FLOAT) {
					// Use llvm.fabs intrinsic
					auto* fabsFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::fabs,
																   {builder->getDoubleTy()});
					auto* result = builder->CreateCall(fabsFn, {val.value});
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		} else if (name == "sq") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				if (val.type == ValueType::INT) {
					auto* result = builder->CreateMul(val.value, val.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else if (val.type == ValueType::FLOAT) {
					auto* result = builder->CreateFMul(val.value, val.value);
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		} else if (name == "sqrt") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}

				auto* sqrtFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sqrt, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(sqrtFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		}
		// Trigonometry
		else if (name == "sin") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}

				auto* sinFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sin, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(sinFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "cos") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}

				auto* cosFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::cos, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(cosFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		}
		// Logical not (for booleans)
		else if (name == "lnot") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				llvm::Value* isZero;
				if (val.type == ValueType::PTR || val.type == ValueType::STRING) {
					// Compare pointer to null
					auto* ptrTy = llvm::PointerType::get(*context, 0);
					auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
					isZero = builder->CreateICmpEQ(val.value, nullPtr);
				} else {
					isZero = builder->CreateICmpEQ(val.value, builder->getInt64(0));
				}
				auto* result = builder->CreateZExt(isZero, builder->getInt64Ty());
				valueStack.push_back(TrackedValue(result, ValueType::BOOL));
			}
		}
		// Min/Max
		else if (name == "min") {
			if (valueStack.size() >= 2) {
				auto b = valueStack.back();
				valueStack.pop_back();
				auto a = valueStack.back();
				valueStack.pop_back();

				if (a.type == ValueType::INT && b.type == ValueType::INT) {
					auto* cmp = builder->CreateICmpSLT(a.value, b.value);
					auto* result = builder->CreateSelect(cmp, a.value, b.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else {
					// Convert to float
					llvm::Value* aFloat = a.type == ValueType::FLOAT
							? a.value
							: builder->CreateSIToFP(a.value, builder->getDoubleTy());
					llvm::Value* bFloat = b.type == ValueType::FLOAT
							? b.value
							: builder->CreateSIToFP(b.value, builder->getDoubleTy());
					auto* cmp = builder->CreateFCmpOLT(aFloat, bFloat);
					auto* result = builder->CreateSelect(cmp, aFloat, bFloat);
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		} else if (name == "max") {
			if (valueStack.size() >= 2) {
				auto b = valueStack.back();
				valueStack.pop_back();
				auto a = valueStack.back();
				valueStack.pop_back();

				if (a.type == ValueType::INT && b.type == ValueType::INT) {
					auto* cmp = builder->CreateICmpSGT(a.value, b.value);
					auto* result = builder->CreateSelect(cmp, a.value, b.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else {
					llvm::Value* aFloat = a.type == ValueType::FLOAT
							? a.value
							: builder->CreateSIToFP(a.value, builder->getDoubleTy());
					llvm::Value* bFloat = b.type == ValueType::FLOAT
							? b.value
							: builder->CreateSIToFP(b.value, builder->getDoubleTy());
					auto* cmp = builder->CreateFCmpOGT(aFloat, bFloat);
					auto* result = builder->CreateSelect(cmp, aFloat, bFloat);
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		}
		// within instruction - ( x low high -- bool ) tests if low <= x < high
		else if (name == "within") {
			if (valueStack.size() >= 3) {
				auto high = valueStack.back();
				valueStack.pop_back();
				auto low = valueStack.back();
				valueStack.pop_back();
				auto x = valueStack.back();
				valueStack.pop_back();

				// low <= x && x <= high (closed interval)
				auto* lowCmp = builder->CreateICmpSLE(low.value, x.value);
				auto* highCmp = builder->CreateICmpSLE(x.value, high.value);
				auto* result = builder->CreateAnd(lowCmp, highCmp);
				auto* intResult = builder->CreateZExt(result, builder->getInt64Ty());
				valueStack.push_back(TrackedValue(intResult, ValueType::BOOL));
			}
		}
		// read instruction - reads from program arguments
		else if (name == "read") {
			// This requires access to argc/argv - we need to store them
			// For now, push a placeholder
			valueStack.push_back(TrackedValue(builder->getInt64(0), ValueType::INT));
		}
		// assert instruction
		else if (name == "assert" || name == "assert!") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();

				// Create assert check
				auto* isTrue = builder->CreateICmpNE(val.value, builder->getInt64(0));

				auto* assertFailBB = llvm::BasicBlock::Create(*context, "assert_fail", currentFunction);
				auto* assertPassBB = llvm::BasicBlock::Create(*context, "assert_pass", currentFunction);

				builder->CreateCondBr(isTrue, assertPassBB, assertFailBB);

				// Assert fail block - call abort
				builder->SetInsertPoint(assertFailBB);
				auto* abortFn = module->getFunction("abort");
				if (!abortFn) {
					auto* abortFnTy = llvm::FunctionType::get(builder->getVoidTy(), {}, false);
					abortFn = llvm::Function::Create(abortFnTy, llvm::Function::ExternalLinkage, "abort", *module);
				}
				builder->CreateCall(abortFn);
				builder->CreateUnreachable();

				// Continue in pass block
				builder->SetInsertPoint(assertPassBB);
			}
		}
		// panic instruction - for fallible functions
		else if (name == "panic") {
			// Stack: ( message:str errcode:i64 -- )
			// Pops message and error code, returns early with error state
			// Error code 0 signals error to caller (if goes to else branch)
			if (valueStack.size() >= 2) {
				valueStack.pop_back();  // Pop and discard user error code
				auto errMsg = valueStack.back();
				valueStack.pop_back();

				// Release the error message string if needed
				if (errMsg.needsRelease && qdStringReleaseFn) {
					builder->CreateCall(qdStringReleaseFn, {errMsg.value});
				}

				if (currentFunctionIsFallible && currentFunction) {
					// Get function return type
					auto* retType = currentFunction->getReturnType();
					if (auto* structRetType = llvm::dyn_cast<llvm::StructType>(retType)) {
						// Build return value with undefined results + error code 0 (error)
						llvm::Value* retVal = llvm::UndefValue::get(structRetType);
						unsigned numFields = structRetType->getNumElements();
						// Last field is error code - 0 means error/panic
						retVal = builder->CreateInsertValue(retVal, builder->getInt64(0), numFields - 1);
						builder->CreateRet(retVal);
					}
				}
			}
		}
		// Boolean literals (true and false are keywords that act like instructions)
		else if (name == "true") {
			valueStack.push_back(TrackedValue(builder->getInt64(1), ValueType::BOOL));
		} else if (name == "false") {
			valueStack.push_back(TrackedValue(builder->getInt64(0), ValueType::BOOL));
		}
		// Additional math builtins
		else if (name == "ln" || name == "log") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				auto* logFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::log, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(logFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "exp") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				auto* expFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::exp, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(expFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "sqrt") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				auto* sqrtFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sqrt, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(sqrtFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "tan") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				// tan(x) = sin(x) / cos(x)
				auto* sinFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::sin, {builder->getDoubleTy()});
				auto* cosFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::cos, {builder->getDoubleTy()});
				auto* sinVal = builder->CreateCall(sinFn, {floatVal});
				auto* cosVal = builder->CreateCall(cosFn, {floatVal});
				auto* result = builder->CreateFDiv(sinVal, cosVal);
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "pow") {
			if (valueStack.size() >= 2) {
				auto exp = valueStack.back();
				valueStack.pop_back();
				auto base = valueStack.back();
				valueStack.pop_back();
				llvm::Value* baseFloat = base.value;
				llvm::Value* expFloat = exp.value;
				if (base.type == ValueType::INT) {
					baseFloat = builder->CreateSIToFP(base.value, builder->getDoubleTy());
				}
				if (exp.type == ValueType::INT) {
					expFloat = builder->CreateSIToFP(exp.value, builder->getDoubleTy());
				}
				auto* powFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::pow, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(powFn, {baseFloat, expFloat});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "ceil") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				auto* ceilFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::ceil, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(ceilFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "floor") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				auto* floorFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::floor, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(floorFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "round") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				llvm::Value* floatVal = val.value;
				if (val.type == ValueType::INT) {
					floatVal = builder->CreateSIToFP(val.value, builder->getDoubleTy());
				}
				auto* roundFn =
						llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::round, {builder->getDoubleTy()});
				auto* result = builder->CreateCall(roundFn, {floatVal});
				valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			}
		} else if (name == "abs" || name == "fabs") {
			if (!valueStack.empty()) {
				auto val = valueStack.back();
				valueStack.pop_back();
				if (val.type == ValueType::INT) {
					// Integer absolute value
					auto* neg = builder->CreateNeg(val.value);
					auto* isNeg = builder->CreateICmpSLT(val.value, builder->getInt64(0));
					auto* result = builder->CreateSelect(isNeg, neg, val.value);
					valueStack.push_back(TrackedValue(result, ValueType::INT));
				} else {
					auto* fabsFn =
							llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::fabs, {builder->getDoubleTy()});
					auto* result = builder->CreateCall(fabsFn, {val.value});
					valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
				}
			}
		}
		// Array operations
		else if (name == "len") {
			// Stack: ( array:ptr -- length:i64 )
			if (!valueStack.empty()) {
				auto arrVal = valueStack.back();
				valueStack.pop_back();
				if (arrVal.type == ValueType::PTR) {
					auto* lenFn = module->getFunction("qd_array_length");
					auto* length = builder->CreateCall(lenFn, {arrVal.value}, "arr_len");
					valueStack.push_back(TrackedValue(length, ValueType::INT));
				}
			}
		} else if (name == "nth") {
			// Stack: ( array:ptr index:i64 -- value )
			if (valueStack.size() >= 2) {
				auto indexVal = valueStack.back();
				valueStack.pop_back();
				auto arrVal = valueStack.back();
				valueStack.pop_back();

				if (arrVal.type == ValueType::PTR && indexVal.type == ValueType::INT) {
					// Check array element type based on structType
					if (arrVal.structType == "array_float") {
						auto* getFloatFn = module->getFunction("qd_array_get_float");
						auto* resultAlloca =
								builder->CreateAlloca(builder->getDoubleTy(), nullptr, "nth_float_result");
						builder->CreateCall(getFloatFn, {arrVal.value, indexVal.value, resultAlloca});
						auto* result = builder->CreateLoad(builder->getDoubleTy(), resultAlloca, "nth_float_val");
						valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
					} else if (arrVal.structType == "array_str") {
						auto* getPtrFn = module->getFunction("qd_array_get_ptr");
						auto* ptrTy = llvm::PointerType::getUnqual(*context);
						auto* resultAlloca = builder->CreateAlloca(ptrTy, nullptr, "nth_str_result");
						builder->CreateCall(getPtrFn, {arrVal.value, indexVal.value, resultAlloca});
						auto* result = builder->CreateLoad(ptrTy, resultAlloca, "nth_str_val");
						valueStack.push_back(TrackedValue(result, ValueType::STRING));
					} else if (arrVal.structType == "array_ptr" ||
							   arrVal.structType.substr(0, 10) == "array_ptr:") {
						auto* getPtrFn = module->getFunction("qd_array_get_ptr");
						auto* ptrTy = llvm::PointerType::getUnqual(*context);
						auto* resultAlloca = builder->CreateAlloca(ptrTy, nullptr, "nth_ptr_result");
						builder->CreateCall(getPtrFn, {arrVal.value, indexVal.value, resultAlloca});
						auto* result = builder->CreateLoad(ptrTy, resultAlloca, "nth_ptr_val");
						// Extract element struct type if present (e.g., "array_ptr:Point" -> "Point")
						std::string elemStructType;
						if (arrVal.structType.length() > 10 && arrVal.structType.substr(0, 10) == "array_ptr:") {
							elemStructType = arrVal.structType.substr(10);
						}
						valueStack.push_back(TrackedValue(result, ValueType::PTR, elemStructType, false));
					} else if (arrVal.structType.empty() && arrVal.type == ValueType::PTR) {
						// Unknown array type - check runtime elemType field (offset 32)
						// elemType: 0=INT, 1=FLOAT, 2=STR, 3=PTR
						auto* ptrTy = llvm::PointerType::getUnqual(*context);

						// Create allocas in entry block for all possible result types
						llvm::IRBuilder<> entryBuilder(&currentFunction->getEntryBlock(),
													   currentFunction->getEntryBlock().begin());
						runtimeTypeAlloca = entryBuilder.CreateAlloca(builder->getInt32Ty(), nullptr, "rt_type");
						runtimeIntAlloca = entryBuilder.CreateAlloca(builder->getInt64Ty(), nullptr, "rt_int");
						runtimeFloatAlloca = entryBuilder.CreateAlloca(builder->getDoubleTy(), nullptr, "rt_float");
						runtimePtrAlloca = entryBuilder.CreateAlloca(ptrTy, nullptr, "rt_ptr");

						// Read elemType from array struct
						auto* elemTypePtr = builder->CreateGEP(builder->getInt8Ty(), arrVal.value,
								builder->getInt64(32), "elem_type_ptr");
						auto* elemType = builder->CreateLoad(builder->getInt32Ty(), elemTypePtr, "elem_type");

						// Store type for later use by print
						builder->CreateStore(elemType, runtimeTypeAlloca);

						// Create basic blocks for each type
						auto* intBlock = llvm::BasicBlock::Create(*context, "nth_rt_int", currentFunction);
						auto* floatBlock = llvm::BasicBlock::Create(*context, "nth_rt_float", currentFunction);
						auto* strBlock = llvm::BasicBlock::Create(*context, "nth_rt_str", currentFunction);
						auto* ptrBlock = llvm::BasicBlock::Create(*context, "nth_rt_ptr", currentFunction);
						auto* mergeBlock = llvm::BasicBlock::Create(*context, "nth_rt_merge", currentFunction);

						// Switch on elemType
						auto* switchInst = builder->CreateSwitch(elemType, intBlock, 3);
						switchInst->addCase(builder->getInt32(1), floatBlock);
						switchInst->addCase(builder->getInt32(2), strBlock);
						switchInst->addCase(builder->getInt32(3), ptrBlock);

						// INT block (type=0, default)
						builder->SetInsertPoint(intBlock);
						{
							auto* getIntFn = module->getFunction("qd_array_get_int");
							auto* resultAlloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "nth_int_tmp");
							builder->CreateCall(getIntFn, {arrVal.value, indexVal.value, resultAlloca});
							auto* result = builder->CreateLoad(builder->getInt64Ty(), resultAlloca, "nth_int_val");
							builder->CreateStore(result, runtimeIntAlloca);
						}
						builder->CreateBr(mergeBlock);

						// FLOAT block (type=1)
						builder->SetInsertPoint(floatBlock);
						{
							auto* getFloatFn = module->getFunction("qd_array_get_float");
							auto* resultAlloca = builder->CreateAlloca(builder->getDoubleTy(), nullptr, "nth_float_tmp");
							builder->CreateCall(getFloatFn, {arrVal.value, indexVal.value, resultAlloca});
							auto* result = builder->CreateLoad(builder->getDoubleTy(), resultAlloca, "nth_float_val");
							builder->CreateStore(result, runtimeFloatAlloca);
						}
						builder->CreateBr(mergeBlock);

						// STR block (type=2)
						builder->SetInsertPoint(strBlock);
						{
							auto* getPtrFn = module->getFunction("qd_array_get_ptr");
							auto* resultAlloca = builder->CreateAlloca(ptrTy, nullptr, "nth_str_tmp");
							builder->CreateCall(getPtrFn, {arrVal.value, indexVal.value, resultAlloca});
							auto* result = builder->CreateLoad(ptrTy, resultAlloca, "nth_str_val");
							builder->CreateStore(result, runtimePtrAlloca);
						}
						builder->CreateBr(mergeBlock);

						// PTR block (type=3)
						builder->SetInsertPoint(ptrBlock);
						{
							auto* getPtrFn = module->getFunction("qd_array_get_ptr");
							auto* resultAlloca = builder->CreateAlloca(ptrTy, nullptr, "nth_ptr_tmp");
							builder->CreateCall(getPtrFn, {arrVal.value, indexVal.value, resultAlloca});
							auto* result = builder->CreateLoad(ptrTy, resultAlloca, "nth_ptr_val");
							builder->CreateStore(result, runtimePtrAlloca);
						}
						builder->CreateBr(mergeBlock);

						// Merge block - push a special marker value
						builder->SetInsertPoint(mergeBlock);
						// We use runtimePtrAlloca as the "value" but mark it as "runtime_element"
						// The actual value will be read from the appropriate alloca based on runtimeTypeAlloca
						valueStack.push_back(TrackedValue(runtimePtrAlloca, ValueType::PTR, "runtime_element", false));
					} else {
						// Default: integer array (known array_int type)
						auto* getIntFn = module->getFunction("qd_array_get_int");
						auto* resultAlloca =
								builder->CreateAlloca(builder->getInt64Ty(), nullptr, "nth_int_result");
						builder->CreateCall(getIntFn, {arrVal.value, indexVal.value, resultAlloca});
						auto* result = builder->CreateLoad(builder->getInt64Ty(), resultAlloca, "nth_int_val");
						valueStack.push_back(TrackedValue(result, ValueType::INT));
					}
				}
			}
		} else if (name == "append") {
			// Stack: ( array:ptr value -- array:ptr )
			if (valueStack.size() >= 2) {
				auto val = valueStack.back();
				valueStack.pop_back();
				auto arrVal = valueStack.back();
				valueStack.pop_back();

				if (arrVal.type == ValueType::PTR) {
					if (val.type == ValueType::INT) {
						auto* pushIntFn = module->getFunction("qd_array_push_int");
						builder->CreateCall(pushIntFn, {arrVal.value, val.value});
					} else if (val.type == ValueType::FLOAT) {
						auto* pushFloatFn = module->getFunction("qd_array_push_float");
						builder->CreateCall(pushFloatFn, {arrVal.value, val.value});
					} else if (val.type == ValueType::PTR || val.type == ValueType::STRING) {
						auto* pushPtrFn = module->getFunction("qd_array_push_ptr");
						builder->CreateCall(pushPtrFn, {arrVal.value, val.value});
					}
					// Return the array
					valueStack.push_back(arrVal);
				}
			}
		} else if (name == "set") {
			// Stack: ( array:ptr index:i64 value -- )
			if (valueStack.size() >= 3) {
				auto val = valueStack.back();
				valueStack.pop_back();
				auto indexVal = valueStack.back();
				valueStack.pop_back();
				auto arrVal = valueStack.back();
				valueStack.pop_back();

				if (arrVal.type == ValueType::PTR && indexVal.type == ValueType::INT) {
					if (val.type == ValueType::INT) {
						// qd_array_set_int(arr, index, value)
						auto* setIntFn = module->getFunction("qd_array_set_int");
						if (!setIntFn) {
							auto* fnTy = llvm::FunctionType::get(builder->getInt32Ty(),
									{llvm::PointerType::getUnqual(*context), builder->getInt64Ty(), builder->getInt64Ty()}, false);
							setIntFn = llvm::Function::Create(
									fnTy, llvm::Function::ExternalLinkage, "qd_array_set_int", *module);
						}
						builder->CreateCall(setIntFn, {arrVal.value, indexVal.value, val.value});
					} else if (val.type == ValueType::FLOAT) {
						auto* setFloatFn = module->getFunction("qd_array_set_float");
						if (!setFloatFn) {
							auto* fnTy = llvm::FunctionType::get(builder->getInt32Ty(),
									{llvm::PointerType::getUnqual(*context), builder->getInt64Ty(), builder->getDoubleTy()}, false);
							setFloatFn = llvm::Function::Create(
									fnTy, llvm::Function::ExternalLinkage, "qd_array_set_float", *module);
						}
						builder->CreateCall(setFloatFn, {arrVal.value, indexVal.value, val.value});
					} else if (val.type == ValueType::PTR || val.type == ValueType::STRING) {
						auto* setPtrFn = module->getFunction("qd_array_set_ptr");
						if (!setPtrFn) {
							auto* fnTy = llvm::FunctionType::get(builder->getInt32Ty(),
									{llvm::PointerType::getUnqual(*context), builder->getInt64Ty(),
											llvm::PointerType::getUnqual(*context)},
									false);
							setPtrFn = llvm::Function::Create(
									fnTy, llvm::Function::ExternalLinkage, "qd_array_set_ptr", *module);
						}
						builder->CreateCall(setPtrFn, {arrVal.value, indexVal.value, val.value});
					}
				}
			}
		} else if (name == "makei") {
			// Stack: ( size:i64 -- array:ptr )
			// Creates array with 'size' elements initialized to 0
			if (!valueStack.empty()) {
				auto sizeVal = valueStack.back();
				valueStack.pop_back();
				if (sizeVal.type == ValueType::INT) {
					auto* createFn = module->getFunction("qd_array_create");
					auto* arr = builder->CreateCall(createFn, {sizeVal.value, builder->getInt32(0)}, "arr_i");

					// qd_array_t layout: magic(0), refcount(8), length(16), capacity(24), elemType(32), data(40)
					// Set arr->length = size
					auto* lengthPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(16), "length_ptr");
					builder->CreateStore(sizeVal.value, lengthPtr);

					// Get data pointer (arr->data.i at offset 40)
					auto* dataPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(40), "data_ptr_ptr");
					auto* data =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), dataPtr, "data_ptr");

					// memset(data, 0, size * 8)
					auto* byteSize = builder->CreateMul(sizeVal.value, builder->getInt64(8), "byte_size");
					builder->CreateMemSet(data, builder->getInt8(0), byteSize, llvm::MaybeAlign(8));

					valueStack.push_back(TrackedValue(arr, ValueType::PTR, "array_int", true));
				}
			}
		} else if (name == "makef") {
			// Stack: ( size:i64 -- array:ptr )
			// Creates array with 'size' elements initialized to 0.0
			if (!valueStack.empty()) {
				auto sizeVal = valueStack.back();
				valueStack.pop_back();
				if (sizeVal.type == ValueType::INT) {
					auto* createFn = module->getFunction("qd_array_create");
					auto* arr = builder->CreateCall(createFn, {sizeVal.value, builder->getInt32(1)}, "arr_f");

					// Set arr->length = size
					auto* lengthPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(16), "length_ptr");
					builder->CreateStore(sizeVal.value, lengthPtr);

					// Get data pointer (arr->data.f at offset 40)
					auto* dataPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(40), "data_ptr_ptr");
					auto* data =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), dataPtr, "data_ptr");

					// memset(data, 0, size * 8) - doubles are 8 bytes, 0 bits = 0.0
					auto* byteSize = builder->CreateMul(sizeVal.value, builder->getInt64(8), "byte_size");
					builder->CreateMemSet(data, builder->getInt8(0), byteSize, llvm::MaybeAlign(8));

					valueStack.push_back(TrackedValue(arr, ValueType::PTR, "array_float", true));
				}
			}
		} else if (name == "makes") {
			// Stack: ( size:i64 -- array:ptr )
			// Creates array with 'size' elements initialized to empty strings
			if (!valueStack.empty()) {
				auto sizeVal = valueStack.back();
				valueStack.pop_back();
				if (sizeVal.type == ValueType::INT) {
					auto* createFn = module->getFunction("qd_array_create");
					auto* arr = builder->CreateCall(createFn, {sizeVal.value, builder->getInt32(2)}, "arr_s");

					// Set arr->length = size
					auto* lengthPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(16), "length_ptr");
					builder->CreateStore(sizeVal.value, lengthPtr);

					// Get data pointer (arr->data.p at offset 40)
					auto* dataPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(40), "data_ptr_ptr");
					auto* data =
							builder->CreateLoad(llvm::PointerType::getUnqual(*context), dataPtr, "data_ptr");

					// Create empty string constant
					auto* emptyStrConst = builder->CreateGlobalString("", "empty_str");
					auto* createStrFn = module->getFunction("qd_string_create");

					// Loop to initialize each element with an empty string
					auto* loopVar = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "makes_idx");
					builder->CreateStore(builder->getInt64(0), loopVar);

					auto* loopCheck = llvm::BasicBlock::Create(*context, "makes_loop_check", currentFunction);
					auto* loopBody = llvm::BasicBlock::Create(*context, "makes_loop_body", currentFunction);
					auto* loopEnd = llvm::BasicBlock::Create(*context, "makes_loop_end", currentFunction);

					builder->CreateBr(loopCheck);
					builder->SetInsertPoint(loopCheck);
					auto* idx = builder->CreateLoad(builder->getInt64Ty(), loopVar, "idx");
					auto* cond = builder->CreateICmpSLT(idx, sizeVal.value, "loop_cond");
					builder->CreateCondBr(cond, loopBody, loopEnd);

					builder->SetInsertPoint(loopBody);
					// Create empty string
					auto* emptyStr = builder->CreateCall(createStrFn, {emptyStrConst}, "empty_qd_str");
					// Store in array: data[idx] = emptyStr
					auto* elemPtr = builder->CreateGEP(llvm::PointerType::getUnqual(*context), data, idx, "elem_ptr");
					builder->CreateStore(emptyStr, elemPtr);
					// Increment index
					auto* nextIdx = builder->CreateAdd(idx, builder->getInt64(1), "next_idx");
					builder->CreateStore(nextIdx, loopVar);
					builder->CreateBr(loopCheck);

					builder->SetInsertPoint(loopEnd);
					valueStack.push_back(TrackedValue(arr, ValueType::PTR, "array_str", true));
				}
			}
		} else if (name == "makep") {
			// Stack: ( size:i64 -- array:ptr )
			// Creates array with 'size' elements initialized to null
			if (!valueStack.empty()) {
				auto sizeVal = valueStack.back();
				valueStack.pop_back();
				if (sizeVal.type == ValueType::INT) {
					auto* createFn = module->getFunction("qd_array_create");
					auto* arr = builder->CreateCall(createFn, {sizeVal.value, builder->getInt32(3)}, "arr_p");

					// Set arr->length = size
					auto* lengthPtr = builder->CreateGEP(builder->getInt8Ty(), arr, builder->getInt64(16), "length_ptr");
					builder->CreateStore(sizeVal.value, lengthPtr);

					// Data is already memset to 0 by qd_array_create for STR/PTR types
					// Use element type from make<T> if available, otherwise just "array_ptr"
					std::string arrayType = makeElementType.empty() ? "array_ptr" : "array_ptr:" + makeElementType;
					valueStack.push_back(TrackedValue(arr, ValueType::PTR, arrayType, true));
				}
			}
		}
		// Unknown instruction
		else {
			// Check if it's a function call
			std::string funcName = mangleName(name, currentModulePrefix);
			auto funcIt = userFunctions.find(funcName);
			if (funcIt != userFunctions.end()) {
				auto* func = funcIt->second;
				auto sigIt = functionSignatures.find(funcName);

				std::vector<llvm::Value*> args;
				if (sigIt != functionSignatures.end()) {
					for (size_t i = 0; i < sigIt->second.params.size() && !valueStack.empty(); i++) {
						args.insert(args.begin(), valueStack.back().value);
						valueStack.pop_back();
					}
				}

				auto* result = builder->CreateCall(func, args);

				if (sigIt != functionSignatures.end()) {
					bool funcIsFallible = sigIt->second.isFallible;

					if (funcIsFallible) {
						// Fallible function returns struct {result..., errcode}
						// Extract all values including error code
						for (size_t i = 0; i < sigIt->second.returns.size(); i++) {
							auto* extracted = builder->CreateExtractValue(result, static_cast<unsigned>(i));
							const auto& retInfo = sigIt->second.returns[i];
							valueStack.push_back(TrackedValue(extracted, retInfo.first, retInfo.second, true));
						}
						// Extract and push error code (last element in struct)
						auto* errCode = builder->CreateExtractValue(result,
							static_cast<unsigned>(sigIt->second.returns.size()));
						valueStack.push_back(TrackedValue(errCode, ValueType::INT, "", false));
					} else if (!sigIt->second.returns.empty()) {
						if (sigIt->second.returns.size() == 1) {
							const auto& retInfo = sigIt->second.returns[0];
							valueStack.push_back(TrackedValue(result, retInfo.first, retInfo.second, true));
						} else {
							for (size_t i = 0; i < sigIt->second.returns.size(); i++) {
								auto* extracted = builder->CreateExtractValue(result, static_cast<unsigned>(i));
								const auto& retInfo = sigIt->second.returns[i];
								valueStack.push_back(TrackedValue(extracted, retInfo.first, retInfo.second, true));
							}
						}
					}
				}
			}
		}
	}

	void RegisterGenerator::generateLocal(AstNodeLocal* local) {
		for (size_t i = 0; i < local->names().size(); i++) {
			const std::string& name = local->names()[local->names().size() - 1 - i];

			if (valueStack.empty()) {
				reportError("Stack underflow in local assignment: " + name, local->line());
				continue;
			}

			auto val = valueStack.back();
			valueStack.pop_back();

			// Check if variable already exists
			auto it = localVariables.find(name);
			if (it != localVariables.end()) {
				// Store to existing variable
				builder->CreateStore(val.value, it->second.alloca);
			} else {
				// Create new variable - alloca must be in entry block for proper SSA domination
				llvm::IRBuilder<> entryBuilder(&currentFunction->getEntryBlock(),
											   currentFunction->getEntryBlock().begin());
				auto* alloca = entryBuilder.CreateAlloca(val.value->getType(), nullptr, name);
				builder->CreateStore(val.value, alloca);

				LocalInfo info;
				info.alloca = alloca;
				info.type = val.type;
				info.structType = val.structType;
				info.needsRelease = val.needsRelease;
				info.closureFuncType = val.closureFuncType;
				localVariables[name] = info;
			}
		}
	}

	void RegisterGenerator::generateScopedIdentifier(AstNodeScopedIdentifier* scopedIdent) {
		const std::string& moduleName = scopedIdent->scope();
		const std::string& memberName = scopedIdent->name();

		// Check for struct constructor
		std::string structName = moduleName + "::" + memberName;
		auto structIt = structDefinitions.find(structName);
		if (structIt != structDefinitions.end()) {
			generateStructConstruction(structName);
			return;
		}

		// Check for constant first
		std::string constName = moduleName + "::" + memberName;
		auto constIt = moduleConstants.find(constName);
		if (constIt != moduleConstants.end()) {
			const std::string& constValue = constIt->second;

			// Check for string constant (quoted)
			if (constValue.size() >= 2 && constValue.front() == '"' && constValue.back() == '"') {
				// Remove quotes
				std::string strContent = constValue.substr(1, constValue.size() - 2);
				auto* strConst = builder->CreateGlobalString(strContent, "str_const");
				auto* createFn = module->getFunction("qd_string_create");
				auto* strVal = builder->CreateCall(createFn, {strConst}, "const_str");
				valueStack.push_back(TrackedValue(strVal, ValueType::STRING, "", true));
				return;
			}

			// Try parsing as integer first (must consume entire string)
			int64_t intValue = 0;
			auto [ptr, ec] =
					std::from_chars(constValue.data(), constValue.data() + constValue.size(), intValue);
			if (ec == std::errc() && ptr == constValue.data() + constValue.size()) {
				valueStack.push_back(TrackedValue(builder->getInt64(static_cast<uint64_t>(intValue)), ValueType::INT));
				return;
			}
			// Try parsing as float
			char* endPtr = nullptr;
			double floatValue = std::strtod(constValue.c_str(), &endPtr);
			if (endPtr != constValue.c_str() && *endPtr == '\0') {
				valueStack.push_back(TrackedValue(
					llvm::ConstantFP::get(builder->getDoubleTy(), floatValue), ValueType::FLOAT));
				return;
			}
		}

		// Handle native module functions BEFORE user-defined functions
		// Native handlers have priority because they use proper C library functions with correct types
		if (moduleName == "math") {
			if (generateNativeMathFunction(memberName)) {
				return;
			}
		}

		if (moduleName == "str") {
			if (generateNativeStrFunction(memberName)) {
				return;
			}
		}

		if (moduleName == "strconv") {
			if (generateNativeStrconvFunction(memberName)) {
				return;
			}
		}

		if (moduleName == "mem") {
			if (generateNativeMemFunction(memberName)) {
				return;
			}
		}

		if (moduleName == "term") {
			if (generateNativeTermFunction(memberName)) {
				return;
			}
		}

		if (moduleName == "os") {
			if (generateNativeOsFunction(memberName)) {
				return;
			}
		}

		if (moduleName == "io") {
			if (generateNativeIoFunction(memberName)) {
				return;
			}
		}

		// Check for user-defined function call
		std::string funcName = "qd_" + moduleName + "_" + memberName;
		auto funcIt = userFunctions.find(funcName);
		if (funcIt == userFunctions.end()) {
			// Also try scoped name format (used by imported functions)
			funcName = moduleName + "::" + memberName;
			funcIt = userFunctions.find(funcName);
		}
		if (funcIt != userFunctions.end()) {
			auto* func = funcIt->second;
			auto sigIt = functionSignatures.find(funcName);

			std::vector<llvm::Value*> args;
			if (sigIt != functionSignatures.end()) {
				for (size_t i = 0; i < sigIt->second.params.size() && !valueStack.empty(); i++) {
					args.insert(args.begin(), valueStack.back().value);
					valueStack.pop_back();
				}
			}

			auto* result = builder->CreateCall(func, args);

			if (sigIt != functionSignatures.end() && !sigIt->second.returns.empty()) {
				if (sigIt->second.returns.size() == 1) {
					const auto& retInfo = sigIt->second.returns[0];
					valueStack.push_back(TrackedValue(result, retInfo.first, retInfo.second, true));
				}
			}
			return;
		}

		reportError("Unknown scoped identifier: " + moduleName + "::" + memberName, scopedIdent->line());
	}

	bool RegisterGenerator::generateNativeMathFunction(const std::string& name) {
		// Single-argument math functions (f64 -> f64)
		// Empty string means special handling required
		static const std::map<std::string, std::string> singleArgFuncs = {
			{"sin", "sin"}, {"cos", "cos"}, {"tan", "tan"},
			{"asin", "asin"}, {"acos", "acos"}, {"atan", "atan"},
			{"sinh", "sinh"}, {"cosh", "cosh"}, {"tanh", "tanh"},
			{"exp", "exp"}, {"log", "log"}, {"ln", "log"}, {"log10", "log10"}, {"log2", "log2"},
			{"sqrt", "sqrt"}, {"cbrt", "cbrt"},
			{"ceil", "ceil"}, {"floor", "floor"}, {"round", "round"}, {"trunc", "trunc"},
			{"abs", "fabs"},
			{"sq", ""},  // Special: x * x
			{"cb", ""},  // Special: x * x * x
			{"inv", ""}, // Special: 1.0 / x
		};

		// Two-argument math functions (f64, f64 -> f64)
		static const std::map<std::string, std::string> twoArgFuncs = {
			{"pow", "pow"}, {"fmod", "fmod"}, {"atan2", "atan2"},
			{"hypot", "hypot"}, {"copysign", "copysign"},
			{"min", "fmin"}, {"max", "fmax"},
		};

		auto singleIt = singleArgFuncs.find(name);
		if (singleIt != singleArgFuncs.end()) {
			if (valueStack.empty()) {
				reportError("Stack underflow for math::" + name, 0);
				return true;
			}
			auto arg = valueStack.back();
			valueStack.pop_back();

			// Convert to double if needed
			llvm::Value* argVal = arg.value;
			if (arg.type == ValueType::INT) {
				argVal = builder->CreateSIToFP(argVal, builder->getDoubleTy(), "int_to_double");
			}

			llvm::Value* result;
			if (singleIt->second.empty()) {
				// Special cases
				if (name == "sq") {
					result = builder->CreateFMul(argVal, argVal, "sq");
				} else if (name == "cb") {
					auto* sq = builder->CreateFMul(argVal, argVal, "sq");
					result = builder->CreateFMul(sq, argVal, "cb");
				} else if (name == "inv") {
					result = builder->CreateFDiv(llvm::ConstantFP::get(builder->getDoubleTy(), 1.0), argVal, "inv");
				} else {
					return false;
				}
			} else {
				// Call C library function
				llvm::Function* fn = module->getFunction(singleIt->second);
				if (!fn) {
					auto* fnTy = llvm::FunctionType::get(builder->getDoubleTy(), {builder->getDoubleTy()}, false);
					fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, singleIt->second, *module);
				}
				result = builder->CreateCall(fn, {argVal}, name);
			}

			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			return true;
		}

		auto twoIt = twoArgFuncs.find(name);
		if (twoIt != twoArgFuncs.end()) {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for math::" + name, 0);
				return true;
			}
			auto arg2 = valueStack.back();
			valueStack.pop_back();
			auto arg1 = valueStack.back();
			valueStack.pop_back();

			// Convert to double if needed
			llvm::Value* arg1Val = arg1.value;
			llvm::Value* arg2Val = arg2.value;
			if (arg1.type == ValueType::INT) {
				arg1Val = builder->CreateSIToFP(arg1Val, builder->getDoubleTy(), "int_to_double");
			}
			if (arg2.type == ValueType::INT) {
				arg2Val = builder->CreateSIToFP(arg2Val, builder->getDoubleTy(), "int_to_double");
			}

			llvm::Function* fn = module->getFunction(twoIt->second);
			if (!fn) {
				auto* fnTy = llvm::FunctionType::get(builder->getDoubleTy(),
					{builder->getDoubleTy(), builder->getDoubleTy()}, false);
				fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, twoIt->second, *module);
			}
			auto* result = builder->CreateCall(fn, {arg1Val, arg2Val}, name);

			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			return true;
		}

		// Integer factorial
		if (name == "fac") {
			if (valueStack.empty()) {
				reportError("Stack underflow for math::fac", 0);
				return true;
			}
			auto arg = valueStack.back();
			valueStack.pop_back();

			// Generate factorial loop inline
			llvm::Value* n = arg.value;
			if (arg.type == ValueType::FLOAT) {
				n = builder->CreateFPToSI(n, builder->getInt64Ty(), "to_int");
			}

			// Simple iterative factorial
			auto* resultAlloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "fac_result");
			auto* iAlloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "fac_i");
			builder->CreateStore(builder->getInt64(1), resultAlloca);
			builder->CreateStore(builder->getInt64(2), iAlloca);

			auto* loopBB = llvm::BasicBlock::Create(*context, "fac_loop", currentFunction);
			auto* endBB = llvm::BasicBlock::Create(*context, "fac_end", currentFunction);

			builder->CreateBr(loopBB);
			builder->SetInsertPoint(loopBB);

			auto* i = builder->CreateLoad(builder->getInt64Ty(), iAlloca, "i");
			auto* cond = builder->CreateICmpSLE(i, n, "fac_cond");

			auto* bodyBB = llvm::BasicBlock::Create(*context, "fac_body", currentFunction);
			builder->CreateCondBr(cond, bodyBB, endBB);

			builder->SetInsertPoint(bodyBB);
			auto* result = builder->CreateLoad(builder->getInt64Ty(), resultAlloca, "result");
			auto* newResult = builder->CreateMul(result, i, "new_result");
			builder->CreateStore(newResult, resultAlloca);
			auto* newI = builder->CreateAdd(i, builder->getInt64(1), "new_i");
			builder->CreateStore(newI, iAlloca);
			builder->CreateBr(loopBB);

			builder->SetInsertPoint(endBB);
			auto* finalResult = builder->CreateLoad(builder->getInt64Ty(), resultAlloca, "fac_final");
			valueStack.push_back(TrackedValue(finalResult, ValueType::INT));
			return true;
		}

		return false;
	}

	bool RegisterGenerator::generateNativeStrFunction(const std::string& name) {
		auto ptrTy = llvm::PointerType::getUnqual(*context);

		// Helper to get a function declaration
		auto getOrDeclareFunc = [this](const std::string& fnName, llvm::FunctionType* fnTy) -> llvm::Function* {
			if (auto* fn = module->getFunction(fnName)) {
				return fn;
			}
			return llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
		};

		// str::len ( s:str -- len:int )
		if (name == "len") {
			if (valueStack.empty()) {
				reportError("Stack underflow for str::len", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			// Get qd_string_length function
			auto* fnTy = llvm::FunctionType::get(builder->getInt64Ty(), {ptrTy}, false);
			auto* fn = getOrDeclareFunc("qd_string_length", fnTy);
			auto* result = builder->CreateCall(fn, {str.value}, "str_len");

			// Release the string after use
			if (str.needsRelease) {
				builder->CreateCall(qdStringReleaseFn, {str.value});
			}

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// str::concat ( s1:str s2:str -- result:str )
		if (name == "concat") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::concat", 0);
				return true;
			}
			auto str2 = valueStack.back();
			valueStack.pop_back();
			auto str1 = valueStack.back();
			valueStack.pop_back();

			// qd_string_concat_smart consumes both strings (releases them)
			auto* fnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false);
			auto* fn = getOrDeclareFunc("qd_string_concat_smart", fnTy);

			// Need to retain if they are string literals (not already ref-counted)
			llvm::Value* s1Val = str1.value;
			llvm::Value* s2Val = str2.value;

			// If string literals, create qd_string from them first
			if (str1.isStringLiteral) {
				auto* createFn = module->getFunction("qd_string_create");
				s1Val = builder->CreateCall(createFn, {str1.value}, "s1_qd");
			} else if (!str1.needsRelease) {
				// Not a literal but also doesn't need release - retain it
				s1Val = builder->CreateCall(qdStringRetainFn, {str1.value});
			}

			if (str2.isStringLiteral) {
				auto* createFn = module->getFunction("qd_string_create");
				s2Val = builder->CreateCall(createFn, {str2.value}, "s2_qd");
			} else if (!str2.needsRelease) {
				s2Val = builder->CreateCall(qdStringRetainFn, {str2.value});
			}

			auto* result = builder->CreateCall(fn, {s1Val, s2Val}, "concat_result");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// str::from_char ( char_code:int -- s:str )
		if (name == "from_char") {
			if (valueStack.empty()) {
				reportError("Stack underflow for str::from_char", 0);
				return true;
			}
			auto charCode = valueStack.back();
			valueStack.pop_back();

			// Allocate 2-byte buffer on stack
			auto* buf = builder->CreateAlloca(builder->getInt8Ty(), builder->getInt32(2), "char_buf");

			// Store character
			llvm::Value* charVal = charCode.value;
			if (charCode.type == ValueType::FLOAT) {
				charVal = builder->CreateFPToSI(charVal, builder->getInt64Ty(), "to_int");
			}
			auto* truncChar = builder->CreateTrunc(charVal, builder->getInt8Ty(), "char8");
			builder->CreateStore(truncChar, buf);

			// Store null terminator
			auto* nullPos = builder->CreateGEP(builder->getInt8Ty(), buf, {builder->getInt32(1)}, "null_pos");
			builder->CreateStore(builder->getInt8(0), nullPos);

			// Create qd_string from buffer
			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {buf}, "str_from_char");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// str::contains ( haystack:str needle:str -- found:int )
		if (name == "contains") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::contains", 0);
				return true;
			}
			auto needle = valueStack.back();
			valueStack.pop_back();
			auto haystack = valueStack.back();
			valueStack.pop_back();

			// Get qd_string_data for both strings
			auto* dataFn = module->getFunction("qd_string_data");
			auto* haystackData = builder->CreateCall(dataFn, {haystack.value}, "haystack_data");
			auto* needleData = builder->CreateCall(dataFn, {needle.value}, "needle_data");

			// Call strstr
			auto* strstrFn = module->getFunction("strstr");
			auto* pos = builder->CreateCall(strstrFn, {haystackData, needleData}, "strstr_result");

			// Convert to boolean (1 if found, 0 if not)
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* found = builder->CreateICmpNE(pos, nullPtr, "found");
			auto* result = builder->CreateZExt(found, builder->getInt64Ty(), "contains_result");

			// Release strings
			if (haystack.needsRelease) {
				builder->CreateCall(qdStringReleaseFn, {haystack.value});
			}
			if (needle.needsRelease) {
				builder->CreateCall(qdStringReleaseFn, {needle.value});
			}

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// str::starts_with ( s:str prefix:str -- result:int )
		if (name == "starts_with") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::starts_with", 0);
				return true;
			}
			auto prefix = valueStack.back();
			valueStack.pop_back();
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");
			auto* prefixData = builder->CreateCall(dataFn, {prefix.value}, "prefix_data");

			// Get prefix length
			auto* strlenFn = module->getFunction("strlen");
			auto* prefixLen = builder->CreateCall(strlenFn, {prefixData}, "prefix_len");

			// strncmp(str, prefix, prefix_len) == 0
			auto* strncmpFn = module->getFunction("strncmp");
			auto* cmp = builder->CreateCall(strncmpFn, {strData, prefixData, prefixLen}, "strncmp_result");
			auto* isZero = builder->CreateICmpEQ(cmp, builder->getInt32(0), "is_match");
			auto* result = builder->CreateZExt(isZero, builder->getInt64Ty(), "starts_with_result");

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});
			if (prefix.needsRelease) builder->CreateCall(qdStringReleaseFn, {prefix.value});

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// str::ends_with ( s:str suffix:str -- result:int )
		if (name == "ends_with") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::ends_with", 0);
				return true;
			}
			auto suffix = valueStack.back();
			valueStack.pop_back();
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");
			auto* suffixData = builder->CreateCall(dataFn, {suffix.value}, "suffix_data");

			auto* strlenFn = module->getFunction("strlen");
			auto* strLen = builder->CreateCall(strlenFn, {strData}, "str_len");
			auto* suffixLen = builder->CreateCall(strlenFn, {suffixData}, "suffix_len");

			// if (suffixLen > strLen) return 0
			auto* lenCmp = builder->CreateICmpUGT(suffixLen, strLen, "len_cmp");

			// Create blocks for conditional
			auto* checkBB = llvm::BasicBlock::Create(*context, "check_suffix", currentFunction);
			auto* falseBB = llvm::BasicBlock::Create(*context, "ends_false", currentFunction);
			auto* mergeBB = llvm::BasicBlock::Create(*context, "ends_merge", currentFunction);

			builder->CreateCondBr(lenCmp, falseBB, checkBB);

			// False block
			builder->SetInsertPoint(falseBB);
			auto* falseVal = builder->getInt64(0);
			builder->CreateBr(mergeBB);

			// Check block
			builder->SetInsertPoint(checkBB);
			auto* offset = builder->CreateSub(strLen, suffixLen, "offset");
			auto* endPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {offset}, "end_ptr");
			auto* strcmpFn = module->getFunction("strcmp");
			auto* cmp = builder->CreateCall(strcmpFn, {endPtr, suffixData}, "strcmp_result");
			auto* isZero = builder->CreateICmpEQ(cmp, builder->getInt32(0), "is_match");
			auto* trueVal = builder->CreateZExt(isZero, builder->getInt64Ty(), "ends_with_result");
			builder->CreateBr(mergeBB);

			// Merge block
			builder->SetInsertPoint(mergeBB);
			auto* phi = builder->CreatePHI(builder->getInt64Ty(), 2, "ends_result");
			phi->addIncoming(falseVal, falseBB);
			phi->addIncoming(trueVal, checkBB);

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});
			if (suffix.needsRelease) builder->CreateCall(qdStringReleaseFn, {suffix.value});

			valueStack.push_back(TrackedValue(phi, ValueType::INT));
			return true;
		}

		// str::index_of ( haystack:str needle:str -- index:int )
		if (name == "index_of") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::index_of", 0);
				return true;
			}
			auto needle = valueStack.back();
			valueStack.pop_back();
			auto haystack = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* haystackData = builder->CreateCall(dataFn, {haystack.value}, "haystack_data");
			auto* needleData = builder->CreateCall(dataFn, {needle.value}, "needle_data");

			auto* strstrFn = module->getFunction("strstr");
			auto* pos = builder->CreateCall(strstrFn, {haystackData, needleData}, "strstr_result");

			// if pos == NULL, return -1, else return pos - haystack
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* isNull = builder->CreateICmpEQ(pos, nullPtr, "is_null");

			auto* foundBB = llvm::BasicBlock::Create(*context, "found", currentFunction);
			auto* notFoundBB = llvm::BasicBlock::Create(*context, "not_found", currentFunction);
			auto* mergeBB = llvm::BasicBlock::Create(*context, "merge", currentFunction);

			builder->CreateCondBr(isNull, notFoundBB, foundBB);

			builder->SetInsertPoint(notFoundBB);
			auto* notFoundVal = builder->getInt64(static_cast<uint64_t>(-1));
			builder->CreateBr(mergeBB);

			builder->SetInsertPoint(foundBB);
			auto* posInt = builder->CreatePtrToInt(pos, builder->getInt64Ty(), "pos_int");
			auto* haystackInt = builder->CreatePtrToInt(haystackData, builder->getInt64Ty(), "haystack_int");
			auto* foundVal = builder->CreateSub(posInt, haystackInt, "index");
			builder->CreateBr(mergeBB);

			builder->SetInsertPoint(mergeBB);
			auto* phi = builder->CreatePHI(builder->getInt64Ty(), 2, "index_result");
			phi->addIncoming(notFoundVal, notFoundBB);
			phi->addIncoming(foundVal, foundBB);

			if (haystack.needsRelease) builder->CreateCall(qdStringReleaseFn, {haystack.value});
			if (needle.needsRelease) builder->CreateCall(qdStringReleaseFn, {needle.value});

			valueStack.push_back(TrackedValue(phi, ValueType::INT));
			return true;
		}

		// str::index_of_from ( haystack:str needle:str start:i64 -- index:i64 )
		if (name == "index_of_from") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for str::index_of_from", 0);
				return true;
			}
			auto start = valueStack.back();
			valueStack.pop_back();
			auto needle = valueStack.back();
			valueStack.pop_back();
			auto haystack = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* haystackData = builder->CreateCall(dataFn, {haystack.value}, "haystack_data");
			auto* needleData = builder->CreateCall(dataFn, {needle.value}, "needle_data");

			// Get pointer at start offset
			llvm::Value* startOffset = start.value;
			if (start.type == ValueType::FLOAT) {
				startOffset = builder->CreateFPToSI(startOffset, builder->getInt64Ty(), "to_int");
			}
			auto* startPtr = builder->CreateGEP(builder->getInt8Ty(), haystackData, {startOffset}, "start_ptr");

			auto* strstrFn = module->getFunction("strstr");
			auto* pos = builder->CreateCall(strstrFn, {startPtr, needleData}, "strstr_result");

			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* isNull = builder->CreateICmpEQ(pos, nullPtr, "is_null");

			auto* foundBB = llvm::BasicBlock::Create(*context, "found", currentFunction);
			auto* notFoundBB = llvm::BasicBlock::Create(*context, "not_found", currentFunction);
			auto* mergeBB = llvm::BasicBlock::Create(*context, "merge", currentFunction);

			builder->CreateCondBr(isNull, notFoundBB, foundBB);

			builder->SetInsertPoint(notFoundBB);
			auto* notFoundVal = builder->getInt64(static_cast<uint64_t>(-1));
			builder->CreateBr(mergeBB);

			builder->SetInsertPoint(foundBB);
			auto* posInt = builder->CreatePtrToInt(pos, builder->getInt64Ty(), "pos_int");
			auto* haystackInt = builder->CreatePtrToInt(haystackData, builder->getInt64Ty(), "haystack_int");
			auto* foundVal = builder->CreateSub(posInt, haystackInt, "index");
			builder->CreateBr(mergeBB);

			builder->SetInsertPoint(mergeBB);
			auto* phi = builder->CreatePHI(builder->getInt64Ty(), 2, "index_result");
			phi->addIncoming(notFoundVal, notFoundBB);
			phi->addIncoming(foundVal, foundBB);

			if (haystack.needsRelease) builder->CreateCall(qdStringReleaseFn, {haystack.value});
			if (needle.needsRelease) builder->CreateCall(qdStringReleaseFn, {needle.value});

			valueStack.push_back(TrackedValue(phi, ValueType::INT));
			return true;
		}

		// str::substring ( str:str start:i64 length:i64 -- result:str )
		if (name == "substring") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for str::substring", 0);
				return true;
			}
			auto length = valueStack.back();
			valueStack.pop_back();
			auto start = valueStack.back();
			valueStack.pop_back();
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* createFn = module->getFunction("qd_string_create_with_length");

			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");

			llvm::Value* startOffset = start.value;
			if (start.type == ValueType::FLOAT) {
				startOffset = builder->CreateFPToSI(startOffset, builder->getInt64Ty(), "to_int");
			}
			llvm::Value* len = length.value;
			if (length.type == ValueType::FLOAT) {
				len = builder->CreateFPToSI(len, builder->getInt64Ty(), "to_int");
			}

			auto* subPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {startOffset}, "sub_ptr");
			auto* result = builder->CreateCall(createFn, {subPtr, len}, "substring_result");

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// str::char_at ( s:str index:int -- char_code:int )
		if (name == "char_at") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::char_at", 0);
				return true;
			}
			auto index = valueStack.back();
			valueStack.pop_back();
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");

			llvm::Value* idx = index.value;
			if (index.type == ValueType::FLOAT) {
				idx = builder->CreateFPToSI(idx, builder->getInt64Ty(), "to_int");
			}

			auto* charPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {idx}, "char_ptr");
			auto* charVal = builder->CreateLoad(builder->getInt8Ty(), charPtr, "char");
			auto* result = builder->CreateZExt(charVal, builder->getInt64Ty(), "char_code");

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// str::compare ( s1:str s2:str -- result:int )
		if (name == "compare") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for str::compare", 0);
				return true;
			}
			auto s2 = valueStack.back();
			valueStack.pop_back();
			auto s1 = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* s1Data = builder->CreateCall(dataFn, {s1.value}, "s1_data");
			auto* s2Data = builder->CreateCall(dataFn, {s2.value}, "s2_data");

			auto* strcmpFn = module->getFunction("strcmp");
			auto* cmp = builder->CreateCall(strcmpFn, {s1Data, s2Data}, "cmp_result");
			auto* result = builder->CreateSExt(cmp, builder->getInt64Ty(), "compare_result");

			if (s1.needsRelease) builder->CreateCall(qdStringReleaseFn, {s1.value});
			if (s2.needsRelease) builder->CreateCall(qdStringReleaseFn, {s2.value});

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// str::upper ( s:str -- result:str )
		if (name == "upper") {
			if (valueStack.empty()) {
				reportError("Stack underflow for str::upper", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			// Get string data and length
			auto* dataFn = module->getFunction("qd_string_data");
			auto* lenFn = module->getFunction("qd_string_length");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");
			auto* strLen = builder->CreateCall(lenFn, {str.value}, "str_len");

			// Allocate buffer for result: len + 1 for null terminator
			auto* lenPlus1 = builder->CreateAdd(strLen, builder->getInt64(1), "len_plus_1");
			auto* buf = builder->CreateCall(mallocFn, {lenPlus1}, "upper_buf");

			// Loop: for each char, convert to uppercase
			auto* entryBB = builder->GetInsertBlock();
			auto* loopBB = llvm::BasicBlock::Create(*context, "upper_loop", currentFunction);
			auto* bodyBB = llvm::BasicBlock::Create(*context, "upper_body", currentFunction);
			auto* exitBB = llvm::BasicBlock::Create(*context, "upper_exit", currentFunction);

			builder->CreateBr(loopBB);
			builder->SetInsertPoint(loopBB);

			// Index phi
			auto* idxPhi = builder->CreatePHI(builder->getInt64Ty(), 2, "idx");
			idxPhi->addIncoming(builder->getInt64(0), entryBB);

			// Check if done
			auto* done = builder->CreateICmpSGE(idxPhi, strLen, "done");
			builder->CreateCondBr(done, exitBB, bodyBB);

			builder->SetInsertPoint(bodyBB);

			// Load char, convert to uppercase, store
			auto* srcPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {idxPhi}, "src_ptr");
			auto* dstPtr = builder->CreateGEP(builder->getInt8Ty(), buf, {idxPhi}, "dst_ptr");
			auto* ch = builder->CreateLoad(builder->getInt8Ty(), srcPtr, "ch");

			// toupper: if (ch >= 'a' && ch <= 'z') ch -= 32
			auto* isLowerA = builder->CreateICmpSGE(ch, builder->getInt8('a'), "is_lower_a");
			auto* isLowerZ = builder->CreateICmpSLE(ch, builder->getInt8('z'), "is_lower_z");
			auto* isLower = builder->CreateAnd(isLowerA, isLowerZ, "is_lower");
			auto* upperCh = builder->CreateSub(ch, builder->getInt8(32), "upper_ch");
			auto* resultCh = builder->CreateSelect(isLower, upperCh, ch, "result_ch");
			builder->CreateStore(resultCh, dstPtr);

			auto* nextIdx = builder->CreateAdd(idxPhi, builder->getInt64(1), "next_idx");
			idxPhi->addIncoming(nextIdx, bodyBB);
			builder->CreateBr(loopBB);

			builder->SetInsertPoint(exitBB);

			// Null terminate
			auto* nullPtr = builder->CreateGEP(builder->getInt8Ty(), buf, {strLen}, "null_ptr");
			builder->CreateStore(builder->getInt8(0), nullPtr);

			// Create qd_string from buffer and free buffer
			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {buf}, "upper_str");
			builder->CreateCall(freeFn, {buf});

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// str::lower ( s:str -- result:str )
		if (name == "lower") {
			if (valueStack.empty()) {
				reportError("Stack underflow for str::lower", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* lenFn = module->getFunction("qd_string_length");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");
			auto* strLen = builder->CreateCall(lenFn, {str.value}, "str_len");

			auto* lenPlus1 = builder->CreateAdd(strLen, builder->getInt64(1), "len_plus_1");
			auto* buf = builder->CreateCall(mallocFn, {lenPlus1}, "lower_buf");

			auto* entryBB = builder->GetInsertBlock();
			auto* loopBB = llvm::BasicBlock::Create(*context, "lower_loop", currentFunction);
			auto* bodyBB = llvm::BasicBlock::Create(*context, "lower_body", currentFunction);
			auto* exitBB = llvm::BasicBlock::Create(*context, "lower_exit", currentFunction);

			builder->CreateBr(loopBB);
			builder->SetInsertPoint(loopBB);

			auto* idxPhi = builder->CreatePHI(builder->getInt64Ty(), 2, "idx");
			idxPhi->addIncoming(builder->getInt64(0), entryBB);

			auto* done = builder->CreateICmpSGE(idxPhi, strLen, "done");
			builder->CreateCondBr(done, exitBB, bodyBB);

			builder->SetInsertPoint(bodyBB);

			auto* srcPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {idxPhi}, "src_ptr");
			auto* dstPtr = builder->CreateGEP(builder->getInt8Ty(), buf, {idxPhi}, "dst_ptr");
			auto* ch = builder->CreateLoad(builder->getInt8Ty(), srcPtr, "ch");

			// tolower: if (ch >= 'A' && ch <= 'Z') ch += 32
			auto* isUpperA = builder->CreateICmpSGE(ch, builder->getInt8('A'), "is_upper_a");
			auto* isUpperZ = builder->CreateICmpSLE(ch, builder->getInt8('Z'), "is_upper_z");
			auto* isUpper = builder->CreateAnd(isUpperA, isUpperZ, "is_upper");
			auto* lowerCh = builder->CreateAdd(ch, builder->getInt8(32), "lower_ch");
			auto* resultCh = builder->CreateSelect(isUpper, lowerCh, ch, "result_ch");
			builder->CreateStore(resultCh, dstPtr);

			auto* nextIdx = builder->CreateAdd(idxPhi, builder->getInt64(1), "next_idx");
			idxPhi->addIncoming(nextIdx, bodyBB);
			builder->CreateBr(loopBB);

			builder->SetInsertPoint(exitBB);

			auto* nullPtr = builder->CreateGEP(builder->getInt8Ty(), buf, {strLen}, "null_ptr");
			builder->CreateStore(builder->getInt8(0), nullPtr);

			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {buf}, "lower_str");
			builder->CreateCall(freeFn, {buf});

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// str::trim ( s:str -- result:str )
		if (name == "trim") {
			if (valueStack.empty()) {
				reportError("Stack underflow for str::trim", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* lenFn = module->getFunction("qd_string_length");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");
			auto* strLen = builder->CreateCall(lenFn, {str.value}, "str_len");

			// Find start (skip leading whitespace)
			auto* entryBB = builder->GetInsertBlock();
			auto* startLoopBB = llvm::BasicBlock::Create(*context, "trim_start_loop", currentFunction);
			auto* startBodyBB = llvm::BasicBlock::Create(*context, "trim_start_body", currentFunction);
			auto* findEndBB = llvm::BasicBlock::Create(*context, "trim_find_end", currentFunction);

			builder->CreateBr(startLoopBB);
			builder->SetInsertPoint(startLoopBB);

			auto* startPhi = builder->CreatePHI(builder->getInt64Ty(), 2, "start");
			startPhi->addIncoming(builder->getInt64(0), entryBB);

			auto* startDone = builder->CreateICmpSGE(startPhi, strLen, "start_done");
			builder->CreateCondBr(startDone, findEndBB, startBodyBB);

			builder->SetInsertPoint(startBodyBB);
			auto* startPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {startPhi}, "start_ptr");
			auto* startCh = builder->CreateLoad(builder->getInt8Ty(), startPtr, "start_ch");

			// Check if whitespace (space, tab, newline, carriage return)
			auto* isSpace = builder->CreateICmpEQ(startCh, builder->getInt8(' '), "is_space");
			auto* isTab = builder->CreateICmpEQ(startCh, builder->getInt8('\t'), "is_tab");
			auto* isNL = builder->CreateICmpEQ(startCh, builder->getInt8('\n'), "is_nl");
			auto* isCR = builder->CreateICmpEQ(startCh, builder->getInt8('\r'), "is_cr");
			auto* isWS1 = builder->CreateOr(isSpace, isTab, "is_ws1");
			auto* isWS2 = builder->CreateOr(isNL, isCR, "is_ws2");
			auto* isWhitespace = builder->CreateOr(isWS1, isWS2, "is_whitespace");

			auto* nextStart = builder->CreateAdd(startPhi, builder->getInt64(1), "next_start");
			auto* startToUse = builder->CreateSelect(isWhitespace, nextStart, startPhi, "start_to_use");
			startPhi->addIncoming(startToUse, startBodyBB);

			// If not whitespace, go to find end; otherwise loop
			builder->CreateCondBr(isWhitespace, startLoopBB, findEndBB);

			// Find end (skip trailing whitespace)
			builder->SetInsertPoint(findEndBB);
			auto* startResult = builder->CreatePHI(builder->getInt64Ty(), 2, "start_result");
			startResult->addIncoming(strLen, startLoopBB); // All whitespace case
			startResult->addIncoming(startPhi, startBodyBB);

			auto* endLoopBB = llvm::BasicBlock::Create(*context, "trim_end_loop", currentFunction);
			auto* endBodyBB = llvm::BasicBlock::Create(*context, "trim_end_body", currentFunction);
			auto* copyBB = llvm::BasicBlock::Create(*context, "trim_copy", currentFunction);

			// Start end search from strLen - 1
			auto* initialEnd = builder->CreateSub(strLen, builder->getInt64(1), "initial_end");
			builder->CreateBr(endLoopBB);

			builder->SetInsertPoint(endLoopBB);
			auto* endPhi = builder->CreatePHI(builder->getInt64Ty(), 2, "end");
			endPhi->addIncoming(initialEnd, findEndBB);

			// If end < start, string is all whitespace
			auto* endDone = builder->CreateICmpSLT(endPhi, startResult, "end_done");
			builder->CreateCondBr(endDone, copyBB, endBodyBB);

			builder->SetInsertPoint(endBodyBB);
			auto* endPtr = builder->CreateGEP(builder->getInt8Ty(), strData, {endPhi}, "end_ptr");
			auto* endCh = builder->CreateLoad(builder->getInt8Ty(), endPtr, "end_ch");

			auto* isSpaceE = builder->CreateICmpEQ(endCh, builder->getInt8(' '), "is_space_e");
			auto* isTabE = builder->CreateICmpEQ(endCh, builder->getInt8('\t'), "is_tab_e");
			auto* isNLE = builder->CreateICmpEQ(endCh, builder->getInt8('\n'), "is_nl_e");
			auto* isCRE = builder->CreateICmpEQ(endCh, builder->getInt8('\r'), "is_cr_e");
			auto* isWS1E = builder->CreateOr(isSpaceE, isTabE, "is_ws1_e");
			auto* isWS2E = builder->CreateOr(isNLE, isCRE, "is_ws2_e");
			auto* isWhitespaceE = builder->CreateOr(isWS1E, isWS2E, "is_whitespace_e");

			auto* prevEnd = builder->CreateSub(endPhi, builder->getInt64(1), "prev_end");
			auto* endToUse = builder->CreateSelect(isWhitespaceE, prevEnd, endPhi, "end_to_use");
			endPhi->addIncoming(endToUse, endBodyBB);

			builder->CreateCondBr(isWhitespaceE, endLoopBB, copyBB);

			// Copy trimmed substring
			builder->SetInsertPoint(copyBB);
			auto* endResult = builder->CreatePHI(builder->getInt64Ty(), 2, "end_result");
			endResult->addIncoming(startResult, endLoopBB); // All whitespace: end < start
			endResult->addIncoming(endPhi, endBodyBB);

			// Calculate length: end - start + 1, but 0 if all whitespace
			auto* allWS = builder->CreateICmpSLT(endResult, startResult, "all_ws");
			auto* trimLen = builder->CreateSub(endResult, startResult, "raw_len");
			auto* trimLenPlus1 = builder->CreateAdd(trimLen, builder->getInt64(1), "trim_len");
			auto* finalLen = builder->CreateSelect(allWS, builder->getInt64(0), trimLenPlus1, "final_len");

			// Allocate buffer
			auto* bufLen = builder->CreateAdd(finalLen, builder->getInt64(1), "buf_len");
			auto* buf = builder->CreateCall(mallocFn, {bufLen}, "trim_buf");

			// Copy using memcpy
			auto* memcpyFn = module->getFunction("memcpy");
			if (!memcpyFn) {
				auto* fnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy, builder->getInt64Ty()}, false);
				memcpyFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "memcpy", *module);
			}
			auto* srcStart = builder->CreateGEP(builder->getInt8Ty(), strData, {startResult}, "src_start");
			builder->CreateCall(memcpyFn, {buf, srcStart, finalLen});

			// Null terminate
			auto* nullPos = builder->CreateGEP(builder->getInt8Ty(), buf, {finalLen}, "null_pos");
			builder->CreateStore(builder->getInt8(0), nullPos);

			// Create qd_string from buffer
			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {buf}, "trim_str");
			builder->CreateCall(freeFn, {buf});

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		return false;
	}

	bool RegisterGenerator::generateNativeStrconvFunction(const std::string& name) {
		auto ptrTy = llvm::PointerType::getUnqual(*context);

		// strconv::parse_int ( s:str -- value:int )
		if (name == "parse_int") {
			if (valueStack.empty()) {
				reportError("Stack underflow for strconv::parse_int", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");

			// strtoll(str, NULL, 10)
			auto* strtollFn = module->getFunction("strtoll");
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* result =
					builder->CreateCall(strtollFn, {strData, nullPtr, builder->getInt32(10)}, "parse_int_result");

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// strconv::format_int ( value:i64 base:i64 -- s:str )
		if (name == "format_int") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for strconv::format_int", 0);
				return true;
			}
			// Stack: value (bottom), base (top)
			auto baseVal = valueStack.back();
			valueStack.pop_back();
			auto val = valueStack.back();
			valueStack.pop_back();

			llvm::Value* intVal = val.value;
			if (val.type == ValueType::FLOAT) {
				intVal = builder->CreateFPToSI(intVal, builder->getInt64Ty(), "to_int");
			}
			llvm::Value* baseInt = baseVal.value;
			if (baseVal.type == ValueType::FLOAT) {
				baseInt = builder->CreateFPToSI(baseInt, builder->getInt64Ty(), "to_int");
			}

			// Declare qd_string_from_int_base if not exists
			auto* fromIntBaseFn = module->getFunction("qd_string_from_int_base");
			if (!fromIntBaseFn) {
				auto fromIntBaseFnTy = llvm::FunctionType::get(ptrTy,
					{builder->getInt64Ty(), builder->getInt32Ty()}, false);
				fromIntBaseFn = llvm::Function::Create(fromIntBaseFnTy,
					llvm::Function::ExternalLinkage, "qd_string_from_int_base", *module);
			}
			auto* base32 = builder->CreateTrunc(baseInt, builder->getInt32Ty(), "base32");
			auto* result = builder->CreateCall(fromIntBaseFn, {intVal, base32}, "format_int_result");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// strconv::parse_float ( s:str -- value:float )
		if (name == "parse_float") {
			if (valueStack.empty()) {
				reportError("Stack underflow for strconv::parse_float", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");

			// strtod(str, NULL)
			auto* strtodFn = module->getFunction("strtod");
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* result = builder->CreateCall(strtodFn, {strData, nullPtr}, "parse_float_result");

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			return true;
		}

		// strconv::format_float ( value:float -- s:str )
		if (name == "format_float") {
			if (valueStack.empty()) {
				reportError("Stack underflow for strconv::format_float", 0);
				return true;
			}
			auto val = valueStack.back();
			valueStack.pop_back();

			llvm::Value* floatVal = val.value;
			if (val.type == ValueType::INT) {
				floatVal = builder->CreateSIToFP(floatVal, builder->getDoubleTy(), "to_float");
			}

			// qd_string_from_double(value)
			auto* fromDoubleFn = module->getFunction("qd_string_from_double");
			auto* result = builder->CreateCall(fromDoubleFn, {floatVal}, "format_float_result");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// strconv::itoa ( value:int -- s:str ) - shorthand for format_int base 10
		if (name == "itoa") {
			if (valueStack.empty()) {
				reportError("Stack underflow for strconv::itoa", 0);
				return true;
			}
			auto val = valueStack.back();
			valueStack.pop_back();

			llvm::Value* intVal = val.value;
			if (val.type == ValueType::FLOAT) {
				intVal = builder->CreateFPToSI(intVal, builder->getInt64Ty(), "to_int");
			}

			// qd_string_from_int(value)
			auto* fromIntFn = module->getFunction("qd_string_from_int");
			auto* result = builder->CreateCall(fromIntFn, {intVal}, "itoa_result");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// strconv::atoi ( s:str -- value:int ) - shorthand for parse_int base 10
		if (name == "atoi") {
			if (valueStack.empty()) {
				reportError("Stack underflow for strconv::atoi", 0);
				return true;
			}
			auto str = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* strData = builder->CreateCall(dataFn, {str.value}, "str_data");

			// strtoll(str, NULL, 10)
			auto* strtollFn = module->getFunction("strtoll");
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* result =
					builder->CreateCall(strtollFn, {strData, nullPtr, builder->getInt32(10)}, "atoi_result");

			if (str.needsRelease) builder->CreateCall(qdStringReleaseFn, {str.value});

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		return false;
	}

	bool RegisterGenerator::generateNativeMemFunction(const std::string& name) {
		auto ptrTy = llvm::PointerType::getUnqual(*context);

		// mem::alloc ( size:int -- ptr:ptr )
		if (name == "alloc") {
			if (valueStack.empty()) {
				reportError("Stack underflow for mem::alloc", 0);
				return true;
			}
			auto size = valueStack.back();
			valueStack.pop_back();

			llvm::Value* sizeVal = size.value;
			if (size.type == ValueType::FLOAT) {
				sizeVal = builder->CreateFPToUI(sizeVal, builder->getInt64Ty(), "to_size");
			}

			auto* result = builder->CreateCall(mallocFn, {sizeVal}, "mem_alloc");

			valueStack.push_back(TrackedValue(result, ValueType::PTR));
			return true;
		}

		// mem::free ( ptr:ptr -- )
		if (name == "free") {
			if (valueStack.empty()) {
				reportError("Stack underflow for mem::free", 0);
				return true;
			}
			auto ptr = valueStack.back();
			valueStack.pop_back();

			builder->CreateCall(freeFn, {ptr.value});
			return true;
		}

		// mem::set_i ( ptr:ptr offset:int value:int -- )
		if (name == "set_i" || name == "set_i64") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for mem::set_i", 0);
				return true;
			}
			auto val = valueStack.back();
			valueStack.pop_back();
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* intVal = val.value;
			if (val.type == ValueType::FLOAT) {
				intVal = builder->CreateFPToSI(intVal, builder->getInt64Ty(), "to_int");
			}

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			// Calculate byte offset (offset * 8 for 64-bit values)
			auto* byteOffset = builder->CreateMul(offsetVal, builder->getInt64(8), "byte_offset");
			auto* destPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {byteOffset}, "dest_ptr");
			builder->CreateStore(intVal, destPtr);

			return true;
		}

		// mem::get_i ( ptr:ptr offset:int -- value:int )
		if (name == "get_i" || name == "get_i64") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::get_i", 0);
				return true;
			}
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			auto* byteOffset = builder->CreateMul(offsetVal, builder->getInt64(8), "byte_offset");
			auto* srcPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {byteOffset}, "src_ptr");
			auto* result = builder->CreateLoad(builder->getInt64Ty(), srcPtr, "mem_value");

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// mem::set_f64 ( ptr:ptr offset:int value:float -- )
		if (name == "set_f" || name == "set_f64") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for mem::set_f64", 0);
				return true;
			}
			auto val = valueStack.back();
			valueStack.pop_back();
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* floatVal = val.value;
			if (val.type == ValueType::INT) {
				floatVal = builder->CreateSIToFP(floatVal, builder->getDoubleTy(), "to_float");
			}

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			auto* byteOffset = builder->CreateMul(offsetVal, builder->getInt64(8), "byte_offset");
			auto* destPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {byteOffset}, "dest_ptr");
			builder->CreateStore(floatVal, destPtr);

			return true;
		}

		// mem::get_f64 ( ptr:ptr offset:int -- value:float )
		if (name == "get_f" || name == "get_f64") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::get_f64", 0);
				return true;
			}
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			auto* byteOffset = builder->CreateMul(offsetVal, builder->getInt64(8), "byte_offset");
			auto* srcPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {byteOffset}, "src_ptr");
			auto* result = builder->CreateLoad(builder->getDoubleTy(), srcPtr, "mem_float");

			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
			return true;
		}

		// mem::set_byte ( value:i64 address:ptr offset:i64 -- )
		if (name == "set_byte") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for mem::set_byte", 0);
				return true;
			}
			// Stack order: value (bottom), address, offset (top)
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();
			auto val = valueStack.back();
			valueStack.pop_back();

			llvm::Value* byteVal = val.value;
			if (val.type == ValueType::FLOAT) {
				byteVal = builder->CreateFPToUI(byteVal, builder->getInt64Ty(), "to_int");
			}
			byteVal = builder->CreateTrunc(byteVal, builder->getInt8Ty(), "to_byte");

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			auto* destPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {offsetVal}, "dest_ptr");
			builder->CreateStore(byteVal, destPtr);

			return true;
		}

		// mem::get_byte ( ptr:ptr offset:int -- value:int )
		if (name == "get_byte") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::get_byte", 0);
				return true;
			}
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			auto* srcPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {offsetVal}, "src_ptr");
			auto* byteVal = builder->CreateLoad(builder->getInt8Ty(), srcPtr, "byte_value");
			auto* result = builder->CreateZExt(byteVal, builder->getInt64Ty(), "mem_byte");

			valueStack.push_back(TrackedValue(result, ValueType::INT));
			return true;
		}

		// mem::set_ptr ( ptr:ptr offset:int value:ptr -- )
		if (name == "set_ptr") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for mem::set_ptr", 0);
				return true;
			}
			auto val = valueStack.back();
			valueStack.pop_back();
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			// Calculate byte offset (offset * 8 for 64-bit pointers)
			auto* byteOffset = builder->CreateMul(offsetVal, builder->getInt64(8), "byte_offset");
			auto* destPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {byteOffset}, "dest_ptr");
			builder->CreateStore(val.value, destPtr);

			return true;
		}

		// mem::get_ptr ( ptr:ptr offset:int -- value:ptr )
		if (name == "get_ptr") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::get_ptr", 0);
				return true;
			}
			auto offset = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* offsetVal = offset.value;
			if (offset.type == ValueType::FLOAT) {
				offsetVal = builder->CreateFPToSI(offsetVal, builder->getInt64Ty(), "to_int");
			}

			auto* byteOffset = builder->CreateMul(offsetVal, builder->getInt64(8), "byte_offset");
			auto* srcPtr = builder->CreateGEP(builder->getInt8Ty(), ptr.value, {byteOffset}, "src_ptr");
			auto* result = builder->CreateLoad(ptrTy, srcPtr, "mem_ptr");

			valueStack.push_back(TrackedValue(result, ValueType::PTR));
			return true;
		}

		// mem::copy ( dest:ptr src:ptr size:int -- )
		if (name == "copy") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for mem::copy", 0);
				return true;
			}
			auto size = valueStack.back();
			valueStack.pop_back();
			auto src = valueStack.back();
			valueStack.pop_back();
			auto dest = valueStack.back();
			valueStack.pop_back();

			llvm::Value* sizeVal = size.value;
			if (size.type == ValueType::FLOAT) {
				sizeVal = builder->CreateFPToUI(sizeVal, builder->getInt64Ty(), "to_size");
			}

			auto* memcpyFn = module->getFunction("memcpy");
			builder->CreateCall(memcpyFn, {dest.value, src.value, sizeVal});

			return true;
		}

		// mem::zero ( ptr:ptr size:int -- )
		if (name == "zero") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::zero", 0);
				return true;
			}
			auto size = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* sizeVal = size.value;
			if (size.type == ValueType::FLOAT) {
				sizeVal = builder->CreateFPToUI(sizeVal, builder->getInt64Ty(), "to_size");
			}

			auto* memsetFn = module->getFunction("memset");
			builder->CreateCall(memsetFn, {ptr.value, builder->getInt32(0), sizeVal});

			return true;
		}

		// mem::fill ( value:int ptr:ptr size:int -- )
		if (name == "fill") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for mem::fill", 0);
				return true;
			}
			auto size = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();
			auto val = valueStack.back();
			valueStack.pop_back();

			llvm::Value* sizeVal = size.value;
			if (size.type == ValueType::FLOAT) {
				sizeVal = builder->CreateFPToUI(sizeVal, builder->getInt64Ty(), "to_size");
			}
			llvm::Value* byteVal = val.value;
			if (val.type == ValueType::FLOAT) {
				byteVal = builder->CreateFPToUI(byteVal, builder->getInt64Ty(), "to_int");
			}
			byteVal = builder->CreateTrunc(byteVal, builder->getInt32Ty(), "to_byte32");

			auto* memsetFn = module->getFunction("memset");
			builder->CreateCall(memsetFn, {ptr.value, byteVal, sizeVal});

			return true;
		}

		// mem::realloc ( ptr:ptr size:int -- ptr:ptr )
		if (name == "realloc") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::realloc", 0);
				return true;
			}
			auto size = valueStack.back();
			valueStack.pop_back();
			auto ptr = valueStack.back();
			valueStack.pop_back();

			llvm::Value* sizeVal = size.value;
			if (size.type == ValueType::FLOAT) {
				sizeVal = builder->CreateFPToUI(sizeVal, builder->getInt64Ty(), "to_size");
			}

			auto* reallocFn = module->getFunction("realloc");
			auto* result = builder->CreateCall(reallocFn, {ptr.value, sizeVal}, "mem_realloc");

			valueStack.push_back(TrackedValue(result, ValueType::PTR));
			return true;
		}

		// mem::to_string ( buffer:ptr length:int -- text:str )
		if (name == "to_string") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for mem::to_string", 0);
				return true;
			}
			auto length = valueStack.back();
			valueStack.pop_back();
			auto buffer = valueStack.back();
			valueStack.pop_back();

			llvm::Value* lenVal = length.value;
			if (length.type == ValueType::FLOAT) {
				lenVal = builder->CreateFPToUI(lenVal, builder->getInt64Ty(), "to_len");
			}

			auto* createFn = module->getFunction("qd_string_create_with_length");
			auto* result = builder->CreateCall(createFn, {buffer.value, lenVal}, "to_string");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// mem::from_string ( text:str -- buffer:ptr length:int )
		if (name == "from_string") {
			if (valueStack.empty()) {
				reportError("Stack underflow for mem::from_string", 0);
				return true;
			}
			auto text = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* lenFn = module->getFunction("qd_string_length");

			auto* data = builder->CreateCall(dataFn, {text.value}, "str_data");
			auto* len = builder->CreateCall(lenFn, {text.value}, "str_len");

			// Allocate new buffer and copy
			auto* newBuf = builder->CreateCall(mallocFn, {len}, "new_buf");
			auto* memcpyFn = module->getFunction("memcpy");
			builder->CreateCall(memcpyFn, {newBuf, data, len});

			if (text.needsRelease) builder->CreateCall(qdStringReleaseFn, {text.value});

			valueStack.push_back(TrackedValue(newBuf, ValueType::PTR));
			valueStack.push_back(TrackedValue(len, ValueType::INT));
			return true;
		}

		return false;
	}

	bool RegisterGenerator::generateNativeTermFunction(const std::string& name) {
		// Terminal color constants
		// term::Reset, term::Red, term::Green, term::Yellow, etc.
		// These are ANSI escape sequences as strings

		static const std::map<std::string, const char*> colorCodes = {
				{"Reset", "\033[0m"},	  {"Red", "\033[31m"},	   {"Green", "\033[32m"},
				{"Yellow", "\033[33m"},	  {"Blue", "\033[34m"},	   {"Magenta", "\033[35m"},
				{"Cyan", "\033[36m"},	  {"White", "\033[37m"},   {"Black", "\033[30m"},
				{"BgRed", "\033[41m"},	  {"BgGreen", "\033[42m"}, {"BgYellow", "\033[43m"},
				{"BgBlue", "\033[44m"},	  {"Bold", "\033[1m"},	   {"Dim", "\033[2m"},
				{"Italic", "\033[3m"},	  {"Underline", "\033[4m"},
		};

		auto it = colorCodes.find(name);
		if (it != colorCodes.end()) {
			// Create a global string constant for the escape sequence
			auto* strConst = builder->CreateGlobalString(it->second, "term_" + name);

			// Create a qd_string from it
			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {strConst}, "term_str");

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		return false;
	}

	bool RegisterGenerator::generateNativeOsFunction(const std::string& name) {
		auto ptrTy = llvm::PointerType::getUnqual(*context);

		// os::cwd ( -- path:str )
		if (name == "cwd") {
			// getcwd(NULL, 0) - returns malloc'd buffer
			auto* getcwdFn = module->getFunction("getcwd");
			if (!getcwdFn) {
				auto getcwdFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, builder->getInt64Ty()}, false);
				getcwdFn = llvm::Function::Create(getcwdFnTy, llvm::Function::ExternalLinkage, "getcwd", *module);
			}
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto* cwdPath = builder->CreateCall(getcwdFn, {nullPtr, builder->getInt64(0)}, "cwd_path");

			// Convert to qd_string
			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {cwdPath}, "cwd_str");

			// Free the getcwd buffer
			builder->CreateCall(freeFn, {cwdPath});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// os::getenv ( name:str -- value:str )
		if (name == "getenv") {
			if (valueStack.empty()) return true;
			auto nameVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* nameData = builder->CreateCall(dataFn, {nameVal.value}, "env_name");

			auto* getenvFn = module->getFunction("getenv");
			if (!getenvFn) {
				auto getenvFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
				getenvFn = llvm::Function::Create(getenvFnTy, llvm::Function::ExternalLinkage, "getenv", *module);
			}
			auto* envVal = builder->CreateCall(getenvFn, {nameData}, "env_val");

			// Check for NULL and create appropriate string
			auto* isNull = builder->CreateICmpEQ(envVal, llvm::ConstantPointerNull::get(ptrTy), "is_null");
			auto* emptyStr = builder->CreateGlobalString("", "empty_str");
			auto* valuePtr = builder->CreateSelect(isNull, emptyStr, envVal, "env_ptr");

			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {valuePtr}, "getenv_str");

			if (nameVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {nameVal.value});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// os::system ( cmd:str -- exitcode:int )
		if (name == "system") {
			if (valueStack.empty()) return true;
			auto cmdVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* cmdData = builder->CreateCall(dataFn, {cmdVal.value}, "cmd_data");

			auto* systemFn = module->getFunction("system");
			if (!systemFn) {
				auto systemFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				systemFn = llvm::Function::Create(systemFnTy, llvm::Function::ExternalLinkage, "system", *module);
			}
			auto* exitCode = builder->CreateCall(systemFn, {cmdData}, "sys_exit");
			auto* extCode = builder->CreateSExt(exitCode, builder->getInt64Ty(), "exit_code");

			if (cmdVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {cmdVal.value});

			valueStack.push_back(TrackedValue(extCode, ValueType::INT));
			return true;
		}

		// os::exit ( code:int -- )
		if (name == "exit") {
			if (valueStack.empty()) return true;
			auto codeVal = valueStack.back();
			valueStack.pop_back();

			auto* exitFn = module->getFunction("exit");
			if (!exitFn) {
				auto exitFnTy = llvm::FunctionType::get(builder->getVoidTy(), {builder->getInt32Ty()}, false);
				exitFn = llvm::Function::Create(exitFnTy, llvm::Function::ExternalLinkage, "exit", *module);
			}
			auto* code32 = builder->CreateTrunc(codeVal.value, builder->getInt32Ty(), "code32");
			builder->CreateCall(exitFn, {code32});
			return true;
		}

		// os::getpid ( -- pid:int )
		if (name == "getpid") {
			auto* getpidFn = module->getFunction("getpid");
			if (!getpidFn) {
				auto getpidFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {}, false);
				getpidFn = llvm::Function::Create(getpidFnTy, llvm::Function::ExternalLinkage, "getpid", *module);
			}
			auto* pid32 = builder->CreateCall(getpidFn, {}, "pid32");
			auto* pid64 = builder->CreateSExt(pid32, builder->getInt64Ty(), "pid64");

			valueStack.push_back(TrackedValue(pid64, ValueType::INT));
			return true;
		}

		// os::exists ( path:str -- exists:int )
		if (name == "exists") {
			if (valueStack.empty()) return true;
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* pathData = builder->CreateCall(dataFn, {pathVal.value}, "path_data");

			// access(path, F_OK) returns 0 if exists
			auto* accessFn = module->getFunction("access");
			if (!accessFn) {
				auto accessFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy, builder->getInt32Ty()}, false);
				accessFn = llvm::Function::Create(accessFnTy, llvm::Function::ExternalLinkage, "access", *module);
			}
			auto* result = builder->CreateCall(accessFn, {pathData, builder->getInt32(0)}, "access_result");
			auto* exists = builder->CreateICmpEQ(result, builder->getInt32(0), "exists");
			auto* existsInt = builder->CreateZExt(exists, builder->getInt64Ty(), "exists_int");

			if (pathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {pathVal.value});

			valueStack.push_back(TrackedValue(existsInt, ValueType::INT));
			return true;
		}

		// os::is_dir ( path:str -- is_dir:int )
		if (name == "is_dir") {
			if (valueStack.empty()) return true;
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* pathData = builder->CreateCall(dataFn, {pathVal.value}, "path_data");

			// Use opendir - returns DIR* if directory, NULL otherwise
			auto* opendirFn = module->getFunction("opendir");
			if (!opendirFn) {
				auto opendirFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
				opendirFn = llvm::Function::Create(opendirFnTy, llvm::Function::ExternalLinkage, "opendir", *module);
			}
			auto* dirPtr = builder->CreateCall(opendirFn, {pathData}, "dir_ptr");

			// Check if not NULL
			auto* isDir = builder->CreateICmpNE(dirPtr, llvm::ConstantPointerNull::get(ptrTy), "is_dir");

			// Close directory if it was opened
			auto* closedirFn = module->getFunction("closedir");
			if (!closedirFn) {
				auto closedirFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				closedirFn = llvm::Function::Create(closedirFnTy, llvm::Function::ExternalLinkage, "closedir", *module);
			}

			// Only close if dirPtr is not null
			auto* curFunc = builder->GetInsertBlock()->getParent();
			auto* closeBlock = llvm::BasicBlock::Create(*context, "close_dir", curFunc);
			auto* afterBlock = llvm::BasicBlock::Create(*context, "after_close", curFunc);
			builder->CreateCondBr(isDir, closeBlock, afterBlock);

			builder->SetInsertPoint(closeBlock);
			builder->CreateCall(closedirFn, {dirPtr});
			builder->CreateBr(afterBlock);

			builder->SetInsertPoint(afterBlock);
			auto* isDirInt = builder->CreateZExt(isDir, builder->getInt64Ty(), "is_dir_int");

			if (pathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {pathVal.value});

			valueStack.push_back(TrackedValue(isDirInt, ValueType::INT));
			return true;
		}

		// os::is_file ( path:str -- is_file:int )
		if (name == "is_file") {
			if (valueStack.empty()) return true;
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* pathData = builder->CreateCall(dataFn, {pathVal.value}, "path_data");

			// Check if it's NOT a directory and DOES exist
			// Use opendir to check if directory
			auto* opendirFn = module->getFunction("opendir");
			if (!opendirFn) {
				auto opendirFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
				opendirFn = llvm::Function::Create(opendirFnTy, llvm::Function::ExternalLinkage, "opendir", *module);
			}
			auto* dirPtr = builder->CreateCall(opendirFn, {pathData}, "dir_ptr");
			auto* isDir = builder->CreateICmpNE(dirPtr, llvm::ConstantPointerNull::get(ptrTy), "is_dir");

			// Close if directory
			auto* closedirFn = module->getFunction("closedir");
			if (!closedirFn) {
				auto closedirFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				closedirFn = llvm::Function::Create(closedirFnTy, llvm::Function::ExternalLinkage, "closedir", *module);
			}

			auto* curFunc = builder->GetInsertBlock()->getParent();
			auto* closeBlock = llvm::BasicBlock::Create(*context, "close_dir_file", curFunc);
			auto* checkExistsBlock = llvm::BasicBlock::Create(*context, "check_exists", curFunc);
			builder->CreateCondBr(isDir, closeBlock, checkExistsBlock);

			builder->SetInsertPoint(closeBlock);
			builder->CreateCall(closedirFn, {dirPtr});
			builder->CreateBr(checkExistsBlock);

			builder->SetInsertPoint(checkExistsBlock);

			// Use access() to check existence
			auto* accessFn = module->getFunction("access");
			if (!accessFn) {
				auto accessFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy, builder->getInt32Ty()}, false);
				accessFn = llvm::Function::Create(accessFnTy, llvm::Function::ExternalLinkage, "access", *module);
			}
			auto* accessResult = builder->CreateCall(accessFn, {pathData, builder->getInt32(0)}, "access_result");
			auto* exists = builder->CreateICmpEQ(accessResult, builder->getInt32(0), "exists");

			// is_file = exists AND NOT is_dir
			auto* notDir = builder->CreateNot(isDir, "not_dir");
			auto* isFile = builder->CreateAnd(exists, notDir, "is_file");
			auto* isFileInt = builder->CreateZExt(isFile, builder->getInt64Ty(), "is_file_int");

			if (pathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {pathVal.value});

			valueStack.push_back(TrackedValue(isFileInt, ValueType::INT));
			return true;
		}

		// os::setenv ( name:str value:str -- )
		if (name == "setenv") {
			if (valueStack.size() < 2) return true;
			auto valueVal = valueStack.back();
			valueStack.pop_back();
			auto nameVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* nameData = builder->CreateCall(dataFn, {nameVal.value}, "env_name");
			auto* valueData = builder->CreateCall(dataFn, {valueVal.value}, "env_value");

			auto* setenvFn = module->getFunction("setenv");
			if (!setenvFn) {
				auto setenvFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
					{ptrTy, ptrTy, builder->getInt32Ty()}, false);
				setenvFn = llvm::Function::Create(setenvFnTy, llvm::Function::ExternalLinkage, "setenv", *module);
			}
			// setenv(name, value, 1) - 1 means overwrite existing
			builder->CreateCall(setenvFn, {nameData, valueData, builder->getInt32(1)});

			if (nameVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {nameVal.value});
			if (valueVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {valueVal.value});

			return true;
		}

		// os::delete ( path:str -- )!
		if (name == "delete") {
			if (valueStack.empty()) return true;
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* pathData = builder->CreateCall(dataFn, {pathVal.value}, "path_data");

			auto* removeFn = module->getFunction("remove");
			if (!removeFn) {
				auto removeFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				removeFn = llvm::Function::Create(removeFnTy, llvm::Function::ExternalLinkage, "remove", *module);
			}
			builder->CreateCall(removeFn, {pathData});

			if (pathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {pathVal.value});

			return true;
		}

		// os::rename ( oldpath:str newpath:str -- )!
		if (name == "rename") {
			if (valueStack.size() < 2) return true;
			auto newPathVal = valueStack.back();
			valueStack.pop_back();
			auto oldPathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* oldPathData = builder->CreateCall(dataFn, {oldPathVal.value}, "old_path");
			auto* newPathData = builder->CreateCall(dataFn, {newPathVal.value}, "new_path");

			auto* renameFn = module->getFunction("rename");
			if (!renameFn) {
				auto renameFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy, ptrTy}, false);
				renameFn = llvm::Function::Create(renameFnTy, llvm::Function::ExternalLinkage, "rename", *module);
			}
			builder->CreateCall(renameFn, {oldPathData, newPathData});

			if (oldPathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {oldPathVal.value});
			if (newPathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {newPathVal.value});

			return true;
		}

		// os::mkdir ( path:str -- )! - creates directory with parents
		if (name == "mkdir") {
			if (valueStack.empty()) return true;
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* pathData = builder->CreateCall(dataFn, {pathVal.value}, "path_data");

			// Use system("mkdir -p <path>") for simplicity
			// Allocate buffer for command
			auto* bufSize = builder->getInt64(4096);
			if (!mallocFn) {
				auto mallocFnTy = llvm::FunctionType::get(ptrTy, {builder->getInt64Ty()}, false);
				mallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
			}
			auto* cmdBuf = builder->CreateCall(mallocFn, {bufSize}, "cmd_buf");

			// snprintf(cmdBuf, 4096, "mkdir -p \"%s\"", path)
			auto* snprintfFn = module->getFunction("snprintf");
			if (!snprintfFn) {
				auto snprintfFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
					{ptrTy, builder->getInt64Ty(), ptrTy}, true);
				snprintfFn = llvm::Function::Create(snprintfFnTy, llvm::Function::ExternalLinkage, "snprintf", *module);
			}
			auto* fmtStr = builder->CreateGlobalString("mkdir -p \"%s\"", "mkdir_fmt");
			builder->CreateCall(snprintfFn, {cmdBuf, bufSize, fmtStr, pathData});

			// Call system()
			auto* systemFn = module->getFunction("system");
			if (!systemFn) {
				auto systemFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				systemFn = llvm::Function::Create(systemFnTy, llvm::Function::ExternalLinkage, "system", *module);
			}
			builder->CreateCall(systemFn, {cmdBuf});

			// Free command buffer
			if (!freeFn) {
				auto freeFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(freeFn, {cmdBuf});

			if (pathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {pathVal.value});

			return true;
		}

		// os::rmdir ( path:str -- )! - removes directory recursively
		if (name == "rmdir") {
			if (valueStack.empty()) return true;
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* pathData = builder->CreateCall(dataFn, {pathVal.value}, "path_data");

			// Use system("rm -rf <path>") for simplicity
			auto* bufSize = builder->getInt64(4096);
			if (!mallocFn) {
				auto mallocFnTy = llvm::FunctionType::get(ptrTy, {builder->getInt64Ty()}, false);
				mallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
			}
			auto* cmdBuf = builder->CreateCall(mallocFn, {bufSize}, "cmd_buf");

			auto* snprintfFn = module->getFunction("snprintf");
			if (!snprintfFn) {
				auto snprintfFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
					{ptrTy, builder->getInt64Ty(), ptrTy}, true);
				snprintfFn = llvm::Function::Create(snprintfFnTy, llvm::Function::ExternalLinkage, "snprintf", *module);
			}
			auto* fmtStr = builder->CreateGlobalString("rm -rf \"%s\"", "rmdir_fmt");
			builder->CreateCall(snprintfFn, {cmdBuf, bufSize, fmtStr, pathData});

			auto* systemFn = module->getFunction("system");
			if (!systemFn) {
				auto systemFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				systemFn = llvm::Function::Create(systemFnTy, llvm::Function::ExternalLinkage, "system", *module);
			}
			builder->CreateCall(systemFn, {cmdBuf});

			if (!freeFn) {
				auto freeFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(freeFn, {cmdBuf});

			if (pathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {pathVal.value});

			return true;
		}

		// os::mktemp ( -- path:str )! - creates temporary directory
		if (name == "mktemp") {
			// Use mkdtemp() with template
			auto* templateStr = builder->CreateGlobalString("/tmp/qd_XXXXXX", "mktemp_template");

			// Need to copy template since mkdtemp modifies it
			auto* strdupFunc = module->getFunction("strdup");
			if (!strdupFunc) {
				auto strdupFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
				strdupFunc = llvm::Function::Create(strdupFnTy, llvm::Function::ExternalLinkage, "strdup", *module);
			}
			auto* tempCopy = builder->CreateCall(strdupFunc, {templateStr}, "temp_copy");

			auto* mkdtempFn = module->getFunction("mkdtemp");
			if (!mkdtempFn) {
				auto mkdtempFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
				mkdtempFn = llvm::Function::Create(mkdtempFnTy, llvm::Function::ExternalLinkage, "mkdtemp", *module);
			}
			auto* resultPath = builder->CreateCall(mkdtempFn, {tempCopy}, "mktemp_result");

			// Convert to qd_string
			auto* createFn = module->getFunction("qd_string_create");
			auto* result = builder->CreateCall(createFn, {resultPath}, "mktemp_str");

			// Free the strdup'd buffer
			if (!freeFn) {
				auto freeFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(freeFn, {tempCopy});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// os::copy ( srcpath:str dstpath:str -- )!
		if (name == "copy") {
			if (valueStack.size() < 2) return true;
			auto dstPathVal = valueStack.back();
			valueStack.pop_back();
			auto srcPathVal = valueStack.back();
			valueStack.pop_back();

			auto* dataFn = module->getFunction("qd_string_data");
			auto* srcPathData = builder->CreateCall(dataFn, {srcPathVal.value}, "src_path");
			auto* dstPathData = builder->CreateCall(dataFn, {dstPathVal.value}, "dst_path");

			// Use system("cp <src> <dst>")
			auto* bufSize = builder->getInt64(8192);
			if (!mallocFn) {
				auto mallocFnTy = llvm::FunctionType::get(ptrTy, {builder->getInt64Ty()}, false);
				mallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
			}
			auto* cmdBuf = builder->CreateCall(mallocFn, {bufSize}, "cmd_buf");

			auto* snprintfFn = module->getFunction("snprintf");
			if (!snprintfFn) {
				auto snprintfFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
					{ptrTy, builder->getInt64Ty(), ptrTy}, true);
				snprintfFn = llvm::Function::Create(snprintfFnTy, llvm::Function::ExternalLinkage, "snprintf", *module);
			}
			auto* fmtStr = builder->CreateGlobalString("cp \"%s\" \"%s\"", "copy_fmt");
			builder->CreateCall(snprintfFn, {cmdBuf, bufSize, fmtStr, srcPathData, dstPathData});

			auto* systemFn = module->getFunction("system");
			if (!systemFn) {
				auto systemFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
				systemFn = llvm::Function::Create(systemFnTy, llvm::Function::ExternalLinkage, "system", *module);
			}
			builder->CreateCall(systemFn, {cmdBuf});

			if (!freeFn) {
				auto freeFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				freeFn = llvm::Function::Create(freeFnTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(freeFn, {cmdBuf});

			if (srcPathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {srcPathVal.value});
			if (dstPathVal.needsRelease) builder->CreateCall(qdStringReleaseFn, {dstPathVal.value});

			return true;
		}

		return false;
	}

	bool RegisterGenerator::generateNativeIoFunction(const std::string& name) {
		auto* ptrTy = llvm::PointerType::getUnqual(*context);

		// Get or create common functions
		auto* fopenFn = module->getFunction("fopen");
		if (!fopenFn) {
			auto fopenFnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false);
			fopenFn = llvm::Function::Create(fopenFnTy, llvm::Function::ExternalLinkage, "fopen", *module);
		}

		auto* fcloseFn = module->getFunction("fclose");
		if (!fcloseFn) {
			auto fcloseFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
			fcloseFn = llvm::Function::Create(fcloseFnTy, llvm::Function::ExternalLinkage, "fclose", *module);
		}

		auto* freadFn = module->getFunction("fread");
		if (!freadFn) {
			auto freadFnTy = llvm::FunctionType::get(builder->getInt64Ty(),
				{ptrTy, builder->getInt64Ty(), builder->getInt64Ty(), ptrTy}, false);
			freadFn = llvm::Function::Create(freadFnTy, llvm::Function::ExternalLinkage, "fread", *module);
		}

		auto* fwriteFn = module->getFunction("fwrite");
		if (!fwriteFn) {
			auto fwriteFnTy = llvm::FunctionType::get(builder->getInt64Ty(),
				{ptrTy, builder->getInt64Ty(), builder->getInt64Ty(), ptrTy}, false);
			fwriteFn = llvm::Function::Create(fwriteFnTy, llvm::Function::ExternalLinkage, "fwrite", *module);
		}

		auto* fseekFn = module->getFunction("fseek");
		if (!fseekFn) {
			auto fseekFnTy = llvm::FunctionType::get(builder->getInt32Ty(),
				{ptrTy, builder->getInt64Ty(), builder->getInt32Ty()}, false);
			fseekFn = llvm::Function::Create(fseekFnTy, llvm::Function::ExternalLinkage, "fseek", *module);
		}

		auto* ftellFn = module->getFunction("ftell");
		if (!ftellFn) {
			auto ftellFnTy = llvm::FunctionType::get(builder->getInt64Ty(), {ptrTy}, false);
			ftellFn = llvm::Function::Create(ftellFnTy, llvm::Function::ExternalLinkage, "ftell", *module);
		}

		auto* feofFn = module->getFunction("feof");
		if (!feofFn) {
			auto feofFnTy = llvm::FunctionType::get(builder->getInt32Ty(), {ptrTy}, false);
			feofFn = llvm::Function::Create(feofFnTy, llvm::Function::ExternalLinkage, "feof", *module);
		}

		auto* qdStrReleaseFn = module->getFunction("qd_string_release");
		auto* qdStringDataFn = module->getFunction("qd_string_data");
		auto* qdStringCreateWithLenFn = module->getFunction("qd_string_create_with_length");

		// io::open ( path:str mode:str -- errcode:i64 handle:ptr )
		// Fallible: pushes Ok(1)+handle on success, error code + null on failure
		if (name == "open") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for io::open", 0);
				return true;
			}
			auto modeVal = valueStack.back();
			valueStack.pop_back();
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			// Get C strings from qd_string
			auto* pathData = builder->CreateCall(qdStringDataFn, {pathVal.value}, "path_data");
			auto* modeData = builder->CreateCall(qdStringDataFn, {modeVal.value}, "mode_data");

			// Call fopen
			auto* fileHandle = builder->CreateCall(fopenFn, {pathData, modeData}, "file_handle");

			// Release input strings if needed
			if (pathVal.needsRelease && qdStrReleaseFn)
				builder->CreateCall(qdStrReleaseFn, {pathVal.value});
			if (modeVal.needsRelease && qdStrReleaseFn)
				builder->CreateCall(qdStrReleaseFn, {modeVal.value});

			// Check if open succeeded (handle != NULL)
			auto* isNull = builder->CreateIsNull(fileHandle, "is_null");
			auto* errCode = builder->CreateSelect(isNull, builder->getInt64(2), builder->getInt64(1), "err_code");

			// Push handle first, then error code (error code is on top for switch)
			valueStack.push_back(TrackedValue(fileHandle, ValueType::PTR, "", false));
			valueStack.push_back(TrackedValue(errCode, ValueType::INT, "", false));
			return true;
		}

		// io::close ( handle:ptr -- )
		if (name == "close") {
			if (valueStack.empty()) {
				reportError("Stack underflow for io::close", 0);
				return true;
			}
			auto handleVal = valueStack.back();
			valueStack.pop_back();

			builder->CreateCall(fcloseFn, {handleVal.value});
			return true;
		}

		// io::read ( handle:ptr buffer:ptr count:i64 -- bytes_read:i64 )!
		if (name == "read") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for io::read", 0);
				return true;
			}
			auto countVal = valueStack.back();
			valueStack.pop_back();
			auto bufferVal = valueStack.back();
			valueStack.pop_back();
			auto handleVal = valueStack.back();
			valueStack.pop_back();

			// fread(buffer, 1, count, handle)
			auto* bytesRead = builder->CreateCall(freadFn,
				{bufferVal.value, builder->getInt64(1), countVal.value, handleVal.value}, "bytes_read");

			valueStack.push_back(TrackedValue(bytesRead, ValueType::INT, "", false));
			valueStack.push_back(TrackedValue(builder->getInt64(1), ValueType::INT, "", false)); // Ok
			return true;
		}

		// io::write ( handle:ptr buffer:ptr count:i64 -- bytes_written:i64 )!
		if (name == "write") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for io::write", 0);
				return true;
			}
			auto countVal = valueStack.back();
			valueStack.pop_back();
			auto bufferVal = valueStack.back();
			valueStack.pop_back();
			auto handleVal = valueStack.back();
			valueStack.pop_back();

			// fwrite(buffer, 1, count, handle)
			auto* bytesWritten = builder->CreateCall(fwriteFn,
				{bufferVal.value, builder->getInt64(1), countVal.value, handleVal.value}, "bytes_written");

			valueStack.push_back(TrackedValue(bytesWritten, ValueType::INT, "", false));
			valueStack.push_back(TrackedValue(builder->getInt64(1), ValueType::INT, "", false)); // Ok
			return true;
		}

		// io::seek ( handle:ptr offset:i64 whence:i64 -- position:i64 )!
		if (name == "seek") {
			if (valueStack.size() < 3) {
				reportError("Stack underflow for io::seek", 0);
				return true;
			}
			auto whenceVal = valueStack.back();
			valueStack.pop_back();
			auto offsetVal = valueStack.back();
			valueStack.pop_back();
			auto handleVal = valueStack.back();
			valueStack.pop_back();

			auto* whence32 = builder->CreateTrunc(whenceVal.value, builder->getInt32Ty(), "whence32");
			builder->CreateCall(fseekFn, {handleVal.value, offsetVal.value, whence32});

			// Return current position
			auto* pos = builder->CreateCall(ftellFn, {handleVal.value}, "position");
			valueStack.push_back(TrackedValue(pos, ValueType::INT, "", false));
			valueStack.push_back(TrackedValue(builder->getInt64(1), ValueType::INT, "", false)); // Ok
			return true;
		}

		// io::tell ( handle:ptr -- position:i64 )!
		if (name == "tell") {
			if (valueStack.empty()) {
				reportError("Stack underflow for io::tell", 0);
				return true;
			}
			auto handleVal = valueStack.back();
			valueStack.pop_back();

			auto* pos = builder->CreateCall(ftellFn, {handleVal.value}, "position");
			valueStack.push_back(TrackedValue(pos, ValueType::INT, "", false));
			valueStack.push_back(TrackedValue(builder->getInt64(1), ValueType::INT, "", false)); // Ok
			return true;
		}

		// io::eof ( handle:ptr -- handle:ptr is_eof:i64 )
		if (name == "eof") {
			if (valueStack.empty()) {
				reportError("Stack underflow for io::eof", 0);
				return true;
			}
			auto handleVal = valueStack.back();
			// Keep handle on stack (don't pop)

			auto* eofResult = builder->CreateCall(feofFn, {handleVal.value}, "eof_result");
			auto* isEof = builder->CreateICmpNE(eofResult, builder->getInt32(0), "is_eof");
			auto* isEofInt = builder->CreateZExt(isEof, builder->getInt64Ty(), "is_eof_int");

			valueStack.push_back(TrackedValue(isEofInt, ValueType::INT, "", false));
			return true;
		}

		// io::read_file ( path:str -- contents:str )!
		if (name == "read_file") {
			if (valueStack.empty()) {
				reportError("Stack underflow for io::read_file", 0);
				return true;
			}
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			// Get path as C string
			auto* pathData = builder->CreateCall(qdStringDataFn, {pathVal.value}, "path_data");

			// Open file in read binary mode
			auto* rbMode = builder->CreateGlobalString("rb", "rb_mode");
			auto* fileHandle = builder->CreateCall(fopenFn, {pathData, rbMode}, "file_handle");

			// Release path if needed
			if (pathVal.needsRelease && qdStrReleaseFn)
				builder->CreateCall(qdStrReleaseFn, {pathVal.value});

			// Seek to end to get file size
			builder->CreateCall(fseekFn, {fileHandle, builder->getInt64(0), builder->getInt32(2)});  // SEEK_END
			auto* fileSize = builder->CreateCall(ftellFn, {fileHandle}, "file_size");
			builder->CreateCall(fseekFn, {fileHandle, builder->getInt64(0), builder->getInt32(0)});  // SEEK_SET

			// Allocate buffer
			auto* mallocFunc = module->getFunction("malloc");
			if (!mallocFunc) {
				auto mallocFuncTy = llvm::FunctionType::get(ptrTy, {builder->getInt64Ty()}, false);
				mallocFunc = llvm::Function::Create(mallocFuncTy, llvm::Function::ExternalLinkage, "malloc", *module);
			}
			auto* bufSize = builder->CreateAdd(fileSize, builder->getInt64(1), "buf_size");
			auto* buffer = builder->CreateCall(mallocFunc, {bufSize}, "buffer");

			// Read file contents
			builder->CreateCall(freadFn, {buffer, builder->getInt64(1), fileSize, fileHandle}, "read_result");

			// Null terminate
			auto* endPtr = builder->CreateGEP(builder->getInt8Ty(), buffer, fileSize, "end_ptr");
			builder->CreateStore(builder->getInt8(0), endPtr);

			// Close file
			builder->CreateCall(fcloseFn, {fileHandle});

			// Create qd_string from buffer
			auto* result = builder->CreateCall(qdStringCreateWithLenFn, {buffer, fileSize}, "contents");

			// Free buffer
			auto* freeFunc = module->getFunction("free");
			if (!freeFunc) {
				auto freeFuncTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);
				freeFunc = llvm::Function::Create(freeFuncTy, llvm::Function::ExternalLinkage, "free", *module);
			}
			builder->CreateCall(freeFunc, {buffer});

			valueStack.push_back(TrackedValue(result, ValueType::STRING, "", true));
			return true;
		}

		// io::write_file ( path:str contents:str -- )!
		if (name == "write_file") {
			if (valueStack.size() < 2) {
				reportError("Stack underflow for io::write_file", 0);
				return true;
			}
			auto contentsVal = valueStack.back();
			valueStack.pop_back();
			auto pathVal = valueStack.back();
			valueStack.pop_back();

			// Get C strings
			auto* pathData = builder->CreateCall(qdStringDataFn, {pathVal.value}, "path_data");
			auto* contentsData = builder->CreateCall(qdStringDataFn, {contentsVal.value}, "contents_data");

			// Get contents length
			auto* qdStringLenFn = module->getFunction("qd_string_length");
			auto* contentsLen = builder->CreateCall(qdStringLenFn, {contentsVal.value}, "contents_len");

			// Open file for writing
			auto* wMode = builder->CreateGlobalString("w", "w_mode");
			auto* fileHandle = builder->CreateCall(fopenFn, {pathData, wMode}, "file_handle");

			// Write contents
			builder->CreateCall(fwriteFn, {contentsData, builder->getInt64(1), contentsLen, fileHandle});

			// Close file
			builder->CreateCall(fcloseFn, {fileHandle});

			// Release strings if needed
			if (pathVal.needsRelease && qdStrReleaseFn)
				builder->CreateCall(qdStrReleaseFn, {pathVal.value});
			if (contentsVal.needsRelease && qdStrReleaseFn)
				builder->CreateCall(qdStrReleaseFn, {contentsVal.value});

			return true;
		}

		return false;
	}

	void RegisterGenerator::generateFunctionPointer(AstNodeFunctionPointerReference* funcPtr) {
		const std::string& name = funcPtr->functionName();
		std::string funcName = mangleName(name, currentModulePrefix);

		auto funcIt = userFunctions.find(funcName);
		if (funcIt != userFunctions.end()) {
			valueStack.push_back(TrackedValue(funcIt->second, ValueType::PTR, "", false));
		} else {
			reportError("Unknown function: " + name, funcPtr->line());
		}
	}

	void RegisterGenerator::generateAnonymousFunction(AstNodeAnonymousFunction* anonFunc) {
		// Create a new function for the anonymous function
		std::string anonName = uniqueName("anon_func");

		// Get captured variables
		const auto& captures = anonFunc->capturedVariables();
		bool hasClosure = !captures.empty();

		// Determine function type from parameters and returns
		std::vector<llvm::Type*> paramTypes;

		// If we have captures, add environment pointer as first hidden parameter
		if (hasClosure) {
			paramTypes.push_back(llvm::PointerType::getUnqual(*context));
		}

		for (size_t i = 0; i < anonFunc->inputParameters().size(); i++) {
			auto* param = static_cast<AstNodeParameter*>(anonFunc->inputParameters()[i]);
			paramTypes.push_back(getLlvmType(typeFromString(param->typeString())));
		}

		llvm::Type* returnType = builder->getVoidTy();
		if (anonFunc->outputParameters().size() == 1) {
			returnType = getLlvmType(
					typeFromString(static_cast<AstNodeParameter*>(anonFunc->outputParameters()[0])->typeString()));
		} else if (anonFunc->outputParameters().size() > 1) {
			// Multiple returns - use a struct type
			std::vector<llvm::Type*> returnTypes;
			for (size_t j = 0; j < anonFunc->outputParameters().size(); j++) {
				auto* outParam = static_cast<AstNodeParameter*>(anonFunc->outputParameters()[j]);
				returnTypes.push_back(getLlvmType(typeFromString(outParam->typeString())));
			}
			returnType = llvm::StructType::get(*context, returnTypes);
		}

		auto* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
		auto* func = llvm::Function::Create(funcType, llvm::Function::InternalLinkage, anonName, *module);

		// Save current state
		auto* savedFunction = currentFunction;
		auto* savedInsertBlock = builder->GetInsertBlock();
		auto savedLocals = localVariables;
		auto savedValueStack = valueStack;
		auto savedCapturedRefs = capturedVariableRefs;
		auto savedCapturedTypes = capturedVariableTypes;

		// Generate function body
		currentFunction = func;
		localVariables.clear();
		valueStack.clear();
		capturedVariableRefs.clear();
		capturedVariableTypes.clear();

		auto* entry = llvm::BasicBlock::Create(*context, "entry", func);
		builder->SetInsertPoint(entry);

		// For closures, load captured variable pointers from environment
		size_t argOffset = 0;
		if (hasClosure) {
			llvm::Value* envPtr = func->getArg(0);
			envPtr->setName("env");
			argOffset = 1;

			// Load each captured variable from environment
			for (size_t i = 0; i < captures.size(); i++) {
				const std::string& capName = captures[i];

				// Create local alloca to hold the pointer to outer variable
				llvm::AllocaInst* ptrAlloca =
						builder->CreateAlloca(llvm::PointerType::getUnqual(*context), nullptr, capName + "_ref");

				// Load pointer from environment array
				llvm::Value* envSlot = builder->CreateGEP(llvm::PointerType::getUnqual(*context), envPtr,
						builder->getInt64(i), capName + "_env_slot");
				llvm::Value* outerPtr =
						builder->CreateLoad(llvm::PointerType::getUnqual(*context), envSlot, capName + "_outer_ptr");

				// Store the pointer in our local alloca
				builder->CreateStore(outerPtr, ptrAlloca);

				// Register as captured variable reference (needs extra indirection when accessed)
				capturedVariableRefs[capName] = ptrAlloca;

				// Look up the type from the outer scope
				auto typeIt = savedLocals.find(capName);
				if (typeIt != savedLocals.end()) {
					capturedVariableTypes[capName] = typeIt->second.type;
				} else {
					capturedVariableTypes[capName] = ValueType::INT;  // Default
				}
			}
		}

		// Set up parameters - push them onto value stack
		// In Quadrate, parameters are on the stack and need explicit `-> name` to bind to local variables
		for (size_t i = 0; i < anonFunc->inputParameters().size(); i++) {
			auto* param = static_cast<AstNodeParameter*>(anonFunc->inputParameters()[i]);
			std::string paramName = param->name();
			ValueType vt = typeFromString(param->typeString());

			llvm::Argument* arg = func->getArg(static_cast<unsigned>(argOffset + i));
			arg->setName(paramName);
			valueStack.push_back(TrackedValue(arg, vt, "", false));
		}

		pushDeferScope();
		if (anonFunc->body()) {
			generateNode(anonFunc->body());
		}
		executeDeferScope();
		popDeferScope();

		// Handle return
		if (!builder->GetInsertBlock()->getTerminator()) {
			if (anonFunc->outputParameters().size() == 0) {
				builder->CreateRetVoid();
			} else if (anonFunc->outputParameters().size() == 1) {
				if (!valueStack.empty()) {
					builder->CreateRet(valueStack.back().value);
					valueStack.pop_back();
				} else {
					builder->CreateRet(llvm::Constant::getNullValue(returnType));
				}
			} else {
				// Multiple return values - pack into struct
				std::vector<llvm::Value*> returnValues;
				for (size_t i = 0; i < anonFunc->outputParameters().size() && !valueStack.empty(); i++) {
					returnValues.push_back(valueStack.back().value);
					valueStack.pop_back();
				}
				// Reverse to get correct order
				std::reverse(returnValues.begin(), returnValues.end());

				auto* retType = llvm::cast<llvm::StructType>(returnType);
				llvm::Value* retVal = llvm::UndefValue::get(retType);
				for (size_t i = 0; i < returnValues.size(); i++) {
					retVal = builder->CreateInsertValue(retVal, returnValues[i], static_cast<unsigned>(i));
				}
				builder->CreateRet(retVal);
			}
		}

		// Restore state
		currentFunction = savedFunction;
		localVariables = savedLocals;
		valueStack = savedValueStack;
		capturedVariableRefs = savedCapturedRefs;
		capturedVariableTypes = savedCapturedTypes;
		builder->SetInsertPoint(savedInsertBlock);

		if (hasClosure) {
			// Allocate closure struct: { magic, fn_ptr, env_ptr, capture_count }
			auto* closureStructTy = llvm::StructType::get(*context,
					{builder->getInt64Ty(), llvm::PointerType::getUnqual(*context),
							llvm::PointerType::getUnqual(*context), builder->getInt64Ty()});

			// Allocate environment array (array of pointers for capture-by-reference)
			size_t envSize = captures.size() * 8;  // sizeof(pointer) = 8 bytes on 64-bit

			// Get or create malloc function
			if (!mallocFn) {
				auto mallocFnTy = llvm::FunctionType::get(
						llvm::PointerType::getUnqual(*context), {builder->getInt64Ty()}, false);
				mallocFn = llvm::Function::Create(mallocFnTy, llvm::Function::ExternalLinkage, "malloc", *module);
			}
			llvm::Value* envAlloc = builder->CreateCall(mallocFn, {builder->getInt64(envSize)}, "env_alloc");

			// Store pointers to captured variables in environment (capture-by-reference)
			for (size_t i = 0; i < captures.size(); i++) {
				const std::string& capName = captures[i];

				// Get the captured variable's alloca from the outer scope
				auto it = savedLocals.find(capName);
				if (it != savedLocals.end()) {
					llvm::AllocaInst* outerAlloca = it->second.alloca;

					// Store pointer to captured variable into environment array
					llvm::Value* envSlot = builder->CreateGEP(llvm::PointerType::getUnqual(*context), envAlloc,
							builder->getInt64(i), capName + "_slot");
					builder->CreateStore(outerAlloca, envSlot);
				}
			}

			// Allocate closure struct (magic + 2 pointers + capture_count = 32 bytes)
			llvm::Value* closureAlloc = builder->CreateCall(mallocFn, {builder->getInt64(32)}, "closure_alloc");

			// Store magic marker (0xCL05UR3E = 0xC105023E in hex)
			llvm::Value* magicSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 0, "magic_slot");
			builder->CreateStore(builder->getInt64(0xC105023E), magicSlot);

			// Store function pointer
			llvm::Value* fnPtrSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 1, "fn_ptr_slot");
			llvm::Value* fnPtrCast = builder->CreateBitCast(func, llvm::PointerType::getUnqual(*context), "fn_ptr_cast");
			builder->CreateStore(fnPtrCast, fnPtrSlot);

			// Store environment pointer
			llvm::Value* envPtrSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 2, "env_ptr_slot");
			builder->CreateStore(envAlloc, envPtrSlot);

			// Store capture count for cleanup
			llvm::Value* capCountSlot = builder->CreateStructGEP(closureStructTy, closureAlloc, 3, "cap_count_slot");
			builder->CreateStore(builder->getInt64(captures.size()), capCountSlot);

			// Register the closure in the closure registry for safe detection
			auto closureRegisterFnTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			auto closureRegisterFn = module->getOrInsertFunction("qd_closure_register", closureRegisterFnTy);
			builder->CreateCall(closureRegisterFn, {closureAlloc});

			// Push closure struct pointer to stack - mark as closure type and store func type
			valueStack.push_back(TrackedValue(closureAlloc, ValueType::PTR, "__closure__", false, funcType));

			// Save the closure's function type for calling (legacy - for backward compat)
			lastClosureFuncType = funcType;
		} else {
			// No captures - just push the function pointer with func type
			valueStack.push_back(TrackedValue(func, ValueType::PTR, "", false, funcType));
			lastClosureFuncType = funcType;
		}
	}

	// Arithmetic operations
	void RegisterGenerator::generateAdd() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			auto* result = builder->CreateAdd(a.value, b.value, "add");
			valueStack.push_back(TrackedValue(result, ValueType::INT));
		} else if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			auto* result = builder->CreateFAdd(aFloat, bFloat, "fadd");
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
		} else if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
			// String concatenation
			auto* concatFn = module->getFunction("qd_string_concat_smart");
			if (concatFn) {
				auto* result = builder->CreateCall(concatFn, {a.value, b.value});
				valueStack.push_back(TrackedValue(result, ValueType::STRING, true));
			}
		}
	}

	void RegisterGenerator::generateSub() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			auto* result = builder->CreateSub(a.value, b.value, "sub");
			valueStack.push_back(TrackedValue(result, ValueType::INT));
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			auto* result = builder->CreateFSub(aFloat, bFloat, "fsub");
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
		}
	}

	void RegisterGenerator::generateMul() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			auto* result = builder->CreateMul(a.value, b.value, "mul");
			valueStack.push_back(TrackedValue(result, ValueType::INT));
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			auto* result = builder->CreateFMul(aFloat, bFloat, "fmul");
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
		}
	}

	void RegisterGenerator::generateDiv() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			auto* result = builder->CreateSDiv(a.value, b.value, "div");
			valueStack.push_back(TrackedValue(result, ValueType::INT));
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			auto* result = builder->CreateFDiv(aFloat, bFloat, "fdiv");
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
		}
	}

	void RegisterGenerator::generateMod() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			auto* result = builder->CreateSRem(a.value, b.value, "mod");
			valueStack.push_back(TrackedValue(result, ValueType::INT));
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			auto* result = builder->CreateFRem(aFloat, bFloat, "fmod");
			valueStack.push_back(TrackedValue(result, ValueType::FLOAT));
		}
	}

	// Comparison operations
	void RegisterGenerator::generateLt() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		llvm::Value* result;
		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			result = builder->CreateICmpSLT(a.value, b.value, "lt");
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			result = builder->CreateFCmpOLT(aFloat, bFloat, "flt");
		}
		auto* extended = builder->CreateZExt(result, builder->getInt64Ty());
		valueStack.push_back(TrackedValue(extended, ValueType::BOOL));
	}

	void RegisterGenerator::generateGt() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		llvm::Value* result;
		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			result = builder->CreateICmpSGT(a.value, b.value, "gt");
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			result = builder->CreateFCmpOGT(aFloat, bFloat, "fgt");
		}
		auto* extended = builder->CreateZExt(result, builder->getInt64Ty());
		valueStack.push_back(TrackedValue(extended, ValueType::BOOL));
	}

	void RegisterGenerator::generateEq() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		llvm::Value* result;
		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			result = builder->CreateICmpEQ(a.value, b.value, "eq");
		} else if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			result = builder->CreateFCmpOEQ(aFloat, bFloat, "feq");
		} else if ((a.type == ValueType::PTR || a.type == ValueType::STRING) &&
				   (b.type == ValueType::PTR || b.type == ValueType::STRING)) {
			// Both are pointers
			result = builder->CreateICmpEQ(a.value, b.value, "ptreq");
		} else if (a.type == ValueType::PTR || a.type == ValueType::STRING) {
			// a is pointer, b is int - convert b to pointer for comparison
			auto* ptrTy = llvm::PointerType::get(*context, 0);
			auto* bPtr = builder->CreateIntToPtr(b.value, ptrTy, "int_to_ptr");
			result = builder->CreateICmpEQ(a.value, bPtr, "ptreq");
		} else if (b.type == ValueType::PTR || b.type == ValueType::STRING) {
			// b is pointer, a is int - convert a to pointer for comparison
			auto* ptrTy = llvm::PointerType::get(*context, 0);
			auto* aPtr = builder->CreateIntToPtr(a.value, ptrTy, "int_to_ptr");
			result = builder->CreateICmpEQ(aPtr, b.value, "ptreq");
		} else {
			// Default: treat as integers
			result = builder->CreateICmpEQ(a.value, b.value, "eq");
		}
		auto* extended = builder->CreateZExt(result, builder->getInt64Ty());
		valueStack.push_back(TrackedValue(extended, ValueType::BOOL));
	}

	void RegisterGenerator::generateNeq() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		llvm::Value* result;
		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			result = builder->CreateICmpNE(a.value, b.value, "neq");
		} else if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			result = builder->CreateFCmpONE(aFloat, bFloat, "fneq");
		} else if ((a.type == ValueType::PTR || a.type == ValueType::STRING) &&
				   (b.type == ValueType::PTR || b.type == ValueType::STRING)) {
			// Both are pointers
			result = builder->CreateICmpNE(a.value, b.value, "ptrneq");
		} else if (a.type == ValueType::PTR || a.type == ValueType::STRING) {
			// a is pointer, b is int - convert b to pointer for comparison
			auto* ptrTy = llvm::PointerType::get(*context, 0);
			auto* bPtr = builder->CreateIntToPtr(b.value, ptrTy, "int_to_ptr");
			result = builder->CreateICmpNE(a.value, bPtr, "ptrneq");
		} else if (b.type == ValueType::PTR || b.type == ValueType::STRING) {
			// b is pointer, a is int - convert a to pointer for comparison
			auto* ptrTy = llvm::PointerType::get(*context, 0);
			auto* aPtr = builder->CreateIntToPtr(a.value, ptrTy, "int_to_ptr");
			result = builder->CreateICmpNE(aPtr, b.value, "ptrneq");
		} else {
			// Default: treat as integers
			result = builder->CreateICmpNE(a.value, b.value, "neq");
		}
		auto* extended = builder->CreateZExt(result, builder->getInt64Ty());
		valueStack.push_back(TrackedValue(extended, ValueType::BOOL));
	}

	void RegisterGenerator::generateLte() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		llvm::Value* result;
		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			result = builder->CreateICmpSLE(a.value, b.value, "lte");
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			result = builder->CreateFCmpOLE(aFloat, bFloat, "flte");
		}
		auto* extended = builder->CreateZExt(result, builder->getInt64Ty());
		valueStack.push_back(TrackedValue(extended, ValueType::BOOL));
	}

	void RegisterGenerator::generateGte() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		llvm::Value* result;
		if (a.type == ValueType::INT && b.type == ValueType::INT) {
			result = builder->CreateICmpSGE(a.value, b.value, "gte");
		} else {
			llvm::Value* aFloat =
					a.type == ValueType::FLOAT ? a.value : builder->CreateSIToFP(a.value, builder->getDoubleTy());
			llvm::Value* bFloat =
					b.type == ValueType::FLOAT ? b.value : builder->CreateSIToFP(b.value, builder->getDoubleTy());
			result = builder->CreateFCmpOGE(aFloat, bFloat, "fgte");
		}
		auto* extended = builder->CreateZExt(result, builder->getInt64Ty());
		valueStack.push_back(TrackedValue(extended, ValueType::BOOL));
	}

	// Bitwise operations
	void RegisterGenerator::generateBitAnd() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		auto* result = builder->CreateAnd(a.value, b.value, "and");
		valueStack.push_back(TrackedValue(result, ValueType::INT));
	}

	void RegisterGenerator::generateBitOr() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		auto* result = builder->CreateOr(a.value, b.value, "or");
		valueStack.push_back(TrackedValue(result, ValueType::INT));
	}

	void RegisterGenerator::generateBitXor() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		auto* result = builder->CreateXor(a.value, b.value, "xor");
		valueStack.push_back(TrackedValue(result, ValueType::INT));
	}

	void RegisterGenerator::generateBitNot() {
		if (valueStack.empty()) return;

		auto a = valueStack.back();
		valueStack.pop_back();

		auto* result = builder->CreateNot(a.value, "not");
		valueStack.push_back(TrackedValue(result, ValueType::INT));
	}

	void RegisterGenerator::generateBitLshift() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		auto* result = builder->CreateShl(a.value, b.value, "shl");
		valueStack.push_back(TrackedValue(result, ValueType::INT));
	}

	void RegisterGenerator::generateBitRshift() {
		if (valueStack.size() < 2) return;

		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();

		auto* result = builder->CreateAShr(a.value, b.value, "shr");
		valueStack.push_back(TrackedValue(result, ValueType::INT));
	}

	// Stack manipulation (compile-time)
	void RegisterGenerator::generateDup() {
		if (valueStack.empty()) return;
		auto val = valueStack.back();
		// For reference-counted types, we need to retain
		if (val.needsRelease && val.type == ValueType::STRING) {
			builder->CreateCall(qdStringRetainFn, {val.value});
		} else if (val.needsRelease && val.type == ValueType::PTR && !val.structType.empty()) {
			builder->CreateCall(qdStructRetainFn, {val.value});
		}
		valueStack.push_back(TrackedValue(val.value, val.type, val.structType, val.needsRelease));
	}

	void RegisterGenerator::generateDrop() {
		if (valueStack.empty()) return;
		auto val = valueStack.back();
		valueStack.pop_back();
		if (val.needsRelease) {
			releaseValue(val);
		}
	}

	void RegisterGenerator::generateSwap() {
		if (valueStack.size() < 2) return;
		auto a = valueStack.back();
		valueStack.pop_back();
		auto b = valueStack.back();
		valueStack.pop_back();
		valueStack.push_back(a);
		valueStack.push_back(b);
	}

	void RegisterGenerator::generateOver() {
		if (valueStack.size() < 2) return;
		auto val = valueStack[valueStack.size() - 2];
		if (val.needsRelease && val.type == ValueType::STRING) {
			builder->CreateCall(qdStringRetainFn, {val.value});
		} else if (val.needsRelease && val.type == ValueType::PTR) {
			builder->CreateCall(qdStructRetainFn, {val.value});
		}
		valueStack.push_back(TrackedValue(val.value, val.type, val.structType, val.needsRelease));
	}

	void RegisterGenerator::generateRot() {
		if (valueStack.size() < 3) return;
		auto c = valueStack.back();
		valueStack.pop_back();
		auto b = valueStack.back();
		valueStack.pop_back();
		auto a = valueStack.back();
		valueStack.pop_back();
		valueStack.push_back(b);
		valueStack.push_back(c);
		valueStack.push_back(a);
	}

	void RegisterGenerator::generateNip() {
		if (valueStack.size() < 2) return;
		auto top = valueStack.back();
		valueStack.pop_back();
		auto second = valueStack.back();
		valueStack.pop_back();
		if (second.needsRelease) {
			releaseValue(second);
		}
		valueStack.push_back(top);
	}

	void RegisterGenerator::generateTuck() {
		if (valueStack.size() < 2) return;
		auto top = valueStack.back();
		valueStack.pop_back();
		auto second = valueStack.back();
		valueStack.pop_back();
		if (top.needsRelease && top.type == ValueType::STRING) {
			builder->CreateCall(qdStringRetainFn, {top.value});
		} else if (top.needsRelease && top.type == ValueType::PTR) {
			builder->CreateCall(qdStructRetainFn, {top.value});
		}
		valueStack.push_back(TrackedValue(top.value, top.type, top.structType, top.needsRelease));
		valueStack.push_back(second);
		valueStack.push_back(top);
	}

	// Control flow
	void RegisterGenerator::generateIf(AstNodeIfStatement* ifStmt) {
		if (valueStack.empty()) {
			reportError("Stack underflow in if condition", ifStmt->line());
			return;
		}

		auto condition = valueStack.back();
		valueStack.pop_back();

		// Create blocks
		auto* thenBB = llvm::BasicBlock::Create(*context, "then", currentFunction);
		auto* elseBB = ifStmt->elseBody() ? llvm::BasicBlock::Create(*context, "else", currentFunction) : nullptr;
		auto* mergeBB = llvm::BasicBlock::Create(*context, "ifmerge", currentFunction);

		// Save pre-if stack state (for pass-through values)
		std::vector<TrackedValue> preIfStack = valueStack;

		// Create conditional branch
		llvm::Value* condValue;
		if (condition.type == ValueType::PTR || condition.type == ValueType::STRING) {
			// Compare pointer to null
			auto* ptrTy = llvm::PointerType::get(*context, 0);
			auto* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
			condValue = builder->CreateICmpNE(condition.value, nullPtr, "ifcond");
		} else if (condition.type == ValueType::BOOL) {
			// BOOL values are stored as i64 (zext from i1), so compare to 0
			condValue = builder->CreateICmpNE(condition.value, builder->getInt64(0), "ifcond");
		} else {
			condValue = builder->CreateICmpNE(condition.value, builder->getInt64(0), "ifcond");
		}
		llvm::BasicBlock* preIfBB = builder->GetInsertBlock();
		if (elseBB) {
			builder->CreateCondBr(condValue, thenBB, elseBB);
		} else {
			builder->CreateCondBr(condValue, thenBB, mergeBB);
		}

		// Generate then block
		builder->SetInsertPoint(thenBB);
		if (ifStmt->thenBody()) {
			generateNode(ifStmt->thenBody());
		}
		auto* thenEndBB = builder->GetInsertBlock();  // May have changed due to nested control flow
		bool thenHasTerminator = thenEndBB->getTerminator() != nullptr;
		if (!thenHasTerminator) {
			builder->CreateBr(mergeBB);
		}

		// Save full stack state after then branch
		std::vector<TrackedValue> thenStack = valueStack;

		// Generate else block
		std::vector<TrackedValue> elseStack;
		llvm::BasicBlock* elseEndBB = nullptr;
		bool elseHasTerminator = false;

		if (elseBB) {
			builder->SetInsertPoint(elseBB);
			// Reset stack to before then branch
			valueStack = preIfStack;
			if (ifStmt->elseBody()) {
				generateNode(ifStmt->elseBody());
			}
			elseEndBB = builder->GetInsertBlock();
			elseHasTerminator = elseEndBB->getTerminator() != nullptr;
			if (!elseHasTerminator) {
				builder->CreateBr(mergeBB);
			}

			// Save full stack state after else branch
			elseStack = valueStack;
		}

		// Continue at merge block
		builder->SetInsertPoint(mergeBB);

		// Clear stack for rebuilding
		valueStack.clear();

		// Create PHI nodes to merge branch results
		if (elseBB && !thenHasTerminator && !elseHasTerminator) {
			// Both branches continue to merge - need PHI nodes for any differing values
			size_t maxStack = std::max(thenStack.size(), elseStack.size());

			for (size_t i = 0; i < maxStack; i++) {
				TrackedValue* thenVal = (i < thenStack.size()) ? &thenStack[i] : nullptr;
				TrackedValue* elseVal = (i < elseStack.size()) ? &elseStack[i] : nullptr;

				if (thenVal && elseVal) {
					// Both branches have a value at this position
					if (thenVal->value == elseVal->value) {
						// Same value in both branches - no PHI needed
						valueStack.push_back(*thenVal);
					} else {
						// Different values - need PHI
						auto* phi = builder->CreatePHI(thenVal->value->getType(), 2, "if_phi");
						phi->addIncoming(thenVal->value, thenEndBB);
						phi->addIncoming(elseVal->value, elseEndBB);
						valueStack.push_back(TrackedValue(phi, thenVal->type, thenVal->structType,
							thenVal->needsRelease || elseVal->needsRelease));
					}
				} else if (thenVal && !elseVal) {
					// Only then has value - else passes through pre-if value
					if (i < preIfStack.size()) {
						auto* phi = builder->CreatePHI(thenVal->value->getType(), 2, "if_phi");
						phi->addIncoming(thenVal->value, thenEndBB);
						phi->addIncoming(preIfStack[i].value, elseEndBB);
						valueStack.push_back(TrackedValue(phi, thenVal->type, thenVal->structType,
							thenVal->needsRelease));
					} else {
						// Value only exists in then branch - stack effect mismatch
						valueStack.push_back(*thenVal);
					}
				} else if (!thenVal && elseVal) {
					// Only else has value - then passes through pre-if value
					if (i < preIfStack.size()) {
						auto* phi = builder->CreatePHI(elseVal->value->getType(), 2, "if_phi");
						phi->addIncoming(preIfStack[i].value, thenEndBB);
						phi->addIncoming(elseVal->value, elseEndBB);
						valueStack.push_back(TrackedValue(phi, elseVal->type, elseVal->structType,
							elseVal->needsRelease));
					} else {
						// Value only exists in else branch - stack effect mismatch
						valueStack.push_back(*elseVal);
					}
				}
			}
		} else if (elseBB && thenHasTerminator && !elseHasTerminator) {
			// Only else continues - use else stack directly
			valueStack = elseStack;
		} else if (elseBB && !thenHasTerminator && elseHasTerminator) {
			// Only then continues - use then stack directly
			valueStack = thenStack;
		} else if (!elseBB && !thenHasTerminator) {
			// No else branch, then continues - need PHI for values that might be modified
			size_t maxStack = std::max(thenStack.size(), preIfStack.size());
			for (size_t i = 0; i < maxStack; i++) {
				TrackedValue* thenVal = (i < thenStack.size()) ? &thenStack[i] : nullptr;
				TrackedValue* preVal = (i < preIfStack.size()) ? &preIfStack[i] : nullptr;

				if (thenVal && preVal) {
					if (thenVal->value == preVal->value) {
						// Same value - no PHI needed
						valueStack.push_back(*thenVal);
					} else {
						// Different values - need PHI with skip path
						auto* phi = builder->CreatePHI(thenVal->value->getType(), 2, "if_phi");
						phi->addIncoming(thenVal->value, thenEndBB);
						phi->addIncoming(preVal->value, preIfBB);
						valueStack.push_back(TrackedValue(phi, thenVal->type, thenVal->structType,
							thenVal->needsRelease));
					}
				} else if (thenVal) {
					// Value only in then branch (was pushed)
					auto* phi = builder->CreatePHI(thenVal->value->getType(), 2, "if_phi");
					phi->addIncoming(thenVal->value, thenEndBB);
					// For skip path, use undef since value doesn't exist
					phi->addIncoming(llvm::UndefValue::get(thenVal->value->getType()), preIfBB);
					valueStack.push_back(TrackedValue(phi, thenVal->type, thenVal->structType,
						thenVal->needsRelease));
				} else if (preVal) {
					// Value only in pre-if (then popped it) - keep pre-if via PHI
					valueStack.push_back(*preVal);
				}
			}
		} else if (!elseBB && thenHasTerminator) {
			// Then has terminator, no else - merge gets pre-if stack
			valueStack = preIfStack;
		}
		// If both have terminators, nothing to do - merge block is unreachable
	}

	void RegisterGenerator::generateFor(AstNodeForStatement* forStmt) {
		if (valueStack.size() < 3) {
			reportError("Stack underflow in for loop (need start, end, step)", forStmt->line());
			return;
		}

		// Pop step, end, start
		auto step = valueStack.back();
		valueStack.pop_back();
		auto end = valueStack.back();
		valueStack.pop_back();
		auto start = valueStack.back();
		valueStack.pop_back();

		// Create loop variable
		std::string iterName = forStmt->iteratorName().empty() ? "$" : forStmt->iteratorName();
		auto* iterAlloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, iterName);
		builder->CreateStore(start.value, iterAlloca);

		LocalInfo iterInfo;
		iterInfo.alloca = iterAlloca;
		iterInfo.type = ValueType::INT;
		iterInfo.needsRelease = false;
		localVariables[iterName] = iterInfo;

		// Create loop blocks
		auto* condBB = llvm::BasicBlock::Create(*context, "for_cond", currentFunction);
		auto* bodyBB = llvm::BasicBlock::Create(*context, "for_body", currentFunction);
		auto* stepBB = llvm::BasicBlock::Create(*context, "for_step", currentFunction);
		auto* endBB = llvm::BasicBlock::Create(*context, "for_end", currentFunction);

		// Push loop context
		loopStack.push_back({endBB, stepBB, nullptr, {}});

		// Branch to condition
		builder->CreateBr(condBB);

		// Condition block
		builder->SetInsertPoint(condBB);
		auto* iterVal = builder->CreateLoad(builder->getInt64Ty(), iterAlloca, "iter");

		// Determine loop direction
		llvm::Value* cond;
		auto* isPositiveStep = builder->CreateICmpSGT(step.value, builder->getInt64(0));
		auto* lessThanEnd = builder->CreateICmpSLT(iterVal, end.value);
		auto* greaterThanEnd = builder->CreateICmpSGT(iterVal, end.value);
		cond = builder->CreateSelect(isPositiveStep, lessThanEnd, greaterThanEnd);

		builder->CreateCondBr(cond, bodyBB, endBB);

		// Body block
		builder->SetInsertPoint(bodyBB);
		pushDeferScope();
		if (forStmt->body()) {
			generateNode(forStmt->body());
		}
		executeDeferScope();
		popDeferScope();
		if (!builder->GetInsertBlock()->getTerminator()) {
			builder->CreateBr(stepBB);
		}

		// Step block
		builder->SetInsertPoint(stepBB);
		auto* currentIter = builder->CreateLoad(builder->getInt64Ty(), iterAlloca);
		auto* nextIter = builder->CreateAdd(currentIter, step.value, "next_iter");
		builder->CreateStore(nextIter, iterAlloca);
		builder->CreateBr(condBB);

		// End block
		builder->SetInsertPoint(endBB);

		// Pop loop context
		loopStack.pop_back();

		// Remove iterator from locals
		localVariables.erase(iterName);
	}

	void RegisterGenerator::generateWhile(AstNodeWhileStatement* whileStmt) {
		// Get initial condition from the stack
		if (valueStack.empty()) {
			reportError("Stack underflow in while condition", whileStmt->line());
			return;
		}

		auto initialCond = valueStack.back();
		valueStack.pop_back();

		// Create an alloca to store the condition (so body can update it)
		auto* condAlloca = builder->CreateAlloca(builder->getInt64Ty(), nullptr, "while_cond_var");
		builder->CreateStore(initialCond.value, condAlloca);

		auto* condBB = llvm::BasicBlock::Create(*context, "while_cond", currentFunction);
		auto* bodyBB = llvm::BasicBlock::Create(*context, "while_body", currentFunction);
		auto* endBB = llvm::BasicBlock::Create(*context, "while_end", currentFunction);

		loopStack.push_back({endBB, condBB, condAlloca, {}});

		builder->CreateBr(condBB);

		// Condition block - load and check the condition
		builder->SetInsertPoint(condBB);
		auto* condLoaded = builder->CreateLoad(builder->getInt64Ty(), condAlloca, "cond_val");
		auto* condValue = builder->CreateICmpNE(condLoaded, builder->getInt64(0), "whilecond");
		builder->CreateCondBr(condValue, bodyBB, endBB);

		// Body block
		builder->SetInsertPoint(bodyBB);
		pushDeferScope();
		if (whileStmt->body()) {
			generateNode(whileStmt->body());
		}
		executeDeferScope();
		popDeferScope();

		// After body, if there's a value on the stack, it's the new condition
		if (!valueStack.empty() && !builder->GetInsertBlock()->getTerminator()) {
			auto newCond = valueStack.back();
			valueStack.pop_back();
			builder->CreateStore(newCond.value, condAlloca);
		}

		if (!builder->GetInsertBlock()->getTerminator()) {
			builder->CreateBr(condBB);
		}

		// End block
		builder->SetInsertPoint(endBB);
		loopStack.pop_back();
	}

	void RegisterGenerator::generateLoop(AstNodeLoopStatement* loopStmt) {
		// For a loop, we need to materialize the current stack into allocas
		// so that values persist across iterations

		// Save initial stack and create allocas for each value
		std::vector<llvm::AllocaInst*> stackAllocas;
		std::vector<ValueType> stackTypes;
		std::vector<std::string> stackStructTypes;

		for (auto& val : valueStack) {
			llvm::Type* ty = getLlvmType(val.type);
			auto* alloca = builder->CreateAlloca(ty, nullptr, "loop_stack_val");
			builder->CreateStore(val.value, alloca);
			stackAllocas.push_back(alloca);
			stackTypes.push_back(val.type);
			stackStructTypes.push_back(val.structType);
		}

		// Clear compile-time stack - we'll reload from allocas
		valueStack.clear();

		auto* bodyBB = llvm::BasicBlock::Create(*context, "loop_body", currentFunction);
		auto* endBB = llvm::BasicBlock::Create(*context, "loop_end", currentFunction);

		loopStack.push_back({endBB, bodyBB, nullptr, stackAllocas});

		builder->CreateBr(bodyBB);

		// Body block - load values at start
		builder->SetInsertPoint(bodyBB);

		// Reload stack from allocas
		for (size_t i = 0; i < stackAllocas.size(); i++) {
			llvm::Type* ty = getLlvmType(stackTypes[i]);
			auto* loaded = builder->CreateLoad(ty, stackAllocas[i], "stack_reload");
			valueStack.push_back(TrackedValue(loaded, stackTypes[i], stackStructTypes[i]));
		}

		pushDeferScope();
		if (loopStmt->body()) {
			generateNode(loopStmt->body());
		}
		executeDeferScope();
		popDeferScope();

		// Before branching back, store current stack to allocas
		if (!builder->GetInsertBlock()->getTerminator()) {
			// Store values back (stack size should match)
			size_t minSize = std::min(valueStack.size(), stackAllocas.size());
			for (size_t i = 0; i < minSize; i++) {
				builder->CreateStore(valueStack[i].value, stackAllocas[i]);
			}
			builder->CreateBr(bodyBB);
		}

		// Clear stack again for end block
		valueStack.clear();

		// End block - reload final values
		builder->SetInsertPoint(endBB);
		for (size_t i = 0; i < stackAllocas.size(); i++) {
			llvm::Type* ty = getLlvmType(stackTypes[i]);
			auto* loaded = builder->CreateLoad(ty, stackAllocas[i], "stack_final");
			valueStack.push_back(TrackedValue(loaded, stackTypes[i], stackStructTypes[i]));
		}

		loopStack.pop_back();
	}

	void RegisterGenerator::generateSwitch(AstNodeSwitchStatement* switchStmt) {
		if (valueStack.empty()) {
			reportError("Stack underflow in switch", switchStmt->line());
			return;
		}

		auto switchVal = valueStack.back();
		valueStack.pop_back();

		auto* endBB = llvm::BasicBlock::Create(*context, "switch_end", currentFunction);

		// Find default case if any
		const auto& cases = switchStmt->cases();
		AstNodeCase* defaultCase = nullptr;
		for (auto* caseNode : cases) {
			if (caseNode->isDefault()) {
				defaultCase = caseNode;
				break;
			}
		}

		auto* defaultBB = defaultCase
				? llvm::BasicBlock::Create(*context, "switch_default", currentFunction)
				: endBB;

		// Count non-default cases
		unsigned numCases = 0;
		for (auto* caseNode : cases) {
			if (!caseNode->isDefault()) {
				numCases++;
			}
		}

		// Create switch instruction
		auto* switchInst = builder->CreateSwitch(switchVal.value, defaultBB, numCases);

		// Generate case blocks
		for (auto* caseNode : cases) {
			if (caseNode->isDefault()) {
				continue;
			}

			auto* caseBB = llvm::BasicBlock::Create(*context, "switch_case", currentFunction);

			// Get case value - the value is an AST node, generate it to get the constant
			if (caseNode->value()) {
				// For integer literals, get the value directly
				if (caseNode->value()->type() == IAstNode::Type::LITERAL) {
					auto* lit = static_cast<AstNodeLiteral*>(caseNode->value());
					int64_t caseValue = std::stoll(lit->value());
					switchInst->addCase(builder->getInt64(static_cast<uint64_t>(caseValue)), caseBB);
				}
			}

			// Generate case body
			builder->SetInsertPoint(caseBB);
			if (caseNode->body()) {
				generateNode(caseNode->body());
			}
			if (!builder->GetInsertBlock()->getTerminator()) {
				builder->CreateBr(endBB);
			}
		}

		// Generate default block
		if (defaultCase) {
			builder->SetInsertPoint(defaultBB);
			if (defaultCase->body()) {
				generateNode(defaultCase->body());
			}
			if (!builder->GetInsertBlock()->getTerminator()) {
				builder->CreateBr(endBB);
			}
		}

		// Continue at end block
		builder->SetInsertPoint(endBB);
	}


	void RegisterGenerator::generateCtxBlock(AstNodeCtx* ctxNode) {
		// ctx blocks create a child context that:
		// 1. Gets a COPY of the current stack values
		// 2. Executes the body with those values
		// 3. Pops EXACTLY ONE value from child and pushes it to parent

		// Save the parent stack - it will be preserved
		std::vector<TrackedValue> parentStack = valueStack;

		// Child starts with COPIES of the same values (but we DON'T transfer ownership)
		// Create a child stack with non-owning copies
		std::vector<TrackedValue> childStack;
		for (const auto& val : parentStack) {
			// Create a copy without ownership transfer
			TrackedValue copy = val;
			copy.needsRelease = false;  // Child doesn't own these copies
			childStack.push_back(copy);
		}

		// Replace valueStack with childStack for the duration of the ctx block
		valueStack = childStack;

		// Execute the ctx block body
		pushDeferScope();
		for (size_t i = 0; i < ctxNode->childCount(); i++) {
			generateNode(ctxNode->child(i));
		}
		executeDeferScope();
		popDeferScope();

		// Get EXACTLY ONE result from the top of the child stack
		TrackedValue childResult(nullptr, ValueType::INT);
		if (!valueStack.empty()) {
			childResult = valueStack.back();
		}

		// Restore the parent stack
		valueStack = parentStack;

		// Push exactly one result onto the parent stack
		if (childResult.value != nullptr) {
			valueStack.push_back(childResult);
		}
	}

	// Struct handling
	void RegisterGenerator::processStructDeclaration(AstNodeStructDeclaration* structDecl,
													 const std::string& moduleName) {
		std::string structName = moduleName + "::" + structDecl->name();

		RegStructLayout layout;
		layout.name = structName;
		layout.isPublic = structDecl->isPublic();
		layout.totalSize = 0;

		for (size_t i = 0; i < structDecl->fields().size(); i++) {
			auto* field = structDecl->fields()[i];

			RegFieldInfo fieldInfo;
			fieldInfo.name = field->name();
			fieldInfo.typeName = field->typeName();
			fieldInfo.offset = layout.totalSize;
			fieldInfo.size = getTypeSize(field->typeName());

			layout.fields.push_back(fieldInfo);
			layout.totalSize += fieldInfo.size;
		}

		// Align total size to 8 bytes
		layout.totalSize = (layout.totalSize + 7) & ~static_cast<size_t>(7);

		structDefinitions[structName] = layout;
	}

	void RegisterGenerator::processImportStatement(AstNodeImport* importNode, const std::string& moduleName) {
		const std::string& namespaceName = importNode->namespaceName();
		const std::string& library = importNode->library();

		// Track library for linking
		importedLibraries.insert(library);

		for (const auto* func : importNode->functions()) {
			// Determine mangled C function name based on library
			std::string mangledName;
			if (library == "libstdqd.so") {
				mangledName = "qd_stdqd_" + func->name;
			} else if ((library.rfind("libqd", 0) == 0 && library.find(".a") != std::string::npos) ||
					   (library.rfind("libstd", 0) == 0 &&
						(library.find("qd_static.a") != std::string::npos ||
						 library.find("qd.so") != std::string::npos))) {
				mangledName = "usr_" + namespaceName + "_" + func->name;
			} else {
				mangledName = func->name;
			}

			// Create function type based on parameters
			std::vector<llvm::Type*> paramTypes;
			for (const auto* param : func->inputParameters) {
				paramTypes.push_back(getLlvmType(typeFromString(param->typeString())));
			}

			// Determine return type
			llvm::Type* returnType = builder->getVoidTy();
			if (func->outputParameters.size() == 1) {
				returnType = getLlvmType(typeFromString(func->outputParameters[0]->typeString()));
			} else if (func->outputParameters.size() > 1) {
				std::vector<llvm::Type*> returnTypes;
				for (const auto* param : func->outputParameters) {
					returnTypes.push_back(getLlvmType(typeFromString(param->typeString())));
				}
				returnType = llvm::StructType::get(*context, returnTypes);
			}

			// Create external function declaration
			auto* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
			llvm::Function* fn = module->getFunction(mangledName);
			if (!fn) {
				fn = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, mangledName, *module);
			}

			// Register the function under namespaced name (e.g., "strconv::itoa")
			std::string scopedName = namespaceName + "::" + func->name;
			userFunctions[scopedName] = fn;

			// Store signature info
			RegFunctionSignature sig;
			for (const auto* param : func->inputParameters) {
				sig.params.push_back({param->name(), typeFromString(param->typeString())});
			}
			for (const auto* param : func->outputParameters) {
				std::string typeName = param->typeString();
				ValueType vt = typeFromString(typeName);
				std::string structTypeName = (vt == ValueType::PTR) ? typeName : "";
				sig.returns.push_back({vt, structTypeName});
			}
			sig.isFallible = func->throws;
			functionSignatures[scopedName] = sig;
			fallibleFunctions[scopedName] = func->throws;

			// Also register as module-prefixed name for cross-module access
			if (func->isPublic) {
				std::string crossModuleName = moduleName + "::" + func->name;
				userFunctions[crossModuleName] = fn;
				functionSignatures[crossModuleName] = sig;
				fallibleFunctions[crossModuleName] = func->throws;
			}
		}
	}

	void RegisterGenerator::generateStructConstruction(const std::string& structName) {
		auto it = structDefinitions.find(structName);
		if (it == structDefinitions.end()) {
			reportError("Unknown struct: " + structName, 0);
			return;
		}

		const auto& layout = it->second;

		// Get destructor function (or null)
		llvm::Value* destructorFn = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context));
		auto destIt = structDestructors.find(structName);
		if (destIt != structDestructors.end()) {
			destructorFn = destIt->second;
		}

		// Allocate struct
		auto* structPtr =
				builder->CreateCall(qdStructAllocFn, {builder->getInt64(layout.totalSize), destructorFn}, "struct");

		// Initialize fields from stack (in reverse order)
		std::vector<TrackedValue> fieldValues;
		for (size_t i = 0; i < layout.fields.size() && !valueStack.empty(); i++) {
			fieldValues.insert(fieldValues.begin(), valueStack.back());
			valueStack.pop_back();
		}

		// Store field values
		for (size_t i = 0; i < fieldValues.size() && i < layout.fields.size(); i++) {
			const auto& field = layout.fields[i];
			auto* fieldPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, builder->getInt64(field.offset),
												field.name + "_ptr");

			builder->CreateStore(fieldValues[i].value, fieldPtr);

			// If the field is reference-counted, we've transferred ownership
			// No need to release - the struct now owns it
		}

		valueStack.push_back(TrackedValue(structPtr, ValueType::PTR, structName, true));
	}

	void RegisterGenerator::generateStructConstructionNamed(AstNodeStructConstruction* structConstruction) {
		const std::string& name = structConstruction->structName();

		// Try to find the struct with current module prefix
		std::string structName = currentModulePrefix + "::" + name;
		auto it = structDefinitions.find(structName);
		if (it == structDefinitions.end()) {
			// Try without prefix (might be using full name)
			it = structDefinitions.find(name);
			if (it == structDefinitions.end()) {
				reportError("Unknown struct: " + name, structConstruction->line());
				return;
			}
			structName = name;
		}

		const auto& layout = it->second;

		// Get destructor function (or null)
		llvm::Value* destructorFn = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context));
		auto destIt = structDestructors.find(structName);
		if (destIt != structDestructors.end()) {
			destructorFn = destIt->second;
		}

		// Allocate struct
		auto* structPtr =
				builder->CreateCall(qdStructAllocFn, {builder->getInt64(layout.totalSize), destructorFn}, "struct");

		// Map of field name -> field info for quick lookup
		std::map<std::string, const RegFieldInfo*> fieldMap;
		for (const auto& field : layout.fields) {
			fieldMap[field.name] = &field;
		}

		// Process field initializers
		const auto& fieldInits = structConstruction->fieldInits();
		for (const auto& init : fieldInits) {
			// Find the field
			auto fieldIt = fieldMap.find(init.fieldName);
			if (fieldIt == fieldMap.end()) {
				reportError("Unknown field: " + init.fieldName, structConstruction->line());
				continue;
			}
			const RegFieldInfo* field = fieldIt->second;

			// Generate the value expression
			for (auto* valueNode : init.valueNodes) {
				generateNode(valueNode);
			}

			// Pop the value from the stack
			if (valueStack.empty()) {
				reportError("No value for field: " + init.fieldName, structConstruction->line());
				continue;
			}
			auto val = valueStack.back();
			valueStack.pop_back();

			// Store the value in the field
			auto* fieldPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr,
												builder->getInt64(field->offset),
												init.fieldName + "_ptr");
			builder->CreateStore(val.value, fieldPtr);
		}

		valueStack.push_back(TrackedValue(structPtr, ValueType::PTR, structName, true));
	}

	void RegisterGenerator::generateFieldAccess(AstNodeFieldAccess* fieldAccess) {
		const std::string& varName = fieldAccess->varName();
		const std::string& fieldName = fieldAccess->fieldName();

		TrackedValue structVal(nullptr, ValueType::PTR);

		// If varName is provided, look up the local variable directly
		if (!varName.empty()) {
			auto it = localVariables.find(varName);
			if (it == localVariables.end()) {
				reportError("Unknown variable in field access: " + varName, fieldAccess->line());
				return;
			}
			auto* loaded = builder->CreateLoad(getLlvmType(it->second.type), it->second.alloca, varName);
			structVal = TrackedValue(loaded, it->second.type, it->second.structType, false);
		} else {
			// Fall back to stack-based access
			if (valueStack.empty()) {
				reportError("Stack underflow in field access", fieldAccess->line());
				return;
			}
			structVal = valueStack.back();
			valueStack.pop_back();
		}

		// Find struct definition
		const RegStructLayout* layout = findStructDefinition(structVal.structType);

		// If struct type is not found, try to infer it from the field name
		// This handles cases where module functions use generic "ptr" types
		if (!layout) {
			// First try with module prefix if structType has one
			if (!structVal.structType.empty()) {
				auto colonPos = structVal.structType.find("::");
				if (colonPos != std::string::npos) {
					std::string modulePrefix = structVal.structType.substr(0, colonPos);
					for (const auto& [structName, structLayout] : structDefinitions) {
						if (structName.find(modulePrefix + "::") == 0) {
							for (const auto& field : structLayout.fields) {
								if (field.name == fieldName) {
									layout = &structLayout;
									break;
								}
							}
							if (layout) break;
						}
					}
				}
			}

			// If still not found, search current module's structs first
			if (!layout && !currentModulePrefix.empty()) {
				for (const auto& [structName, structLayout] : structDefinitions) {
					if (structName.find(currentModulePrefix + "::") == 0) {
						for (const auto& field : structLayout.fields) {
							if (field.name == fieldName) {
								layout = &structLayout;
								break;
							}
						}
						if (layout) break;
					}
				}
			}

			// If still not found, search all structs
			if (!layout) {
				for (const auto& [structName, structLayout] : structDefinitions) {
					for (const auto& field : structLayout.fields) {
						if (field.name == fieldName) {
							layout = &structLayout;
							break;
						}
					}
					if (layout) break;
				}
			}
		}

		if (!layout) {
			reportError("Unknown struct type for field access: " + structVal.structType, fieldAccess->line());
			return;
		}

		// Find field
		for (const auto& field : layout->fields) {
			if (field.name == fieldName) {
				auto* fieldPtr = builder->CreateGEP(builder->getInt8Ty(), structVal.value,
													builder->getInt64(field.offset), fieldName + "_ptr");

				llvm::Type* fieldType = getLlvmType(typeFromString(field.typeName)); auto* fieldValue = builder->CreateLoad(fieldType, fieldPtr, fieldName);

				ValueType vt = typeFromString(field.typeName);
				bool needsRelease = (vt == ValueType::STRING || vt == ValueType::PTR);

				// If it's a reference type, retain it
				if (needsRelease && vt == ValueType::STRING) {
					builder->CreateCall(qdStringRetainFn, {fieldValue});
				} else if (needsRelease && vt == ValueType::PTR) {
					builder->CreateCall(qdStructRetainFn, {fieldValue});
				}

				// Determine the struct type for nested structs
				std::string nestedStructType;
				if (vt == ValueType::PTR) {
					// Check if the field type is a known struct type
					// Try with current module prefix first
					std::string prefixedType = currentModulePrefix + "::" + field.typeName;
					if (findStructDefinition(prefixedType)) {
						nestedStructType = prefixedType;
					} else if (findStructDefinition(field.typeName)) {
						nestedStructType = field.typeName;
					}
				}

				valueStack.push_back(TrackedValue(fieldValue, vt, nestedStructType, needsRelease));

				// Release the struct we accessed
				if (structVal.needsRelease) {
					releaseValue(structVal);
				}
				return;
			}
		}

		reportError("Unknown field: " + fieldName, fieldAccess->line());
	}

	void RegisterGenerator::generateFieldSet(AstNodeFieldSet* fieldSet) {
		const std::string& varName = fieldSet->varName();
		const std::string& fieldName = fieldSet->fieldName();

		// Get the new value from the stack
		if (valueStack.empty()) {
			reportError("Stack underflow in field set", fieldSet->line());
			return;
		}
		auto newValue = valueStack.back();
		valueStack.pop_back();

		TrackedValue structVal(nullptr, ValueType::PTR);

		// If varName is provided, look up the local variable directly
		if (!varName.empty()) {
			auto it = localVariables.find(varName);
			if (it == localVariables.end()) {
				reportError("Unknown variable in field set: " + varName, fieldSet->line());
				return;
			}
			auto* loaded = builder->CreateLoad(getLlvmType(it->second.type), it->second.alloca, varName);
			structVal = TrackedValue(loaded, it->second.type, it->second.structType, false);
		} else {
			// Fall back to stack-based access
			if (valueStack.empty()) {
				reportError("Stack underflow in field set (struct)", fieldSet->line());
				return;
			}
			structVal = valueStack.back();
			valueStack.pop_back();
		}

		const RegStructLayout* layout = findStructDefinition(structVal.structType);

		// If struct type is not found, try to infer it from the field name
		// This handles cases where module functions return generic "ptr" types
		if (!layout && !structVal.structType.empty()) {
			auto colonPos = structVal.structType.find("::");
			if (colonPos != std::string::npos) {
				std::string modulePrefix = structVal.structType.substr(0, colonPos);
				for (const auto& [structName, structLayout] : structDefinitions) {
					if (structName.find(modulePrefix + "::") == 0) {
						for (const auto& field : structLayout.fields) {
							if (field.name == fieldName) {
								layout = &structLayout;
								break;
							}
						}
						if (layout) break;
					}
				}
			}
		}

		if (!layout) {
			reportError("Unknown struct type for field set: " + structVal.structType, fieldSet->line());
			return;
		}

		for (const auto& field : layout->fields) {
			if (field.name == fieldName) {
				auto* fieldPtr = builder->CreateGEP(builder->getInt8Ty(), structVal.value,
													builder->getInt64(field.offset), fieldName + "_ptr");

				ValueType vt = typeFromString(field.typeName);

				// Release old value if reference-counted
				if (vt == ValueType::STRING || vt == ValueType::PTR) {
					llvm::Type* fieldType = getLlvmType(vt);
					auto* oldValue = builder->CreateLoad(fieldType, fieldPtr, "old_" + fieldName);
					if (vt == ValueType::STRING) {
						builder->CreateCall(qdStringReleaseFn, {oldValue});
					} else {
						builder->CreateCall(qdStructReleaseFn, {oldValue});
					}
				}

				// Store new value
				builder->CreateStore(newValue.value, fieldPtr);

				// The struct still exists, push it back
				valueStack.push_back(structVal);
				return;
			}
		}

		reportError("Unknown field: " + fieldName, fieldSet->line());
	}

	void RegisterGenerator::generateStructDestructors() {
		for (auto& pair : structDefinitions) {
			const std::string& structName = pair.first;
			const RegStructLayout& layout = pair.second;

			// Check if any fields need destruction
			bool needsDestructor = false;
			for (const auto& field : layout.fields) {
				ValueType vt = typeFromString(field.typeName);
				if (vt == ValueType::STRING || vt == ValueType::PTR) {
					needsDestructor = true;
					break;
				}
			}

			if (!needsDestructor) continue;

			// Create destructor function
			std::string destructorName = "destruct_" + structName;
			std::replace(destructorName.begin(), destructorName.end(), ':', '_');

			auto* destructorTy =
					llvm::FunctionType::get(builder->getVoidTy(), {llvm::PointerType::getUnqual(*context)}, false);
			auto* destructor =
					llvm::Function::Create(destructorTy, llvm::Function::InternalLinkage, destructorName, *module);

			auto* entry = llvm::BasicBlock::Create(*context, "entry", destructor);
			builder->SetInsertPoint(entry);

			auto* structPtr = destructor->arg_begin();

			// Release reference-counted fields
			for (const auto& field : layout.fields) {
				ValueType vt = typeFromString(field.typeName);
				if (vt == ValueType::STRING || vt == ValueType::PTR) {
					auto* fieldPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr,
														builder->getInt64(field.offset), field.name + "_ptr");

					llvm::Type* fieldType = getLlvmType(vt);
					auto* fieldValue = builder->CreateLoad(fieldType, fieldPtr, field.name);

					if (vt == ValueType::STRING) {
						builder->CreateCall(qdStringReleaseFn, {fieldValue});
					} else {
						builder->CreateCall(qdStructReleaseFn, {fieldValue});
					}
				}
			}

			builder->CreateRetVoid();

			structDestructors[structName] = destructor;
		}
	}

	const RegStructLayout* RegisterGenerator::findStructDefinition(const std::string& structName) const {
		auto it = structDefinitions.find(structName);
		if (it != structDefinitions.end()) {
			return &it->second;
		}

		// Try with current module prefix
		std::string prefixedName = currentModulePrefix + "::" + structName;
		it = structDefinitions.find(prefixedName);
		if (it != structDefinitions.end()) {
			return &it->second;
		}

		return nullptr;
	}

	size_t RegisterGenerator::getTypeSize(const std::string& typeName) {
		if (typeName == "i64" || typeName == "int" || typeName == "i") {
			return 8;
		} else if (typeName == "f64" || typeName == "float" || typeName == "f") {
			return 8;
		} else if (typeName == "str" || typeName == "string" || typeName == "s") {
			return 8; // pointer
		} else if (typeName == "ptr" || typeName == "p") {
			return 8;
		} else if (typeName == "bool" || typeName == "b") {
			return 8;
		}

		// Check if it's a struct type
		auto it = structDefinitions.find(typeName);
		if (it != structDefinitions.end()) {
			return 8; // Structs are always pointer-sized
		}

		return 8; // Default to 8 bytes
	}

	void RegisterGenerator::releaseValue(const TrackedValue& val) {
		if (!val.needsRelease) return;

		if (val.type == ValueType::STRING) {
			builder->CreateCall(qdStringReleaseFn, {val.value});
		} else if (val.type == ValueType::PTR && !val.structType.empty()) {
			builder->CreateCall(qdStructReleaseFn, {val.value});
		}
	}

	// Defer scope management
	void RegisterGenerator::pushDeferScope() { deferScopeStack.push_back({}); }

	void RegisterGenerator::popDeferScope() {
		if (!deferScopeStack.empty()) {
			deferScopeStack.pop_back();
		}
	}

	void RegisterGenerator::executeDeferScope() {
		if (deferScopeStack.empty()) return;

		// Execute defers in reverse order
		auto& defers = deferScopeStack.back();
		for (auto it = defers.rbegin(); it != defers.rend(); ++it) {
			if (((*it)->childCount() > 0)) {
				for (size_t di = 0; di < (*it)->childCount(); di++) { generateNode((*it)->child(di)); }
			}
		}
	}

	// Helper methods
	std::string RegisterGenerator::mangleName(const std::string& name, const std::string& prefix) {
		if (prefix.empty() || prefix == "main") {
			return "qd_main_" + name;
		}
		return "qd_" + prefix + "_" + name;
	}

	llvm::Type* RegisterGenerator::getLlvmType(ValueType type) {
		switch (type) {
			case ValueType::INT:
			case ValueType::BOOL:
				return builder->getInt64Ty();
			case ValueType::FLOAT:
				return builder->getDoubleTy();
			case ValueType::STRING:
			case ValueType::PTR:
				return llvm::PointerType::getUnqual(*context);
		}
		return builder->getInt64Ty();
	}

	ValueType RegisterGenerator::typeFromString(const std::string& typeStr) {
		if (typeStr == "i64" || typeStr == "int" || typeStr == "i") {
			return ValueType::INT;
		} else if (typeStr == "f64" || typeStr == "float" || typeStr == "f") {
			return ValueType::FLOAT;
		} else if (typeStr == "str" || typeStr == "string" || typeStr == "s") {
			return ValueType::STRING;
		} else if (typeStr == "ptr" || typeStr == "p") {
			return ValueType::PTR;
		} else if (typeStr == "bool" || typeStr == "b") {
			return ValueType::BOOL;
		} else if (typeStr == "any") {
			// 'any' type is polymorphic - treat as i64 for storage
			// Operations on 'any' values preserve whatever type was pushed
			return ValueType::INT;
		}
		// Assume struct types are pointers
		return ValueType::PTR;
	}

	std::string RegisterGenerator::uniqueName(const std::string& base) { return base + "_" + std::to_string(varCounter++); }

	void RegisterGenerator::reportError(const std::string& msg, size_t line) {
		std::cerr << "Error";
		if (line > 0) {
			std::cerr << " (line " << line << ")";
		}
		std::cerr << ": " << msg << "\n";
		compilationFailed = true;
	}

	// Output methods
	bool RegisterGenerator::writeIR(const std::string& filename) {
		std::error_code ec;
		llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);

		if (ec) {
			std::cerr << "Could not open file: " << ec.message() << "\n";
			return false;
		}

		module->print(dest, nullptr);
		return true;
	}

	std::string RegisterGenerator::getIRString() const {
		std::string ir;
		llvm::raw_string_ostream stream(ir);
		module->print(stream, nullptr);
		return ir;
	}

	bool RegisterGenerator::writeObject(const std::string& filename) {
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmParser();
		llvm::InitializeNativeTargetAsmPrinter();

		auto targetTriple = llvm::sys::getDefaultTargetTriple();

		std::string error;
		auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
		if (!target) {
			std::cerr << error << "\n";
			return false;
		}

		auto cpu = "generic";
		auto features = "";
		llvm::TargetOptions opt;
		auto rm = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
		auto targetMachine = target->createTargetMachine(llvm::Triple(targetTriple), cpu, features, opt, rm);

		module->setDataLayout(targetMachine->createDataLayout());

		std::error_code ec;
		llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);

		if (ec) {
			std::cerr << "Could not open file: " << ec.message() << "\n";
			return false;
		}

		llvm::legacy::PassManager pass;
		if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
			std::cerr << "Target machine can't emit object file\n";
			return false;
		}

		pass.run(*module);
		dest.flush();

		return true;
	}

	bool RegisterGenerator::writeExecutable(const std::string& filename) {
		// Write object file first
		std::string objFile = filename + ".o";
		if (!writeObject(objFile)) {
			return false;
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
			std::error_code ec;
			std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe", ec);
			if (!ec) {
				std::filesystem::path exeDir = exePath.parent_path();
				std::filesystem::path installedLib = exeDir / ".." / "lib";
				if (std::filesystem::exists(installedLib)) {
					libDir = installedLib.string();
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

		// Find the runtime library
		std::string qdrtStaticPath;
		std::string flatPath = libDir + "/libqdrt.a";
		std::string flatPathLegacy = libDir + "/libqdrt_static.a";

		if (std::filesystem::exists(flatPath)) {
			qdrtStaticPath = flatPath;
		} else if (std::filesystem::exists(flatPathLegacy)) {
			qdrtStaticPath = flatPathLegacy;
		} else {
			// Fallback to flat path (will error later if doesn't exist)
			qdrtStaticPath = flatPath;
		}

		// Build link command
		std::string cmd = "clang -o " + filename + " " + objFile + " " + qdrtStaticPath + " -lm";

		// Add library search paths
		for (const auto& path : librarySearchPaths) {
			cmd += " -L" + path;
		}

		// Add main library directory
		if (!libDir.empty()) {
			cmd += " -L" + libDir;
		}

		// Add imported libraries
		for (const auto& lib : importedLibraries) {
			// Check if it's a static library (.a file)
			if (lib.size() >= 2 && lib.substr(lib.size() - 2) == ".a") {
				// Try to find the library
				std::string foundLibPath;
				for (const auto& searchPath : librarySearchPaths) {
					std::string candidatePath = searchPath + "/" + lib;
					if (std::filesystem::exists(candidatePath)) {
						foundLibPath = candidatePath;
						break;
					}
				}
				if (foundLibPath.empty()) {
					std::string flatLib = libDir + "/" + lib;
					if (std::filesystem::exists(flatLib)) {
						foundLibPath = flatLib;
					} else {
						foundLibPath = lib;
					}
				}
				cmd += " " + foundLibPath;
			} else {
				cmd += " -l" + lib;
			}
		}

		int result = std::system(cmd.c_str());

		// Clean up object file
		std::filesystem::remove(objFile);

		return result == 0;
	}

	void RegisterGenerator::generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral) {
		const auto& elements = arrayLiteral->elements();
		size_t numElements = elements.size();

		auto* ptrTy = llvm::PointerType::getUnqual(*context);
		auto* i64Ty = builder->getInt64Ty();
		auto* i32Ty = builder->getInt32Ty();

		// Get array functions
		auto* createArrayFn = module->getFunction("qd_array_create");
		auto* pushIntArrFn = module->getFunction("qd_array_push_int");
		auto* pushFloatArrFn = module->getFunction("qd_array_push_float");
		auto* pushPtrArrFn = module->getFunction("qd_array_push_ptr");

		if (numElements == 0) {
			// Empty array - create with default type (INT)
			auto* arrPtr = builder->CreateCall(createArrayFn, {builder->getInt64(8), builder->getInt32(0)}, "empty_arr");
			valueStack.push_back(TrackedValue(arrPtr, ValueType::PTR, "", true));
			return;
		}

		// Determine array element type from first element
		// QD_ARRAY_TYPE_INT = 0, QD_ARRAY_TYPE_FLOAT = 1, QD_ARRAY_TYPE_STR = 2, QD_ARRAY_TYPE_PTR = 3
		int32_t arrayType = 0; // Default to INT
		IAstNode* firstElem = elements[0];
		if (firstElem->type() == IAstNode::Type::LITERAL) {
			auto* lit = static_cast<AstNodeLiteral*>(firstElem);
			if (lit->literalType() == AstNodeLiteral::LiteralType::FLOAT) {
				arrayType = 1; // FLOAT
			} else if (lit->literalType() == AstNodeLiteral::LiteralType::STRING) {
				arrayType = 2; // STR
			}
		}

		// Create array with initial capacity
		auto* arrPtr = builder->CreateCall(createArrayFn,
				{builder->getInt64(static_cast<uint64_t>(numElements)), builder->getInt32(static_cast<uint32_t>(arrayType))},
				"arr_ptr");

		// Push elements to the array
		for (IAstNode* elem : elements) {
			if (elem->type() == IAstNode::Type::LITERAL) {
				auto* lit = static_cast<AstNodeLiteral*>(elem);
				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					int64_t val = 0;
					auto [ptr, ec] =
							std::from_chars(lit->value().data(), lit->value().data() + lit->value().size(), val);
					if (ec == std::errc()) {
						if (arrayType == 1) {
							// Coerce int to float
							builder->CreateCall(pushFloatArrFn,
									{arrPtr, llvm::ConstantFP::get(builder->getDoubleTy(), static_cast<double>(val))});
						} else {
							builder->CreateCall(pushIntArrFn, {arrPtr, builder->getInt64(static_cast<uint64_t>(val))});
						}
					}
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::FLOAT) {
					double val = std::stod(lit->value());
					if (arrayType == 0) {
						// Coerce float to int for int array
						builder->CreateCall(pushIntArrFn,
								{arrPtr, builder->getInt64(static_cast<uint64_t>(static_cast<int64_t>(val)))});
					} else {
						builder->CreateCall(
								pushFloatArrFn, {arrPtr, llvm::ConstantFP::get(builder->getDoubleTy(), val)});
					}
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::STRING) {
					// Create qd_string and push
					std::string strVal = lit->value();
					// Remove quotes if present
					if (strVal.size() >= 2 && strVal.front() == '"' && strVal.back() == '"') {
						strVal = strVal.substr(1, strVal.size() - 2);
					}
					auto* createStrFn = module->getFunction("qd_string_create");
					auto* strConstant = builder->CreateGlobalString(strVal, "arr_str");
					auto* qdStr = builder->CreateCall(createStrFn, {strConstant}, "qd_str");
					builder->CreateCall(pushPtrArrFn, {arrPtr, qdStr});
					// Release our reference since array now owns it
					builder->CreateCall(qdStringReleaseFn, {qdStr});
				}
			}
		}

		// Push array pointer onto the value stack with type info
		std::string arrayStructType;
		switch (arrayType) {
			case 0:
				arrayStructType = "array_int";
				break;
			case 1:
				arrayStructType = "array_float";
				break;
			case 2:
				arrayStructType = "array_str";
				break;
			case 3:
				arrayStructType = "array_ptr";
				break;
		}
		valueStack.push_back(TrackedValue(arrPtr, ValueType::PTR, arrayStructType, true));

		(void)ptrTy;
		(void)i64Ty;
		(void)i32Ty;
	}

	// Configuration methods
	void RegisterGenerator::setDebugInfo(bool enabled) { debugInfoEnabled = enabled; }

	void RegisterGenerator::setOptimizationLevel(int level) { optimizationLevel = level; }

	void RegisterGenerator::addLibrarySearchPath(const std::string& path) { librarySearchPaths.push_back(path); }

	void RegisterGenerator::setTestMode(bool enabled) { testMode = enabled; }

	void RegisterGenerator::setStackSize(size_t /* size */) {
		// No-op: register-based generator doesn't use a runtime stack
	}

} // namespace Qd
