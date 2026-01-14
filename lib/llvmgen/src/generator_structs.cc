#include "generator_impl.h"

namespace Qd {

	bool LlvmGenerator::Impl::isKnownStruct(const std::string& typeName) {
		return findStructDefinition(typeName) != nullptr;
	}

	bool looksLikeStructType(const std::string& typeStr) {
		if (typeStr.empty()) {
			return false;
		}
		// Check for qualified name (module::StructName)
		size_t colonPos = typeStr.find("::");
		if (colonPos != std::string::npos) {
			std::string structPart = typeStr.substr(colonPos + 2);
			return !structPart.empty() && std::isupper(structPart[0]);
		}
		// Unqualified name - check first character
		return std::isupper(typeStr[0]);
	}

	// Helper function to get struct name from type string (preserves qualified names)
	// "vec2::Vec2" -> "vec2::Vec2", "Response" -> "Response"
	std::string extractStructName(const std::string& typeStr) {
		// Return the full type string, including module prefix if present
		// This ensures qualified types like "vec2::Vec2" are preserved for proper lookup
		// in structDefinitions which uses qualified names as keys
		return typeStr;
	}

	size_t LlvmGenerator::Impl::getTypeSize(const std::string& typeName) {
		if (typeName == "i64" || typeName == "f64") {
			return 8;
		} else if (typeName == "i32" || typeName == "f32") {
			return 4;
		} else if (typeName == "str" || typeName.find('*') != std::string::npos) {
			return 8; // Pointer size
		} else if (!typeName.empty() && std::isupper(typeName[0]) && isKnownStruct(typeName)) {
			// Struct-typed field - stored as pointer, not inline
			return 8; // Pointer size
		}
		return 8; // Default to pointer size (also handles type parameters like T, U)
	}

	void LlvmGenerator::Impl::processStructDeclaration(
			AstNodeStructDeclaration* structDecl, const std::string& moduleName) {
		StructLayout layout;
		// Use qualified name for module structs to avoid collisions
		if (!moduleName.empty()) {
			layout.name = moduleName + "::" + structDecl->name();
		} else {
			layout.name = structDecl->name();
		}
		layout.isPublic = structDecl->isPublic();
		layout.totalSize = 0;

		// Calculate field offsets
		for (const auto* field : structDecl->fields()) {
			FieldInfo fieldInfo;
			fieldInfo.name = field->name();
			fieldInfo.typeName = field->typeName();
			fieldInfo.offset = layout.totalSize;
			fieldInfo.size = getTypeSize(field->typeName());

			// Copy default value nodes (we don't own them, just reference)
			if (field->hasDefaultValue()) {
				for (auto* node : field->defaultValue()) {
					fieldInfo.defaultValue.push_back(node);
				}
			}

			// Add field to layout
			layout.fields.push_back(fieldInfo);

			// Update total size with alignment (8-byte alignment)
			layout.totalSize += fieldInfo.size;
			if (layout.totalSize % 8 != 0) {
				layout.totalSize = (layout.totalSize + 7) & ~static_cast<size_t>(7); // Round up to next 8-byte boundary
			}
		}

		// Store struct definition
		structDefinitions[layout.name] = layout;
	}

	const LlvmGenerator::Impl::StructLayout* LlvmGenerator::Impl::findStructDefinition(
			const std::string& structName) const {
		// If the name is already qualified (contains "::"), try direct lookup first
		if (structName.find("::") != std::string::npos) {
			auto it = structDefinitions.find(structName);
			if (it != structDefinitions.end()) {
				return &it->second;
			}
			return nullptr; // Qualified name not found
		}

		// Unqualified name - if we're in a module context, first try with module prefix
		// This ensures module's own structs are found before local (main file) structs
		if (!currentModuleName.empty()) {
			std::string qualifiedName = currentModuleName + "::" + structName;
			auto qualIt = structDefinitions.find(qualifiedName);
			if (qualIt != structDefinitions.end()) {
				return &qualIt->second;
			}
		}

		// Try direct lookup (for local/main file structs)
		auto it = structDefinitions.find(structName);
		if (it != structDefinitions.end()) {
			return &it->second;
		}

		// Fallback: search all modules for this unqualified struct name
		for (const auto& pair : structDefinitions) {
			size_t colonPos = pair.first.find("::");
			if (colonPos != std::string::npos) {
				std::string unqualified = pair.first.substr(colonPos + 2);
				if (unqualified == structName) {
					return &pair.second;
				}
			}
		}

		return nullptr;
	}

	void LlvmGenerator::Impl::generateStructCleanup(llvm::Value* structPtr, const std::string& structTypeName) {
		const StructLayout* layoutPtr = findStructDefinition(structTypeName);
		if (layoutPtr == nullptr) {
			return;
		}

		const StructLayout& layout = *layoutPtr;

		// First, recursively cleanup nested struct fields
		for (const auto& field : layout.fields) {
			// Check if field is a known struct type (not a type parameter like T)
			if (!field.typeName.empty() && std::isupper(field.typeName[0]) && isKnownStruct(field.typeName)) {
				// Load the nested struct pointer from this field
				auto fieldOffset = builder->getInt64(field.offset);
				auto fieldBytePtr =
						builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "nested_field_ptr");
				llvm::Value* nestedStructPtr =
						builder->CreateLoad(ptrTy, fieldBytePtr, "nested_struct_ptr");

				// Recursively cleanup the nested struct
				generateStructCleanup(nestedStructPtr, field.typeName);

				// Free the nested struct memory
				builder->CreateCall(this->freeFn, {nestedStructPtr});
			}
		}

		// Then, release string fields
		for (const auto& field : layout.fields) {
			if (field.typeName == "str") {
				// Calculate field offset and load string pointer
				auto fieldOffset = builder->getInt64(field.offset);
				auto fieldBytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "str_field_ptr");
				llvm::Value* stringPtr =
						builder->CreateLoad(ptrTy, fieldBytePtr, "string_ptr");

				// Call qd_string_release() on the string
				builder->CreateCall(qdStringReleaseFn, {stringPtr});
			}
		}
	}

	void LlvmGenerator::Impl::generateStructDestructors() {
		// Destructor function type: void (*)(void*)
		auto destructorFnTy =
				llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);

		for (const auto& [structName, layout] : structDefinitions) {
			// Check if this struct has any fields that need cleanup
			bool needsDestructor = false;
			for (const auto& field : layout.fields) {
				// Check for struct type (Inner) or pointer to struct (*Inner)
				std::string baseTypeName = field.typeName;
				if (!baseTypeName.empty() && baseTypeName[0] == '*') {
					baseTypeName = baseTypeName.substr(1);
				}
				if (field.typeName == "str" ||
						(!baseTypeName.empty() && std::isupper(baseTypeName[0]) && isKnownStruct(baseTypeName))) {
					needsDestructor = true;
					break;
				}
			}

			if (!needsDestructor) {
				// No destructor needed - will pass nullptr to qd_struct_alloc
				structDestructors[structName] = nullptr;
				continue;
			}

			// Generate destructor function
			std::string dtorName = "__qd_dtor_" + structName;
			auto dtorFn = llvm::Function::Create(destructorFnTy, llvm::Function::InternalLinkage, dtorName, *module);

			// Save current insertion point
			auto savedBlock = builder->GetInsertBlock();
			auto savedPoint = builder->GetInsertPoint();

			// Create entry block for destructor
			auto entryBlock = llvm::BasicBlock::Create(*context, "entry", dtorFn);
			builder->SetInsertPoint(entryBlock);

			// Get struct pointer argument
			llvm::Value* structPtr = dtorFn->getArg(0);

			// Release nested struct fields first (call qd_struct_release)
			for (const auto& field : layout.fields) {
				// Check for struct type (Inner) or pointer to struct (*Inner)
				std::string baseTypeName = field.typeName;
				if (!baseTypeName.empty() && baseTypeName[0] == '*') {
					baseTypeName = baseTypeName.substr(1);
				}
				if (!baseTypeName.empty() && std::isupper(baseTypeName[0]) && isKnownStruct(baseTypeName)) {
					// Nested struct field
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "nested_field_ptr");
					llvm::Value* nestedStructPtr = builder->CreateLoad(
							ptrTy, fieldBytePtr, "nested_struct_ptr");
					builder->CreateCall(qdStructReleaseFn, {nestedStructPtr});
				}
			}

			// Release string fields
			for (const auto& field : layout.fields) {
				if (field.typeName == "str") {
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "str_field_ptr");
					llvm::Value* stringPtr =
							builder->CreateLoad(ptrTy, fieldBytePtr, "string_ptr");
					builder->CreateCall(qdStringReleaseFn, {stringPtr});
				}
			}

			builder->CreateRetVoid();

			// Restore insertion point
			if (savedBlock) {
				builder->SetInsertPoint(savedBlock, savedPoint);
			}

			structDestructors[structName] = dtorFn;
		}
	}

	void LlvmGenerator::Impl::generateStructConstruction(const std::string& structName, llvm::Value* ctx) {
		const StructLayout* layoutPtr = findStructDefinition(structName);
		if (layoutPtr == nullptr) {
			std::cerr << "Error: Unknown struct type: " << structName << std::endl;
			return;
		}

		const StructLayout& layout = *layoutPtr;


		// Allocate struct - use stack if function doesn't return pointer, heap otherwise
		llvm::Value* structPtr = nullptr;

		if (!currentFunctionReturnsPtr) {
			// Stack allocation - struct lives only within this function
			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::IRBuilder<> entryBuilder(&currentFn->getEntryBlock(), currentFn->getEntryBlock().begin());
			auto structAlloca = entryBuilder.CreateAlloca(
					llvm::ArrayType::get(builder->getInt8Ty(), layout.totalSize), nullptr, structName + "_stack");
			structPtr = builder->CreateBitCast(structAlloca, ptrTy, "struct_ptr");
		} else {
			// Heap allocation - struct may be returned, needs refcounting
			llvm::Value* destructorPtr = nullptr;
			auto destructorIt = structDestructors.find(structName);
			if (destructorIt != structDestructors.end() && destructorIt->second != nullptr) {
				destructorPtr = destructorIt->second;
			} else {
				destructorPtr = llvm::ConstantPointerNull::get(ptrTy);
			}
			structPtr = builder->CreateCall(
					qdStructAllocFn, {builder->getInt64(layout.totalSize), destructorPtr}, "struct_ptr");
		}

		// Pop values from stack in reverse order and write to struct fields
		for (auto fieldIt = layout.fields.rbegin(); fieldIt != layout.fields.rend(); ++fieldIt) {
			const FieldInfo& field = *fieldIt;

			// Calculate field pointer
			auto fieldOffset = builder->getInt64(field.offset);
			auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

			// Pop value from stack
			llvm::Value* stackPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
			llvm::Value* st = builder->CreateLoad(ptrTy, stackPtr, "st");

			// Get stack size
			llvm::Value* sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
			llvm::Value* size = builder->CreateLoad(int64Ty, sizePtr, "size");

			// Decrement size
			llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
			builder->CreateStore(newSize, sizePtr);

			// Get element pointer
			llvm::Value* dataPtr = builder->CreateStructGEP(stackStructTy, st, 0, "data_ptr");
			llvm::Value* data = builder->CreateLoad(ptrTy, dataPtr, "data");
			llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, newSize, "elem_ptr");

			// Load value from stack element
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");

			// Store to struct field based on type
			if (field.typeName == "f64") {
				llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "float_val");
				builder->CreateStore(floatValue, bytePtr);
			} else if (field.typeName == "i64") {
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
				builder->CreateStore(intValue, bytePtr);
			} else if (field.typeName == "i32") {
				// Load as i64 from stack (stack elements are always 64-bit), truncate to i32
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
				llvm::Value* truncValue = builder->CreateTrunc(intValue, int32Ty, "int32_val");
				builder->CreateStore(truncValue, bytePtr);
			} else if (field.typeName == "ptr" || field.typeName == "str" ||
					   field.typeName.find('*') != std::string::npos ||
					   (!field.typeName.empty() && std::isupper(field.typeName[0]) && isKnownStruct(field.typeName))) {
				// Pointer type (including ptr, str, raw pointers, and struct-typed fields)
				llvm::Value* ptrValue =
						builder->CreateLoad(ptrTy, valuePtr, "ptr_val");
				// NOTE: We do NOT retain nested struct fields here.
				// Retain happens when pushing a local to the stack (in generateIdentifier).
				// The containing struct's destructor will release nested structs.
				builder->CreateStore(ptrValue, bytePtr);
			} else {
				// Unknown type (including type parameters like T) - treat as i64
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "generic_val");
				builder->CreateStore(intValue, bytePtr);
			}
		}

		// Push struct pointer onto stack
		builder->CreateCall(pushPtrFn, {ctx, structPtr});

		// Track that we just constructed this struct type
		lastStructConstructed = structName;
	}

	void LlvmGenerator::Impl::generateFieldAccess(AstNodeFieldAccess* fieldAccess, llvm::Value* ctx) {
		const std::string& varName = fieldAccess->varName();
		const std::string& fieldName = fieldAccess->fieldName();

		// Special handling for global error access: error @code or error @message
		if (varName == "__global_error__") {
			if (fieldName == "code") {
				// Access ctx->error_code (offset 1) and push as int
				llvm::Value* errorCodePtr = builder->CreateStructGEP(contextStructTy, ctx, 1, "error_code_ptr");
				llvm::Value* errorCode = builder->CreateLoad(int64Ty, errorCodePtr, "error_code");

				// Push error code onto stack (use class member pushIntFn)
				if (!pushIntFn) {
					auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, int64Ty}, false);
					pushIntFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_push_i", *module);
				}
				builder->CreateCall(pushIntFn, {ctx, errorCode});
			} else if (fieldName == "message") {
				// Access ctx->error_msg (offset 2) and push as string
				llvm::Value* errorMsgPtr = builder->CreateStructGEP(contextStructTy, ctx, 2, "error_msg_ptr");
				llvm::Value* errorMsg =
						builder->CreateLoad(ptrTy, errorMsgPtr, "error_msg");

				// If error_msg is NULL, use empty string
				llvm::Value* isNull = builder->CreateICmpEQ(
						errorMsg, llvm::ConstantPointerNull::get(ptrTy));
				llvm::Value* emptyStr = builder->CreateGlobalString("", "empty_str");
				llvm::Value* msgToUse = builder->CreateSelect(isNull, emptyStr, errorMsg, "msg_to_use");

				// Push error message onto stack using qd_push_str_cstr (use class member pushStrFn)
				if (!pushStrFn) {
					auto fnTy = llvm::FunctionType::get(
							execResultTy, {contextPtrTy, ptrTy}, false);
					pushStrFn =
							llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_push_str_cstr", *module);
				}
				builder->CreateCall(pushStrFn, {ctx, msgToUse});
			}
			return;
		}

		llvm::Value* structPtr = nullptr;
		std::string structTypeName;
		bool needsReleaseAfterAccess = false; // Track if we need to release the struct after field access

		if (varName.empty()) {
			// Stack-based field access - could be:
			// 1. Chained field access (p @origin @x) - use lastFieldAccessResultType
			// 2. Direct access after struct construction (Point @x) - use lastStructConstructed
			if (!lastFieldAccessResultType.empty()) {
				structTypeName = lastFieldAccessResultType;
			} else if (!lastStructConstructed.empty()) {
				structTypeName = lastStructConstructed;
				lastStructConstructed.clear(); // Consume it
			}

			// Pop struct pointer from stack
			llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
			llvm::Value* stackPtr = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");

			// Allocate temp for popped element
			llvm::Value* tempElem = builder->CreateAlloca(stackElementTy, nullptr, "temp_elem");
			builder->CreateCall(stackPopFn, {stackPtr, tempElem});

			// Load the pointer value from the element
			llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
			structPtr = builder->CreateLoad(ptrTy, valuePtr, "struct_ptr");

			// The struct pointer was retained when pushed to stack, needs release after access
			needsReleaseAfterAccess = true;
		} else {
			// Check if varName is actually a struct type (e.g., Point @x after 1 2 Point @x)
			// The parser created AstNodeFieldAccess("Point", "x") but Point is a struct, not a variable
			auto structDefIt = structDefinitions.find(varName);
			if (structDefIt != structDefinitions.end()) {
				// varName is a struct type - generate struct construction first, then stack-based access
				generateStructConstruction(varName, ctx);
				structTypeName = varName;

				// Pop the just-constructed struct pointer from stack
				llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
				llvm::Value* stackPtr =
						builder->CreateLoad(ptrTy, stackPtrPtr, "stack");

				llvm::Value* tempElem = builder->CreateAlloca(stackElementTy, nullptr, "temp_elem");
				builder->CreateCall(stackPopFn, {stackPtr, tempElem});

				llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
				structPtr = builder->CreateLoad(ptrTy, valuePtr, "struct_ptr");
			} else {
				// Check if varName is a function - call it first, then do stack-based field access
				auto funcIt = userFunctions.find(varName);
				std::string funcLookupName = varName;
				if (funcIt == userFunctions.end() && currentModulePrefix != "main") {
					// Try with current module prefix
					std::string qualifiedName = currentModulePrefix + "::" + varName;
					funcIt = userFunctions.find(qualifiedName);
					if (funcIt != userFunctions.end()) {
						funcLookupName = qualifiedName;
					}
				}

				if (funcIt != userFunctions.end()) {
					// varName is a function - call it first
					builder->CreateCall(funcIt->second, {ctx});

					// Pop the result (struct pointer) from stack
					llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
					llvm::Value* stackPtr =
							builder->CreateLoad(ptrTy, stackPtrPtr, "stack");

					llvm::Value* tempElem = builder->CreateAlloca(stackElementTy, nullptr, "temp_elem");
					builder->CreateCall(stackPopFn, {stackPtr, tempElem});

					llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
					structPtr = builder->CreateLoad(ptrTy, valuePtr, "struct_ptr");

					// Look up the return struct type from function signature
					auto returnTypeIt = functionReturnStructType.find(funcLookupName);
					if (returnTypeIt != functionReturnStructType.end()) {
						structTypeName = returnTypeIt->second;
					}
				} else {
					// Check if it's a captured variable (by reference)
					auto capIt = capturedVariableRefs.find(varName);
					if (capIt != capturedVariableRefs.end()) {
						// Load the pointer to the outer variable, then access the value
						llvm::AllocaInst* ptrAlloca = capIt->second;
						llvm::Value* outerVarPtr = builder->CreateLoad(
								ptrTy, ptrAlloca, varName + "_cap_ptr");

						// Look up the struct type from local variable tracking
						auto typeIt = localVariableStructTypes.find(varName);
						if (typeIt != localVariableStructTypes.end()) {
							structTypeName = typeIt->second;
						}

						// Extract the value field from the outer variable (qd_stack_element_t)
						llvm::Value* valuePtr =
								builder->CreateStructGEP(stackElementTy, outerVarPtr, 0, varName + "_cap_value_ptr");
						structPtr = builder->CreateLoad(ptrTy, valuePtr, "struct_ptr");
					} else {
						// Normal field access from local variable
						auto it = localVariables.find(varName);
						if (it == localVariables.end()) {
							std::cerr << "Error: Undefined variable: " << varName << std::endl;
							return;
						}

						// Look up the struct type from local variable tracking
						auto typeIt = localVariableStructTypes.find(varName);
						if (typeIt != localVariableStructTypes.end()) {
							structTypeName = typeIt->second;
						}

						// Local variables are stored as qd_stack_element_t, need to extract the value field
						llvm::Value* structPtrAlloca = it->second;
						// For indirect (captured) variables, load the actual storage pointer first
						if (indirectLocalVariables.find(varName) != indirectLocalVariables.end()) {
							structPtrAlloca = builder->CreateLoad(
									ptrTy, it->second, varName + "_storage");
						}
						llvm::Value* valuePtr =
								builder->CreateStructGEP(stackElementTy, structPtrAlloca, 0, varName + "_value_ptr");
						structPtr = builder->CreateLoad(ptrTy, valuePtr, "struct_ptr");
					}
				}
			}
		}

		// Find the field in the specific struct type if known
		const FieldInfo* matchingField = nullptr;

		if (!structTypeName.empty()) {
			// Look up field in the specific struct type (use findStructDefinition for proper module handling)
			const StructLayout* layoutPtr = findStructDefinition(structTypeName);
			if (layoutPtr != nullptr) {
				for (const auto& field : layoutPtr->fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
			}
		}

		// Fallback: search all struct types if we don't know the type
		if (!matchingField) {
			for (const auto& pair : structDefinitions) {
				for (const auto& field : pair.second.fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
				if (matchingField) {
					break;
				}
			}
		}

		if (!matchingField) {
			std::cerr << "Error: Unknown field: " << fieldName << std::endl;
			return;
		}

		// Calculate field offset
		auto fieldOffset = builder->getInt64(matchingField->offset);
		auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

		// Load value from field based on type and update lastFieldAccessResultType for chaining
		if (matchingField->typeName == "f64") {
			llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), bytePtr, "field_value");
			builder->CreateCall(pushFloatFn, {ctx, floatValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "i64") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, bytePtr, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "i32") {
			// Load i32, sign-extend to i64 for the stack
			llvm::Value* int32Value = builder->CreateLoad(int32Ty, bytePtr, "field_value_i32");
			llvm::Value* intValue = builder->CreateSExt(int32Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "str") {
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue =
					builder->CreateLoad(ptrTy, fieldPtr, "field_value");
			builder->CreateCall(pushStrRefFn, {ctx, ptrValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "ptr" || matchingField->typeName.find('*') != std::string::npos) {
			// Handle ptr type and raw pointer types
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue =
					builder->CreateLoad(ptrTy, fieldPtr, "field_value");
			// Retain the pointer before pushing (it could be an array/struct that will be released after use)
			builder->CreateCall(qdPtrRetainFn, {ptrValue});
			builder->CreateCall(pushPtrFn, {ctx, ptrValue});
			lastFieldAccessResultType.clear(); // Raw pointer, not a known struct type
		} else if (!matchingField->typeName.empty() && std::isupper(matchingField->typeName[0]) &&
				   isKnownStruct(matchingField->typeName)) {
			// Struct-typed field - stored as pointer, push as PTR
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue =
					builder->CreateLoad(ptrTy, fieldPtr, "field_value");
			// Retain the struct pointer before pushing
			builder->CreateCall(qdPtrRetainFn, {ptrValue});
			builder->CreateCall(pushPtrFn, {ctx, ptrValue});
			// Track the struct type for chained field access
			lastFieldAccessResultType = matchingField->typeName;
		} else {
			// Type parameter or unknown type - treat as i64 value
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* intValue = builder->CreateLoad(int64Ty, fieldPtr, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		}

		// Release the struct pointer if it was popped from stack (was retained when pushed)
		if (needsReleaseAfterAccess) {
			builder->CreateCall(qdPtrReleaseFn, {structPtr});
		}
	}

	void LlvmGenerator::Impl::generateFieldSet(AstNodeFieldSet* fieldSet, llvm::Value* ctx) {
		const std::string& varName = fieldSet->varName();
		const std::string& fieldName = fieldSet->fieldName();

		llvm::Value* structPtr = nullptr;
		std::string structTypeName;

		// Check if it's a captured variable (by reference)
		auto capIt = capturedVariableRefs.find(varName);
		if (capIt != capturedVariableRefs.end()) {
			// Load the pointer to the outer variable, then access the value
			llvm::AllocaInst* ptrAlloca = capIt->second;
			llvm::Value* outerVarPtr =
					builder->CreateLoad(ptrTy, ptrAlloca, varName + "_cap_ptr");

			// Look up the struct type from local variable tracking
			auto typeIt = localVariableStructTypes.find(varName);
			if (typeIt != localVariableStructTypes.end()) {
				structTypeName = typeIt->second;
			}

			// Extract the value field from the outer variable (qd_stack_element_t)
			llvm::Value* structValuePtr =
					builder->CreateStructGEP(stackElementTy, outerVarPtr, 0, varName + "_cap_value_ptr");
			structPtr = builder->CreateLoad(ptrTy, structValuePtr, "struct_ptr");
		} else {
			// Get struct pointer from local variable
			auto it = localVariables.find(varName);
			if (it == localVariables.end()) {
				std::cerr << "Error: Undefined variable in field set: " << varName << std::endl;
				return;
			}

			// Look up the struct type from local variable tracking
			auto typeIt = localVariableStructTypes.find(varName);
			if (typeIt != localVariableStructTypes.end()) {
				structTypeName = typeIt->second;
			}

			// Local variables are stored as qd_stack_element_t, need to extract the value field
			llvm::Value* structPtrAlloca = it->second;
			// For indirect (captured) variables, load the actual storage pointer first
			if (indirectLocalVariables.find(varName) != indirectLocalVariables.end()) {
				structPtrAlloca =
						builder->CreateLoad(ptrTy, it->second, varName + "_storage");
			}
			llvm::Value* structValuePtr =
					builder->CreateStructGEP(stackElementTy, structPtrAlloca, 0, varName + "_value_ptr");
			structPtr = builder->CreateLoad(ptrTy, structValuePtr, "struct_ptr");
		}

		// Find the field in the specific struct type if known
		const FieldInfo* matchingField = nullptr;

		if (!structTypeName.empty()) {
			// Look up field in the specific struct type (use findStructDefinition for proper module handling)
			const StructLayout* layoutPtr = findStructDefinition(structTypeName);
			if (layoutPtr != nullptr) {
				for (const auto& field : layoutPtr->fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
			}
		}

		// Fallback: search all struct types if we don't know the type
		if (!matchingField) {
			for (const auto& pair : structDefinitions) {
				for (const auto& field : pair.second.fields) {
					if (field.name == fieldName) {
						matchingField = &field;
						break;
					}
				}
				if (matchingField) {
					break;
				}
			}
		}

		if (!matchingField) {
			std::cerr << "Error: Unknown field in field set: " << fieldName << std::endl;
			return;
		}

		// Calculate field offset
		auto fieldOffset = builder->getInt64(matchingField->offset);
		auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

		// Pop value from stack
		llvm::Value* stackPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "st_ptr");
		llvm::Value* st = builder->CreateLoad(ptrTy, stackPtr, "st");

		// Get stack size
		llvm::Value* sizePtr = builder->CreateStructGEP(stackStructTy, st, 2, "size_ptr");
		llvm::Value* size = builder->CreateLoad(int64Ty, sizePtr, "size");

		// Decrement size
		llvm::Value* newSize = builder->CreateSub(size, builder->getInt64(1), "new_size");
		builder->CreateStore(newSize, sizePtr);

		// Get element pointer
		llvm::Value* dataPtr = builder->CreateStructGEP(stackStructTy, st, 0, "data_ptr");
		llvm::Value* data = builder->CreateLoad(ptrTy, dataPtr, "data");
		llvm::Value* elemPtr = builder->CreateGEP(stackElementTy, data, newSize, "elem_ptr");

		// Load value from stack element
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, elemPtr, 0, "value_ptr");

		// Store to struct field based on type
		if (matchingField->typeName == "f64") {
			llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "float_val");
			builder->CreateStore(floatValue, bytePtr);
		} else if (matchingField->typeName == "i64") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
			builder->CreateStore(intValue, bytePtr);
		} else if (matchingField->typeName == "i32") {
			// Load as i64 from stack (stack elements are always 64-bit), truncate to i32
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
			llvm::Value* truncValue = builder->CreateTrunc(intValue, int32Ty, "int32_val");
			builder->CreateStore(truncValue, bytePtr);
		} else if (matchingField->typeName == "ptr" || matchingField->typeName == "str" ||
				   matchingField->typeName.find('*') != std::string::npos ||
				   (!matchingField->typeName.empty() && std::isupper(matchingField->typeName[0]) &&
						   isKnownStruct(matchingField->typeName))) {
			// Pointer type (including ptr, str, raw pointers, and struct-typed fields)
			llvm::Value* ptrValue = builder->CreateLoad(ptrTy, valuePtr, "ptr_val");
			builder->CreateStore(ptrValue, bytePtr);
		} else {
			// Type parameter or unknown type - treat as i64 value
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "generic_val");
			builder->CreateStore(intValue, bytePtr);
		}
	}

	void LlvmGenerator::Impl::generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral, llvm::Value* ctx) {
		const auto& elements = arrayLiteral->elements();
		size_t numElements = elements.size();

		if (numElements == 0) {
			// Empty array - create with default type (INT)
			llvm::Function* createArrayFn = module->getFunction("qd_array_create");
			if (!createArrayFn) {
				auto fnTy = llvm::FunctionType::get(
						ptrTy, {int64Ty, int32Ty}, false);
				createArrayFn =
						llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
			}
			llvm::Value* arrPtr =
					builder->CreateCall(createArrayFn, {builder->getInt64(8), builder->getInt32(0)}, "empty_arr");
			builder->CreateCall(pushPtrFn, {ctx, arrPtr});
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

		// Declare array functions if not already declared
		llvm::Function* createArrayFn = module->getFunction("qd_array_create");
		if (!createArrayFn) {
			auto fnTy = llvm::FunctionType::get(
					ptrTy, {int64Ty, int32Ty}, false);
			createArrayFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
		}

		llvm::Function* pushIntArrFn = module->getFunction("qd_array_push_int");
		if (!pushIntArrFn) {
			auto fnTy = llvm::FunctionType::get(
					int32Ty, {ptrTy, int64Ty}, false);
			pushIntArrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_int", *module);
		}

		llvm::Function* pushFloatArrFn = module->getFunction("qd_array_push_float");
		if (!pushFloatArrFn) {
			auto fnTy = llvm::FunctionType::get(
					int32Ty, {ptrTy, builder->getDoubleTy()}, false);
			pushFloatArrFn =
					llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_float", *module);
		}

		llvm::Function* pushPtrArrFn = module->getFunction("qd_array_push_ptr");
		if (!pushPtrArrFn) {
			auto fnTy = llvm::FunctionType::get(int32Ty,
					{ptrTy, ptrTy}, false);
			pushPtrArrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_ptr", *module);
		}

		// Create array with initial capacity
		llvm::Value* arrPtr = builder->CreateCall(createArrayFn,
				{builder->getInt64(numElements), builder->getInt32(static_cast<uint32_t>(arrayType))}, "arr_ptr");

		// Push elements to the array
		for (IAstNode* elem : elements) {
			if (elem->type() == IAstNode::Type::LITERAL) {
				auto* lit = static_cast<AstNodeLiteral*>(elem);
				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					int64_t val = 0;
					safeParseInt64(lit->value(), val);
					if (arrayType == 1) {
						// Coerce int to float
						builder->CreateCall(pushFloatArrFn,
								{arrPtr, llvm::ConstantFP::get(builder->getDoubleTy(), static_cast<double>(val))});
					} else {
						builder->CreateCall(pushIntArrFn, {arrPtr, builder->getInt64(static_cast<uint64_t>(val))});
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
					// Create string constant
					std::string strVal = lit->value();
					// Remove quotes if present
					if (strVal.size() >= 2 && strVal.front() == '"' && strVal.back() == '"') {
						strVal = strVal.substr(1, strVal.size() - 2);
					}
					// Create qd_string and push
					llvm::Function* createStrFn = module->getFunction("qd_string_create");
					if (!createStrFn) {
						auto fnTy = llvm::FunctionType::get(ptrTy,
								{ptrTy}, false);
						createStrFn = llvm::Function::Create(
								fnTy, llvm::Function::ExternalLinkage, "qd_string_create", *module);
					}
					llvm::Value* strConstant = builder->CreateGlobalString(strVal, "arr_str");
					llvm::Value* qdStr = builder->CreateCall(createStrFn, {strConstant}, "qd_str");
					builder->CreateCall(pushPtrArrFn, {arrPtr, qdStr});
					// Release our reference since array now owns it
					builder->CreateCall(qdStringReleaseFn, {qdStr});
				}
			}
		}

		// Push array pointer onto the Quadrate stack
		builder->CreateCall(pushPtrFn, {ctx, arrPtr});

		// Mark that the last pushed value was an array
		lastPushedWasArray = true;
	}

	void LlvmGenerator::Impl::pushDeferScope() {
		deferScopeStack.push_back(std::vector<AstNodeDefer*>());
	}

	void LlvmGenerator::Impl::popDeferScope() {
		if (!deferScopeStack.empty()) {
			deferScopeStack.pop_back();
		}
	}

	void LlvmGenerator::Impl::executeDeferScope(llvm::Value* ctx) {
		if (deferScopeStack.empty()) {
			return;
		}

		auto& currentScope = deferScopeStack.back();
		// Execute defers in REVERSE order (LIFO)
		for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
			AstNodeDefer* deferNode = *it;
			// Generate defer body
			for (auto* child : deferNode->children()) {
				
				// If the child is a block, generate its children directly
				if (child && child->type() == IAstNode::Type::BLOCK) {
					for (auto* innerChild : child->children()) {
						generateNode(innerChild, ctx);
					}
				} else {
					generateNode(child, ctx);
				}
			}
		}

		deferScopeStack.pop_back();
	}

	std::string LlvmGenerator::Impl::findLastStructConstruction(IAstNode* node) {
		if (!node) {
			return "";
		}

		std::string result;

		// Check if this node is a struct construction (identifier that's a struct name)
		if (auto* ident = dynamic_cast<AstNodeIdentifier*>(node)) {
			if (structDefinitions.find(ident->name()) != structDefinitions.end()) {
				result = ident->name();
			}
		}

		// Check if this node is an explicit struct construction ('new StructName')
		if (auto* construct = dynamic_cast<AstNodeStructConstruction*>(node)) {
			result = construct->structName();
		}

		// Recursively search children
		for (auto* child : node->children()) {
			std::string childResult = findLastStructConstruction(child);
			if (!childResult.empty()) {
				result = childResult; // Keep the last one found
			}
		}

		return result;
	}

} // namespace Qd
