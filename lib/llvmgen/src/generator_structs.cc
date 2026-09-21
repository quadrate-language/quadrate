#include "generator_impl.h"

namespace Qd {

	bool LlvmGenerator::Impl::isKnownStruct(const std::string& typeName) {
		return findStructDefinition(typeName) != nullptr;
	}

	bool isFnType(const std::string& typeStr) {
		return typeStr.size() > 3 && typeStr[0] == 'f' && typeStr[1] == 'n' && typeStr[2] == '(';
	}

	bool isArrayType(const std::string& typeStr) {
		return typeStr.size() > 2 && typeStr[0] == '[' && typeStr[1] == ']';
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
		if (typeName == "i64" || typeName == "u64" || typeName == "f64") {
			return 8;
		} else if (typeName == "i32" || typeName == "u32" || typeName == "f32") {
			return 4;
		} else if (typeName == "i16" || typeName == "u16") {
			return 2;
		} else if (typeName == "i8" || typeName == "u8") {
			return 1;
		} else if (typeName == "str" || typeName.find('*') != std::string::npos) {
			return 8; // Pointer size
		} else if (looksLikeStructType(typeName) && isKnownStruct(typeName)) {
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
		layout.isPacked = structDecl->isPacked();
		layout.totalSize = 0;

		// Calculate field offsets.
		// Default (un-packed) layout rounds each field up to an 8-byte slot —
		// a single fixed alignment that handles pointers, i64, and f64 uniformly
		// without needing per-type natural alignment. For `packed struct`, fields
		// are laid out back-to-back at their exact size, matching C's
		// __attribute__((packed)) and `#pragma pack(1)`. This is required for
		// parsing on-disk binary formats like WAD files.
		for (const auto& field : structDecl->fields()) {
			FieldInfo fieldInfo;
			fieldInfo.name = field->name();
			// Resolve type aliases
			std::string resolvedType = field->typeName();
			auto aliasIt = typeAliases.find(resolvedType);
			if (aliasIt != typeAliases.end()) {
				resolvedType = aliasIt->second;
			}
			fieldInfo.typeName = resolvedType;
			fieldInfo.offset = layout.totalSize;
			fieldInfo.size = getTypeSize(resolvedType);

			// Check if this field is a type parameter (generic field)
			if (structDecl->isGeneric()) {
				for (const auto& tp : structDecl->typeParams()) {
					if (field->typeName() == tp) {
						fieldInfo.isTypeParam = true;
						fieldInfo.size = 16; // 8 value + 8 type tag
						break;
					}
				}
			}

			// Copy default value nodes (we don't own them, just reference)
			if (field->hasDefaultValue()) {
				for (const auto& node : field->defaultValue()) {
					fieldInfo.defaultValue.push_back(node.get());
				}
			}

			// Add field to layout
			layout.fields.push_back(fieldInfo);

			layout.totalSize += fieldInfo.size;
			if (!layout.isPacked && layout.totalSize % 8 != 0) {
				layout.totalSize = (layout.totalSize + 7) & ~static_cast<size_t>(7);
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

	void LlvmGenerator::Impl::generateStructDestructors() {
		// Destructor function type: void (*)(void*)
		auto destructorFnTy = llvm::FunctionType::get(builder->getVoidTy(), {ptrTy}, false);

		for (const auto& [structName, layout] : structDefinitions) {
			// Check if this struct has any fields that need cleanup
			bool needsDestructor = false;
			for (const auto& field : layout.fields) {
				// Check for struct type (Inner) or pointer to struct (*Inner)
				std::string baseTypeName = field.typeName;
				if (!baseTypeName.empty() && baseTypeName[0] == '*') {
					baseTypeName = baseTypeName.substr(1);
				}
				if (field.typeName == "str" || field.isTypeParam || isArrayType(field.typeName) ||
						(looksLikeStructType(baseTypeName) && isKnownStruct(baseTypeName))) {
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
				if (looksLikeStructType(baseTypeName) && isKnownStruct(baseTypeName)) {
					// Nested struct field
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "nested_field_ptr");
					llvm::Value* nestedStructPtr = builder->CreateLoad(ptrTy, fieldBytePtr, "nested_struct_ptr");
					builder->CreateCall(qdStructReleaseFn, {nestedStructPtr});
				}
			}

			// Release string fields
			for (const auto& field : layout.fields) {
				if (field.typeName == "str") {
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "str_field_ptr");
					llvm::Value* stringPtr = builder->CreateLoad(ptrTy, fieldBytePtr, "string_ptr");
					builder->CreateCall(qdStringReleaseFn, {stringPtr});
				}
			}

			// Release array fields. An array field owns a reference the same way a str field
			// does -- construction moves the one the stack held into the slot, and generateFieldSet
			// hands back the one an overwritten slot held -- so the struct's last reference has to
			// hand back what the slot still holds, or the qd_array_t and its buffer outlive it.
			for (const auto& field : layout.fields) {
				if (isArrayType(field.typeName)) {
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "array_field_ptr");
					llvm::Value* arrayPtr = builder->CreateLoad(ptrTy, fieldBytePtr, "array_ptr");
					builder->CreateCall(qdPtrReleaseFn, {arrayPtr});
				}
			}

			// Release generic type parameter fields that hold strings
			for (const auto& field : layout.fields) {
				if (field.isTypeParam) {
					auto tagOffset = builder->getInt64(field.offset + 8);
					auto tagPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, tagOffset, "typeparam_tag_ptr");
					llvm::Value* typeTag = builder->CreateLoad(int64Ty, tagPtr, "typeparam_tag");

					llvm::BasicBlock* releaseStr =
							llvm::BasicBlock::Create(*context, field.name + "_release_str", dtorFn);
					llvm::BasicBlock* skipRelease =
							llvm::BasicBlock::Create(*context, field.name + "_skip_release", dtorFn);

					llvm::Value* isStr = builder->CreateICmpEQ(typeTag, builder->getInt64(3), "is_str_typeparam");
					builder->CreateCondBr(isStr, releaseStr, skipRelease);

					builder->SetInsertPoint(releaseStr);
					auto fieldOffset = builder->getInt64(field.offset);
					auto fieldBytePtr =
							builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "typeparam_str_ptr");
					llvm::Value* stringPtr = builder->CreateLoad(ptrTy, fieldBytePtr, "typeparam_str");
					builder->CreateCall(qdStringReleaseFn, {stringPtr});
					builder->CreateBr(skipRelease);

					builder->SetInsertPoint(skipRelease);
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

	// The cloner for a struct type as the validator spells it, which is not always the key the
	// map uses: a type argument list (`Box<str>`) names the same layout as the bare `Box`,
	// because a generic struct has one layout with a tagged slot per parameter, and an
	// unqualified name may belong to a module. Resolution follows findStructDefinition.
	llvm::Function* LlvmGenerator::Impl::findStructCloner(const std::string& structName) const {
		const std::string base = structName.substr(0, structName.find('<'));
		auto direct = structCloners.find(base);
		if (direct != structCloners.end()) {
			return direct->second;
		}
		if (base.find("::") != std::string::npos) {
			return nullptr; // a qualified name names exactly one struct
		}
		if (!currentModuleName.empty()) {
			auto qualified = structCloners.find(currentModuleName + "::" + base);
			if (qualified != structCloners.end()) {
				return qualified->second;
			}
		}
		for (const auto& entry : structCloners) {
			size_t colonPos = entry.first.find("::");
			if (colonPos != std::string::npos && entry.first.substr(colonPos + 2) == base) {
				return entry.second;
			}
		}
		return nullptr;
	}

	// The shallow copy `clone` calls: a fresh heap struct of the same type, the bytes copied, and
	// one reference taken for every field that holds one. It is the destructor read backwards --
	// the same field walk, retain where that releases -- which is what keeps the two in step when
	// a field kind is added.
	//
	// Shallow is the whole contract: what a field points to is shared, not copied. A `str` field
	// makes that invisible, because a string is immutable and a shared one cannot be told from a
	// copy. An array or a nested struct field is visibly shared, and deliberately so -- a struct
	// is a mutable reference in this language (R21), so a field holding one behaves on `clone`
	// exactly as `a -> b` does.
	void LlvmGenerator::Impl::generateStructCloners() {
		// void* __qd_clone_<S>(void* src)
		auto clonerFnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);

		for (const auto& [structName, layout] : structDefinitions) {
			std::string clonerName = "__qd_clone_" + structName;
			auto clonerFn = llvm::Function::Create(clonerFnTy, llvm::Function::InternalLinkage, clonerName, *module);

			auto savedBlock = builder->GetInsertBlock();
			auto savedPoint = builder->GetInsertPoint();

			auto entryBlock = llvm::BasicBlock::Create(*context, "entry", clonerFn);
			builder->SetInsertPoint(entryBlock);

			llvm::Value* src = clonerFn->getArg(0);

			// Always heap, never the stack allocation generateStructConstruction can use: the copy
			// is a value the caller binds and may return, and it carries the same destructor as
			// the original so the references taken below are handed back.
			llvm::Value* destructorPtr = llvm::ConstantPointerNull::get(ptrTy);
			auto destructorIt = structDestructors.find(structName);
			if (destructorIt != structDestructors.end() && destructorIt->second != nullptr) {
				destructorPtr = destructorIt->second;
			}
			llvm::Value* dst = builder->CreateCall(
					qdStructAllocFn, {builder->getInt64(layout.totalSize), destructorPtr}, "clone_ptr");

			builder->CreateMemCpy(
					dst, llvm::MaybeAlign(8), src, llvm::MaybeAlign(8), builder->getInt64(layout.totalSize));

			// One reference per field that holds one, in the same order the destructor hands them
			// back. Without this the copy and the original share a field whose count says one
			// owner, and whichever dies first takes it with it.
			for (const auto& field : layout.fields) {
				std::string baseTypeName = field.typeName;
				if (!baseTypeName.empty() && baseTypeName[0] == '*') {
					baseTypeName = baseTypeName.substr(1);
				}
				auto fieldPtr = [&](const char* name) {
					return builder->CreateGEP(builder->getInt8Ty(), dst, builder->getInt64(field.offset), name);
				};
				if (looksLikeStructType(baseTypeName) && isKnownStruct(baseTypeName)) {
					llvm::Value* nested = builder->CreateLoad(ptrTy, fieldPtr("nested_field_ptr"), "nested_struct_ptr");
					builder->CreateCall(qdStructRetainFn, {nested});
				} else if (field.typeName == "str") {
					llvm::Value* str = builder->CreateLoad(ptrTy, fieldPtr("str_field_ptr"), "string_ptr");
					builder->CreateCall(qdStringRetainFn, {str});
				} else if (isArrayType(field.typeName)) {
					llvm::Value* arr = builder->CreateLoad(ptrTy, fieldPtr("array_field_ptr"), "array_ptr");
					builder->CreateCall(qdPtrRetainFn, {arr});
				} else if (field.isTypeParam) {
					// A generic field carries its concrete kind in the tag beside it, the way the
					// destructor reads it; 3 is `str`, the only kind that holds a reference.
					auto tagPtr = builder->CreateGEP(
							builder->getInt8Ty(), dst, builder->getInt64(field.offset + 8), "typeparam_tag_ptr");
					llvm::Value* typeTag = builder->CreateLoad(int64Ty, tagPtr, "typeparam_tag");

					auto* retainStr = llvm::BasicBlock::Create(*context, field.name + "_retain_str", clonerFn);
					auto* skipRetain = llvm::BasicBlock::Create(*context, field.name + "_skip_retain", clonerFn);
					llvm::Value* isStr = builder->CreateICmpEQ(typeTag, builder->getInt64(3), "is_str_typeparam");
					builder->CreateCondBr(isStr, retainStr, skipRetain);

					builder->SetInsertPoint(retainStr);
					llvm::Value* str = builder->CreateLoad(ptrTy, fieldPtr("typeparam_str_ptr"), "typeparam_str");
					builder->CreateCall(qdStringRetainFn, {str});
					builder->CreateBr(skipRetain);

					builder->SetInsertPoint(skipRetain);
				}
			}

			builder->CreateRet(dst);

			if (savedBlock) {
				builder->SetInsertPoint(savedBlock, savedPoint);
			}
			structCloners[structName] = clonerFn;
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
			} else if (field.typeName == "i64" || field.typeName == "u64") {
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
				builder->CreateStore(intValue, bytePtr);
			} else if (field.typeName == "i32" || field.typeName == "u32") {
				// Load as i64 from stack (stack elements are always 64-bit), truncate to i32
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
				llvm::Value* truncValue = builder->CreateTrunc(intValue, int32Ty, "int32_val");
				builder->CreateStore(truncValue, bytePtr);
			} else if (field.typeName == "i16" || field.typeName == "u16") {
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
				llvm::Value* truncValue = builder->CreateTrunc(intValue, builder->getInt16Ty(), "int16_val");
				builder->CreateStore(truncValue, bytePtr);
			} else if (field.typeName == "i8" || field.typeName == "u8") {
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
				llvm::Value* truncValue = builder->CreateTrunc(intValue, builder->getInt8Ty(), "int8_val");
				builder->CreateStore(truncValue, bytePtr);
			} else if (field.typeName == "ptr" || field.typeName == "str" ||
					   field.typeName.find('*') != std::string::npos || isArrayType(field.typeName) ||
					   isFnType(field.typeName) ||
					   (looksLikeStructType(field.typeName) && isKnownStruct(field.typeName))) {
				// Pointer type (including ptr, str, raw pointers, arrays, and struct-typed fields)
				llvm::Value* ptrValue = builder->CreateLoad(ptrTy, valuePtr, "ptr_val");
				builder->CreateStore(ptrValue, bytePtr);
			} else if (field.isTypeParam) {
				// Generic type parameter field - store raw 8-byte value + type tag
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "generic_val");
				builder->CreateStore(intValue, bytePtr);
				// Read type tag from stack element (GEP index 1)
				llvm::Value* typeTagPtr = builder->CreateStructGEP(stackElementTy, elemPtr, 1, "elem_type_ptr");
				llvm::Value* typeTag = builder->CreateLoad(int32Ty, typeTagPtr, "type_tag");
				// Store type tag at field.offset + 8
				llvm::Value* tagOffset = builder->getInt64(field.offset + 8);
				llvm::Value* tagPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, tagOffset, "tag_ptr");
				llvm::Value* tagExt = builder->CreateZExt(typeTag, int64Ty, "tag_i64");
				builder->CreateStore(tagExt, tagPtr);
			} else {
				// Unknown type - treat as i64
				llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "generic_val");
				builder->CreateStore(intValue, bytePtr);
			}
		}

		// Push struct pointer onto stack
		builder->CreateCall(pushPtrFn, {ctx, structPtr});

		// Track that we just constructed this struct type
		lastStructConstructed = structName;
		lastStructWasConstructedInPlace = !currentFunctionReturnsPtr;
		// Reset array flag — the struct is not an array, even if a field value was
		lastPushedWasArray = false;
	}

	void LlvmGenerator::Impl::generateFieldAccess(AstNodeFieldAccess* fieldAccess, llvm::Value* ctx) {
		const std::string& fieldName = fieldAccess->fieldName();

		// `<<field` reads the struct on top of the stack, whatever put it there. Its type is
		// the hint the producer left: a chained `<<` sets lastFieldAccessResultType; a local, a
		// captured variable, a call, a construction and `as` set lastStructConstructed.
		std::string structTypeName;
		if (!lastFieldAccessResultType.empty()) {
			structTypeName = lastFieldAccessResultType;
		} else if (!lastStructConstructed.empty()) {
			structTypeName = lastStructConstructed;
			lastStructConstructed.clear(); // Consume it
		}

		// Pop the struct pointer from the stack
		llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
		llvm::Value* stackPtr = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");
		llvm::Value* tempElem = createEntryAlloca(stackElementTy, "temp_elem");
		builder->CreateCall(stackPopFn, {stackPtr, tempElem});
		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, tempElem, 0, "value_ptr");
		llvm::Value* structPtr = builder->CreateLoad(ptrTy, valuePtr, "struct_ptr");

		// The struct pointer was retained when pushed to stack, needs release after access
		bool needsReleaseAfterAccess = true;

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
			int matchCount = 0;
			std::string selectedStruct;
			for (const auto& pair : structDefinitions) {
				for (const auto& field : pair.second.fields) {
					if (field.name == fieldName) {
						if (!matchingField) {
							matchingField = &field;
							selectedStruct = pair.first;
						}
						matchCount++;
						break;
					}
				}
			}
			// The fallback picks whichever struct comes first in the map, so it is only
			// answerable when exactly one declares the field. With more than one it is a coin
			// toss decided by map order, and it computes the offset from the wrong layout
			// without saying so -- `b <<x` in a method on `Bbb` read `Aaa`'s `x` and returned
			// the value of `Bbb`'s `y`. Say so instead, whether or not a type was known: a
			// known one that reached here is a type that resolved to nothing, which is no more
			// information than none.
			if (matchCount > 1) {
				std::cerr << "Error: ambiguous field '<<" << fieldName << "' found in " << matchCount << " structs";
				if (!structTypeName.empty()) {
					std::cerr << " and '" << structTypeName << "' names no struct that is known here";
				}
				std::cerr << ". Use 'as StructType' to disambiguate, e.g.: value as " << selectedStruct << " <<"
						  << fieldName << std::endl;
				// Both of these give up without emitting the read, so the build has to fail:
				// printing to stderr and carrying on produced a binary with a field access
				// missing from it and an exit status that said everything was fine.
				compilationFailed = true;
				return;
			}
		}

		if (!matchingField) {
			std::cerr << "Error: Unknown field: " << fieldName << std::endl;
			compilationFailed = true;
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
		} else if (matchingField->typeName == "i64" || matchingField->typeName == "u64") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, bytePtr, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "i32") {
			// Load i32, sign-extend to i64 for the stack
			llvm::Value* int32Value = builder->CreateLoad(int32Ty, bytePtr, "field_value_i32");
			llvm::Value* intValue = builder->CreateSExt(int32Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "u32") {
			llvm::Value* int32Value = builder->CreateLoad(int32Ty, bytePtr, "field_value_u32");
			llvm::Value* intValue = builder->CreateZExt(int32Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		} else if (matchingField->typeName == "i16") {
			llvm::Value* int16Value = builder->CreateLoad(builder->getInt16Ty(), bytePtr, "field_value_i16");
			llvm::Value* intValue = builder->CreateSExt(int16Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		} else if (matchingField->typeName == "u16") {
			llvm::Value* int16Value = builder->CreateLoad(builder->getInt16Ty(), bytePtr, "field_value_u16");
			llvm::Value* intValue = builder->CreateZExt(int16Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		} else if (matchingField->typeName == "i8") {
			llvm::Value* int8Value = builder->CreateLoad(builder->getInt8Ty(), bytePtr, "field_value_i8");
			llvm::Value* intValue = builder->CreateSExt(int8Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		} else if (matchingField->typeName == "u8") {
			llvm::Value* int8Value = builder->CreateLoad(builder->getInt8Ty(), bytePtr, "field_value_u8");
			llvm::Value* intValue = builder->CreateZExt(int8Value, int64Ty, "field_value");
			builder->CreateCall(pushIntFn, {ctx, intValue});
			lastFieldAccessResultType.clear();
		} else if (matchingField->typeName == "str") {
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue = builder->CreateLoad(ptrTy, fieldPtr, "field_value");
			builder->CreateCall(pushStrRefFn, {ctx, ptrValue});
			lastFieldAccessResultType.clear(); // Not a struct type
		} else if (matchingField->typeName == "ptr" || matchingField->typeName.find('*') != std::string::npos ||
				   isArrayType(matchingField->typeName) || isFnType(matchingField->typeName)) {
			// Handle ptr type, raw pointer types, and array types
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue = builder->CreateLoad(ptrTy, fieldPtr, "field_value");
			// Retain the pointer before pushing (it could be an array/struct that will be released after use)
			builder->CreateCall(qdPtrRetainFn, {ptrValue});
			builder->CreateCall(pushPtrFn, {ctx, ptrValue});
			// `*T` points at a struct, and the value read out of it can take a method or a
			// chained `<<` exactly as a `T` field's can -- only the name was missing, so
			// `node <<next -> c  c … method` resolved to a builtin of that name, or to
			// nothing at all, and `as T` was the only way to say what the field already said.
			std::string pointeeType;
			if (matchingField->typeName.size() > 1 && matchingField->typeName[0] == '*') {
				pointeeType = matchingField->typeName.substr(1);
			}
			if (!pointeeType.empty() && isKnownStruct(pointeeType)) {
				lastFieldAccessResultType = pointeeType;
			} else {
				lastFieldAccessResultType.clear(); // Raw pointer/array, not a known struct type
			}
		} else if (looksLikeStructType(matchingField->typeName) && isKnownStruct(matchingField->typeName)) {
			// Struct-typed field - stored as pointer, push as PTR
			llvm::Value* fieldPtr = bytePtr;
			llvm::Value* ptrValue = builder->CreateLoad(ptrTy, fieldPtr, "field_value");
			// Retain the struct pointer before pushing
			builder->CreateCall(qdPtrRetainFn, {ptrValue});
			builder->CreateCall(pushPtrFn, {ctx, ptrValue});
			// Track the struct type for chained field access
			lastFieldAccessResultType = matchingField->typeName;
		} else if (matchingField->isTypeParam) {
			// Generic type parameter field - switch on stored type tag to push correctly
			llvm::Value* tagOffset = builder->getInt64(matchingField->offset + 8);
			llvm::Value* tagPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, tagOffset, "tag_ptr");
			llvm::Value* typeTag = builder->CreateLoad(int64Ty, tagPtr, "type_tag");
			llvm::Value* rawValue = builder->CreateLoad(int64Ty, bytePtr, "raw_value");

			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			auto* intBB = llvm::BasicBlock::Create(*context, "generic.int", currentFn);
			auto* floatBB = llvm::BasicBlock::Create(*context, "generic.float", currentFn);
			auto* ptrBB = llvm::BasicBlock::Create(*context, "generic.ptr", currentFn);
			auto* strBB = llvm::BasicBlock::Create(*context, "generic.str", currentFn);
			auto* endBB = llvm::BasicBlock::Create(*context, "generic.end", currentFn);

			auto* sw = builder->CreateSwitch(typeTag, intBB, 3);
			sw->addCase(builder->getInt64(1), floatBB);
			sw->addCase(builder->getInt64(2), ptrBB);
			sw->addCase(builder->getInt64(3), strBB);

			builder->SetInsertPoint(intBB);
			builder->CreateCall(pushIntFn, {ctx, rawValue});
			builder->CreateBr(endBB);

			builder->SetInsertPoint(floatBB);
			auto* fval = builder->CreateBitCast(rawValue, builder->getDoubleTy(), "float_val");
			builder->CreateCall(pushFloatFn, {ctx, fval});
			builder->CreateBr(endBB);

			builder->SetInsertPoint(ptrBB);
			auto* pval = builder->CreateIntToPtr(rawValue, ptrTy, "ptr_val");
			builder->CreateCall(pushPtrFn, {ctx, pval});
			builder->CreateBr(endBB);

			builder->SetInsertPoint(strBB);
			auto* sval = builder->CreateIntToPtr(rawValue, ptrTy, "str_val");
			builder->CreateCall(pushStrRefFn, {ctx, sval});
			builder->CreateBr(endBB);

			builder->SetInsertPoint(endBB);
			lastFieldAccessResultType.clear();
		} else {
			// Unknown type - treat as i64 value
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
		const std::string& fieldName = fieldSet->fieldName();
		llvm::Value* structPtr = nullptr;
		std::string structTypeName;

		// Stack-based field set: stack has [struct, value] (value on top)
		// Pop value first, then pop struct, set field, push struct back

		// Determine struct type from context
		if (!lastFieldAccessResultType.empty()) {
			structTypeName = lastFieldAccessResultType;
		} else if (!lastStructConstructed.empty()) {
			structTypeName = lastStructConstructed;
		}

		// Pop value from stack into temp
		llvm::Value* stackPtrPtr = builder->CreateStructGEP(contextStructTy, ctx, 0, "stack_ptr");
		llvm::Value* stackPtrVal = builder->CreateLoad(ptrTy, stackPtrPtr, "stack");

		llvm::Value* valueTempElem = builder->CreateAlloca(stackElementTy, nullptr, "value_temp_elem");
		builder->CreateCall(stackPopFn, {stackPtrVal, valueTempElem});

		// Pop struct from stack into temp
		llvm::Value* structTempElem = builder->CreateAlloca(stackElementTy, nullptr, "struct_temp_elem");
		builder->CreateCall(stackPopFn, {stackPtrVal, structTempElem});

		// Load the struct pointer
		llvm::Value* structValuePtr = builder->CreateStructGEP(stackElementTy, structTempElem, 0, "struct_value_ptr");
		structPtr = builder->CreateLoad(ptrTy, structValuePtr, "struct_ptr");

		// Find the field
		const FieldInfo* matchingField = nullptr;
		if (!structTypeName.empty()) {
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

		// Calculate field offset and store value
		auto fieldOffset = builder->getInt64(matchingField->offset);
		auto bytePtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, fieldOffset, "field_byte_ptr");

		llvm::Value* valuePtr = builder->CreateStructGEP(stackElementTy, valueTempElem, 0, "value_ptr");

		if (matchingField->typeName == "f64") {
			llvm::Value* floatValue = builder->CreateLoad(builder->getDoubleTy(), valuePtr, "float_val");
			builder->CreateStore(floatValue, bytePtr);
		} else if (matchingField->typeName == "i64" || matchingField->typeName == "u64") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
			builder->CreateStore(intValue, bytePtr);
		} else if (matchingField->typeName == "i32" || matchingField->typeName == "u32") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
			llvm::Value* truncValue = builder->CreateTrunc(intValue, int32Ty, "int32_val");
			builder->CreateStore(truncValue, bytePtr);
		} else if (matchingField->typeName == "i16" || matchingField->typeName == "u16") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
			llvm::Value* truncValue = builder->CreateTrunc(intValue, builder->getInt16Ty(), "int16_val");
			builder->CreateStore(truncValue, bytePtr);
		} else if (matchingField->typeName == "i8" || matchingField->typeName == "u8") {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "int_val");
			llvm::Value* truncValue = builder->CreateTrunc(intValue, builder->getInt8Ty(), "int8_val");
			builder->CreateStore(truncValue, bytePtr);
		} else if (matchingField->typeName == "ptr" || matchingField->typeName == "str" ||
				   matchingField->typeName.find('*') != std::string::npos || isArrayType(matchingField->typeName) ||
				   (looksLikeStructType(matchingField->typeName) && isKnownStruct(matchingField->typeName))) {
			llvm::Value* ptrValue = builder->CreateLoad(ptrTy, valuePtr, "ptr_val");
			// A field of one of these types owns a reference: pushing the value onto the stack
			// took one (generateFieldAccess and the identifier load both retain), the store
			// below keeps it, and the destructor hands it back. Overwriting has to hand back the
			// one the old value held as well, or nothing ever does -- a list whose tail pointer
			// moved on leaked every node it had pointed at, and with it everything those nodes
			// held. Read before the store, release after it, so old == new nets out to no change.
			// Both release calls are safe on null, on a pointer that is not refcounted, and on
			// the uninitialised zero a fresh struct body carries.
			llvm::Value* oldFieldValue = builder->CreateLoad(ptrTy, bytePtr, "old_field_val");
			builder->CreateStore(ptrValue, bytePtr);
			if (matchingField->typeName == "str") {
				builder->CreateCall(qdStringReleaseFn, {oldFieldValue});
			} else {
				builder->CreateCall(qdPtrReleaseFn, {oldFieldValue});
			}
		} else if (matchingField->isTypeParam) {
			// Release old string value if the generic field currently holds a string
			{
				llvm::Value* oldTagOffset = builder->getInt64(matchingField->offset + 8);
				llvm::Value* oldTagPtr =
						builder->CreateGEP(builder->getInt8Ty(), structPtr, oldTagOffset, "old_tag_ptr");
				llvm::Value* oldTag = builder->CreateLoad(int64Ty, oldTagPtr, "old_tag");
				llvm::Value* wasStr = builder->CreateICmpEQ(oldTag, builder->getInt64(3), "was_str");
				llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
				llvm::BasicBlock* releaseOldStr =
						llvm::BasicBlock::Create(*context, "release_old_typeparam_str", currentFn);
				llvm::BasicBlock* afterRelease =
						llvm::BasicBlock::Create(*context, "after_old_typeparam_release", currentFn);
				builder->CreateCondBr(wasStr, releaseOldStr, afterRelease);

				builder->SetInsertPoint(releaseOldStr);
				llvm::Value* oldValPtr = builder->CreateGEP(
						builder->getInt8Ty(), structPtr, builder->getInt64(matchingField->offset), "old_val_ptr");
				llvm::Value* oldStr = builder->CreateLoad(ptrTy, oldValPtr, "old_str");
				builder->CreateCall(qdStringReleaseFn, {oldStr});
				builder->CreateBr(afterRelease);

				builder->SetInsertPoint(afterRelease);
			}
			// Generic type parameter field - store raw 8-byte value + type tag
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "generic_val");
			builder->CreateStore(intValue, bytePtr);
			// Read type tag from stack element (GEP index 1)
			llvm::Value* typeTagPtr = builder->CreateStructGEP(stackElementTy, valueTempElem, 1, "elem_type_ptr");
			llvm::Value* typeTag = builder->CreateLoad(int32Ty, typeTagPtr, "type_tag");
			// Store type tag at field.offset + 8
			llvm::Value* tagOffset = builder->getInt64(matchingField->offset + 8);
			llvm::Value* tagPtr = builder->CreateGEP(builder->getInt8Ty(), structPtr, tagOffset, "tag_ptr");
			llvm::Value* tagExt = builder->CreateZExt(typeTag, int64Ty, "tag_i64");
			builder->CreateStore(tagExt, tagPtr);
		} else {
			llvm::Value* intValue = builder->CreateLoad(int64Ty, valuePtr, "generic_val");
			builder->CreateStore(intValue, bytePtr);
		}

		// Push the struct back onto the stack for chaining. Always: the `>>field!`
		// form that suppressed this is gone, and callers write `>>field drop`.
		if (!pushPtrFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy, ptrTy}, false);
			pushPtrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_push_p", *module);
		}
		builder->CreateCall(pushPtrFn, {ctx, structPtr});
	}

	// The qd_make* entry point that builds an array of `elemType`. A type parameter of the
	// enclosing generic reaches qd_makea, whose array adopts a type on first append, because
	// generics are erased and the concrete type is not known at run time.
	std::string LlvmGenerator::Impl::arrayMakeFunction(const std::string& elemType) {
		if (elemType == "i64" || elemType == "i32" || elemType == "i16" || elemType == "i8" || elemType == "u64" ||
				elemType == "u32" || elemType == "u16" || elemType == "u8") {
			return "qd_makei";
		}
		if (elemType == "f64" || elemType == "f32") {
			return "qd_makef";
		}
		if (elemType == "str" || elemType == "string") {
			return "qd_makes";
		}
		if (elemType == "ptr" || isArrayType(elemType) || isKnownStruct(elemType)) {
			return "qd_makep";
		}
		return "qd_makea";
	}

	// Builds an array of `elemType` from the size already on the Quadrate stack.
	void LlvmGenerator::Impl::callArrayMake(const std::string& elemType, llvm::Value* ctx) {
		const std::string fnName = arrayMakeFunction(elemType);
		llvm::Function* makeFn = module->getFunction(fnName);
		if (!makeFn) {
			auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
			makeFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fnName, *module);
		}
		builder->CreateCall(makeFn, {ctx});
	}

	// Emits `[n]T` for a T whose zero is a value that has to be built rather than a bit pattern
	// -- a struct instance, or an empty array. The size is already on the Quadrate stack.
	//
	// The array itself comes from qd_makep, the same entry point the scalar forms use, so the
	// size is validated in one place -- a negative size reports "makep: negative size" rather
	// than reaching qd_array_create, where it became a huge size_t, a failed malloc and a null
	// array that only showed up as "len: null array" further down the program. The loop then
	// overwrites each of the n null slots with whatever `emitElement` pushes.
	void LlvmGenerator::Impl::generateFilledArray(llvm::Value* ctx, const std::function<void()>& emitElement) {
		// Take the size for the loop bound, then put it back for qd_makep to consume.
		llvm::Value* count = generateInlinePopInt(ctx);
		builder->CreateCall(pushIntFn, {ctx, count});

		auto declare = [&](const char* name) {
			llvm::Function* fn = module->getFunction(name);
			if (!fn) {
				auto fnTy = llvm::FunctionType::get(execResultTy, {contextPtrTy}, false);
				fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, name, *module);
			}
			return fn;
		};
		llvm::Function* makepFn = declare("qd_makep");
		llvm::Function* dupFn = declare("qd_dup");
		llvm::Function* setFn = declare("qd_set");

		builder->CreateCall(makepFn, {ctx});

		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock& entryBlock = currentFn->getEntryBlock();
		llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.getFirstInsertionPt());
		llvm::Value* idxPtr = entryBuilder.CreateAlloca(int64Ty, nullptr, "fill_idx");
		builder->CreateStore(builder->getInt64(0), idxPtr);

		auto* condBlock = llvm::BasicBlock::Create(*context, "arrfill_cond", currentFn);
		auto* bodyBlock = llvm::BasicBlock::Create(*context, "arrfill_body", currentFn);
		auto* doneBlock = llvm::BasicBlock::Create(*context, "arrfill_done", currentFn);

		builder->CreateBr(condBlock);

		builder->SetInsertPoint(condBlock);
		llvm::Value* idx = builder->CreateLoad(int64Ty, idxPtr, "arrfill_i");
		builder->CreateCondBr(builder->CreateICmpSLT(idx, count, "arrfill_cmp"), bodyBlock, doneBlock);

		// ( arr -- arr ) per iteration: dup the array, push the index, build the struct, and
		// let `set` consume the three of them.
		builder->SetInsertPoint(bodyBlock);
		builder->CreateCall(dupFn, {ctx});
		builder->CreateCall(pushIntFn, {ctx, builder->CreateLoad(int64Ty, idxPtr, "arrfill_i2")});
		emitElement();
		builder->CreateCall(setFn, {ctx});
		llvm::Value* nextIdx = builder->CreateAdd(
				builder->CreateLoad(int64Ty, idxPtr, "arrfill_i3"), builder->getInt64(1), "arrfill_next");
		builder->CreateStore(nextIdx, idxPtr);
		builder->CreateBr(condBlock);

		builder->SetInsertPoint(doneBlock);
		lastPushedWasArray = true;
	}

	// `[n]T` for a struct T: n distinct `T {}`. Null slots are what makes field access on an
	// unfilled one segfault, so the zero of a struct is a constructed one. The validator has
	// already checked that every field has a default, which is what makes `Point {}` legal.
	void LlvmGenerator::Impl::generateStructFilledArray(const std::string& structName, llvm::Value* ctx) {
		const StructLayout* layoutPtr = findStructDefinition(structName);
		if (layoutPtr == nullptr) {
			std::cerr << "Error: Unknown struct type in array literal: " << structName << std::endl;
			return;
		}
		const StructLayout& layout = *layoutPtr;
		generateFilledArray(ctx, [&] {
			for (const auto& field : layout.fields) {
				for (IAstNode* defaultNode : field.defaultValue) {
					generateNode(defaultNode, ctx);
				}
			}
			generateStructConstruction(layout.name, ctx);
		});
	}

	// `[n][]T` : n distinct empty arrays. The zero of an array is an empty one, the way the zero
	// of str is a real empty string rather than null -- a null slot only shows up later, as
	// "len: null array" from the first thing that reads it.
	void LlvmGenerator::Impl::generateArrayFilledArray(const std::string& elemType, llvm::Value* ctx) {
		const std::string inner = elemType.substr(2);
		generateFilledArray(ctx, [&] {
			builder->CreateCall(pushIntFn, {ctx, builder->getInt64(0)});
			callArrayMake(inner, ctx);
		});
	}

	// Emits the `[size]T` form. The size expression leaves one integer on the Quadrate stack
	// (or zero is pushed when the size was left out, as in `[]i64`), then the entry point for
	// the element type pops it and returns the array.
	void LlvmGenerator::Impl::generateSizedArrayLiteral(AstNodeArrayLiteral* arrayLiteral, llvm::Value* ctx) {
		const auto& sizeExpr = arrayLiteral->elements();
		if (sizeExpr.empty()) {
			builder->CreateCall(pushIntFn, {ctx, builder->getInt64(0)});
		} else {
			for (const auto& node : sizeExpr) {
				generateNode(node.get(), ctx);
			}
		}

		const std::string& elemType = arrayLiteral->elementType();

		// The two element types whose zero has to be built rather than allocated.
		if (isKnownStruct(elemType)) {
			generateStructFilledArray(elemType, ctx);
			return;
		}
		if (isArrayType(elemType)) {
			generateArrayFilledArray(elemType, ctx);
			return;
		}

		callArrayMake(elemType, ctx);
		lastPushedWasArray = true;
	}

	void LlvmGenerator::Impl::generateArrayLiteral(AstNodeArrayLiteral* arrayLiteral, llvm::Value* ctx) {
		const auto& elements = arrayLiteral->elements();
		size_t numElements = elements.size();

		// `[size]T` -- the nodes above are the size expression, not elements. This is the
		// lowering `make<T>` used to have: the size goes on the stack and one of the qd_make*
		// entry points allocates and zero-fills.
		if (arrayLiteral->hasElementType()) {
			generateSizedArrayLiteral(arrayLiteral, ctx);
			return;
		}

		if (numElements == 0) {
			// Empty array - element type unknown until the first append (QD_ARRAY_TYPE_ANY = 4).
			// Creating it as INT meant `[] "x" append` failed at run time, and made a generic
			// `map<T, U>` impossible to write: the result array could only ever hold integers.
			llvm::Function* createArrayFn = module->getFunction("qd_array_create");
			if (!createArrayFn) {
				auto fnTy = llvm::FunctionType::get(ptrTy, {int64Ty, int32Ty}, false);
				createArrayFn =
						llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
			}
			llvm::Value* arrPtr =
					builder->CreateCall(createArrayFn, {builder->getInt64(8), builder->getInt32(4)}, "empty_arr");
			builder->CreateCall(pushPtrFn, {ctx, arrPtr});
			return;
		}

		// An element that is not a scalar literal -- a nested array literal, a struct
		// literal, a local, a call -- cannot be emitted by the constant path below, which
		// reads its value straight out of the AST. Such elements used to be skipped in
		// silence: `7 -> x  [x 2 3]` compiled to the two-element array `[2 3]`, and a
		// nested `[[1 2] [3 4]]` would have done the same had the parser accepted it.
		//
		// Build through the Quadrate stack instead. The array goes on the stack, each
		// element generates itself on top of it, and `append` consumes the pair and leaves
		// the array -- which is where this function has to leave it anyway. The array is
		// created as QD_ARRAY_TYPE_ANY (4) and adopts its element type from the first value
		// appended, exactly as `[] x append` does, so a nested array yields an array of
		// pointers without anything here having to work out the element type statically.
		bool allScalarLiterals = true;
		for (const auto& elemPtr : elements) {
			if (elemPtr->type() != IAstNode::Type::LITERAL) {
				allScalarLiterals = false;
				break;
			}
		}

		if (!allScalarLiterals) {
			llvm::Function* createAnyArrayFn = module->getFunction("qd_array_create");
			if (!createAnyArrayFn) {
				auto fnTy = llvm::FunctionType::get(ptrTy, {int64Ty, int32Ty}, false);
				createAnyArrayFn =
						llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
			}
			llvm::Function* appendFn = module->getFunction("qd_append");
			if (!appendFn) {
				auto fnTy = llvm::FunctionType::get(int32Ty, {ptrTy}, false);
				appendFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_append", *module);
			}

			llvm::Value* dynArr = builder->CreateCall(
					createAnyArrayFn, {builder->getInt64(numElements), builder->getInt32(4)}, "arr_dyn");
			builder->CreateCall(pushPtrFn, {ctx, dynArr});
			for (const auto& elemPtr : elements) {
				generateNode(elemPtr.get(), ctx);
				builder->CreateCall(appendFn, {ctx});
			}
			lastPushedWasArray = true;
			return;
		}

		// Determine array element type from first element
		// QD_ARRAY_TYPE_INT = 0, QD_ARRAY_TYPE_FLOAT = 1, QD_ARRAY_TYPE_STR = 2, QD_ARRAY_TYPE_PTR = 3
		int32_t arrayType = 0; // Default to INT
		IAstNode* firstElem = elements[0].get();
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
			auto fnTy = llvm::FunctionType::get(ptrTy, {int64Ty, int32Ty}, false);
			createArrayFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_create", *module);
		}

		llvm::Function* pushIntArrFn = module->getFunction("qd_array_push_int");
		if (!pushIntArrFn) {
			auto fnTy = llvm::FunctionType::get(int32Ty, {ptrTy, int64Ty}, false);
			pushIntArrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_int", *module);
		}

		llvm::Function* pushFloatArrFn = module->getFunction("qd_array_push_float");
		if (!pushFloatArrFn) {
			auto fnTy = llvm::FunctionType::get(int32Ty, {ptrTy, builder->getDoubleTy()}, false);
			pushFloatArrFn =
					llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_float", *module);
		}

		llvm::Function* pushPtrArrFn = module->getFunction("qd_array_push_ptr");
		if (!pushPtrArrFn) {
			auto fnTy = llvm::FunctionType::get(int32Ty, {ptrTy, ptrTy}, false);
			pushPtrArrFn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "qd_array_push_ptr", *module);
		}

		// Create array with initial capacity
		llvm::Value* arrPtr = builder->CreateCall(createArrayFn,
				{builder->getInt64(numElements), builder->getInt32(static_cast<uint32_t>(arrayType))}, "arr_ptr");

		// Push elements to the array
		for (const auto& elemPtr : elements) {
			IAstNode* elem = elemPtr.get();
			if (elem->type() == IAstNode::Type::LITERAL) {
				auto* lit = static_cast<AstNodeLiteral*>(elem);
				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER ||
						lit->literalType() == AstNodeLiteral::LiteralType::BOOL) {
					int64_t val = 0;
					if (lit->literalType() == AstNodeLiteral::LiteralType::BOOL) {
						val = (lit->value() == "true" || lit->value() == "Ok") ? 1 : 0;
					} else {
						parseIntegerLiteral(lit->value(), val);
					}
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
						auto fnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
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
		deferScopeStack.push_back(std::vector<DeferEntry>());
	}

	void LlvmGenerator::Impl::popDeferScope() {
		if (!deferScopeStack.empty()) {
			deferScopeStack.pop_back();
		}
	}

	void LlvmGenerator::Impl::registerDefer(AstNodeDefer* deferNode, llvm::Value* ctx) {
		(void)ctx;
		if (deferScopeStack.empty()) {
			pushDeferScope();
		}

		// The flag lives in the entry block so it dominates every exit, and starts false so a
		// path that skips this statement leaves it false.
		llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
		llvm::BasicBlock& entryBlock = currentFn->getEntryBlock();
		llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.getFirstInsertionPt());
		llvm::Value* reached = entryBuilder.CreateAlloca(builder->getInt1Ty(), nullptr, "defer_reached");
		entryBuilder.CreateStore(builder->getInt1(false), reached);

		// Reaching the `defer` statement is what arms it.
		builder->CreateStore(builder->getInt1(true), reached);

		deferScopeStack.back().push_back(DeferEntry{deferNode, reached});
	}

	void LlvmGenerator::Impl::emitDeferScope(llvm::Value* ctx) {
		if (deferScopeStack.empty()) {
			return;
		}

		// Copy: generating a defer body can itself register defers (a nested scope), which would
		// reallocate the vector we are iterating.
		std::vector<DeferEntry> currentScope = deferScopeStack.back();

		// Execute defers in REVERSE order (LIFO)
		for (auto it = currentScope.rbegin(); it != currentScope.rend(); ++it) {
			AstNodeDefer* deferNode = it->node;
			llvm::Value* reached = it->reached;

			// Nothing to guard against if the block already ended (e.g. the body panicked).
			if (builder->GetInsertBlock() == nullptr || isTerminated(builder->GetInsertBlock())) {
				break;
			}

			llvm::Function* currentFn = builder->GetInsertBlock()->getParent();
			llvm::BasicBlock* runBB = llvm::BasicBlock::Create(*context, "defer.run", currentFn);
			llvm::BasicBlock* skipBB = llvm::BasicBlock::Create(*context, "defer.skip", currentFn);

			llvm::Value* wasReached = builder->CreateLoad(builder->getInt1Ty(), reached, "defer_was_reached");
			builder->CreateCondBr(wasReached, runBB, skipBB);

			builder->SetInsertPoint(runBB);
			// Disarm before running, so a defer inside a loop body does not fire again on a
			// later iteration that did not reach it.
			builder->CreateStore(builder->getInt1(false), reached);
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
			if (builder->GetInsertBlock() != nullptr && !isTerminated(builder->GetInsertBlock())) {
				builder->CreateBr(skipBB);
			}

			builder->SetInsertPoint(skipBB);
		}
	}

	void LlvmGenerator::Impl::executeDeferScope(llvm::Value* ctx) {
		if (deferScopeStack.empty()) {
			return;
		}
		emitDeferScope(ctx);
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
