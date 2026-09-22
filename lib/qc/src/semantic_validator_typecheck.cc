#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_array_literal.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/ast_node_while.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/numeric_literal.h>
#include <quadrate/qc/semantic_validator.h>
#include <sstream>
#include <unordered_set>

namespace Qd {

#include "semantic_validator_internal.h"

	// Helper: Convert StackValueType to Quadrate type name
	static const char* stackTypeToQdType(StackValueType type) {
		switch (type) {
		case StackValueType::INT:
			return "i64";
		case StackValueType::FLOAT:
			return "f64";
		case StackValueType::STRING:
			return "str";
		case StackValueType::PTR:
			return "ptr";
		default:
			return "any";
		}
	}

	// Helper: Build fn type string from FunctionSignature, e.g. "fn(i64 -- i64)".
	// Prefers the declared type name of each parameter and result, so a function returning a
	// struct renders as `fn( -- Point)` rather than `fn( -- ptr)`; the coarse stack types cannot
	// tell those apart, and a field declared `fn( -- Point)` would not match.
	static std::string buildFnTypeString(const FunctionSignature& sig) {
		auto nameAt = [](const std::unordered_map<size_t, std::string>& names, size_t i, StackValueType t) {
			auto it = names.find(i);
			return it != names.end() && !it->second.empty() ? it->second : std::string(stackTypeToQdType(t));
		};
		std::string s = "fn(";
		for (size_t i = 0; i < sig.consumes.size(); i++) {
			if (i > 0) {
				s += " ";
			}
			s += nameAt(sig.parameterTypeNames, i, sig.consumes[i]);
		}
		if (!sig.consumes.empty()) {
			s += " ";
		}
		s += "--";
		for (size_t i = 0; i < sig.produces.size(); i++) {
			s += " ";
			s += nameAt(sig.producesTypeNames, i, sig.produces[i]);
		}
		s += ")";
		return s;
	}

	// Helper: Collect all identifier references in an AST subtree (for unused param detection)
	static void collectIdentifierRefs(const IAstNode* node, std::unordered_set<std::string>& refs,
			const std::unordered_set<std::string>& paramNames) {
		if (!node) {
			return;
		}
		if (node->type() == IAstNode::Type::IDENTIFIER) {
			const auto* ident = static_cast<const AstNodeIdentifier*>(node);
			refs.insert(ident->name());
		}
		if (node->type() == IAstNode::Type::INSTRUCTION) {
			const auto* instr = static_cast<const AstNodeInstruction*>(node);
			if (paramNames.count(instr->name())) {
				refs.insert(instr->name());
			}
		}
		for (size_t i = 0; i < node->childCount(); i++) {
			collectIdentifierRefs(node->child(i), refs, paramNames);
		}
	}

	// Helper: Convert literal type to stack value type
	static StackValueType getLiteralStackType(AstNodeLiteral::LiteralType litType) {
		switch (litType) {
		case AstNodeLiteral::LiteralType::INTEGER:
			return StackValueType::INT;
		case AstNodeLiteral::LiteralType::BOOL:
			return StackValueType::INT;
		case AstNodeLiteral::LiteralType::FLOAT:
			return StackValueType::FLOAT;
		case AstNodeLiteral::LiteralType::STRING:
			return StackValueType::STRING;
		case AstNodeLiteral::LiteralType::NULL_PTR:
			return StackValueType::PTR;
		default:
			return StackValueType::INT;
		}
	}

	// Helper: Check if a block ends with a diverging instruction (like `panic`)
	// Diverging instructions never return, so stack effects don't need to balance
	// The stdlib entry points that consume more of the stack than their signature says.
	// `fmt::printf`/`sprintf` pop one value per `%` in the format string, a count known only at
	// runtime, so no static model applies inside them. Call sites already special-case exactly
	// these names; this is the same list seen from the definition side.
	//
	// `flag::parse` used to be here too, because it consumed the argument pile `read` splayed
	// across the stack. It takes an array now, so its effect is ordinary and it is checked like
	// anything else.
	static bool isVariadicStackConsumer(const std::string& functionName, const char* filename) {
		// Keyed on the defining file rather than the package name, which is derived differently
		// depending on which pass is validating the module.
		const std::string module = filename ? std::filesystem::path(filename).stem().string() : std::string();
		if (module == "fmt") {
			return functionName == "printf" || functionName == "sprintf";
		}
		return false;
	}

	// A block diverges when control cannot reach its end: some top-level statement in it
	// is `panic`, `return`, `break` or `continue` (spec 6.1.1). Everything after such a
	// statement is unreachable, so it is the statement's presence that matters, not whether
	// it happens to be last -- `"msg" 1 panic  0` diverges just as `"msg" 1 panic` does.
	static bool blockEndsDiverging(IAstNode* block);

	static bool isDivergingStatement(IAstNode* node) {
		if (!node) {
			return false;
		}
		switch (node->type()) {
		case IAstNode::Type::RETURN_STATEMENT:
		case IAstNode::Type::BREAK_STATEMENT:
		case IAstNode::Type::CONTINUE_STATEMENT:
			return true;
		case IAstNode::Type::INSTRUCTION:
			return static_cast<AstNodeInstruction*>(node)->name() == "panic";
		case IAstNode::Type::BLOCK:
			return blockEndsDiverging(node);
		default:
			return false;
		}
	}

	static bool blockEndsDiverging(IAstNode* block) {
		if (!block) {
			return false;
		}
		for (size_t i = 0; i < block->childCount(); i++) {
			if (isDivergingStatement(block->child(i))) {
				return true;
			}
		}
		return false;
	}

	// The signature of the bare fallible call at `node->child(i - 1)`, or nullptr. "Bare" means
	// no `!` or `?`: the call leaves a status value for the `if`/`switch` at child(i) to consume.
	// Methods are keyed by their mangled name, which is why this cannot be a plain name lookup.
	const FunctionSignature* SemanticValidator::bareFallibleCallBefore(IAstNode* node, size_t i) const {
		if (i == 0) {
			return nullptr;
		}
		IAstNode* prev = node->child(i - 1);
		if (!prev) {
			return nullptr;
		}
		std::string key;
		if (prev->type() == IAstNode::Type::INSTRUCTION) {
			// `call` on a fallible function pointer. There is no name to look up, so the
			// signature the call site resolved is what says what the success arm receives.
			auto* callInst = static_cast<AstNodeInstruction*>(prev);
			if (callInst->name() != "call" || !callInst->calleeFallible() || callInst->abortOnError() ||
					callInst->propagateOnError()) {
				return nullptr;
			}
			auto sigIt = mCallSiteSignatures.find(prev);
			return (sigIt != mCallSiteSignatures.end()) ? &sigIt->second : nullptr;
		}
		if (prev->type() == IAstNode::Type::IDENTIFIER) {
			auto* id = static_cast<AstNodeIdentifier*>(prev);
			if (id->abortOnError() || id->propagateOnError()) {
				return nullptr;
			}
			key = id->isMethodCall() ? id->receiverType() + "::" + id->name() : id->name();
		} else if (prev->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			auto* sc = static_cast<AstNodeScopedIdentifier*>(prev);
			if (sc->abortOnError() || sc->propagateOnError()) {
				return nullptr;
			}
			if (sc->isMethodCall()) {
				auto it = mFunctionSignatures.find(sc->scope() + "::" + sc->receiverType() + "::" + sc->name());
				if (it != mFunctionSignatures.end() && it->second.throws) {
					return &it->second;
				}
				key = sc->receiverType() + "::" + sc->name();
			} else {
				key = sc->scope() + "::" + sc->name();
			}
		} else {
			return nullptr;
		}
		auto it = mFunctionSignatures.find(key);
		if (it == mFunctionSignatures.end() || !it->second.throws) {
			return nullptr;
		}
		return &it->second;
	}

	// Drops the values a failed fallible call never produced. On failure the callee returns
	// before pushing its results, so the failure arm of the `if`/`switch` that consumed the
	// status sees the stack as it was before the call. Modelling the arm from the success
	// stack instead is what let a `drop` in a failure arm type-check while underflowing --
	// or eating the caller's value -- at runtime.
	static void removeProducedValues(const FunctionSignature& sig, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack) {
		for (size_t k = 0; k < sig.produces.size() && !typeStack.empty(); k++) {
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
		}
	}

	// The struct type a pointer result inherits from a pointer argument in the same position
	// when the signature declares none: `fn f(p:ptr -- q:ptr)` passing a `Point` through.
	static std::string passThroughStructType(
			const FunctionSignature& sig, size_t produceIdx, const std::vector<std::string>& consumedStructTypes) {
		size_t producedPtrBefore = 0;
		for (size_t pi = 0; pi < produceIdx && pi < sig.produces.size(); pi++) {
			if (sig.produces[pi] == StackValueType::PTR) {
				producedPtrBefore++;
			}
		}
		size_t ptrIdx = 0;
		for (size_t ci = 0; ci < sig.consumes.size() && ci < consumedStructTypes.size(); ci++) {
			if (sig.consumes[ci] == StackValueType::PTR) {
				if (ptrIdx == producedPtrBefore) {
					return consumedStructTypes[ci];
				}
				ptrIdx++;
			}
		}
		return "";
	}

	bool SemanticValidator::bindCallTypeParams(const FunctionSignature& sig,
			const std::vector<StackValueType>& typeStack, const std::vector<std::string>& structTypeStack,
			IAstNode* site, const std::string& displayName, std::map<std::string, std::string>& bindings) {
		if (typeStack.size() < sig.consumes.size()) {
			return true; // the underflow is reported by the caller
		}
		bool ok = true;
		auto canon = [this](const std::string& n) { return canonicalStructName(n); };
		size_t base = typeStack.size() - sig.consumes.size();
		for (size_t idx = 0; idx < sig.consumes.size(); idx++) {
			StackValueType declared = sig.consumes[idx];
			if (declared != StackValueType::PTR && declared != StackValueType::TYPEVAR) {
				continue; // scalars were checked against the coarse types already
			}
			std::string expected;
			auto nameIt = sig.parameterTypeNames.find(idx);
			if (nameIt != sig.parameterTypeNames.end()) {
				expected = nameIt->second;
			} else {
				auto stIt = sig.parameterStructTypes.find(idx);
				if (stIt != sig.parameterStructTypes.end()) {
					expected = stIt->second;
				}
			}
			// A declared `ptr` used to be skipped here alongside `any`. It is still a top type, but
			// `unifyTypeName` has one thing to say about it -- a `[]T` is not a raw buffer -- so
			// the check has to run.
			if (expected.empty() || expected == "any") {
				continue;
			}
			size_t stackIdx = base + idx;
			StackValueType actualType = typeStack[stackIdx];
			std::string actual;
			size_t fromTop = typeStack.size() - stackIdx; // 1 = top
			if (structTypeStack.size() >= fromTop) {
				actual = structTypeStack[structTypeStack.size() - fromTop];
			}
			if (actual.empty()) {
				actual = qdTypeNameOf(actualType);
			}
			if (actual.empty()) {
				if (actualType == StackValueType::PTR && expected.compare(0, 3, "fn(") == 0) {
					std::string warnMsg = "Untyped 'ptr' passed to function '" + displayName + "': Parameter " +
										  std::to_string(idx + 1) + " expects '" + expected +
										  "'. Consider adding a type annotation";
					reportWarning(site, warnMsg.c_str());
				}
				continue; // nothing known about the argument
			}
			std::string why;
			if (!unifyTypeName(expected, actual, sig.typeParams, bindings, mMergedModules, canon, why)) {
				std::string msg = "Type error in function '" + displayName + "': Parameter " + std::to_string(idx + 1) +
								  (why.compare(0, 7, "expects") == 0 ? " " : ": ") + why;
				reportError(site, msg.c_str());
				ok = false;
			}
		}
		return ok;
	}

	// What a receiver's type arguments bind its struct's type parameters to: a `Box<i64>`
	// receiver binds T to i64 for every method declared on `Box<T>`. Without it a method's
	// result was pushed as the bare type parameter -- `b unwrap 2 *` on a `Box<i64>` failed
	// with "Expected numeric types, got typevar and int", and a generic container could only be
	// read into something that takes `any`, which is why nothing in the corpus passed one to a
	// function.
	std::map<std::string, std::string> SemanticValidator::receiverTypeBindings(
			const std::string& receiverStructType) const {
		std::map<std::string, std::string> bindings;
		std::vector<std::string> args = typeArgsOf(receiverStructType);
		if (args.empty()) {
			return bindings;
		}

		const AstNodeStructDeclaration* decl = nullptr;
		std::string base = typeBaseName(receiverStructType);
		auto declIt = mStructDeclarations.find(base);
		if (declIt != mStructDeclarations.end()) {
			decl = declIt->second;
		} else {
			auto moduleIt = mModuleStructDeclarations.find(base);
			if (moduleIt != mModuleStructDeclarations.end()) {
				decl = moduleIt->second;
			} else if (base.find("::") == std::string::npos) {
				// A method's own signature names its struct unqualified -- `push` on a
				// `ct::Vec<i64>` returns `Vec<T>` -- so the type the result carries is
				// unqualified from there on, and the next call has only `Vec` to look up.
				const std::string suffix = "::" + base;
				for (const auto& entry : mModuleStructDeclarations) {
					if (entry.first.size() > suffix.size() &&
							entry.first.compare(entry.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
						decl = entry.second;
						break;
					}
				}
			}
		}
		if (decl == nullptr) {
			return bindings;
		}

		const auto& params = decl->typeParams();
		for (size_t idx = 0; idx < params.size() && idx < args.size(); idx++) {
			bindings[params[idx]] = args[idx];
		}
		return bindings;
	}

	void SemanticValidator::pushCallResults(const FunctionSignature& sig,
			const std::map<std::string, std::string>& bindings, const std::vector<std::string>& consumedStructTypes,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		for (size_t idx = 0; idx < sig.produces.size(); idx++) {
			StackValueType type = sig.produces[idx];
			std::string structType;
			std::string declared;
			auto declIt = sig.producesTypeNames.find(idx);
			if (declIt != sig.producesTypeNames.end()) {
				declared = declIt->second;
			}
			std::string resolved = declared.empty() ? std::string() : substituteTypeParams(declared, bindings);
			bool substituted = !declared.empty() && resolved != declared;
			if (substituted) {
				// A type parameter was bound at this call: the result has the concrete type.
				if (resolved.compare(0, 2, "[]") == 0 || resolved.compare(0, 3, "fn(") == 0) {
					type = StackValueType::PTR;
					structType = resolved;
				} else {
					StackValueType concrete = stringToStackValueType(typeBaseName(resolved));
					if (concrete == StackValueType::INT || concrete == StackValueType::FLOAT ||
							concrete == StackValueType::STRING) {
						type = concrete;
					} else if (concrete == StackValueType::PTR) {
						type = StackValueType::PTR;
						structType = resolved; // keeps `<...>` so the fields resolve to concrete types
					}
					// otherwise the binding itself was unknown: keep the declared coarse type
				}
			} else if (type == StackValueType::PTR) {
				auto stIt = sig.producesStructTypes.find(idx);
				if (stIt != sig.producesStructTypes.end()) {
					structType = stIt->second;
				} else {
					structType = passThroughStructType(sig, idx, consumedStructTypes);
				}
			}
			typeStack.push_back(type);
			structTypeStack.push_back(structType);
		}
	}

	// True when this struct's field was declared `*T` rather than `T`.
	bool SemanticValidator::isPointerField(const std::string& structName, const std::string& fieldName) const {
		auto it = mStructPointerFields.find(structName);
		if (it == mStructPointerFields.end()) {
			// Module structs may be registered under the unqualified name.
			size_t colon = structName.rfind("::");
			if (colon == std::string::npos) {
				return false;
			}
			it = mStructPointerFields.find(structName.substr(colon + 2));
			if (it == mStructPointerFields.end()) {
				return false;
			}
		}
		return it->second.find(fieldName) != it->second.end();
	}

	// Whether a value of type `actual` may initialise a field declared `expected`. Structural,
	// so spelling differences that denote the same type do not matter.
	bool SemanticValidator::fieldTypesCompatible(const std::string& expected, const std::string& actual) const {
		std::map<std::string, std::string> noBindings;
		std::string why;
		auto canon = [this](const std::string& n) { return canonicalStructName(n); };
		// Strict from the start: a field initializer is a definite assignment, so a bare `ptr`
		// does not satisfy a typed field.
		return unifyTypeName(expected, actual, {}, noBindings, mMergedModules, canon, why, true);
	}

	// Writing a field happens in five places -- a named initializer, a positional construction of
	// a local struct, the two positional forms for a module's struct (bare name and `mod::Name`),
	// and `>>field` -- and they had five answers to the same question. The coarse
	// `StackValueType` check each does first catches a string going into an `i64`; this is the
	// part underneath it, where both sides are pointers and the pointer's real type is what
	// separates them. Three of the five did not have that part at all, `>>field` among them, so
	// they took any pointer for any pointer field: an array into a raw `ptr`, a `Point` into a
	// `[]i64`.
	//
	// All five are definite assignments rather than values of unknown provenance, which is what
	// makes the rule strict: an untyped pointer satisfies only a field that asked for a raw `ptr`,
	// and a `[]T` never satisfies one, because an array addresses its header rather than its
	// elements. A field declared `*T` is exempt -- a pointer of unknown provenance is exactly what
	// `null` is, and `next = null` is how every linked structure starts (specification 3.6.2).
	bool SemanticValidator::fieldValueRejected(const std::string& structName, const std::string& fieldName,
			const std::string& expected, const std::string& actual, std::string& why) const {
		if (expected.empty() || isPointerField(structName, fieldName)) {
			return false; // the field's type was never recorded, or it is a `*T`
		}
		if (actual.empty()) {
			if (expected == "ptr") {
				return false; // a raw pointer is what it asked for
			}
			why = "expects " + expected + ", but got ptr";
			return true;
		}
		if (expected == "ptr") {
			if (isArrayTypeName(actual)) {
				why = rawPtrArrayWhy(actual);
				return true;
			}
			return false;
		}
		if (fieldTypesCompatible(expected, actual)) {
			return false;
		}
		why = "expects " + expected + ", but got " + actual;
		return true;
	}

	// Warns when a `switch` whose every arm names a variant of one enum leaves some variant
	// unhandled and has no `_` arm -- the arm was probably forgotten, and with no `_` nothing runs.
	//
	// A warning rather than an error, deliberately: an enum variant is an `i64`, and nothing tracks
	// that the value being switched on came from that enum, so this reads the author's intent off
	// the case labels rather than off a type. Inferred intent should not be fatal. If a real sum
	// type ever lands (R3) the subject would have a type and this could become an error.
	void SemanticValidator::checkEnumSwitchExhaustive(IAstNode* switchNode, const std::vector<AstNodeCase*>& cases) {
		std::string enumName;
		std::unordered_set<std::string> covered;
		for (const auto* caseNode : cases) {
			if (caseNode->isDefault()) {
				return; // a `_` arm handles whatever is left
			}
			IAstNode* label = caseNode->value();
			if (!label || label->type() != IAstNode::Type::SCOPED_IDENTIFIER) {
				return; // a literal or constant arm: this is not a switch over one enum
			}
			auto* scoped = static_cast<AstNodeScopedIdentifier*>(label);
			if (enumName.empty()) {
				enumName = scoped->scope();
			} else if (enumName != scoped->scope()) {
				return; // arms from two different enums
			}
			covered.insert(scoped->name());
		}
		if (enumName.empty()) {
			return;
		}
		auto it = mEnumVariants.find(enumName);
		if (it == mEnumVariants.end()) {
			// The scope is not a known enum -- a module constant, say.
			return;
		}
		std::vector<std::string> missing;
		for (const std::string& variant : it->second) {
			if (covered.find(variant) == covered.end()) {
				missing.push_back(variant);
			}
		}
		if (missing.empty()) {
			return;
		}
		std::string msg = "switch on enum '" + enumName + "' does not handle ";
		for (size_t i = 0; i < missing.size() && i < 4; i++) {
			if (i > 0) {
				msg += ", ";
			}
			msg += missing[i];
		}
		if (missing.size() > 4) {
			msg += " and " + std::to_string(missing.size() - 4) + " more";
		}
		msg += "; add the missing arm(s), or a '_' arm if that is deliberate";
		reportWarning(switchNode, msg.c_str());
	}

	std::string SemanticValidator::canonicalStructName(const std::string& name) const {
		if (name.empty() || name.find("::") != std::string::npos || mDefinedStructs.count(name) > 0) {
			return name;
		}
		for (const auto& moduleEntry : mModuleStructs) {
			if (moduleEntry.second.find(name) != moduleEntry.second.end()) {
				return moduleEntry.first + "::" + name;
			}
		}
		return name;
	}

	std::optional<FunctionSignature> SemanticValidator::parseFnTypeString(const std::string& typeStr) const {
		std::string resolved = typeStr;
		auto aliasIt = mTypeAliases.find(typeStr);
		if (aliasIt != mTypeAliases.end()) {
			resolved = aliasIt->second;
		}
		std::vector<std::string> ins, outs;
		if (!splitFnType(resolved, ins, outs)) {
			return std::nullopt;
		}
		FunctionSignature sig;
		auto add = [&](const std::vector<std::string>& names, std::vector<StackValueType>& types,
						   std::unordered_map<size_t, std::string>& structTypes,
						   std::unordered_map<size_t, std::string>& typeNames) {
			for (size_t i = 0; i < names.size(); i++) {
				std::string name = names[i];
				auto a = mTypeAliases.find(name);
				if (a != mTypeAliases.end()) {
					name = a->second;
				}
				StackValueType t = stringToStackValueType(name);
				types.push_back(t);
				typeNames[i] = name;
				if (t == StackValueType::PTR && name != "ptr") {
					structTypes[i] = name;
				}
			}
		};
		add(ins, sig.consumes, sig.parameterStructTypes, sig.parameterTypeNames);
		add(outs, sig.produces, sig.producesStructTypes, sig.producesTypeNames);
		return sig;
	}

	StackValueType SemanticValidator::resolveFieldType(const std::string& structType, const std::string& fieldName,
			std::string& fieldStructType, bool* unboundTypeParam) const {
		fieldStructType.clear();
		if (unboundTypeParam) {
			*unboundTypeParam = false;
		}
		std::string base = typeBaseName(structType);
		const auto* fields = lookupStructFieldTypes(base);
		if (fields == nullptr) {
			return StackValueType::UNKNOWN;
		}
		auto fieldIt = fields->find(fieldName);
		if (fieldIt == fields->end()) {
			return StackValueType::UNKNOWN;
		}
		StackValueType type = fieldIt->second;
		std::string declared;
		auto fstIt = mStructFieldStructTypes.find(base);
		if (fstIt == mStructFieldStructTypes.end()) {
			// Module structs may be registered under the unqualified name.
			size_t colon = base.rfind("::");
			if (colon != std::string::npos) {
				fstIt = mStructFieldStructTypes.find(base.substr(colon + 2));
			}
		}
		if (fstIt != mStructFieldStructTypes.end()) {
			auto nm = fstIt->second.find(fieldName);
			if (nm != fstIt->second.end()) {
				declared = nm->second;
			}
		}
		// Is the declared field type one of the struct's type parameters?
		AstNodeStructDeclaration* decl = nullptr;
		auto dIt = mStructDeclarations.find(base);
		if (dIt != mStructDeclarations.end()) {
			decl = dIt->second;
		} else {
			auto mIt = mModuleStructDeclarations.find(base);
			if (mIt != mModuleStructDeclarations.end()) {
				decl = mIt->second;
			}
		}
		if (decl != nullptr && !declared.empty()) {
			const auto& tps = decl->typeParams();
			auto tp = std::find(tps.begin(), tps.end(), declared);
			if (tp != tps.end()) {
				std::vector<std::string> args = typeArgsOf(structType);
				size_t index = static_cast<size_t>(tp - tps.begin());
				if (index < args.size()) {
					const std::string& arg = args[index];
					if (arg.compare(0, 2, "[]") == 0 || arg.compare(0, 3, "fn(") == 0) {
						fieldStructType = arg;
						return StackValueType::PTR;
					}
					StackValueType concrete = stringToStackValueType(typeBaseName(arg));
					if (concrete == StackValueType::INT || concrete == StackValueType::FLOAT ||
							concrete == StackValueType::STRING) {
						return concrete;
					}
					if (concrete == StackValueType::PTR) {
						fieldStructType = arg;
						return StackValueType::PTR;
					}
				}
				if (unboundTypeParam) {
					*unboundTypeParam = true;
				}
				fieldStructType = declared;
				return type;
			}
		}
		fieldStructType = declared;
		return type;
	}

	// Check if a type string is a known struct name (local or imported)
	// Supports both unqualified names (Response) and qualified names (http::Response)

	void SemanticValidator::analyzeBlockInIsolation(IAstNode* node, std::vector<StackValueType>& typeStack,
			const std::unordered_map<std::string, StackValueType>& initialLocalVars) {
		if (!node) {
			return;
		}

		// Dummy structTypeStack for signature analysis (not used, but required by API)
		std::vector<std::string> structTypeStack;

		// Track local variable types for accurate signature analysis
		// Start with any initial local variables (e.g., function parameters)
		std::unordered_map<std::string, StackValueType> localVarTypes = initialLocalVars;

		// Process each child in the block (using index-based loop for peek-ahead access)
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}
			if (mUndefinedNodes.count(child)) {
				typeStack.push_back(StackValueType::UNKNOWN);
				structTypeStack.push_back("");
				mHasUnpredictableStack = true;
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::LITERAL: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				typeStack.push_back(getLiteralStackType(lit->literalType()));
				break;
			}

			case IAstNode::Type::ARRAY_LITERAL: {
				// Array literal pushes a pointer (array reference) onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				const std::string& instrName = instr->name();

				// Check if instruction name shadows a local variable - if so, treat as variable reference
				auto localIt = localVarTypes.find(instrName);
				if (localIt != localVarTypes.end()) {
					// Push the local variable's type onto the stack
					typeStack.push_back(localIt->second);
					break;
				}

				// During signature analysis, don't report errors - just simulate the stack
				typeCheckInstructionInternal(child, instrName.c_str(), typeStack, structTypeStack, false);
				break;
			}

			case IAstNode::Type::BLOCK: {
				// Recursively analyze nested blocks
				analyzeBlockInIsolation(child, typeStack);
				break;
			}

			case IAstNode::Type::IDENTIFIER: {
				// Apply function signature if known (for iterative analysis)
				AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
				const std::string& name = ident->name();

				// First check if it's a local variable reference
				auto localIt = localVarTypes.find(name);
				if (localIt != localVarTypes.end()) {
					// Push the local variable's type
					typeStack.push_back(localIt->second);
					// Push the struct type if this is a struct variable
					auto structTypeIt = mLocalVariableStructTypes.find(name);
					if (structTypeIt != mLocalVariableStructTypes.end()) {
						structTypeStack.push_back(structTypeIt->second);
					} else {
						structTypeStack.push_back("");
					}
					break;
				}

				auto sigIt = mFunctionSignatures.find(name);
				if (sigIt != mFunctionSignatures.end()) {
					// Apply the known signature: pop consumes, push produces
					const FunctionSignature& sig = sigIt->second;
					// Pop consumed types
					for (size_t j = 0; j < sig.consumes.size() && !typeStack.empty(); j++) {
						typeStack.pop_back();
					}
					// Push produced types
					for (const auto& type : sig.produces) {
						typeStack.push_back(type);
					}
					break;
				}

				// Check if it's a struct construction
				if (mDefinedStructs.find(name) != mDefinedStructs.end()) {
					// Struct construction produces a pointer
					const auto* structFields = lookupStructFieldTypes(name);
					if (structFields != nullptr) {
						size_t fieldCount = structFields->size();
						// Pop field values from both stacks
						for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}
					}
					typeStack.push_back(StackValueType::PTR);
					structTypeStack.push_back(name); // Track the struct type
					break;
				}

				// Check if it's a struct from an imported module
				for (const auto& moduleEntry : mModuleStructs) {
					const std::string& moduleName = moduleEntry.first;
					const auto& structs = moduleEntry.second;
					if (structs.find(name) != structs.end()) {
						std::string qualifiedName = moduleName + "::" + name;
						const auto* structFields = lookupStructFieldTypes(qualifiedName);
						if (structFields != nullptr) {
							size_t fieldCount = structFields->size();
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}
						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(qualifiedName); // Track the qualified struct type
						break;
					}
				}

				// Check if it's a constant
				auto constIt = mConstantValues.find(name);
				if (constIt != mConstantValues.end()) {
					// Push the constant's type onto the stack
					StackValueType constType = getConstantType(constIt->second);
					typeStack.push_back(constType);
				}
				// Check if it's a module-level mutable `var` — push its
				// declared type. Lives on the runtime stack.
				auto gvIt = mGlobalVarTypes.find(name);
				if (gvIt != mGlobalVarTypes.end()) {
					typeStack.push_back(stringToStackValueType(gvIt->second));
				}
				// If signature not known yet, skip (will be resolved in next iteration)
				break;
			}

			case IAstNode::Type::FIELD_ACCESS: {
				// Field access pushes a value onto the stack
				// Detailed validation is done in the main type checking pass (case at line ~3266)
				// Here we just handle the stack effect for signature analysis
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
				const std::string& fieldName = fieldAccess->fieldName();

				StackValueType fieldType = StackValueType::UNKNOWN;
				// Search in all known structs to determine the field type
				for (const auto& structEntry : mStructFieldTypes) {
					const auto& fields = structEntry.second;
					auto it = fields.find(fieldName);
					if (it != fields.end()) {
						fieldType = it->second;
						break;
					}
				}

				// If still unknown, use ANY as fallback
				if (fieldType == StackValueType::UNKNOWN) {
					fieldType = StackValueType::ANY;
				}

				// The struct being read is on the stack (its producer is a separate node); replace it.
				if (!typeStack.empty()) {
					typeStack.pop_back();
				}
				typeStack.push_back(fieldType);
				break;
			}
			case IAstNode::Type::FIELD_SET: {
				// >>field : pop value, struct stays (net: -1)
				if (!typeStack.empty()) {
					typeStack.pop_back();
				}
				break;
			}
			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Apply module function signature if known
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);

				// In pass 1 (signature analysis), don't resolve sb::append_any —
				// just apply the same stack effect as sb::append (ptr, any -> ptr)
				if (scoped->scope() == "sb" && scoped->name() == "append_any") {
					if (!typeStack.empty()) {
						typeStack.pop_back(); // pop value
					}
					if (!typeStack.empty()) {
						typeStack.pop_back(); // pop sb ptr
					}
					typeStack.push_back(StackValueType::PTR); // push result sb ptr
					break;
				}

				const std::string& moduleName = scoped->scope();
				const std::string& functionName = scoped->name();
				std::string qualifiedName = moduleName + "::" + functionName;

				auto sigIt = mFunctionSignatures.find(qualifiedName);
				if (sigIt != mFunctionSignatures.end()) {
					// Apply the known signature: pop consumes, push produces
					const FunctionSignature& sig = sigIt->second;
					// Pop consumed types
					for (size_t j = 0; j < sig.consumes.size() && !typeStack.empty(); j++) {
						typeStack.pop_back();
					}
					// Push produced types
					for (const auto& type : sig.produces) {
						typeStack.push_back(type);
					}
				} else {
					// Try as a method call — search module's struct methods
					bool foundMethod = false;
					for (const auto& structEntry : mStructMethods) {
						const std::string& structType = structEntry.first;
						// Check if struct belongs to this module
						if (structType.find(moduleName + "::") != 0 &&
								!mStructMethods.count(moduleName + "::" + structType)) {
							// Also check unqualified struct names from this module
							bool belongsToModule = false;
							for (const auto& methodEntry : structEntry.second) {
								std::string methodKey = structType + "::" + methodEntry.first;
								if (mFunctionSignatures.count(methodKey)) {
									belongsToModule = true;
									break;
								}
							}
							if (!belongsToModule) {
								continue;
							}
						}
						if (structEntry.second.count(functionName)) {
							std::string methodKey = structType + "::" + functionName;
							auto methodSigIt = mFunctionSignatures.find(methodKey);
							if (methodSigIt != mFunctionSignatures.end()) {
								const FunctionSignature& sig = methodSigIt->second;
								for (size_t j = 0; j < sig.consumes.size() && !typeStack.empty(); j++) {
									typeStack.pop_back();
								}
								for (const auto& type : sig.produces) {
									typeStack.push_back(type);
								}
								foundMethod = true;
								break;
							}
						}
					}
					// If not found, skip (will be resolved in next iteration)
					(void)foundMethod;
				}
				break;
			}

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
				// Function pointer references push a pointer type onto the stack
				typeStack.push_back(StackValueType::PTR);
				break;

			case IAstNode::Type::ANONYMOUS_FUNCTION:
				// Anonymous functions push a function pointer onto the stack. The body is checked
				// by typeCheckAnonymousFunction, from the walk in typeCheckBlock; this walk is
				// measuring an enclosing block's stack effect and only needs what the lambda
				// leaves behind.
				typeStack.push_back(StackValueType::PTR);
				break;

			case IAstNode::Type::STRUCT_CONSTRUCTION: {
				// Struct construction with named fields: StructName { field: expr ... }
				// Field expressions are self-contained, so struct construction just pushes PTR
				AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(child);
				const std::string& structName = construct->structName();
				typeStack.push_back(StackValueType::PTR);

				// Check if struct is from a module - use qualified name for method resolution
				std::string qualifiedStructName = structName;
				if (structName.find("::") == std::string::npos) {
					// Unqualified name - check if it's from a module
					for (const auto& moduleEntry : mModuleStructs) {
						const auto& structs = moduleEntry.second;
						if (structs.find(structName) != structs.end() && structs.at(structName)) {
							qualifiedStructName = moduleEntry.first + "::" + structName;
							break;
						}
					}
				}
				structTypeStack.push_back(qualifiedStructName);
				break;
			}

			case IAstNode::Type::LOCAL: {
				// Local variable binding: pop value(s) from stack and store type(s)
				// Supports multiple assignment: -> a b c
				// Special case: -> _ discards the value (like drop)
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				for (const std::string& varName : local->names()) {
					if (!typeStack.empty()) {
						StackValueType varType = typeStack.back();
						typeStack.pop_back();
						// Don't store _ as a variable - it's a discard
						if (varName != "_") {
							localVarTypes[varName] = varType;
						}
					}
				}
				break;
			}

			default:
				// Other node types don't affect the type stack during signature analysis
				break;
			}
		}
	}

	void SemanticValidator::typeCheckFunction(IAstNode* node) {
		if (!node) {
			return;
		}

		// A body that referenced a removed builtin has already been diagnosed. Its
		// stack simulation would be off by the removed op's effect from that point
		// on, so every downstream underflow/arity error would be noise pointing at
		// lines the user must not change. Skip the simulation, keep the real error.
		if (mBodiesWithRemovedBuiltins.count(node)) {
			return;
		}

		// Type check each function definition
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* func = static_cast<AstNodeFunctionDeclaration*>(node);
			std::vector<StackValueType> typeStack;
			std::vector<std::string> structTypeStack;
			std::unordered_map<std::string, StackValueType> localVariables;

			// Set current type parameters for generic functions
			mCurrentTypeParams = func->typeParams();

			// Clear local variable struct types for this function
			mLocalVariableStructTypes.clear();

			// Initialize type stack with input parameters. They are auto-bound as local
			// variables at function entry unless the function is `stack fn`, where they
			// stay on the stack and any names they carry are documentation only.
			const bool bindParams = !func->isStack();
			for (size_t i = 0; i < func->inputParameters().size(); i++) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
				const std::string& typeStr = param->typeString();

				// Validate type name
				if (!isValidTypeName(typeStr)) {
					std::string where;
					if (param->hasName()) {
						where = " in parameter '" + param->name() + "'";
					}
					reportError(param, invalidTypeMessage(typeStr, where).c_str());
				}

				// Resolve type aliases
				std::string resolvedType = typeStr;
				auto tcAliasIt = mTypeAliases.find(typeStr);
				if (tcAliasIt != mTypeAliases.end()) {
					resolvedType = tcAliasIt->second;
				}

				StackValueType paramType = stringToStackValueType(resolvedType);
				std::string structType = "";

				// Track struct/array/fn types for PTR parameters
				if (paramType == StackValueType::PTR &&
						(isStructTypeName(resolvedType) ||
								(resolvedType.size() > 2 && resolvedType[0] == '[' && resolvedType[1] == ']') ||
								(resolvedType.size() > 3 && resolvedType.substr(0, 3) == "fn("))) {
					structType = resolvedType;
				}

				if (bindParams && param->hasName()) {
					// Named parameter: auto-bound as a local variable
					localVariables[param->name()] = paramType;
					if (!structType.empty()) {
						mLocalVariableStructTypes[param->name()] = structType;
					}
				} else {
					// `stack fn` parameter: stays on the stack
					typeStack.push_back(paramType);
					structTypeStack.push_back(structType);
				}
			}

			// Track whether current function is fallible (for panic validation)
			mCurrentFunctionFallible = func->throws();
			mCurrentFunctionOutputCount = func->outputParameters().size();
			// The variadic entry points consume values their signature does not declare -- the
			// pile `read` leaves below the frame, which nothing in the body names. Call sites
			// already treat them this way (see the same list in the SCOPED_IDENTIFIER case); the
			// bodies need it too, or their own loops read as consuming values that are not there.
			mHasUnpredictableStack = isVariadicStackConsumer(func->name(), mFilename);

			// For methods, register the receiver as a local variable (it's implicitly bound)
			if (func->hasReceiver()) {
				const std::string& receiverName = func->receiverName();
				const std::string& receiverType = func->receiverType();
				localVariables[receiverName] = StackValueType::PTR;
				mLocalVariableStructTypes[receiverName] = receiverType;
			}

			// Type check the function body
			if (func->body()) {
				typeCheckBlock(func->body(), typeStack, localVariables, structTypeStack);
			}

			mFinalStackFunction = func->name();
			mFinalStackTypes = typeStack;
			mFinalStackStructTypes = structTypeStack;

			// Warn about unused named parameters. A `stack fn` name is documentation and
			// is never referenced by definition, so there is nothing to warn about.
			if (func->body() && bindParams) {
				// Collect named param names
				std::unordered_set<std::string> paramNames;
				for (size_t i = 0; i < func->inputParameters().size(); i++) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
					if (param->hasName() && param->name()[0] != '_') {
						paramNames.insert(param->name());
					}
				}
				if (!paramNames.empty()) {
					std::unordered_set<std::string> refs;
					collectIdentifierRefs(func->body(), refs, paramNames);
					for (size_t i = 0; i < func->inputParameters().size(); i++) {
						AstNodeParameter* param = static_cast<AstNodeParameter*>(func->inputParameters()[i].get());
						if (param->hasName() && param->name()[0] != '_' && refs.find(param->name()) == refs.end()) {
							reportWarning(param, ("Unused parameter '" + param->name() + "'").c_str());
						}
					}
				}
			}

			// Validate that the type stack matches declared output parameters
			// Skip if the function body diverges (e.g., ends with panic or return)
			// Skip if stack effects are unpredictable (e.g., 'read' instruction or unhandled instructions)
			if (func->body() && !blockEndsDiverging(func->body()) && !mHasUnpredictableStack) {
				size_t expectedOutputs = func->outputParameters().size();
				size_t actualOutputs = typeStack.size();

				if (actualOutputs != expectedOutputs) {
					std::string errorMsg = "Function '";
					errorMsg += func->name();
					errorMsg += "' declares ";
					errorMsg += std::to_string(expectedOutputs);
					errorMsg += " output(s) but body leaves ";
					errorMsg += std::to_string(actualOutputs);
					errorMsg += " value(s) on the stack";
					reportError(func, errorMsg.c_str());
				} else if (actualOutputs == expectedOutputs) {
					// Check that types match
					for (size_t i = 0; i < expectedOutputs; i++) {
						AstNodeParameter* outParam = static_cast<AstNodeParameter*>(func->outputParameters()[i].get());
						StackValueType expectedType = stringToStackValueType(outParam->typeString());
						StackValueType actualType = typeStack[i];

						if (expectedType == StackValueType::ANY || expectedType == StackValueType::UNKNOWN ||
								expectedType == StackValueType::TYPEVAR) {
							continue;
						}
						if (actualType == StackValueType::UNKNOWN || actualType == StackValueType::ANY ||
								actualType == StackValueType::TYPEVAR) {
							continue;
						}

						if (actualType != expectedType) {
							std::string errorMsg = "Function '";
							errorMsg += func->name();
							errorMsg += "' output ";
							errorMsg += std::to_string(i + 1);
							errorMsg += " ('";
							errorMsg += outParam->name();
							errorMsg += "') expects ";
							errorMsg += stackValueTypeToString(expectedType);
							errorMsg += " but got ";
							errorMsg += stackValueTypeToString(actualType);
							reportError(func, errorMsg.c_str());
						} else if (actualType == StackValueType::PTR) {
							// Both are PTR — compare the pointer's real type structurally, with this
							// function's own type parameters as wildcards. Inside `fn map<T, U>(…
							// -- r:[]U)` the result is built from an empty `[]` literal, which is
							// `[]any`: U is unbound in the body, so it must accept that. The compare
							// here used to be a string equality over array types only, which
							// rejected it and skipped struct results entirely.
							std::string expectedStructType = outParam->typeString();
							std::string actualStructType = (i < structTypeStack.size()) ? structTypeStack[i] : "";
							std::map<std::string, std::string> outBindings;
							std::string outWhy;
							auto canon = [this](const std::string& n) { return canonicalStructName(n); };
							bool mismatch = !expectedStructType.empty() && !actualStructType.empty() &&
											!unifyTypeName(expectedStructType, actualStructType, mCurrentTypeParams,
													outBindings, mMergedModules, canon, outWhy);
							if (mismatch) {
								std::string errorMsg = "Function '";
								errorMsg += func->name();
								errorMsg += "' output '";
								errorMsg += outParam->name();
								errorMsg += "' expects type '";
								errorMsg += expectedStructType;
								errorMsg += "' but got '";
								errorMsg += actualStructType;
								errorMsg += "'";
								reportError(func, errorMsg.c_str());
							}
						}
					}
				}
			}

			// Reset function state
			mCurrentFunctionFallible = false;
			mCurrentFunctionOutputCount = 0;

			// Clear type parameters after type checking
			mCurrentTypeParams.clear();
		}

		// Type check test declarations
		if (node->type() == IAstNode::Type::TEST_DECLARATION) {
			AstNodeTest* test = static_cast<AstNodeTest*>(node);
			std::vector<StackValueType> typeStack;
			std::vector<std::string> structTypeStack;
			std::unordered_map<std::string, StackValueType> localVariables;

			// Clear local variable struct types for this test
			mLocalVariableStructTypes.clear();

			// Tests have no parameters - start with empty stack

			// Type check the test body
			if (test->body()) {
				typeCheckBlock(test->body(), typeStack, localVariables, structTypeStack);
			}
		}

		// Recursively process children
		for (auto* child : node->children()) {
			typeCheckFunction(child);
		}
	}

	void SemanticValidator::typeCheckTest(IAstNode* node) {
		// Tests are handled in typeCheckFunction along with functions
		// This method exists for potential future specialized test validation
		typeCheckFunction(node);
	}

	namespace {
		// Element type of an array literal, as a type *name* ("i64", "[]f64", "Point"), or an
		// empty string when this element's type cannot be decided here.
		//
		// The validator does not walk an array literal's children, so before nested literals
		// and non-literal elements worked this only ever looked at element zero and gave up
		// on anything that was not a scalar literal. That is why `[x 2 3]` typed as `[]any`
		// and could not be returned from a function declaring `[]i64`.
		std::string arrayElementTypeName(
				IAstNode* elem, const std::unordered_map<std::string, StackValueType>& localVariables) {
			if (elem == nullptr) {
				return "";
			}
			switch (elem->type()) {
			case IAstNode::Type::LITERAL: {
				auto* lit = static_cast<AstNodeLiteral*>(elem);
				switch (lit->literalType()) {
				case AstNodeLiteral::LiteralType::INTEGER:
				case AstNodeLiteral::LiteralType::BOOL:
					return "i64";
				case AstNodeLiteral::LiteralType::FLOAT:
					return "f64";
				case AstNodeLiteral::LiteralType::STRING:
					return "str";
				default:
					return "";
				}
			}
			case IAstNode::Type::STRUCT_CONSTRUCTION:
				return static_cast<AstNodeStructConstruction*>(elem)->structName();
			case IAstNode::Type::ARRAY_LITERAL: {
				auto* nested = static_cast<AstNodeArrayLiteral*>(elem);
				if (nested->hasElementType()) {
					return nested->declaredArrayType();
				}
				if (nested->elements().empty()) {
					return "[]any";
				}
				// Every element must agree, or the nested array's own type is unknown.
				std::string inner = arrayElementTypeName(nested->elements()[0].get(), localVariables);
				if (inner.empty()) {
					return "[]any";
				}
				for (size_t i = 1; i < nested->elements().size(); i++) {
					if (arrayElementTypeName(nested->elements()[i].get(), localVariables) != inner) {
						return "[]any";
					}
				}
				return "[]" + inner;
			}
			case IAstNode::Type::IDENTIFIER: {
				// A local's coarse stack type is enough for the scalar cases; a pointer could
				// be an array, a struct or a string-like, and nothing here can tell which.
				auto it = localVariables.find(static_cast<AstNodeIdentifier*>(elem)->name());
				if (it == localVariables.end()) {
					return "";
				}
				switch (it->second) {
				case StackValueType::INT:
					return "i64";
				case StackValueType::FLOAT:
					return "f64";
				case StackValueType::STRING:
					return "str";
				default:
					return "";
				}
			}
			default:
				return "";
			}
		}

		// Type of an array literal itself: "[]i64", "[][]f64", "[]Point", or "[]any" when the
		// elements do not agree on one or cannot be decided here.
		std::string arrayLiteralTypeName(
				AstNodeArrayLiteral* arrLit, const std::unordered_map<std::string, StackValueType>& localVariables) {
			if (arrLit->hasElementType()) {
				return arrLit->declaredArrayType();
			}
			if (arrLit->elements().empty()) {
				return "[]any";
			}
			std::string elemType = arrayElementTypeName(arrLit->elements()[0].get(), localVariables);
			for (size_t e = 1; e < arrLit->elements().size() && !elemType.empty(); e++) {
				if (arrayElementTypeName(arrLit->elements()[e].get(), localVariables) != elemType) {
					elemType.clear();
				}
			}
			return elemType.empty() ? "[]any" : "[]" + elemType;
		}
	} // namespace

	// A loop body has to leave the stack exactly as it found it: the next iteration starts where
	// the last one ended, so a body with a non-zero effect makes the depth at the loop head a
	// function of the trip count, which is a runtime value.
	//
	// The exits are the other half. `for` can leave by running off the end of its range, at the
	// head depth, so each of its `break`s has to agree with that. `loop` has no fall-through exit,
	// so its `break`s are the only way out and they are what define the depth after the loop --
	// `0 loop { ... dup 10 gt if { drop break } ... }` carries a counter on the stack and drops it
	// on the way out, a net effect of -1 that is perfectly well defined. They only have to agree
	// with each other. A `continue` jumps back to the head, so it must be at the head depth in
	// both forms.
	//
	// Nothing checked any of this before. `loop` instead declared the whole enclosing function to
	// have an unpredictable stack, which switched off the declared-effect check, the if-arm
	// balance check and the defer-effect check for every function containing one -- most
	// non-trivial functions in the corpus, with `loop` at 182 uses and `while` gone. A
	// `( -- r:i64)` function whose loop body pushed an unconsumed value each iteration compiled
	// clean and returned with junk on the stack.
	void SemanticValidator::checkLoopStackEffect(IAstNode* body, const char* loopKeyword, bool exitsByFallthrough,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, size_t bodyEndDepth) {
		// Something in the body already defeated the model (an unresolved name, `read`, an FFI
		// call); its depth numbers are not evidence of anything.
		if (mHasUnpredictableStack || !body) {
			return;
		}

		const size_t headDepth = typeStack.size();

		auto describe = [](size_t depth, size_t against) {
			long long delta = static_cast<long long>(depth) - static_cast<long long>(against);
			std::string s = std::to_string(delta < 0 ? -delta : delta);
			s += " value";
			if (delta != 1 && delta != -1) {
				s += "s";
			}
			return s + (delta > 0 ? " deeper" : " shallower");
		};

		if (!blockEndsDiverging(body) && bodyEndDepth != headDepth) {
			std::string msg = std::string(loopKeyword) + " body leaves the stack " + describe(bodyEndDepth, headDepth) +
							  " than it found it; a loop body must leave the stack as it found it, since the "
							  "next iteration starts where the last one ended (bind what you are accumulating "
							  "to a named local instead)";
			reportErrorConditional(body, msg.c_str(), true);
		}

		const LoopJump* firstBreak = nullptr;
		for (const auto& jump : mLoopJumps) {
			if (!jump.isBreak) {
				if (jump.typeStack.size() != headDepth) {
					std::string msg = std::string("'continue' leaves the stack ") +
									  describe(jump.typeStack.size(), headDepth) + " than the " + loopKeyword +
									  " head; 'continue' jumps back to the head, so it has to leave the stack "
									  "the way an iteration starts";
					reportErrorConditional(jump.node, msg.c_str(), true);
				}
				continue;
			}
			if (exitsByFallthrough) {
				if (jump.typeStack.size() != headDepth) {
					std::string msg = std::string("'break' leaves the stack ") +
									  describe(jump.typeStack.size(), headDepth) + " than the " + loopKeyword +
									  " head; a '" + loopKeyword +
									  "' also exits by running off the end of its range, and what follows cannot "
									  "read a value whose presence depends on which way it left";
					reportErrorConditional(jump.node, msg.c_str(), true);
				}
				continue;
			}
			if (!firstBreak) {
				firstBreak = &jump;
			} else if (jump.typeStack.size() != firstBreak->typeStack.size()) {
				std::string msg = std::string("'break' leaves the stack ") +
								  describe(jump.typeStack.size(), firstBreak->typeStack.size()) +
								  " than another 'break' in the same loop; every way out has to agree, since "
								  "what follows cannot read a value whose presence depends on which one ran";
				reportErrorConditional(jump.node, msg.c_str(), true);
			}
		}

		// A `loop` leaves the stack the way its breaks did. With no break it never falls out at
		// all, so whatever the parent had still stands.
		if (!exitsByFallthrough && firstBreak) {
			typeStack = firstBreak->typeStack;
			structTypeStack = firstBreak->structTypeStack;
		}
	}

	// An anonymous function's body is a function body: it starts on an empty stack, its named
	// parameters are bound as locals the way a named function's are, and what it leaves has to
	// match what its own signature declares. Nothing checked it before -- the case in
	// typeCheckBlock pushed a PTR and moved on, under a comment saying code generation would
	// validate the body, which it does not -- so `fn (x:i64 -- r:i64) { 2 * }` compiled and
	// underflowed at run time where the identical body in a named function is a compile error.
	void SemanticValidator::typeCheckAnonymousFunction(AstNodeAnonymousFunction* anonFunc,
			const std::unordered_map<std::string, StackValueType>& enclosingLocals) {
		if (!anonFunc || !anonFunc->body()) {
			return;
		}
		// The same lambda can be reached more than once: a module's AST is validated again for
		// each importer, and a value can be walked both as a statement and as an initializer.
		if (!mCheckedAnonFunctions.insert(anonFunc).second) {
			return;
		}
		// Same reason typeCheckFunction skips these: past a removed builtin the simulation is
		// off by whatever the name used to push, and every error after it points at lines the
		// user must not change.
		if (mBodiesWithRemovedBuiltins.count(anonFunc->body()) || mBodiesWithRemovedBuiltins.count(anonFunc)) {
			return;
		}

		// Everything saved here belongs to the enclosing function, whose own walk is in progress
		// and resumes as soon as this one is done.
		auto savedStructTypes = mLocalVariableStructTypes;
		const bool savedFallible = mCurrentFunctionFallible;
		const size_t savedOutputCount = mCurrentFunctionOutputCount;
		const bool savedUnpredictable = mHasUnpredictableStack;
		const bool savedInLoopBody = mInLoopBody;
		auto savedLoopJumps = mLoopJumps;
		auto savedPendingSig = mPendingFnSignature;
		const IAstNode* savedProbeBlock = mDepthProbeBlock;
		std::vector<size_t>* savedProbe = mDepthProbe;

		mCurrentFunctionOutputCount = anonFunc->outputParameters().size();
		// A lambda's own `!` is what makes its body fallible. Inheriting the enclosing function's
		// would let a `panic` inside a plain lambda pass validation and then be compiled by a
		// generator that knows the lambda cannot throw.
		mCurrentFunctionFallible = anonFunc->throws();
		mHasUnpredictableStack = false;
		// A lambda written inside a loop body is not itself a loop body: its stack starts at a
		// known depth, so its errors are evidence and are worth reporting.
		mInLoopBody = false;
		mLoopJumps.clear();
		mDepthProbeBlock = nullptr;
		mDepthProbe = nullptr;
		mPendingFnSignature.reset();

		std::vector<StackValueType> typeStack;
		std::vector<std::string> structTypeStack;
		std::unordered_map<std::string, StackValueType> localVariables;

		// A captured variable keeps the type it has where it was captured. Its struct type is
		// already in mLocalVariableStructTypes, which is why that map is carried in rather than
		// cleared.
		for (const std::string& captured : anonFunc->capturedVariables()) {
			auto capturedIt = enclosingLocals.find(captured);
			localVariables[captured] =
					(capturedIt != enclosingLocals.end()) ? capturedIt->second : StackValueType::UNKNOWN;
		}

		// Whether inputs are bound is carried by `stack`, exactly as it is for a named function:
		// without it every input must be named and is bound as a local; with it they stay on the
		// stack and any names are documentation.
		const bool bindParams = !anonFunc->isStack();
		if (anonFunc->isStack() && anonFunc->inputParameters().empty()) {
			reportError(anonFunc, "'stack fn' takes no input parameters, so there is nothing to leave on the stack");
		}
		for (const auto& paramNode : anonFunc->inputParameters()) {
			AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
			if (bindParams && !param->hasName()) {
				std::string errorMsg = "Unnamed input parameter '" + param->typeString() +
									   "' in an anonymous function. Name it ('value:" + param->typeString() +
									   "'), or write 'stack fn' to leave the arguments on the stack for the "
									   "body to work on";
				reportError(param, errorMsg.c_str());
			}
			if (param->hasName() && isReservedKeyword(param->name())) {
				std::string errorMsg =
						"'" + param->name() + "' is a reserved keyword and cannot be used as a parameter name";
				reportError(param, errorMsg.c_str());
			}
			const std::string sizedAnonParam = sizedIntTypeThroughAliases(param->typeString());
			if (!sizedAnonParam.empty()) {
				reportError(param, sizedTypeMisuseMessage(param->typeString(), sizedAnonParam, "a parameter").c_str());
			}
			if (!isValidTypeName(param->typeString())) {
				std::string where;
				if (param->hasName()) {
					where = " in parameter '" + param->name() + "'";
				}
				reportError(param, invalidTypeMessage(param->typeString(), where).c_str());
			}

			std::string resolvedType = param->typeString();
			auto aliasIt = mTypeAliases.find(resolvedType);
			if (aliasIt != mTypeAliases.end()) {
				resolvedType = aliasIt->second;
			}
			const StackValueType paramType = stringToStackValueType(resolvedType);
			std::string structType;
			if (paramType == StackValueType::PTR &&
					(isStructTypeName(resolvedType) ||
							(resolvedType.size() > 2 && resolvedType[0] == '[' && resolvedType[1] == ']') ||
							(resolvedType.size() > 3 && resolvedType.substr(0, 3) == "fn("))) {
				structType = resolvedType;
			}
			if (bindParams && param->hasName()) {
				localVariables[param->name()] = paramType;
				if (structType.empty()) {
					// A parameter shadows a capture of the same name, including its struct type.
					mLocalVariableStructTypes.erase(param->name());
				} else {
					mLocalVariableStructTypes[param->name()] = structType;
				}
			} else {
				typeStack.push_back(paramType);
				structTypeStack.push_back(structType);
			}
		}
		for (const auto& paramNode : anonFunc->outputParameters()) {
			AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
			const std::string sizedAnonOutput = sizedIntTypeThroughAliases(param->typeString());
			if (!sizedAnonOutput.empty()) {
				reportError(
						param, sizedTypeMisuseMessage(param->typeString(), sizedAnonOutput, "a return value").c_str());
			}
			if (!isValidTypeName(param->typeString())) {
				std::string where;
				if (param->hasName()) {
					where = " in output '" + param->name() + "'";
				}
				reportError(param, invalidTypeMessage(param->typeString(), where).c_str());
			}
		}

		typeCheckBlock(anonFunc->body(), typeStack, localVariables, structTypeStack);

		if (!blockEndsDiverging(anonFunc->body()) && !mHasUnpredictableStack) {
			const size_t expectedOutputs = anonFunc->outputParameters().size();
			if (typeStack.size() != expectedOutputs) {
				std::string errorMsg = "Anonymous function declares ";
				errorMsg += std::to_string(expectedOutputs);
				errorMsg += " output(s) but body leaves ";
				errorMsg += std::to_string(typeStack.size());
				errorMsg += " value(s) on the stack";
				reportError(anonFunc, errorMsg.c_str());
			} else {
				for (size_t i = 0; i < expectedOutputs; i++) {
					AstNodeParameter* outParam = static_cast<AstNodeParameter*>(anonFunc->outputParameters()[i].get());
					const StackValueType expectedType = stringToStackValueType(outParam->typeString());
					const StackValueType actualType = typeStack[i];
					if (expectedType == StackValueType::ANY || expectedType == StackValueType::UNKNOWN ||
							expectedType == StackValueType::TYPEVAR || actualType == StackValueType::ANY ||
							actualType == StackValueType::UNKNOWN || actualType == StackValueType::TYPEVAR) {
						continue;
					}
					if (actualType != expectedType) {
						std::string errorMsg = "Anonymous function output ";
						errorMsg += std::to_string(i + 1);
						errorMsg += " expects ";
						errorMsg += stackValueTypeToString(expectedType);
						errorMsg += " but got ";
						errorMsg += stackValueTypeToString(actualType);
						reportError(anonFunc, errorMsg.c_str());
					} else if (actualType == StackValueType::PTR) {
						// Both are PTR, so compare the pointer's real type, with the enclosing
						// function's type parameters as wildcards -- the same compare a named
						// function's outputs get.
						const std::string expectedStructType = outParam->typeString();
						const std::string actualStructType = (i < structTypeStack.size()) ? structTypeStack[i] : "";
						std::map<std::string, std::string> outBindings;
						std::string outWhy;
						auto canon = [this](const std::string& n) { return canonicalStructName(n); };
						const bool mismatch = !expectedStructType.empty() && !actualStructType.empty() &&
											  !unifyTypeName(expectedStructType, actualStructType, mCurrentTypeParams,
													  outBindings, mMergedModules, canon, outWhy);
						if (mismatch) {
							std::string errorMsg = "Anonymous function output ";
							errorMsg += std::to_string(i + 1);
							errorMsg += " expects type '";
							errorMsg += expectedStructType;
							errorMsg += "' but got '";
							errorMsg += actualStructType;
							errorMsg += "'";
							reportError(anonFunc, errorMsg.c_str());
						}
					}
				}
			}
		}

		// Unused named parameters, warned about the way a named function's are. Under `stack` the
		// names are documentation and are never referenced, so there is nothing to warn about.
		if (bindParams) {
			std::unordered_set<std::string> paramNames;
			for (const auto& paramNode : anonFunc->inputParameters()) {
				AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
				if (param->hasName() && param->name()[0] != '_') {
					paramNames.insert(param->name());
				}
			}
			if (!paramNames.empty()) {
				std::unordered_set<std::string> refs;
				collectIdentifierRefs(anonFunc->body(), refs, paramNames);
				for (const auto& paramNode : anonFunc->inputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					if (param->hasName() && param->name()[0] != '_' && refs.find(param->name()) == refs.end()) {
						reportWarning(param, ("Unused parameter '" + param->name() + "'").c_str());
					}
				}
			}
		}

		mLocalVariableStructTypes = savedStructTypes;
		mCurrentFunctionFallible = savedFallible;
		mCurrentFunctionOutputCount = savedOutputCount;
		mHasUnpredictableStack = savedUnpredictable;
		mInLoopBody = savedInLoopBody;
		mLoopJumps = savedLoopJumps;
		mPendingFnSignature = savedPendingSig;
		mDepthProbeBlock = savedProbeBlock;
		mDepthProbe = savedProbe;
	}

	void SemanticValidator::checkAnonymousFunctionsWithin(
			IAstNode* node, const std::unordered_map<std::string, StackValueType>& enclosingLocals) {
		if (!node) {
			return;
		}
		if (node->type() == IAstNode::Type::ANONYMOUS_FUNCTION) {
			// Its own walk reaches anything nested inside it, with the right scope.
			typeCheckAnonymousFunction(static_cast<AstNodeAnonymousFunction*>(node), enclosingLocals);
			return;
		}
		for (auto* child : node->children()) {
			checkAnonymousFunctionsWithin(child, enclosingLocals);
		}
	}

	// `[n]T` has to name a type whose zero exists. The scalars and `ptr` always have one. A
	// struct has one when every field has a default, because the form means n distinct
	// `T {}` and that is exactly when `T {}` is legal -- which also puts the decision about
	// whether a type has a sensible zero on the struct's author rather than on the array. A
	// type parameter of the enclosing generic is erased, so its answer comes at run time from
	// an array that adopts its element type on first append.
	void SemanticValidator::validateSizedArrayLiteral(IAstNode* literal) {
		auto* lit = static_cast<AstNodeArrayLiteral*>(literal);
		const std::string& elem = lit->elementType();
		if (elem.empty()) {
			return;
		}

		static const char* const kScalarTypes[] = {"i64", "i32", "i16", "i8", "u64", "u32", "u16", "u8", "f64", "f32",
				"str", "string", "ptr", "bool", "any"};
		for (const char* scalar : kScalarTypes) {
			if (elem == scalar) {
				return;
			}
		}
		if (isTypeParamName(elem, mCurrentTypeParams)) {
			return;
		}

		// An array element type -- `[n][]i64`, `[n][][]Point` -- makes n empty arrays, and an
		// empty array constructs nothing, so there is no zero to ask the innermost type for.
		// What has to hold is that the type names something, which isValidTypeName answers by
		// recursing the same way the parser did to read it.
		if (elem.size() > 2 && elem[0] == '[' && elem[1] == ']') {
			if (!isValidTypeName(elem)) {
				std::string errorMsg = "Unknown element type \'";
				errorMsg += elem;
				errorMsg += "\' in array literal";
				reportError(lit, errorMsg.c_str());
			}
			return;
		}

		std::string resolved = elem;
		auto aliasIt = mTypeAliases.find(resolved);
		if (aliasIt != mTypeAliases.end()) {
			resolved = aliasIt->second;
		}

		const auto* fieldTypes = lookupStructFieldTypes(resolved);
		if (fieldTypes == nullptr) {
			std::string errorMsg = "Unknown element type \'";
			errorMsg += elem;
			errorMsg += "\' in array literal";
			reportError(lit, errorMsg.c_str());
			return;
		}

		// With the size left out -- `[]Point` -- there are zero elements and nothing is ever
		// constructed, so the struct needs no zero. This is the form to reach for when a type
		// has no sensible default, and requiring one here would have made it unreachable. The
		// type still has to name something, which is why this sits below that check.
		if (lit->elements().empty()) {
			return;
		}

		const std::unordered_set<std::string>* fieldsWithDefaults = nullptr;
		auto defaultsIt = mStructFieldsWithDefaults.find(resolved);
		if (defaultsIt != mStructFieldsWithDefaults.end()) {
			fieldsWithDefaults = &defaultsIt->second;
		}
		for (const auto& fieldEntry : *fieldTypes) {
			const bool hasDefault = fieldsWithDefaults != nullptr && fieldsWithDefaults->count(fieldEntry.first) > 0;
			if (!hasDefault) {
				std::string errorMsg = "Missing field \'";
				errorMsg += fieldEntry.first;
				errorMsg += "\' in struct construction \'";
				errorMsg += elem;
				errorMsg += "\': \'[n]";
				errorMsg += elem;
				errorMsg += "\' builds n of them, so every field needs a default";
				reportError(lit, errorMsg.c_str());
			}
		}
	}

	void SemanticValidator::typeCheckBlock(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		if (!node) {
			return;
		}

		// Process each child in the block (using index-based loop for peek-ahead access)
		for (size_t i = 0; i < node->childCount(); i++) {
			IAstNode* child = node->child(i);
			if (!child) {
				continue;
			}
			if (mDepthProbe != nullptr && node == mDepthProbeBlock) {
				mDepthProbe->push_back(typeStack.size());
			}
			if (mUndefinedNodes.count(child)) {
				typeStack.push_back(StackValueType::UNKNOWN);
				structTypeStack.push_back("");
				mHasUnpredictableStack = true;
				continue;
			}

			switch (child->type()) {
			case IAstNode::Type::LITERAL: {
				AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(child);
				if (lit->literalType() == AstNodeLiteral::LiteralType::INTEGER) {
					std::string problem = integerLiteralProblem(lit->value());
					if (!problem.empty()) {
						reportError(lit, problem.c_str());
					}
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::FLOAT) {
					std::string problem = floatLiteralProblem(lit->value());
					if (!problem.empty()) {
						reportError(lit, problem.c_str());
					}
				}
				typeStack.push_back(getLiteralStackType(lit->literalType()));
				structTypeStack.push_back("");
				break;
			}

			case IAstNode::Type::ARRAY_LITERAL: {
				// Array literal pushes a pointer (array reference) onto the stack
				typeStack.push_back(StackValueType::PTR);
				// Infer the element type from the elements. They must all agree: a literal
				// array is one array, and the runtime adopts a single element type from the
				// first value that goes in (QD_ARRAY_TYPE_ANY), so a mixed literal is not a
				// thing that can be built. Anything undecidable lands on "[]any", which is
				// the permissive answer and matches an empty literal.
				AstNodeArrayLiteral* arrLit = static_cast<AstNodeArrayLiteral*>(child);
				checkAnonymousFunctionsWithin(arrLit, localVariables);
				// The `[size]T` form declares its element type, so there is nothing to infer:
				// what is inside the brackets is the size expression, not elements.
				if (arrLit->hasElementType()) {
					validateSizedArrayLiteral(arrLit);
				}
				structTypeStack.push_back(arrayLiteralTypeName(arrLit, localVariables));
				break;
			}

			case IAstNode::Type::INSTRUCTION: {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(child);
				const std::string& instrName = instr->name();

				// Check if instruction name shadows a local variable - if so, treat as variable reference
				auto localIt = localVariables.find(instrName);
				if (localIt != localVariables.end()) {
					// Push the local variable's type onto the stack (variable shadowing)
					typeStack.push_back(localIt->second);
					if (localIt->second == StackValueType::PTR) {
						auto structTypeIt = mLocalVariableStructTypes.find(instrName);
						if (structTypeIt != mLocalVariableStructTypes.end()) {
							structTypeStack.push_back(structTypeIt->second);
						} else {
							structTypeStack.push_back("");
						}
					} else {
						structTypeStack.push_back("");
					}
					break;
				}

				// Check if this is a method call on a struct
				// Methods require: receiver pushed first, then parameters on top
				// e.g., for method set(idx, elem): v idx elem set!
				std::string receiverStructType;
				std::string registeredStructType;

				// First pass: find any struct on the stack that has this method
				// to determine the method signature and expected parameter count
				size_t searchLimit = std::min(structTypeStack.size(), typeStack.size());
				for (size_t idx = searchLimit; idx > 0; idx--) {
					const std::string& structType = structTypeStack[idx - 1];
					if (!structType.empty()) {
						std::string potentialRegisteredType = findMethodStructType(structType, instrName);
						if (!potentialRegisteredType.empty()) {
							registeredStructType = potentialRegisteredType;
							break;
						}
					}
				}

				if (!registeredStructType.empty()) {
					// This is a method call - look up the signature with mangled name
					std::string mangledName = registeredStructType + "::" + instrName;
					auto methodSigIt = mFunctionSignatures.find(mangledName);
					if (methodSigIt != mFunctionSignatures.end()) {
						const FunctionSignature& sig = methodSigIt->second;

						// Calculate expected receiver position based on parameter count
						// With receiver-first: receiver should be at position additionalParams from top
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;

						// Second pass: check if there's a valid receiver at the expected position
						// Expected receiver index in stack (0-indexed from bottom)
						// For additionalParams=1 and stack [receiver, param], receiver is at index 0
						if (typeStack.size() <= additionalParams) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += instrName;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(instr, errorMsg.c_str());
							break;
						}

						size_t expectedReceiverIdx = typeStack.size() - 1 - additionalParams;

						// Verify the struct at expected position has this method
						bool foundValidReceiver = false;
						if (expectedReceiverIdx < structTypeStack.size()) {
							const std::string& structAtExpectedPos = structTypeStack[expectedReceiverIdx];
							std::string actualRegisteredType = findMethodStructType(structAtExpectedPos, instrName);
							if (!actualRegisteredType.empty()) {
								// Valid receiver-first call
								receiverStructType = structAtExpectedPos;
								registeredStructType = actualRegisteredType;
								foundValidReceiver = true;
							}
						}

						if (!foundValidReceiver) {
							std::string errorMsg = "Method call '";
							errorMsg += instrName;
							errorMsg += "': receiver must be pushed first, then ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " parameter(s) on top (e.g., 'receiver";
							for (size_t p = 0; p < additionalParams; p++) {
								errorMsg += " param";
							}
							errorMsg += " ";
							errorMsg += instrName;
							errorMsg += "')";
							reportError(instr, errorMsg.c_str());
							break;
						}

						// receiverPositionFromTop equals additionalParams by construction
						// (we verified the receiver is at the expected position above)
						size_t receiverPositionFromTop = additionalParams;

						// Save consumed struct types before popping (for pass-through tracking)
						std::vector<std::string> consumedStructTypes;
						if (sig.consumes.size() > 0 && structTypeStack.size() >= sig.consumes.size()) {
							size_t startIdx = structTypeStack.size() - sig.consumes.size();
							for (size_t j = 0; j < sig.consumes.size(); j++) {
								consumedStructTypes.push_back(structTypeStack[startIdx + j]);
							}
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values, with whatever the receiver's type arguments bound
						pushCallResults(sig, receiverTypeBindings(receiverStructType), consumedStructTypes, typeStack,
								structTypeStack);

						// Mark instruction as a method call for code generation
						// Use registeredStructType (the generic type) for proper function name lookup
						instr->setIsMethodCall(true);
						instr->setReceiverType(registeredStructType);
						instr->setMethodInputParamCount(additionalParams);
						// Use the receiver position calculated earlier (before stack modifications)
						instr->setMethodReceiverPositionFromTop(receiverPositionFromTop);
						break;
					}
				}

				// A literal zero *integer* divisor is knowable now, and reaching it at run time
				// is a fatal error that kills the process. Report it here instead, the way C,
				// Go and Rust all do. Only the immediately preceding literal is examined --
				// this is a peephole check, not constant propagation, so `0 -> z x z /`
				// still fails at run time.
				//
				// Floats are deliberately excluded: `x 0.0 /` is a defined IEEE 754 operation
				// yielding +/-infinity (or NaN for 0.0/0.0), not an error. The integer rule had
				// been applied to them, which made the language contradict itself -- the inlined
				// float path emits a bare fdiv and produces `inf`, so `1.0 0.0 d` through a
				// typed function already returned infinity while `1.0 z /` on a local killed the
				// process.
				if ((instrName == "div" || instrName == "/" || instrName == "mod" || instrName == "%") && i > 0) {
					IAstNode* prev = node->child(i - 1);
					if (prev != nullptr && prev->type() == IAstNode::Type::LITERAL) {
						auto* lit = static_cast<AstNodeLiteral*>(prev);
						const std::string& text = lit->value();
						const bool zeroInt = lit->literalType() == AstNodeLiteral::LiteralType::INTEGER && text == "0";
						if (zeroInt) {
							std::string errorMsg = "Division by zero in '";
							errorMsg += instrName;
							errorMsg += "': the divisor is the literal ";
							errorMsg += text;
							reportError(child, errorMsg.c_str());
						}
					}
				}

				// Fall back to builtin instruction handling
				typeCheckInstruction(child, instrName.c_str(), typeStack, structTypeStack);
				break;
			}

			case IAstNode::Type::BLOCK: {
				// Recursively check nested blocks
				typeCheckBlock(child, typeStack, localVariables, structTypeStack);
				break;
			}

			case IAstNode::Type::IF_STATEMENT: {
				// Check that stack has a condition value
				if (typeStack.empty()) {
					reportErrorWithHint(child, "Type error in 'if': Stack underflow (requires 1 condition value)",
							"push a condition before 'if', e.g. 'x 0 > if { ... }'");
					break;
				}
				// Pop the condition value
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}

				// Analyze branches to track stack effects
				AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(child);
				IAstNode* thenBody = ifStmt->thenBody();
				IAstNode* elseBody = ifStmt->elseBody();

				if (thenBody && elseBody) {
					// A bare fallible call before this `if` leaves its results for the success arm
					// only; the failure arm starts from the stack as it was before the call.
					const FunctionSignature* fallible = bareFallibleCallBefore(node, i);

					// Analyze then branch
					std::vector<StackValueType> thenStack = typeStack;
					std::unordered_map<std::string, StackValueType> thenVars = localVariables;
					std::vector<std::string> thenStructStack = structTypeStack;
					typeCheckBlock(thenBody, thenStack, thenVars, thenStructStack);

					// Analyze else branch
					std::vector<StackValueType> elseStack = typeStack;
					std::unordered_map<std::string, StackValueType> elseVars = localVariables;
					std::vector<std::string> elseStructStack = structTypeStack;
					if (fallible) {
						removeProducedValues(*fallible, elseStack, elseStructStack);
					}
					typeCheckBlock(elseBody, elseStack, elseVars, elseStructStack);

					// A diverging arm never reaches the merge point and contributes no stack effect.
					bool thenDiverges = blockEndsDiverging(thenBody);
					bool elseDiverges = blockEndsDiverging(elseBody);

					if (thenDiverges && elseDiverges) {
						// Both arms diverge -- nothing reaches the code after the `if`.
					} else if (thenDiverges) {
						typeStack = elseStack;
						structTypeStack = elseStructStack;
					} else if (elseDiverges) {
						typeStack = thenStack;
						structTypeStack = thenStructStack;
					} else if (thenStack.size() != elseStack.size()) {
						// The arms leave the stack at different depths, so whatever follows reads a
						// value whose identity depends on which arm ran -- and codegen silently
						// truncates to the shallower arm. Only report when the model is trustworthy:
						// mHasUnpredictableStack means something in this function consumed an amount
						// its signature does not describe, so the effects were computed from a model
						// already known to be wrong.
						if (!mHasUnpredictableStack) {
							std::string msg = "if/else arms leave the stack at different depths (then: " +
											  std::to_string(thenStack.size()) +
											  " value(s), else: " + std::to_string(elseStack.size()) +
											  "); both arms must leave the same depth, since whatever follows "
											  "reads a value whose identity would otherwise depend on which arm ran";
							if (fallible) {
								reportErrorWithHint(child, msg.c_str(),
										"the failure arm never receives the call's result(s); consume or drop them "
										"in the success arm so both arms leave the same depth");
							} else {
								reportError(child, msg.c_str());
							}
						}
						// Use the then (success) arm as authoritative.
						typeStack = thenStack;
						structTypeStack = thenStructStack;
					} else {
						// Balanced arms: the stack after the `if` is what either arm left, including
						// any values the arms changed in place. Keeping the pre-`if` types here was what
						// made `x if { -> s  s parse } else { ... }` report the *input's* type as the
						// function's output.
						typeStack = thenStack;
						structTypeStack = thenStructStack;
					}
				} else if (thenBody) {
					const FunctionSignature* fallible = bareFallibleCallBefore(node, i);
					std::vector<StackValueType> thenStack = typeStack;
					std::unordered_map<std::string, StackValueType> thenVars = localVariables;
					std::vector<std::string> thenStructStack = structTypeStack;
					typeCheckBlock(thenBody, thenStack, thenVars, thenStructStack);

					if (fallible) {
						// `call if { ... }` with no else: on failure nothing was produced and nothing
						// runs, so what follows sees the stack as it was before the call. The success
						// arm must therefore consume the results (or diverge) to leave that same depth.
						removeProducedValues(*fallible, typeStack, structTypeStack);
						if (!blockEndsDiverging(thenBody) && thenStack.size() != typeStack.size() &&
								!mHasUnpredictableStack) {
							std::string msg = "success arm of a fallible call leaves the stack at a different depth (" +
											  std::to_string(thenStack.size()) +
											  " value(s)) than the not-taken failure path (" +
											  std::to_string(typeStack.size()) + " value(s))";
							reportErrorWithHint(child, msg.c_str(),
									"consume or drop the call's result(s) inside the arm, or add an else arm");
						}
					} else if (!blockEndsDiverging(thenBody) && !mHasUnpredictableStack &&
							   thenStack.size() != typeStack.size()) {
						// An `if` with no `else` is an `if` whose other arm is empty, so the same rule
						// applies: the arm has to leave the stack as it found it. This used to be
						// unconstrained, on the reasoning that the enclosing function's declared effect
						// would govern -- but the arm's effect was never applied to the model, so that
						// check ran against a stack that pretended the arm did nothing. A body could
						// leave values behind and nothing anywhere reported it, which is how fourteen
						// guards in `ct` came to push their error message onto the stack as data and
						// fall through into the code they were guarding.
						const long long delta =
								static_cast<long long>(thenStack.size()) - static_cast<long long>(typeStack.size());
						std::string msg = "'if' without an 'else' leaves the stack " +
										  std::to_string(delta < 0 ? -delta : delta) + " value" +
										  ((delta == 1 || delta == -1) ? "" : "s") +
										  (delta > 0 ? " deeper" : " shallower") +
										  " than it found it; the arm may not run, so what follows cannot "
										  "read a value whose presence depends on it";
						reportErrorWithHint(child, msg.c_str(),
								"consume what the arm produces inside it, or add an 'else' arm that leaves "
								"the same depth");
					}
					// The pre-`if` stack stands: the arm is neutral, or the error above was reported.
				}
				break;
			}

			case IAstNode::Type::DEFER_STATEMENT: {
				// Defer blocks must have zero net stack effect
				size_t stackSizeBefore = typeStack.size();

				// Type check the defer body
				for (auto* deferChild : child->children()) {
					if (deferChild && deferChild->type() == IAstNode::Type::BLOCK) {
						typeCheckBlock(deferChild, typeStack, localVariables, structTypeStack);
					}
				}

				int deferEffect = static_cast<int>(typeStack.size()) - static_cast<int>(stackSizeBefore);
				if (deferEffect != 0 && !mHasUnpredictableStack) {
					std::string errorMsg = "Stack effect error in 'defer': block must have zero net stack effect, but "
										   "changes stack by ";
					errorMsg += std::to_string(deferEffect);
					reportError(child, errorMsg.c_str());
				}

				// Restore stack to before defer (defer shouldn't affect the surrounding stack)
				while (typeStack.size() > stackSizeBefore) {
					typeStack.pop_back();
					if (!structTypeStack.empty()) {
						structTypeStack.pop_back();
					}
				}
				break;
			}

			case IAstNode::Type::SWITCH_STATEMENT: {
				// Check that stack has a value to switch on
				if (typeStack.empty()) {
					reportErrorWithHint(child, "Type error in 'switch': Stack underflow (requires 1 value to match)",
							"push a value before 'switch', e.g. 'x switch { case 1 { ... } }'");
					break;
				}
				// Pop the switch value
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}

				// Analyze all case branches to track stack effects
				AstNodeSwitchStatement* switchStmt = static_cast<AstNodeSwitchStatement*>(child);
				const auto& cases = switchStmt->cases();

				checkEnumSwitchExhaustive(child, cases);

				// After a bare fallible call the status carries the error code: the `Ok` arm sees the
				// call's results, every other arm sees the stack as it was before the call.
				const FunctionSignature* fallible = bareFallibleCallBefore(node, i);
				std::vector<StackValueType> failureStack = typeStack;
				std::vector<std::string> failureStructStack = structTypeStack;
				if (fallible) {
					removeProducedValues(*fallible, failureStack, failureStructStack);
				}

				bool hasDefault = false;
				bool hasOkCase = false;
				std::vector<StackValueType> okCaseStack;
				std::vector<std::string> okCaseStructStack;
				// The first non-diverging arm fixes the depth the others must match.
				bool haveReference = false;
				int referenceEffect = 0;
				std::vector<StackValueType> referenceStack;
				std::vector<std::string> referenceStructStack;
				bool allDiverge = true;
				bool armsDisagree = false;
				int disagreeingEffect = 0;

				for (const auto* caseNode : cases) {
					if (caseNode->isDefault()) {
						hasDefault = true;
					}
					IAstNode* caseBody = caseNode->body();
					if (!caseBody) {
						continue;
					}

					bool isOk = false;
					if (!caseNode->isDefault() && caseNode->value() &&
							caseNode->value()->type() == IAstNode::Type::LITERAL) {
						AstNodeLiteral* caseLit = static_cast<AstNodeLiteral*>(caseNode->value());
						// `Ok`/`true` keep their keyword text as BOOL literals. A literal `1` is an error
						// *code* arm, not the success arm (spec 10.3: a panic carrying code 1 does not
						// match Ok), so it must be seeded from the failure stack like any other code.
						isOk = caseLit->literalType() == AstNodeLiteral::LiteralType::BOOL &&
							   (caseLit->value() == "Ok" || caseLit->value() == "true");
					}
					bool seedFromSuccess = !fallible || isOk;

					std::vector<StackValueType> caseStack = seedFromSuccess ? typeStack : failureStack;
					std::unordered_map<std::string, StackValueType> caseVars = localVariables;
					std::vector<std::string> caseStructStack = seedFromSuccess ? structTypeStack : failureStructStack;
					size_t base = caseStack.size();
					typeCheckBlock(caseBody, caseStack, caseVars, caseStructStack);
					int caseEffect = static_cast<int>(caseStack.size()) - static_cast<int>(base);

					if (isOk && fallible) {
						okCaseStack = caseStack;
						okCaseStructStack = caseStructStack;
						hasOkCase = true;
					}

					if (blockEndsDiverging(caseBody)) {
						continue;
					}
					allDiverge = false;

					if (fallible) {
						// Arms of a fallible switch legitimately differ (Ok has the results); the Ok arm,
						// when present, is authoritative for what follows.
						continue;
					}
					if (!haveReference) {
						haveReference = true;
						referenceEffect = caseEffect;
						referenceStack = caseStack;
						referenceStructStack = caseStructStack;
					} else if (caseEffect != referenceEffect && !armsDisagree) {
						armsDisagree = true;
						disagreeingEffect = caseEffect;
					}
				}

				if (fallible) {
					if (hasOkCase) {
						typeStack = okCaseStack;
						structTypeStack = okCaseStructStack;
					} else {
						// No Ok arm: the results were never handled, so nothing of them survives.
						typeStack = failureStack;
						structTypeStack = failureStructStack;
					}
					break;
				}

				// The same rule as for `if`/`else`: every arm that reaches the merge point must leave
				// the stack at the same depth. Without a `_` arm the switch may match nothing, so
				// the arms must additionally leave it at the depth it had before the switch.
				if (!mHasUnpredictableStack && !allDiverge) {
					if (armsDisagree) {
						std::string msg = "switch arms leave different numbers of values on the stack (" +
										  std::to_string(referenceEffect) + " and " +
										  std::to_string(disagreeingEffect) +
										  "); every arm must leave the same number, since whatever follows reads a "
										  "value whose identity would otherwise depend on which arm ran";
						reportError(child, msg.c_str());
					} else if (!hasDefault && haveReference && referenceEffect != 0) {
						std::string msg = "switch without a '_' arm changes the stack by " +
										  std::to_string(referenceEffect) +
										  "; when no arm matches nothing runs, so the arms must leave the stack "
										  "as they found it (add a '_' arm with the same effect, or balance the arms)";
						reportError(child, msg.c_str());
					}
				}
				if (haveReference && (hasDefault || referenceEffect == 0)) {
					// Balanced arms: what follows sees what an arm left, in-place changes included.
					typeStack = referenceStack;
					structTypeStack = referenceStructStack;
				}
				break;
			}

			case IAstNode::Type::FOR_STATEMENT: {
				// For loops: pop 3 values (start, end, step), type check body with iterator variable
				AstNodeForStatement* forStmt = static_cast<AstNodeForStatement*>(child);

				// Pop start, end, step from type stack
				for (int popIdx = 0; popIdx < 3; popIdx++) {
					if (!typeStack.empty()) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}
				}

				// Type check body with a copy of the state (loop body effects are complex with break/continue)
				std::vector<StackValueType> loopStack = typeStack;
				std::unordered_map<std::string, StackValueType> loopVars = localVariables;
				std::vector<std::string> loopStructStack = structTypeStack;

				// Add iterator variable as INT
				const std::string& iterName = forStmt->iteratorName();
				loopVars[iterName] = StackValueType::INT;

				// Type check the body with error suppression (marks method calls without reporting type errors)
				// This is necessary because loop bodies can have complex stack effects that are hard to analyze
				bool wasInLoopBody = mInLoopBody;
				mInLoopBody = true;
				auto savedJumps = std::move(mLoopJumps);
				mLoopJumps.clear();
				if (forStmt->body()) {
					typeCheckBlock(forStmt->body(), loopStack, loopVars, loopStructStack);
				}
				checkLoopStackEffect(forStmt->body(), "for", true, typeStack, structTypeStack, loopStack.size());
				mLoopJumps = std::move(savedJumps);
				mInLoopBody = wasInLoopBody;

				// A `for` always leaves the stack at the head depth, so the parent stack already
				// describes what follows. (Types a neutral body rewrote in place are still not
				// modelled; only the depth is.)
				break;
			}

			case IAstNode::Type::LOOP_STATEMENT: {
				std::vector<StackValueType> loopStack = typeStack;
				std::unordered_map<std::string, StackValueType> loopVars = localVariables;
				std::vector<std::string> loopStructStack = structTypeStack;

				bool wasInLoopBody = mInLoopBody;
				mInLoopBody = true;
				auto savedJumps = std::move(mLoopJumps);
				mLoopJumps.clear();
				AstNodeLoopStatement* loopStmt = static_cast<AstNodeLoopStatement*>(child);
				if (loopStmt->body()) {
					typeCheckBlock(loopStmt->body(), loopStack, loopVars, loopStructStack);
				}
				// Applies the loop's effect: a `loop` leaves the stack the way its breaks did.
				checkLoopStackEffect(loopStmt->body(), "loop", false, typeStack, structTypeStack, loopStack.size());
				mLoopJumps = std::move(savedJumps);
				mInLoopBody = wasInLoopBody;
				break;
			}

			case IAstNode::Type::WHILE_STATEMENT: {
				AstNodeWhileStatement* whileStmt = static_cast<AstNodeWhileStatement*>(child);

				// The condition runs before every iteration, including the first, so it is checked
				// against the stack as it stands here and must leave exactly one value -- the flag
				// the loop tests. The parser hands over the run of expression nodes that preceded
				// the keyword; a run that nets to anything else is the author having written
				// something the condition was never going to mean.
				const size_t headDepth = typeStack.size();
				std::vector<size_t> condDepths;
				if (whileStmt->condition()) {
					const IAstNode* savedProbeBlock = mDepthProbeBlock;
					std::vector<size_t>* savedProbe = mDepthProbe;
					mDepthProbeBlock = whileStmt->condition();
					mDepthProbe = &condDepths;
					typeCheckBlock(whileStmt->condition(), typeStack, localVariables, structTypeStack);
					mDepthProbeBlock = savedProbeBlock;
					mDepthProbe = savedProbe;
				}

				// Trim to the shortest suffix that produces the flag: the last point where the stack
				// stood at the head depth, with nothing after it dipping below. What precedes it is a
				// statement that happened to be expression-shaped, and is generated once.
				if (!mHasUnpredictableStack && typeStack.size() == headDepth + 1) {
					size_t start = 0;
					for (size_t d = condDepths.size(); d-- > 0;) {
						if (condDepths[d] < headDepth) {
							break;
						}
						if (condDepths[d] == headDepth) {
							// First match scanning backwards is the shortest such suffix.
							start = d;
							break;
						}
					}
					whileStmt->setConditionStart(start);
				}
				if (!mHasUnpredictableStack) {
					if (typeStack.size() != headDepth + 1) {
						long long delta = static_cast<long long>(typeStack.size()) - static_cast<long long>(headDepth);
						std::string msg = "the 'while' condition must leave exactly one value for the loop to "
										  "test, but it leaves ";
						msg += std::to_string(delta);
						msg += (delta == 1 || delta == -1) ? " value" : " values";
						msg += "; the condition is the words between the previous statement and 'while'";
						reportErrorConditional(child, msg.c_str(), true);
					} else {
						// Pop the flag the loop consumes.
						typeStack.pop_back();
						structTypeStack.pop_back();
					}
				}

				std::vector<StackValueType> loopStack = typeStack;
				std::unordered_map<std::string, StackValueType> loopVars = localVariables;
				std::vector<std::string> loopStructStack = structTypeStack;

				bool wasInLoopBody = mInLoopBody;
				mInLoopBody = true;
				auto savedJumps = std::move(mLoopJumps);
				mLoopJumps.clear();
				if (whileStmt->body()) {
					typeCheckBlock(whileStmt->body(), loopStack, loopVars, loopStructStack);
				}
				// A `while` may run zero times, so unlike `loop` it cannot leave anything behind:
				// the body has to be neutral, which is what the fallthrough flag asks for.
				checkLoopStackEffect(whileStmt->body(), "while", true, typeStack, structTypeStack, loopStack.size());
				mLoopJumps = std::move(savedJumps);
				mInLoopBody = wasInLoopBody;
				break;
			}

			case IAstNode::Type::LOCAL: {
				// Handle local variable declaration: pop value from stack and store
				// Supports multiple assignment: -> a b c pops 3 values
				// Special case: -> _ discards the value (like drop)
				AstNodeLocal* local = static_cast<AstNodeLocal*>(child);
				const std::vector<std::string>& varNames = local->names();

				// Process each variable name (in order: first name gets top of stack)
				for (const std::string& varName : varNames) {
					// Special case: _ is discard (drop), not a variable
					if (varName == "_") {
						// Check if stack is empty
						if (typeStack.empty()) {
							reportError(local, "Type error in discard '-> _': Stack underflow (no value to discard)");
							continue;
						}
						// Pop the value type from the stack but don't store it
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
						continue;
					}

					// Check if variable name shadows a function
					if (mDefinedFunctions.find(varName) != mDefinedFunctions.end()) {
						std::string errorMsg = "Local variable '";
						errorMsg += varName;
						errorMsg += "' shadows function with same name";
						reportError(local, errorMsg.c_str());
						continue;
					}

					// Check if stack is empty
					if (typeStack.empty()) {
						std::string errorMsg = "Type error in local variable '";
						errorMsg += varName;
						errorMsg += "': Stack underflow (no value to store)";
						reportError(local, errorMsg.c_str());
						continue;
					}

					// Pop the value type from the stack and store it as the variable's type
					StackValueType varType = typeStack.back();
					typeStack.pop_back();
					localVariables[varName] = varType;

					// If it's a PTR type, also store which struct type it is (if any)
					if (varType == StackValueType::PTR && !structTypeStack.empty()) {
						std::string structType = structTypeStack.back();
						structTypeStack.pop_back();
						mLocalVariableStructTypes[varName] = structType;
						static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
						if (debug) {
							std::cerr << "[DEBUG TC] Store -> " << varName << " with struct type: " << structType
									  << std::endl;
						}

						// If there's a pending function signature, store it with this variable
						if (mPendingFnSignature.has_value()) {
							mLocalVariableFnSignatures[varName] = mPendingFnSignature.value();
							mPendingFnSignature.reset();
						}
					} else if (!structTypeStack.empty()) {
						structTypeStack.pop_back();
					}
				}
				break;
			}

			case IAstNode::Type::STRUCT_CONSTRUCTION: {
				// Handle struct construction with named fields: StructName { field: expr ... }
				AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(child);
				checkAnonymousFunctionsWithin(construct, localVariables);
				const std::string& name = construct->structName();
				const auto& fieldInits = construct->fieldInits();

				// Get struct declaration - try local first, then module structs
				// For module structs, we need to look up with qualified key
				AstNodeStructDeclaration* structDecl = nullptr;
				std::string lookupKey = name; // Key for mStructFieldTypes lookup
				auto structDeclIt = mStructDeclarations.find(name);
				if (structDeclIt != mStructDeclarations.end()) {
					structDecl = structDeclIt->second;
				} else {
					// Try qualified lookup first (for qualified names like "vec2::Vec2")
					auto moduleDeclIt = mModuleStructDeclarations.find(name);
					if (moduleDeclIt != mModuleStructDeclarations.end()) {
						structDecl = moduleDeclIt->second;
						lookupKey = name;
					} else if (name.find("::") == std::string::npos) {
						// Unqualified name - search in modules
						for (const auto& modulePair : mModuleStructs) {
							const std::string& moduleName = modulePair.first;
							if (modulePair.second.find(name) != modulePair.second.end()) {
								std::string qualifiedName = moduleName + "::" + name;
								auto qualIt = mModuleStructDeclarations.find(qualifiedName);
								if (qualIt != mModuleStructDeclarations.end()) {
									structDecl = qualIt->second;
									lookupKey = qualifiedName;
									break;
								}
							}
						}
					}
				}

				if (structDecl) {
					std::unordered_set<std::string> providedFields;

					// Process each field initializer
					for (const auto& fieldInit : fieldInits) {
						const std::string& fieldName = fieldInit.fieldName;
						providedFields.insert(fieldName);

						// Check if field exists in struct using mStructFieldTypes
						// (don't use structDecl->fields() as it may have been freed for imported modules)
						bool fieldExists = false;
						StackValueType expectedType = StackValueType::UNKNOWN;
						std::string expectedStructType;

						const auto* structFieldTypes = lookupStructFieldTypes(lookupKey);
						if (structFieldTypes != nullptr) {
							auto fieldTypeIt = structFieldTypes->find(fieldName);
							if (fieldTypeIt != structFieldTypes->end()) {
								fieldExists = true;
								expectedType = fieldTypeIt->second;
							}
						}

						// Also check mStructFieldStructTypes for PTR field types
						auto structFieldStructTypesIt = mStructFieldStructTypes.find(lookupKey);
						if (structFieldStructTypesIt != mStructFieldStructTypes.end()) {
							auto fieldStructTypeIt = structFieldStructTypesIt->second.find(fieldName);
							if (fieldStructTypeIt != structFieldStructTypesIt->second.end()) {
								expectedStructType = fieldStructTypeIt->second;
							}
						}

						if (!fieldExists) {
							std::string errorMsg = "Unknown field '";
							errorMsg += fieldName;
							errorMsg += "' in struct '";
							errorMsg += name;
							errorMsg += "'";
							// Try to suggest a similar field name
							if (structFieldTypes != nullptr) {
								std::string suggestion = findSimilarNameInMap(fieldName, *structFieldTypes);
								if (!suggestion.empty()) {
									errorMsg += "; did you mean '";
									errorMsg += suggestion;
									errorMsg += "'?";
								}
							}
							reportError(construct, errorMsg.c_str());
							continue;
						}

						// Process the field's expression nodes to determine the type it produces
						// We create a temporary type stack to track what the expression produces
						std::vector<StackValueType> exprTypeStack;
						std::vector<std::string> exprStructTypeStack;

						for (const auto& exprNodePtr : fieldInit.valueNodes) {
							IAstNode* exprNode = exprNodePtr.get();
							// Recursively type-check the expression node
							// For simplicity, we simulate basic type inference here
							switch (exprNode->type()) {
							case IAstNode::Type::LITERAL: {
								AstNodeLiteral* lit = static_cast<AstNodeLiteral*>(exprNode);
								exprTypeStack.push_back(getLiteralStackType(lit->literalType()));
								exprStructTypeStack.push_back("");
								break;
							}
							case IAstNode::Type::IDENTIFIER: {
								AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(exprNode);
								auto localIt = localVariables.find(ident->name());
								if (localIt != localVariables.end()) {
									exprTypeStack.push_back(localIt->second);
									if (localIt->second == StackValueType::PTR) {
										auto structTypeIt = mLocalVariableStructTypes.find(ident->name());
										if (structTypeIt != mLocalVariableStructTypes.end()) {
											exprStructTypeStack.push_back(structTypeIt->second);
										} else {
											exprStructTypeStack.push_back("");
										}
									} else {
										exprStructTypeStack.push_back("");
									}
								} else {
									exprTypeStack.push_back(StackValueType::UNKNOWN);
									exprStructTypeStack.push_back("");
								}
								break;
							}
							case IAstNode::Type::STRUCT_CONSTRUCTION: {
								// Nested struct construction
								AstNodeStructConstruction* nested = static_cast<AstNodeStructConstruction*>(exprNode);
								exprTypeStack.push_back(StackValueType::PTR);
								// Qualify struct name if from module
								std::string nestedStructName = nested->structName();
								if (nestedStructName.find("::") == std::string::npos) {
									for (const auto& moduleEntry : mModuleStructs) {
										const auto& structs = moduleEntry.second;
										if (structs.find(nestedStructName) != structs.end() &&
												structs.at(nestedStructName)) {
											nestedStructName = moduleEntry.first + "::" + nestedStructName;
											break;
										}
									}
								}
								exprStructTypeStack.push_back(nestedStructName);
								break;
							}
							case IAstNode::Type::ARRAY_LITERAL: {
								// Array literal pushes a pointer with element type info
								AstNodeArrayLiteral* arrLit = static_cast<AstNodeArrayLiteral*>(exprNode);
								exprTypeStack.push_back(StackValueType::PTR);
								// Same inference as a literal anywhere else. This used to be a third
								// copy that read element zero and recognised only the three scalar
								// literals, so `xs = [[1 2]]` typed as `[]any` and a field declared
								// `[][]i64` rejected the one literal that can fill it.
								exprStructTypeStack.push_back(arrayLiteralTypeName(arrLit, localVariables));
								break;
							}
							case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
								// `&f` in a field initializer: carry the function's type so a field declared
								// `fn(...)` is checked against it. Without these two cases the initializer was
								// UNKNOWN and any function pointer satisfied any fn-typed field.
								auto* fnRef = static_cast<AstNodeFunctionPointerReference*>(exprNode);
								exprTypeStack.push_back(StackValueType::PTR);
								auto refSig = mFunctionSignatures.find(fnRef->functionName());
								exprStructTypeStack.push_back(
										refSig != mFunctionSignatures.end() ? buildFnTypeString(refSig->second) : "");
								break;
							}
							case IAstNode::Type::ANONYMOUS_FUNCTION: {
								auto* anonFn = static_cast<AstNodeAnonymousFunction*>(exprNode);
								FunctionSignature anonSig;
								for (const auto& pn : anonFn->inputParameters()) {
									anonSig.consumes.push_back(stringToStackValueType(
											static_cast<AstNodeParameter*>(pn.get())->typeString()));
								}
								for (const auto& pn : anonFn->outputParameters()) {
									anonSig.produces.push_back(stringToStackValueType(
											static_cast<AstNodeParameter*>(pn.get())->typeString()));
								}
								anonSig.throws = anonFn->throws();
								exprTypeStack.push_back(StackValueType::PTR);
								exprStructTypeStack.push_back(buildFnTypeString(anonSig));
								break;
							}
							case IAstNode::Type::FIELD_ACCESS: {
								// `<<field` replaces the struct on top with the field's value. Before the
								// parser stopped folding the operand into this node there was no operand
								// here to pop, and no case at all: the struct itself was reported as the
								// field's value ("expects float, but got Vec2").
								AstNodeFieldAccess* fa = static_cast<AstNodeFieldAccess*>(exprNode);
								std::string operandStruct;
								if (!exprTypeStack.empty()) {
									exprTypeStack.pop_back();
								}
								if (!exprStructTypeStack.empty()) {
									operandStruct = exprStructTypeStack.back();
									exprStructTypeStack.pop_back();
								}
								StackValueType faType = StackValueType::UNKNOWN;
								std::string faStruct;
								const auto* faFields =
										operandStruct.empty() ? nullptr : lookupStructFieldTypes(operandStruct);
								if (faFields != nullptr) {
									if (faFields->find(fa->fieldName()) != faFields->end()) {
										faType = resolveFieldType(operandStruct, fa->fieldName(), faStruct);
									}
								} else {
									for (const auto& structEntry : mStructFieldTypes) {
										auto it = structEntry.second.find(fa->fieldName());
										if (it != structEntry.second.end()) {
											faType = it->second;
											break;
										}
									}
								}
								exprTypeStack.push_back(faType);
								exprStructTypeStack.push_back(faStruct);
								break;
							}
							case IAstNode::Type::INSTRUCTION: {
								// Instructions modify the stack - simplified handling
								AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(exprNode);
								const std::string& instrName = instr->name();
								// Handle common arithmetic operations
								if (instrName == "+" || instrName == "-" || instrName == "*" || instrName == "/" ||
										instrName == "add" || instrName == "sub" || instrName == "mul" ||
										instrName == "div") {
									if (exprTypeStack.size() >= 2) {
										exprTypeStack.pop_back();
										exprStructTypeStack.pop_back();
									}
								}
								break;
							}
							default:
								// For other node types, assume they push one value
								exprTypeStack.push_back(StackValueType::UNKNOWN);
								exprStructTypeStack.push_back("");
								break;
							}
						}

						// After processing expression, check the resulting type
						if (!exprTypeStack.empty()) {
							StackValueType actualType = exprTypeStack.back();
							std::string actualStructType =
									exprStructTypeStack.empty() ? "" : exprStructTypeStack.back();

							// Skip type checking if expected type is a type parameter (for generics)
							bool isGenericField = !expectedStructType.empty() && isCurrentTypeParam(expectedStructType);

							if (!isGenericField && actualType != StackValueType::UNKNOWN &&
									expectedType != StackValueType::UNKNOWN) {
								if (actualType != expectedType) {
									if (isImplicitCastAllowed(actualType, expectedType)) {
										// Only warn for casts that should be warned about
										if (shouldWarnImplicitCast(actualType, expectedType)) {
											std::string warnMsg = "Implicit cast in struct construction '";
											warnMsg += name;
											warnMsg += "': Field '";
											warnMsg += fieldName;
											warnMsg += "' expects ";
											warnMsg += stackValueTypeToString(expectedType);
											warnMsg += ", but got ";
											warnMsg += stackValueTypeToString(actualType);
											reportWarning(construct, warnMsg.c_str());
										}
									} else {
										std::string errorMsg = "Type error in struct construction '";
										errorMsg += name;
										errorMsg += "': Field '";
										errorMsg += fieldName;
										errorMsg += "' expects ";
										errorMsg += expectedStructType.empty() ? stackValueTypeToString(expectedType)
																			   : expectedStructType;
										errorMsg += ", but got ";
										errorMsg += actualStructType.empty() ? stackValueTypeToString(actualType)
																			 : actualStructType;
										reportError(construct, errorMsg.c_str());
									}
								} else if (std::string why; fieldValueRejected(
												   lookupKey, fieldName, expectedStructType, actualStructType, why)) {
									// Both are the coarse type the field declared, so what separates them is
									// the pointer's real type. Structural rather than by string: `fn( -- f64)`
									// and the `fn(-- f64)` buildFnTypeString emits are the same type, and so
									// are `Point` and `mod::Point`.
									std::string errorMsg = "Type error in struct construction '";
									errorMsg += name;
									errorMsg += "': Field '";
									errorMsg += fieldName;
									errorMsg += "' ";
									errorMsg += why;
									reportError(construct, errorMsg.c_str());
								}
							}
						}
					}

					// Check for missing fields using mStructFieldTypes (safe for imported modules)
					const auto* structFieldTypesForMissing = lookupStructFieldTypes(lookupKey);
					if (structFieldTypesForMissing != nullptr) {
						// Get fields with defaults for this struct
						const std::unordered_set<std::string>* fieldsWithDefaults = nullptr;
						auto defaultsIt = mStructFieldsWithDefaults.find(lookupKey);
						if (defaultsIt != mStructFieldsWithDefaults.end()) {
							fieldsWithDefaults = &defaultsIt->second;
						}

						for (const auto& fieldEntry : *structFieldTypesForMissing) {
							if (providedFields.find(fieldEntry.first) == providedFields.end()) {
								// Check if field has a default value
								bool hasDefault =
										fieldsWithDefaults != nullptr &&
										fieldsWithDefaults->find(fieldEntry.first) != fieldsWithDefaults->end();
								if (!hasDefault) {
									std::string errorMsg = "Missing field '";
									errorMsg += fieldEntry.first;
									errorMsg += "' in struct construction '";
									errorMsg += name;
									errorMsg += "'";
									reportError(construct, errorMsg.c_str());
								}
							}
						}
					}
				}

				// Push pointer type for the constructed struct. An instantiated generic keeps its
				// arguments (`Box<i64>`) so that its fields resolve to concrete types later.
				std::string typeArgSuffix;
				if (!construct->typeArgs().empty()) {
					typeArgSuffix = "<";
					for (size_t ta = 0; ta < construct->typeArgs().size(); ta++) {
						if (ta > 0) {
							typeArgSuffix += ", ";
						}
						typeArgSuffix += construct->typeArgs()[ta];
					}
					typeArgSuffix += ">";
				}
				typeStack.push_back(StackValueType::PTR);
				// Use qualified name for method resolution (already computed in lookupKey for module structs)
				if (lookupKey.find("::") != std::string::npos) {
					structTypeStack.push_back(lookupKey + typeArgSuffix);
				} else {
					// Check if struct is from a module - use qualified name
					std::string qualifiedStructName = name;
					for (const auto& moduleEntry : mModuleStructs) {
						const auto& structs = moduleEntry.second;
						if (structs.find(name) != structs.end() && structs.at(name)) {
							qualifiedStructName = moduleEntry.first + "::" + name;
							break;
						}
					}
					structTypeStack.push_back(qualifiedStructName + typeArgSuffix);
				}
				break;
			}

			case IAstNode::Type::IDENTIFIER: {
				// Handle function calls - apply their stack effect
				AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
				const std::string& name = ident->name();

				// Check if it's a local variable reference
				auto localIt = localVariables.find(name);
				if (localIt != localVariables.end()) {
					// Push the local variable's type onto the stack
					typeStack.push_back(localIt->second);
					// If it's a PTR, also push its struct type (if any)
					if (localIt->second == StackValueType::PTR) {
						auto structTypeIt = mLocalVariableStructTypes.find(name);
						if (structTypeIt != mLocalVariableStructTypes.end()) {
							structTypeStack.push_back(structTypeIt->second);
							static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
							if (debug) {
								std::cerr << "[DEBUG TC] Local var '" << name
										  << "' pushed struct type: " << structTypeIt->second << std::endl;
							}
						} else {
							structTypeStack.push_back(""); // Unknown struct type
							static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
							if (debug) {
								std::cerr << "[DEBUG TC] Local var '" << name
										  << "' has no struct type in mLocalVariableStructTypes" << std::endl;
							}
						}

						// If the variable has a known function signature, set it as pending
						auto fnSigIt = mLocalVariableFnSignatures.find(name);
						if (fnSigIt != mLocalVariableFnSignatures.end()) {
							mPendingFnSignature = fnSigIt->second;
						}
					} else {
						structTypeStack.push_back("");
					}
					break;
				}

				// Check if it's a constant
				auto constIt = mConstantValues.find(name);
				if (constIt != mConstantValues.end()) {
					// Push the constant's type onto the stack
					StackValueType constType = getConstantType(constIt->second);
					typeStack.push_back(constType);
					structTypeStack.push_back("");
					break;
				}

				// Module-level `var`. Pushes its declared type on the stack.
				// If the declared type is a struct name, carry the struct type
				// forward so a following `<<field` access resolves to the
				// right layout.
				auto gvIt2 = mGlobalVarTypes.find(name);
				if (gvIt2 != mGlobalVarTypes.end()) {
					const std::string& gvType = gvIt2->second;
					typeStack.push_back(stringToStackValueType(gvType));
					structTypeStack.push_back(isStructTypeName(gvType) ? gvType : "");
					break;
				}

				// Check if it's a struct construction
				if (mDefinedStructs.find(name) != mDefinedStructs.end()) {
					// Struct construction: consumes field values, produces pointer
					auto structDeclIt = mStructDeclarations.find(name);
					if (structDeclIt != mStructDeclarations.end()) {
						AstNodeStructDeclaration* structDecl = structDeclIt->second;
						const auto& fields = structDecl->fields();
						size_t fieldCount = fields.size();

						// Check if we have enough values on the stack
						if (typeStack.size() < fieldCount) {
							std::string errorMsg = "Type error in struct construction '";
							errorMsg += name;
							errorMsg += "': Stack underflow (requires ";
							errorMsg += std::to_string(fieldCount);
							errorMsg += " values, have ";
							errorMsg += std::to_string(typeStack.size());
							errorMsg += ")";
							reportError(ident, errorMsg.c_str());
						} else {
							// Validate each field type (fields are in declaration order)
							// Fields are consumed bottom-to-top from the stack
							for (size_t fi = 0; fi < fieldCount; fi++) {
								size_t stackIdx = typeStack.size() - fieldCount + fi;
								StackValueType actual = typeStack[stackIdx];
								std::string actualStructType =
										(stackIdx < structTypeStack.size()) ? structTypeStack[stackIdx] : "";
								const AstNodeStructField* field = fields[fi].get();
								const std::string& fieldName = field->name();

								// Get expected type from mStructFieldTypes
								const auto* structFieldTypesPtr = lookupStructFieldTypes(name);
								if (structFieldTypesPtr == nullptr) {
									continue;
								}
								auto fieldTypeIt = structFieldTypesPtr->find(fieldName);
								if (fieldTypeIt == structFieldTypesPtr->end()) {
									continue;
								}
								StackValueType expected = fieldTypeIt->second;

								// Check if this field expects a specific struct type
								std::string expectedStructType;
								auto structFieldStructTypesIt = mStructFieldStructTypes.find(name);
								if (structFieldStructTypesIt != mStructFieldStructTypes.end()) {
									auto fieldStructTypeIt = structFieldStructTypesIt->second.find(fieldName);
									if (fieldStructTypeIt != structFieldStructTypesIt->second.end()) {
										expectedStructType = fieldStructTypeIt->second;
									}
								}

								// Skip check if expected type is a type parameter (for generics)
								bool isGenericField =
										!expectedStructType.empty() && isCurrentTypeParam(expectedStructType);
								if (isGenericField) {
									continue;
								}

								// Skip check if actual type is UNKNOWN (can't determine type)
								if (actual == StackValueType::UNKNOWN) {
									continue;
								}

								// Check for type mismatch
								if (actual != expected) {
									// Check if implicit cast is allowed (int <-> float)
									if (isImplicitCastAllowed(actual, expected)) {
										// Only warn for casts that should be warned about
										if (shouldWarnImplicitCast(actual, expected)) {
											std::string warnMsg = "Implicit cast in struct construction '";
											warnMsg += name;
											warnMsg += "': Field '";
											warnMsg += fieldName;
											warnMsg += "' expects ";
											warnMsg += stackValueTypeToString(expected);
											warnMsg += ", but got ";
											warnMsg += stackValueTypeToString(actual);
											reportWarning(ident, warnMsg.c_str());
										}
									} else {
										// Type mismatch error
										std::string errorMsg = "Type error in struct construction '";
										errorMsg += name;
										errorMsg += "': Field '";
										errorMsg += fieldName;
										errorMsg += "' expects ";
										errorMsg += expectedStructType.empty() ? stackValueTypeToString(expected)
																			   : expectedStructType;
										errorMsg += ", but got ";
										errorMsg += actualStructType.empty() ? stackValueTypeToString(actual)
																			 : actualStructType;
										reportError(ident, errorMsg.c_str());
									}
								} else if (std::string why; fieldValueRejected(
												   name, fieldName, expectedStructType, actualStructType, why)) {
									// Both are PTR, so the pointer's real type is what separates them.
									std::string errorMsg = "Type error in struct construction '";
									errorMsg += name;
									errorMsg += "': Field '";
									errorMsg += fieldName;
									errorMsg += "' ";
									errorMsg += why;
									reportError(ident, errorMsg.c_str());
								}
							}
						}

						// Pop field values from stack
						for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}
					}

					// Struct pointers can now be used:
					// - Stored to variable with -> var
					// - Accessed with @field
					// - As arguments to other struct constructors
					// - Returned on stack
					// The code generator handles all these cases correctly.

					// Push pointer type for the constructed struct, along with its struct type
					typeStack.push_back(StackValueType::PTR);
					structTypeStack.push_back(name); // Track which struct type this is
					break;
				}

				// Check if it's a struct from an imported module
				for (const auto& moduleEntry : mModuleStructs) {
					const std::string& moduleName = moduleEntry.first;
					const auto& structs = moduleEntry.second;
					if (structs.find(name) != structs.end() && structs.at(name)) {
						// Struct construction from module - use qualified name for lookups
						std::string qualifiedStructName = moduleName + "::" + name;
						// Try to find struct declaration
						auto structDeclIt = mModuleStructDeclarations.find(qualifiedStructName);
						if (structDeclIt != mModuleStructDeclarations.end()) {
							AstNodeStructDeclaration* structDecl = structDeclIt->second;
							const auto& fields = structDecl->fields();
							size_t fieldCount = fields.size();

							// Check if we have enough values on the stack
							if (typeStack.size() < fieldCount) {
								std::string errorMsg = "Type error in struct construction '";
								errorMsg += name;
								errorMsg += "': Stack underflow (requires ";
								errorMsg += std::to_string(fieldCount);
								errorMsg += " values, have ";
								errorMsg += std::to_string(typeStack.size());
								errorMsg += ")";
								reportError(ident, errorMsg.c_str());
							} else {
								// Validate each field type (fields are in declaration order)
								// Fields are consumed bottom-to-top from the stack
								for (size_t fi = 0; fi < fieldCount; fi++) {
									size_t stackIdx = typeStack.size() - fieldCount + fi;
									StackValueType actual = typeStack[stackIdx];
									const std::string actualStructType =
											(stackIdx < structTypeStack.size()) ? structTypeStack[stackIdx] : "";
									const AstNodeStructField* field = fields[fi].get();
									const std::string& fieldName = field->name();

									// Get expected type from mStructFieldTypes
									const auto* structFieldTypesPtr = lookupStructFieldTypes(qualifiedStructName);
									if (structFieldTypesPtr == nullptr) {
										continue;
									}
									auto fieldTypeIt = structFieldTypesPtr->find(fieldName);
									if (fieldTypeIt == structFieldTypesPtr->end()) {
										continue;
									}
									StackValueType expected = fieldTypeIt->second;

									// Check if field expects a struct type (including type parameters)
									std::string expectedStructType;
									auto structFieldStructTypesIt = mStructFieldStructTypes.find(qualifiedStructName);
									if (structFieldStructTypesIt != mStructFieldStructTypes.end()) {
										auto fieldStructTypeIt = structFieldStructTypesIt->second.find(fieldName);
										if (fieldStructTypeIt != structFieldStructTypesIt->second.end()) {
											expectedStructType = fieldStructTypeIt->second;
										}
									}

									// Skip check if expected type is a type parameter (for generics)
									bool isGenericField =
											!expectedStructType.empty() && isCurrentTypeParam(expectedStructType);
									if (isGenericField) {
										continue;
									}

									// Skip check if actual type is UNKNOWN (can't determine type)
									if (actual == StackValueType::UNKNOWN) {
										continue;
									}

									// Check for type mismatch
									if (actual != expected) {
										// Check if implicit cast is allowed (int <-> float)
										if (isImplicitCastAllowed(actual, expected)) {
											// Only warn for casts that should be warned about
											if (shouldWarnImplicitCast(actual, expected)) {
												std::string warnMsg = "Implicit cast in struct construction '";
												warnMsg += name;
												warnMsg += "': Field '";
												warnMsg += fieldName;
												warnMsg += "' expects ";
												warnMsg += stackValueTypeToString(expected);
												warnMsg += ", but got ";
												warnMsg += stackValueTypeToString(actual);
												reportWarning(ident, warnMsg.c_str());
											}
										} else {
											// Type mismatch error
											std::string errorMsg = "Type error in struct construction '";
											errorMsg += name;
											errorMsg += "': Field '";
											errorMsg += fieldName;
											errorMsg += "' expects ";
											errorMsg += stackValueTypeToString(expected);
											errorMsg += ", but got ";
											errorMsg += stackValueTypeToString(actual);
											reportError(ident, errorMsg.c_str());
										}
									} else if (std::string why; fieldValueRejected(qualifiedStructName, fieldName,
													   expectedStructType, actualStructType, why)) {
										// A module's struct gets the same pointer-type check a local one
										// does. This form had only the coarse compare above.
										std::string errorMsg = "Type error in struct construction '";
										errorMsg += name;
										errorMsg += "': Field '";
										errorMsg += fieldName;
										errorMsg += "' ";
										errorMsg += why;
										reportError(ident, errorMsg.c_str());
									}
								}
							}

							// Pop field values from stack
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}

						// Struct pointers can now be used flexibly - codegen handles all cases

						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(qualifiedStructName); // Track the qualified struct type
						break;
					}
				}

				// Check if this is a method call on a struct
				// Methods require: receiver pushed first, then parameters on top
				// e.g., for method set(idx, elem): v idx elem set!
				std::string receiverStructType;
				std::string registeredStructType;
				size_t receiverStackIdx = 0;

				{
					static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
					if (debug) {
						std::cerr << "[DEBUG TC] Checking for method call '" << name << "' - structTypeStack: [";
						for (size_t k = 0; k < structTypeStack.size(); k++) {
							if (k > 0) {
								std::cerr << ", ";
							}
							std::cerr << "'" << structTypeStack[k] << "'";
						}
						std::cerr << "]" << std::endl;
					}
				}

				// First pass: find any struct on the stack that has this method
				// to determine the method signature and expected parameter count
				size_t searchLimit = std::min(structTypeStack.size(), typeStack.size());
				for (size_t idx = searchLimit; idx > 0; idx--) {
					const std::string& structType = structTypeStack[idx - 1];
					if (!structType.empty()) {
						std::string potentialRegisteredType = findMethodStructType(structType, name);
						if (!potentialRegisteredType.empty()) {
							registeredStructType = potentialRegisteredType;
							break;
						}
					}
				}
				if (!registeredStructType.empty()) {
					// This is a method call - look up the signature with mangled name
					std::string mangledName = registeredStructType + "::" + name;
					auto methodSigIt = mFunctionSignatures.find(mangledName);
					if (methodSigIt != mFunctionSignatures.end()) {
						const FunctionSignature& sig = methodSigIt->second;

						// Validate '!' and '?' usage
						if ((ident->abortOnError() || ident->propagateOnError()) && !sig.throws) {
							std::string op = ident->abortOnError() ? "!" : "?";
							std::string errorMsg = "Cannot use '" + op + "' operator on method '" + name +
												   "' which is not marked as fallible";
							reportError(ident, errorMsg.c_str());
						}

						// Check '?' requires caller to be fallible
						if (ident->propagateOnError() && !mCurrentFunctionFallible) {
							std::string errorMsg =
									"Cannot use '?' operator on method '" + name +
									"': enclosing function must be fallible (add '!' to function signature)";
							reportError(ident, errorMsg.c_str());
						}

						// Check fallible methods without ! or ? must be followed by 'if' or 'switch'
						if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
							IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
							if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
													 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
								std::string errorMsg =
										"Fallible method '" + name +
										"' must be immediately followed by 'if' or 'switch' to check for "
										"errors, or use '!' to abort on error, or '?' to propagate";
								reportError(ident, errorMsg.c_str());
							}
						}

						// Calculate expected receiver position based on parameter count
						// With receiver-first: receiver should be at position additionalParams from top
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;

						// Second pass: check if there's a valid receiver at the expected position
						if (typeStack.size() <= additionalParams) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += name;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(ident, errorMsg.c_str());
							break;
						}

						size_t expectedReceiverIdx = typeStack.size() - 1 - additionalParams;

						// Verify the struct at expected position has this method
						bool foundValidReceiver = false;
						if (expectedReceiverIdx < structTypeStack.size()) {
							const std::string& structAtExpectedPos = structTypeStack[expectedReceiverIdx];
							std::string actualRegisteredType = findMethodStructType(structAtExpectedPos, name);
							if (!actualRegisteredType.empty()) {
								// Valid receiver-first call
								receiverStructType = structAtExpectedPos;
								registeredStructType = actualRegisteredType;
								receiverStackIdx = expectedReceiverIdx;
								foundValidReceiver = true;
							}
						}

						if (!foundValidReceiver) {
							std::string errorMsg = "Method call '";
							errorMsg += name;
							errorMsg += "': receiver must be pushed first, then ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " parameter(s) on top (e.g., 'receiver";
							for (size_t p = 0; p < additionalParams; p++) {
								errorMsg += " param";
							}
							errorMsg += " ";
							errorMsg += name;
							errorMsg += "')";
							reportError(ident, errorMsg.c_str());
							break;
						}

						// receiverPositionFromTop is now always additionalParams (by construction)
						size_t receiverPositionFromTop = additionalParams;

						if (typeStack.size() < sig.consumes.size()) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += name;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(ident, errorMsg.c_str());
							break;
						}

						// Validate parameter types (skip receiver at index 0)
						// Stack order: [receiver, param1, param2, ..., paramN] with receiver at bottom
						// Signature: [receiver, param1, param2, ..., paramN]
						// For sig index j (1 to N), stack index is: receiverIdx + j
						for (size_t j = 1; j < sig.consumes.size(); j++) {
							size_t stackIdx = receiverStackIdx + j;
							// Bounds check before accessing typeStack
							if (stackIdx >= typeStack.size()) {
								break;
							}
							StackValueType expected = sig.consumes[j];
							StackValueType actual = typeStack[stackIdx];

							// ANY and UNKNOWN cannot be checked; a TYPEVAR is a generic parameter that accepts
							// any concrete type at the call site, exactly as in the function-call path.
							if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
									expected == StackValueType::TYPEVAR) {
								continue;
							}
							if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY ||
									actual == StackValueType::TYPEVAR) {
								continue;
							}

							if (actual != expected) {
								if (isImplicitCastAllowed(actual, expected)) {
									std::string warnMsg = "Implicit cast in method call '";
									warnMsg += name;
									warnMsg += "': Parameter ";
									warnMsg += std::to_string(j);
									warnMsg += " expects ";
									warnMsg += stackValueTypeToString(expected);
									warnMsg += ", but got ";
									warnMsg += stackValueTypeToString(actual);
									reportWarning(ident, warnMsg.c_str());
								} else {
									std::string errorMsg = "Type error in method call '";
									errorMsg += name;
									errorMsg += "': Parameter ";
									errorMsg += std::to_string(j);
									errorMsg += " expects ";
									errorMsg += stackValueTypeToString(expected);
									errorMsg += ", but got ";
									errorMsg += stackValueTypeToString(actual);
									reportError(ident, errorMsg.c_str());
								}
							}
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values
						{
							// The receiver's type arguments bind the method's type parameters.
							std::vector<std::string> noConsumed;
							pushCallResults(sig, receiverTypeBindings(receiverStructType), noConsumed, typeStack,
									structTypeStack);
						}
						if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
							typeStack.push_back(StackValueType::INT); // Error status
							structTypeStack.push_back("");
						}

						// Mark identifier as a method call for code generation
						// Use registeredStructType (the generic type) for proper function name lookup
						{
							static bool debug = std::getenv("QUADC_DEBUG_MERGE") != nullptr;
							if (debug) {
								std::cerr << "[DEBUG TC] Marking '" << name << "' as method call on "
										  << registeredStructType << std::endl;
							}
						}
						ident->setIsMethodCall(true);
						ident->setReceiverType(registeredStructType);
						ident->setMethodInputParamCount(additionalParams);
						// Use the receiver position calculated earlier (before stack modifications)
						ident->setMethodReceiverPositionFromTop(receiverPositionFromTop);
						break;
					}
				}

				// Check if this is a user-defined function
				auto sigIt = mFunctionSignatures.find(name);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Validate '!' and '?' usage: only allowed on fallible functions (marked with '!')
					if ((ident->abortOnError() || ident->propagateOnError()) && !sig.throws) {
						std::string op = ident->abortOnError() ? "!" : "?";
						std::string errorMsg = "Cannot use '" + op + "' operator on function '" + name +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(ident, errorMsg.c_str());
					}

					// Check '?' requires caller to be fallible
					if (ident->propagateOnError() && !mCurrentFunctionFallible) {
						std::string errorMsg = "Cannot use '?' operator on function '" + name +
											   "': enclosing function must be fallible (add '!' to function signature)";
						reportError(ident, errorMsg.c_str());
					}

					// Check fallible functions without ! or ? must be followed by 'if' or 'switch'
					if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
						IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
						if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
												 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
							std::string errorMsg = "Fallible function '" + name +
												   "' must be immediately followed by 'if' or 'switch' to check for "
												   "errors, or use '!' to abort on error, or '?' to propagate";
							reportError(ident, errorMsg.c_str());
						}
					}

					// Check if stack has enough values for function parameters
					if (typeStack.size() < sig.consumes.size()) {
						std::string errorMsg = "Type error in function call '";
						errorMsg += name;
						errorMsg += "': Stack underflow (requires ";
						errorMsg += std::to_string(sig.consumes.size());
						errorMsg += " values, have ";
						errorMsg += std::to_string(typeStack.size());
						errorMsg += ")";
						reportError(ident, errorMsg.c_str());
						break;
					}

					// Track which parameters need casts
					std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);

					// Check if the types match
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
						StackValueType expected = sig.consumes[j];
						StackValueType actual = typeStack[stackIdx];

						// Skip check if expected type is ANY, UNKNOWN, or TYPEVAR (generic type parameter)
						// TYPEVAR accepts any concrete type at the call site
						if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
								expected == StackValueType::TYPEVAR) {
							continue;
						}

						// Skip check if actual type is UNKNOWN, ANY, or TYPEVAR (can't determine type at compile time)
						// TYPEVAR occurs when a generic type parameter (like T) is passed to a typed function
						if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY ||
								actual == StackValueType::TYPEVAR) {
							continue;
						}

						// Check for type mismatch
						if (actual != expected) {
							// Check if implicit cast is allowed (int <-> float)
							if (isImplicitCastAllowed(actual, expected)) {
								// Warn about implicit cast
								std::string warnMsg = "Implicit cast in function call '";
								warnMsg += name;
								warnMsg += "': Parameter ";
								warnMsg += std::to_string(j + 1);
								warnMsg += " expects ";
								warnMsg += stackValueTypeToString(expected);
								warnMsg += ", but got ";
								warnMsg += stackValueTypeToString(actual);
								reportWarning(ident, warnMsg.c_str());

								// Record the cast direction
								if (actual == StackValueType::INT && expected == StackValueType::FLOAT) {
									paramCasts[j] = CastDirection::INT_TO_FLOAT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::FLOAT;
								} else if (actual == StackValueType::FLOAT && expected == StackValueType::INT) {
									paramCasts[j] = CastDirection::FLOAT_TO_INT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::INT;
								}
							} else {
								// Type mismatch error
								std::string errorMsg = "Type error in function call '";
								errorMsg += name;
								errorMsg += "': Parameter ";
								errorMsg += std::to_string(j + 1);
								errorMsg += " expects ";
								errorMsg += stackValueTypeToString(expected);
								errorMsg += ", but got ";
								errorMsg += stackValueTypeToString(actual);
								reportError(ident, errorMsg.c_str());
							}
						}
					}

					// Store cast information in the identifier node
					ident->setParameterCasts(paramCasts);

					// Validate struct field requirements for PTR parameters
					if (!sig.parameterFieldAccess.empty()) {
						// Build a mapping from struct type name to required fields
						// by matching parameterStructTypes to parameterFieldAccess
						std::unordered_map<std::string, const std::unordered_map<std::string, StackValueType>*>
								structTypeToRequiredFields;
						for (const auto& paramField : sig.parameterFieldAccess) {
							// Find the struct type for this parameter by scanning parameterStructTypes
							for (const auto& pst : sig.parameterStructTypes) {
								// We need to match by name somehow - check if the parameter name matches
								// Since we don't have direct name->index, we'll use struct type matching
								const std::string& expectedStructType = pst.second;
								// If this struct type hasn't been assigned required fields yet, assign them
								if (structTypeToRequiredFields.find(expectedStructType) ==
										structTypeToRequiredFields.end()) {
									// Check if this param's field accesses match this struct type's fields
									const auto* structFieldPtr = lookupStructFieldTypes(expectedStructType);
									if (structFieldPtr != nullptr) {
										const auto& availableFields = *structFieldPtr;
										bool allFieldsMatch = true;
										for (const auto& reqField : paramField.second) {
											if (availableFields.find(reqField.first) == availableFields.end()) {
												allFieldsMatch = false;
												break;
											}
										}
										if (allFieldsMatch && !paramField.second.empty()) {
											structTypeToRequiredFields[expectedStructType] = &paramField.second;
										}
									}
								}
							}
						}

						// Validate each PTR parameter on the stack
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							if (sig.consumes[j] == StackValueType::PTR) {
								size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
								std::string actualStructType = "";
								if (stackIdx < structTypeStack.size()) {
									actualStructType = structTypeStack[stackIdx];
								}

								// Get expected struct type for this parameter index
								std::string expectedStructType = "";
								auto pstIt = sig.parameterStructTypes.find(j);
								if (pstIt != sig.parameterStructTypes.end()) {
									expectedStructType = pstIt->second;
								}

								// Validate struct type matches expectation
								if (!expectedStructType.empty() && !actualStructType.empty() &&
										actualStructType != expectedStructType) {
									std::string errorMsg = "Type error in function call '";
									errorMsg += name;
									errorMsg += "': Parameter ";
									errorMsg += std::to_string(j + 1);
									errorMsg += " expects struct '";
									errorMsg += expectedStructType;
									errorMsg += "', but got '";
									errorMsg += actualStructType;
									errorMsg += "'";
									reportError(ident, errorMsg.c_str());
								}

								// Validate field requirements for this specific struct type
								if (!actualStructType.empty()) {
									auto reqFieldsIt = structTypeToRequiredFields.find(actualStructType);
									if (reqFieldsIt != structTypeToRequiredFields.end()) {
										const auto& requiredFields = *reqFieldsIt->second;
										const auto* structFieldPtr = lookupStructFieldTypes(actualStructType);
										if (structFieldPtr != nullptr) {
											const auto& availableFields = *structFieldPtr;

											for (const auto& requiredFieldEntry : requiredFields) {
												const std::string& requiredField = requiredFieldEntry.first;
												StackValueType expectedType = requiredFieldEntry.second;

												auto fieldIt = availableFields.find(requiredField);
												if (fieldIt == availableFields.end()) {
													std::string errorMsg = "Type error in function call '";
													errorMsg += name;
													errorMsg += "': Struct '";
													errorMsg += actualStructType;
													errorMsg += "' is missing required field '";
													errorMsg += requiredField;
													errorMsg += "'";
													reportError(ident, errorMsg.c_str());
												} else {
													StackValueType actualType = fieldIt->second;
													if (actualType != expectedType &&
															expectedType != StackValueType::UNKNOWN &&
															actualType != StackValueType::UNKNOWN) {
														if (!isImplicitCastAllowed(actualType, expectedType)) {
															std::string errorMsg = "Type error in function call '";
															errorMsg += name;
															errorMsg += "': Field '";
															errorMsg += requiredField;
															errorMsg += "' in struct '";
															errorMsg += actualStructType;
															errorMsg += "' has type ";
															errorMsg += stackValueTypeToString(actualType);
															errorMsg += ", but function expects ";
															errorMsg += stackValueTypeToString(expectedType);
															reportError(ident, errorMsg.c_str());
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}

					// Save consumed struct types before popping (for pass-through tracking)
					std::vector<std::string> consumedStructTypes;
					if (sig.consumes.size() > 0 && structTypeStack.size() >= sig.consumes.size()) {
						// Save struct types in order (bottom to top of consumed portion)
						size_t startIdx = structTypeStack.size() - sig.consumes.size();
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							consumedStructTypes.push_back(structTypeStack[startIdx + j]);
						}
					}

					// Check struct, array and fn-pointer argument types and bind generic type parameters.
					std::map<std::string, std::string> typeBindings;
					bindCallTypeParams(sig, typeStack, structTypeStack, ident, name, typeBindings);

					// Check if any parameter is PTR (function pointer) before consuming
					bool consumesPtrParam = false;
					for (const auto& paramType : sig.consumes) {
						if (paramType == StackValueType::PTR) {
							consumesPtrParam = true;
							break;
						}
					}

					// Consume the parameters from the stack
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}

					// Reset pending function signature only if we consumed a PTR parameter -
					// this ensures signatures from input function pointers don't leak to
					// returned function pointers, but preserves signatures for functions
					// that return closures without taking function pointer parameters
					if (consumesPtrParam) {
						mPendingFnSignature.reset();
					}

					// Results, with bound type parameters substituted; a bare fallible call also
					// leaves the status for the following `if`/`switch`.
					pushCallResults(sig, typeBindings, consumedStructTypes, typeStack, structTypeStack);
					if (sig.throws && !ident->abortOnError() && !ident->propagateOnError()) {
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					}
				} else {
					// Not a local, not a method, not a user function: unresolved, so its stack
					// effect is unknown and the function-level checks cannot be trusted.
					// This must stay inside the else: placed after the block it ran for every
					// call, which switched off the arity, if-arm and defer checks in any
					// function that called anything.
					mHasUnpredictableStack = true;
				}
				break;
			}

			case IAstNode::Type::FIELD_ACCESS: {
				// `struct <<field`: pop the struct, push the field. The struct's type is what its
				// producer left on the struct-type stack -- a local, a call, a construction, `as`,
				// or a previous `<<` of a struct-typed field -- so nothing here needs to know what
				// the operand *was*, only what it is.
				AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
				const std::string& fieldName = fieldAccess->fieldName();

				if (typeStack.empty()) {
					std::string msg =
							"Type error in field access '<<" + fieldName + "': Stack underflow (requires a struct)";
					reportErrorWithHint(fieldAccess, msg.c_str(), "push the struct first, e.g. 'p <<x'");
					typeStack.push_back(StackValueType::UNKNOWN);
					structTypeStack.push_back("");
					break;
				}
				StackValueType operandType = typeStack.back();
				std::string structType = structTypeStack.empty() ? "" : structTypeStack.back();
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}

				StackValueType fieldType = StackValueType::UNKNOWN;
				std::string fieldStructType;
				const auto* knownFields = structType.empty() ? nullptr : lookupStructFieldTypes(structType);
				if (operandType == StackValueType::INT || operandType == StackValueType::FLOAT ||
						operandType == StackValueType::STRING) {
					std::string msg = "Type error in field access '<<" + fieldName + "': the value on the stack is " +
									  stackValueTypeToString(operandType) + ", not a struct";
					reportError(fieldAccess, msg.c_str());
					fieldType = StackValueType::ANY;
				} else if (knownFields != nullptr) {
					auto fieldIt = knownFields->find(fieldName);
					if (fieldIt != knownFields->end()) {
						fieldType = resolveFieldType(structType, fieldName, fieldStructType);
					} else {
						std::string msg = "Type error in field access '<<" + fieldName + "': Struct '" + structType +
										  "' has no field named '" + fieldName + "'";
						std::string suggestion = findSimilarNameInMap(fieldName, *knownFields);
						if (!suggestion.empty()) {
							msg += "; did you mean '" + suggestion + "'?";
						}
						reportError(fieldAccess, msg.c_str());
						fieldType = StackValueType::ANY;
					}
				} else {
					// An untyped pointer: the first struct with a field of this name supplies the type.
					bool found = false;
					for (const auto& structEntry : mStructFieldTypes) {
						auto it = structEntry.second.find(fieldName);
						if (it != structEntry.second.end()) {
							fieldType = it->second;
							found = true;
							break;
						}
					}
					if (!found) {
						std::unordered_set<std::string> allFields;
						for (const auto& structEntry : mStructFieldTypes) {
							for (const auto& field : structEntry.second) {
								allFields.insert(field.first);
							}
						}
						std::string msg = "Type error in field access '<<" + fieldName +
										  "': no struct has a field named '" + fieldName + "'";
						std::string suggestion = findSimilarName(fieldName, allFields);
						if (!suggestion.empty()) {
							msg += "; did you mean '" + suggestion + "'?";
						}
						reportError(fieldAccess, msg.c_str());
						fieldType = StackValueType::ANY;
					}
				}
				typeStack.push_back(fieldType);
				structTypeStack.push_back(fieldStructType);
				break;
			}

			case IAstNode::Type::FIELD_SET: {
				// `struct value >>field`: pop the value, check the field against the struct beneath
				// it, and leave the struct on the stack for chaining.
				AstNodeFieldSet* fieldSet = static_cast<AstNodeFieldSet*>(child);
				const std::string& fieldName = fieldSet->fieldName();

				if (typeStack.size() < 2) {
					std::string msg = "Type error in field set '>>" + fieldName +
									  "': Stack underflow (requires struct and value)";
					reportErrorWithHint(fieldSet, msg.c_str(), "push the struct, then the value, e.g. 'p 42 >>x'");
					if (!typeStack.empty()) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}
					break;
				}
				std::string structType =
						structTypeStack.size() >= 2 ? structTypeStack[structTypeStack.size() - 2] : std::string();
				StackValueType valueType = typeStack.back();
				const auto* knownFields = structType.empty() ? nullptr : lookupStructFieldTypes(structType);
				if (knownFields != nullptr) {
					auto fieldIt = knownFields->find(fieldName);
					if (fieldIt == knownFields->end()) {
						std::string msg = "Field '" + fieldName + "' does not exist in struct '" + structType + "'";
						std::string suggestion = findSimilarNameInMap(fieldName, *knownFields);
						if (!suggestion.empty()) {
							msg += "; did you mean '" + suggestion + "'?";
						}
						reportError(fieldSet, msg.c_str());
					} else {
						std::string fieldOwnType;
						bool genericField = false;
						StackValueType expected = resolveFieldType(structType, fieldName, fieldOwnType, &genericField);
						if (!genericField && !fieldOwnType.empty()) {
							genericField = isCurrentTypeParam(fieldOwnType);
						}
						bool checkable = !genericField && expected != StackValueType::ANY &&
										 expected != StackValueType::UNKNOWN && expected != StackValueType::TYPEVAR &&
										 valueType != StackValueType::ANY && valueType != StackValueType::UNKNOWN &&
										 valueType != StackValueType::TYPEVAR;
						if (checkable && valueType != expected && !isImplicitCastAllowed(valueType, expected)) {
							std::string msg = "Type error in field set '>>" + fieldName + "': field '" + fieldName +
											  "' of struct '" + structType + "' is " +
											  stackValueTypeToString(expected) + ", but the value is " +
											  stackValueTypeToString(valueType);
							reportError(fieldSet, msg.c_str());
						} else if (checkable && expected == StackValueType::PTR) {
							// Both are pointers, which is where the coarse compare above stops. The
							// field's own type is what separates them, and `>>field` is a definite
							// assignment like a struct construction, so it gets the same rule.
							std::string why;
							const std::string valueStructType =
									structTypeStack.empty() ? std::string() : structTypeStack.back();
							if (fieldValueRejected(structType, fieldName, fieldOwnType, valueStructType, why)) {
								std::string msg = "Type error in field set '>>" + fieldName + "': field '" + fieldName +
												  "' of struct '" + structType + "' " + why;
								reportError(fieldSet, msg.c_str());
							}
						}
					}
				} else {
					bool fieldFound = false;
					for (const auto& structEntry : mStructFieldTypes) {
						if (structEntry.second.find(fieldName) != structEntry.second.end()) {
							fieldFound = true;
							break;
						}
					}
					if (!fieldFound) {
						std::unordered_set<std::string> allFields;
						for (const auto& structEntry : mStructFieldTypes) {
							for (const auto& field : structEntry.second) {
								allFields.insert(field.first);
							}
						}
						std::string msg = "Type error in field set '>>" + fieldName +
										  "': no struct has a field named '" + fieldName + "'";
						std::string suggestion = findSimilarName(fieldName, allFields);
						if (!suggestion.empty()) {
							msg += "; did you mean '" + suggestion + "'?";
						}
						reportError(fieldSet, msg.c_str());
					}
				}

				// Pop the value; the struct stays.
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
				break;
			}

			case IAstNode::Type::SCOPED_IDENTIFIER: {
				// Handle module constants, structs, or function calls
				AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);

				// Resolve sb::append_any at type-check time based on the type stack
				// This is generated by $"..." string interpolation
				// Use mResolvedInterpolations to only resolve each node once
				if (scoped->scope() == "sb" && scoped->name() == "append_any" &&
						mResolvedInterpolations.find(scoped) == mResolvedInterpolations.end()) {
					mResolvedInterpolations.insert(scoped);
					if (!typeStack.empty() && typeStack.back() == StackValueType::STRING) {
						scoped->setName("append");
					} else {
						scoped->setName("append_int");
					}
				}

				const std::string& moduleName = scoped->scope();
				const std::string& functionName = scoped->name();
				std::string qualifiedName = moduleName + "::" + functionName;

				// Imported C functions and variadic stdlib functions may have signatures
				// that don't reflect all consumed values. Mark stack as unpredictable.
				if (mImportedLibraryFunctions.find(qualifiedName) != mImportedLibraryFunctions.end() ||
						qualifiedName == "fmt::printf" || qualifiedName == "fmt::sprintf") {
					mHasUnpredictableStack = true;
				}

				// Check if this is a local enum variant (e.g., Color::Red)
				if (mDefinedEnums.find(moduleName) != mDefinedEnums.end()) {
					if (mConstantValues.find(qualifiedName) != mConstantValues.end()) {
						typeStack.push_back(StackValueType::INT);
						structTypeStack.push_back("");
						break;
					}
				}

				// Also check local constant values directly (for enum variants in module files)
				if (mConstantValues.find(qualifiedName) != mConstantValues.end()) {
					typeStack.push_back(StackValueType::INT);
					structTypeStack.push_back("");
					break;
				}

				// Self-qualified module constant. `time.qd` declares
				// `import "libtime.a" as "time"` and reads its own `pub const Millisecond` as
				// `time::Millisecond`; the constant is registered under the bare name, so the
				// qualified lookup above misses it. Guarded on the scope being an FFI namespace
				// so this cannot swallow a genuinely unresolved `othermodule::Name`. Without it
				// the reference pushed nothing and the following `/` reported a spurious
				// "Stack underflow (requires 2 numeric values)".
				if (mImportedLibraries.find(moduleName) != mImportedLibraries.end()) {
					auto selfConstIt = mConstantValues.find(functionName);
					if (selfConstIt != mConstantValues.end()) {
						const std::string& value = selfConstIt->second;
						if (!value.empty() && value[0] == '"') {
							typeStack.push_back(StackValueType::STRING);
						} else if (isFloatLiteralText(value)) {
							typeStack.push_back(StackValueType::FLOAT);
						} else {
							typeStack.push_back(StackValueType::INT);
						}
						structTypeStack.push_back("");
						break;
					}
				}

				// Check if this is a constant first
				auto constIt = mModuleConstants.find(moduleName);
				if (constIt != mModuleConstants.end()) {
					const auto& constants = constIt->second;
					if (constants.find(functionName) != constants.end()) {
						// This is a constant - determine its type and push onto the stack
						std::string constQualifiedName = moduleName + "::" + functionName;
						auto valueIt = mModuleConstantValues.find(constQualifiedName);
						if (valueIt != mModuleConstantValues.end()) {
							const std::string& value = valueIt->second;
							// Infer type from value
							if (!value.empty() && value[0] == '"') {
								typeStack.push_back(StackValueType::STRING);
							} else if (isFloatLiteralText(value)) {
								typeStack.push_back(StackValueType::FLOAT);
							} else {
								typeStack.push_back(StackValueType::INT);
							}
						} else {
							typeStack.push_back(StackValueType::UNKNOWN);
						}
						structTypeStack.push_back("");
						break;
					}
				}

				// Check if this is a struct construction
				auto moduleStructsIt = mModuleStructs.find(moduleName);
				if (moduleStructsIt != mModuleStructs.end()) {
					const auto& structs = moduleStructsIt->second;
					auto structIt = structs.find(functionName);
					if (structIt != structs.end() && structIt->second) {
						// This is a struct construction from a module
						// Use mStructFieldTypes and mStructFieldOrder since the AST node may be invalid
						// Use qualified name for lookup since module structs are stored with qualified keys
						const auto* structFieldTypesPtr = lookupStructFieldTypes(qualifiedName);
						auto structFieldOrderIt = mStructFieldOrder.find(qualifiedName);

						if (structFieldTypesPtr != nullptr && structFieldOrderIt != mStructFieldOrder.end()) {
							const auto& fieldTypes = *structFieldTypesPtr;
							const auto& fieldOrder = structFieldOrderIt->second;
							size_t fieldCount = fieldOrder.size();

							// Check if we have enough values on the stack
							if (typeStack.size() < fieldCount) {
								std::string errorMsg = "Type error in struct construction '";
								errorMsg += qualifiedName;
								errorMsg += "': Stack underflow (requires ";
								errorMsg += std::to_string(fieldCount);
								errorMsg += " values, have ";
								errorMsg += std::to_string(typeStack.size());
								errorMsg += ")";
								reportError(scoped, errorMsg.c_str());
							} else {
								// Validate each field type (fields are in declaration order)
								// Fields are consumed bottom-to-top from the stack
								for (size_t fi = 0; fi < fieldOrder.size(); fi++) {
									size_t stackIdx = typeStack.size() - fieldOrder.size() + fi;
									StackValueType actual = typeStack[stackIdx];
									const std::string actualStructType =
											(stackIdx < structTypeStack.size()) ? structTypeStack[stackIdx] : "";
									const std::string& fieldName = fieldOrder[fi];

									// Get expected type from fieldTypes
									auto fieldTypeIt = fieldTypes.find(fieldName);
									if (fieldTypeIt == fieldTypes.end()) {
										continue;
									}
									StackValueType expected = fieldTypeIt->second;

									std::string expectedStructType;
									auto fstIt = mStructFieldStructTypes.find(qualifiedName);
									if (fstIt != mStructFieldStructTypes.end()) {
										auto nm = fstIt->second.find(fieldName);
										if (nm != fstIt->second.end()) {
											expectedStructType = nm->second;
										}
									}
									if (!expectedStructType.empty() && isCurrentTypeParam(expectedStructType)) {
										continue; // a generic field binds to whatever it is given
									}

									// Skip check if actual type is UNKNOWN (can't determine type)
									if (actual == StackValueType::UNKNOWN) {
										continue;
									}

									// Check for type mismatch
									if (actual != expected) {
										// Check if implicit cast is allowed (int <-> float)
										if (isImplicitCastAllowed(actual, expected)) {
											// Only warn for casts that should be warned about
											if (shouldWarnImplicitCast(actual, expected)) {
												std::string warnMsg = "Implicit cast in struct construction '";
												warnMsg += qualifiedName;
												warnMsg += "': Field '";
												warnMsg += fieldName;
												warnMsg += "' expects ";
												warnMsg += stackValueTypeToString(expected);
												warnMsg += ", but got ";
												warnMsg += stackValueTypeToString(actual);
												reportWarning(scoped, warnMsg.c_str());
											}
										} else {
											// Type mismatch error
											std::string errorMsg = "Type error in struct construction '";
											errorMsg += qualifiedName;
											errorMsg += "': Field '";
											errorMsg += fieldName;
											errorMsg += "' expects ";
											errorMsg += stackValueTypeToString(expected);
											errorMsg += ", but got ";
											errorMsg += stackValueTypeToString(actual);
											reportError(scoped, errorMsg.c_str());
										}
									} else if (std::string why; fieldValueRejected(qualifiedName, fieldName,
													   expectedStructType, actualStructType, why)) {
										// `mod::Name` constructed positionally: the last of the five, and
										// the one that had no pointer-type check of any kind.
										std::string errorMsg = "Type error in struct construction '";
										errorMsg += qualifiedName;
										errorMsg += "': Field '";
										errorMsg += fieldName;
										errorMsg += "' ";
										errorMsg += why;
										reportError(scoped, errorMsg.c_str());
									}
								}
							}

							// Pop field values from stack
							for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
								typeStack.pop_back();
								if (!structTypeStack.empty()) {
									structTypeStack.pop_back();
								}
							}
						}

						// Struct pointers can now be used flexibly - codegen handles all cases

						// Push pointer type for the constructed struct, with qualified name as type
						typeStack.push_back(StackValueType::PTR);
						structTypeStack.push_back(functionName); // Track the struct type (bare name)
						break;
					}
				}

				// Check if this is an explicit method call (StructType::method)
				// The scope could be a struct type name
				if ((mDefinedStructs.count(moduleName) || mStructMethods.count(moduleName)) &&
						mStructMethods.count(moduleName) && mStructMethods.at(moduleName).count(functionName)) {
					// Explicit method call - check that stack top matches the receiver type
					std::string receiverStructType;
					if (!typeStack.empty() && typeStack.back() == StackValueType::PTR && !structTypeStack.empty()) {
						receiverStructType = structTypeStack.back();
					}

					// Verify receiver type matches the explicit type (or is a subtype in the future)
					if (!receiverStructType.empty() && receiverStructType != moduleName) {
						std::string errorMsg = "Type error: Explicit method call '";
						errorMsg += qualifiedName;
						errorMsg += "' expects receiver of type '";
						errorMsg += moduleName;
						errorMsg += "' but stack top is '";
						errorMsg += receiverStructType;
						errorMsg += "'";
						reportError(scoped, errorMsg.c_str());
					}

					// Look up method signature
					auto methodSigIt = mFunctionSignatures.find(qualifiedName);
					if (methodSigIt != mFunctionSignatures.end()) {
						const FunctionSignature& sig = methodSigIt->second;

						// Validate '!' and '?' usage
						if ((scoped->abortOnError() || scoped->propagateOnError()) && !sig.throws) {
							std::string op = scoped->abortOnError() ? "!" : "?";
							std::string errorMsg = "Cannot use '" + op + "' operator on method '" + functionName +
												   "' which is not marked as fallible";
							reportError(scoped, errorMsg.c_str());
						}

						// Check '?' requires caller to be fallible
						if (scoped->propagateOnError() && !mCurrentFunctionFallible) {
							std::string errorMsg =
									"Cannot use '?' operator on method '" + functionName +
									"': enclosing function must be fallible (add '!' to function signature)";
							reportError(scoped, errorMsg.c_str());
						}

						// Check fallible methods without ! or ? must be followed by 'if' or 'switch'
						if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
							IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
							if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
													 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
								std::string errorMsg =
										"Fallible method '" + functionName +
										"' must be immediately followed by 'if' or 'switch' to check for "
										"errors, or use '!' to abort on error, or '?' to propagate";
								reportError(scoped, errorMsg.c_str());
							}
						}

						// Check if stack has enough values (receiver + params)
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;
						if (typeStack.size() < 1 + additionalParams) {
							std::string errorMsg = "Type error in method call '";
							errorMsg += functionName;
							errorMsg += "': Stack underflow (requires receiver + ";
							errorMsg += std::to_string(additionalParams);
							errorMsg += " values)";
							reportError(scoped, errorMsg.c_str());
							break;
						}

						// Validate parameter types (skip receiver at index 0)
						// Stack order: [param1, param2, ..., paramN, receiver]
						// Signature: [receiver, param1, param2, ..., paramN]
						// For sig index j (1 to N), stack index is: size - consumes.size() + (j-1)
						for (size_t j = 1; j < sig.consumes.size(); j++) {
							size_t stackIdx = typeStack.size() - sig.consumes.size() + (j - 1);
							StackValueType expected = sig.consumes[j];
							StackValueType actual = typeStack[stackIdx];

							// ANY and UNKNOWN cannot be checked; a TYPEVAR is a generic parameter that accepts
							// any concrete type at the call site, exactly as in the function-call path.
							if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
									expected == StackValueType::TYPEVAR) {
								continue;
							}
							if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY ||
									actual == StackValueType::TYPEVAR) {
								continue;
							}

							if (actual != expected) {
								if (isImplicitCastAllowed(actual, expected)) {
									std::string warnMsg = "Implicit cast in method call '";
									warnMsg += functionName;
									warnMsg += "': Parameter ";
									warnMsg += std::to_string(j);
									warnMsg += " expects ";
									warnMsg += stackValueTypeToString(expected);
									warnMsg += ", but got ";
									warnMsg += stackValueTypeToString(actual);
									reportWarning(scoped, warnMsg.c_str());
								} else {
									std::string errorMsg = "Type error in method call '";
									errorMsg += functionName;
									errorMsg += "': Parameter ";
									errorMsg += std::to_string(j);
									errorMsg += " expects ";
									errorMsg += stackValueTypeToString(expected);
									errorMsg += ", but got ";
									errorMsg += stackValueTypeToString(actual);
									reportError(scoped, errorMsg.c_str());
								}
							}
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values
						{
							std::vector<std::string> noConsumed;
							pushCallResults(sig, receiverTypeBindings(receiverStructType), noConsumed, typeStack,
									structTypeStack);
						}
						if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
							typeStack.push_back(StackValueType::INT);
							structTypeStack.push_back("");
						}

						// Mark as method call for code generation
						scoped->setIsMethodCall(true);
						scoped->setMethodInputParamCount(additionalParams);
						// For explicit method calls, receiver is always on top of stack
						scoped->setMethodReceiverPositionFromTop(0);
						break;
					}
				}

				// Check if this is a module method call (e.g., sb::append on a StringBuilder)
				// Search the type stack for a struct from this module at the expected depth
				if (!typeStack.empty() && !structTypeStack.empty()) {
					// Try to find a method with this name on any struct from this module
					// by searching the structTypeStack from top down
					bool foundModuleMethod = false;
					for (size_t searchDepth = 0; searchDepth < typeStack.size() && !foundModuleMethod; searchDepth++) {
						size_t stackIdx = typeStack.size() - 1 - searchDepth;
						if (typeStack[stackIdx] != StackValueType::PTR) {
							continue;
						}
						if (stackIdx >= structTypeStack.size()) {
							continue;
						}
						std::string receiverStructType = structTypeStack[stackIdx];
						if (receiverStructType.empty()) {
							continue;
						}

						// Check if receiver struct belongs to this module
						bool isModuleStruct = receiverStructType.find(moduleName + "::") == 0;
						if (!isModuleStruct) {
							std::string qualified = moduleName + "::" + receiverStructType;
							if (mStructMethods.count(qualified) || mStructMethods.count(receiverStructType)) {
								isModuleStruct = true;
							}
						}
						if (!isModuleStruct) {
							continue;
						}

						// Find the registered struct type for method lookup
						std::string registeredStructType = findMethodStructType(receiverStructType, functionName);
						if (registeredStructType.empty() && receiverStructType.find("::") == std::string::npos) {
							registeredStructType =
									findMethodStructType(moduleName + "::" + receiverStructType, functionName);
						}
						if (registeredStructType.empty()) {
							continue;
						}

						// Found a method — verify parameter count matches receiver position
						std::string methodQualifiedName = registeredStructType + "::" + functionName;
						auto methodSigIt = mFunctionSignatures.find(methodQualifiedName);
						if (methodSigIt == mFunctionSignatures.end()) {
							continue;
						}
						const FunctionSignature& sig = methodSigIt->second;
						size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;

						// Receiver should be at exactly additionalParams from top
						if (searchDepth != additionalParams) {
							continue;
						}

						// Validate '!' and '?' usage
						if ((scoped->abortOnError() || scoped->propagateOnError()) && !sig.throws) {
							std::string op = scoped->abortOnError() ? "!" : "?";
							std::string errorMsg = "Cannot use '" + op + "' operator on method '" + functionName +
												   "' which is not marked as fallible";
							reportError(scoped, errorMsg.c_str());
						}

						// Check '?' requires caller to be fallible
						if (scoped->propagateOnError() && !mCurrentFunctionFallible) {
							std::string errorMsg =
									"Cannot use '?' operator on method '" + functionName +
									"': enclosing function must be fallible (add '!' to function signature)";
							reportError(scoped, errorMsg.c_str());
						}

						// Pop all consumed values (receiver + params)
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							typeStack.pop_back();
							if (!structTypeStack.empty()) {
								structTypeStack.pop_back();
							}
						}

						// Push return values. A bare fallible call also leaves the status the
						// following `if`/`switch` tests -- the explicit `Type::method` path above
						// has always done this, and this one did not, so `f "--name"
						// flag::string if { ... }` had the `if` consume the *result* as its
						// condition and the success arm read an empty stack. It went unnoticed
						// because the only callers sat downstream of `read`, whose sixteen
						// synthetic strings left enough fiction on the type stack to absorb it.
						{
							std::vector<std::string> noConsumed;
							pushCallResults(sig, receiverTypeBindings(receiverStructType), noConsumed, typeStack,
									structTypeStack);
						}
						if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
							typeStack.push_back(StackValueType::INT);
							structTypeStack.push_back("");
						}

						// Mark as method call for code generation
						scoped->setIsMethodCall(true);
						scoped->setReceiverType(registeredStructType);
						scoped->setMethodInputParamCount(additionalParams);
						scoped->setMethodReceiverPositionFromTop(additionalParams);
						foundModuleMethod = true;
						break;
					}
					if (foundModuleMethod) {
						break;
					}
				}

				// Look up the module function signature
				auto sigIt = mFunctionSignatures.find(qualifiedName);
				if (sigIt != mFunctionSignatures.end()) {
					const FunctionSignature& sig = sigIt->second;

					// Validate '!' and '?' usage: only allowed on fallible functions (marked with '!')
					if ((scoped->abortOnError() || scoped->propagateOnError()) && !sig.throws) {
						std::string op = scoped->abortOnError() ? "!" : "?";
						std::string errorMsg = "Cannot use '" + op + "' operator on function '" + qualifiedName +
											   "' which is not marked as fallible (add '!' after signature)";
						reportError(scoped, errorMsg.c_str());
					}

					// Check '?' requires caller to be fallible
					if (scoped->propagateOnError() && !mCurrentFunctionFallible) {
						std::string errorMsg = "Cannot use '?' operator on function '" + qualifiedName +
											   "': enclosing function must be fallible (add '!' to function signature)";
						reportError(scoped, errorMsg.c_str());
					}

					// Check fallible functions without ! or ? must be followed by 'if' or 'switch'
					if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
						IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
						if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
												 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
							std::string errorMsg = "Fallible function '" + qualifiedName +
												   "' must be immediately followed by 'if' or 'switch' to check for "
												   "errors, or use '!' to abort on error, or '?' to propagate";
							reportError(scoped, errorMsg.c_str());
						}
					}

					// Check if stack has enough values for function parameters
					if (typeStack.size() < sig.consumes.size()) {
						std::string errorMsg = "Type error in function call '";
						errorMsg += qualifiedName;
						errorMsg += "': Stack underflow (requires ";
						errorMsg += std::to_string(sig.consumes.size());
						errorMsg += " values, have ";
						errorMsg += std::to_string(typeStack.size());
						errorMsg += ")";
						reportError(scoped, errorMsg.c_str());
						break;
					}

					// Track which parameters need casts
					std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);

					// Check if the types match
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
						StackValueType expected = sig.consumes[j];
						StackValueType actual = typeStack[stackIdx];

						// Skip check if expected type is ANY, UNKNOWN, or TYPEVAR (generic type parameter)
						// TYPEVAR accepts any concrete type at the call site
						if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
								expected == StackValueType::TYPEVAR) {
							continue;
						}

						// Skip check if actual type is UNKNOWN, ANY, or TYPEVAR (can't determine type at compile time)
						// TYPEVAR occurs when a generic type parameter (like T) is passed to a typed function
						if (actual == StackValueType::UNKNOWN || actual == StackValueType::ANY ||
								actual == StackValueType::TYPEVAR) {
							continue;
						}

						// Check for type mismatch
						if (actual != expected) {
							// Check if implicit cast is allowed (int <-> float)
							if (isImplicitCastAllowed(actual, expected)) {
								// Warn about implicit cast
								std::string warnMsg = "Implicit cast in function call '";
								warnMsg += qualifiedName;
								warnMsg += "': Parameter ";
								warnMsg += std::to_string(j + 1);
								warnMsg += " expects ";
								warnMsg += stackValueTypeToString(expected);
								warnMsg += ", but got ";
								warnMsg += stackValueTypeToString(actual);
								reportWarning(scoped, warnMsg.c_str());

								// Record the cast direction
								if (actual == StackValueType::INT && expected == StackValueType::FLOAT) {
									paramCasts[j] = CastDirection::INT_TO_FLOAT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::FLOAT;
								} else if (actual == StackValueType::FLOAT && expected == StackValueType::INT) {
									paramCasts[j] = CastDirection::FLOAT_TO_INT;
									// Update type stack to reflect the cast
									typeStack[stackIdx] = StackValueType::INT;
								}
							} else {
								// Type mismatch error
								std::string errorMsg = "Type error in function call '";
								errorMsg += qualifiedName;
								errorMsg += "': Parameter ";
								errorMsg += std::to_string(j + 1);
								errorMsg += " expects ";
								errorMsg += stackValueTypeToString(expected);
								errorMsg += ", but got ";
								errorMsg += stackValueTypeToString(actual);
								reportError(scoped, errorMsg.c_str());
							}
						}
					}

					// Store cast information in the scoped identifier node
					scoped->setParameterCasts(paramCasts);

					// Struct types of the arguments, bottom to top, for pass-through results.
					std::vector<std::string> consumedStructTypes;
					if (sig.consumes.size() > 0 && structTypeStack.size() >= sig.consumes.size()) {
						size_t startIdx = structTypeStack.size() - sig.consumes.size();
						for (size_t j = 0; j < sig.consumes.size(); j++) {
							consumedStructTypes.push_back(structTypeStack[startIdx + j]);
						}
					}

					// Check struct, array and fn-pointer argument types and bind generic type parameters.
					// Module-qualified calls never had this check; the identifier path always did.
					std::map<std::string, std::string> typeBindings;
					bindCallTypeParams(sig, typeStack, structTypeStack, scoped, qualifiedName, typeBindings);

					// Check if any parameter is PTR (function pointer) before consuming
					bool consumesPtrParam = false;
					for (const auto& paramType : sig.consumes) {
						if (paramType == StackValueType::PTR) {
							consumesPtrParam = true;
							break;
						}
					}

					// Consume the parameters from the stack
					for (size_t j = 0; j < sig.consumes.size(); j++) {
						typeStack.pop_back();
						if (!structTypeStack.empty()) {
							structTypeStack.pop_back();
						}
					}

					if (consumesPtrParam) {
						mPendingFnSignature.reset();
					}

					pushCallResults(sig, typeBindings, consumedStructTypes, typeStack, structTypeStack);
					if (sig.throws && !scoped->abortOnError() && !scoped->propagateOnError()) {
						typeStack.push_back(StackValueType::INT); // Error status (0 or 1)
						structTypeStack.push_back("");
					}
				} else {
					// Signature not found: the module was not loaded or analysed, so the stack
					// effect is unknown. Inside the else for the same reason as the IDENTIFIER case.
					mHasUnpredictableStack = true;
				}
				break;
			}

			case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
				// Function pointer references push a pointer type onto the stack
				AstNodeFunctionPointerReference* fnRef = static_cast<AstNodeFunctionPointerReference*>(child);
				typeStack.push_back(StackValueType::PTR);
				// Look up the function's signature and build fn type string
				std::string fnTypeStr = "";
				auto sigIt = mFunctionSignatures.find(fnRef->functionName());
				if (sigIt != mFunctionSignatures.end()) {
					mPendingFnSignature = sigIt->second;
					fnTypeStr = buildFnTypeString(sigIt->second);
				}
				structTypeStack.push_back(fnTypeStr);
				break;
			}

			case IAstNode::Type::ANONYMOUS_FUNCTION: {
				// Anonymous functions push a function pointer onto the stack
				AstNodeAnonymousFunction* anonFunc = static_cast<AstNodeAnonymousFunction*>(child);

				// Extract the function signature from input/output parameters
				FunctionSignature sig;
				for (const auto& paramNode : anonFunc->inputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					sig.consumes.push_back(stringToStackValueType(param->typeString()));
				}
				for (const auto& paramNode : anonFunc->outputParameters()) {
					AstNodeParameter* param = static_cast<AstNodeParameter*>(paramNode.get());
					sig.produces.push_back(stringToStackValueType(param->typeString()));
				}
				// `fn(...)` type strings cannot carry it, so a lambda's fallibility reaches the
				// call site only through the pending signature -- the same route a named
				// function's does via `&f`.
				sig.throws = anonFunc->throws();
				std::string fnTypeStr = buildFnTypeString(sig);

				// Store as pending signature for subsequent LOCAL or call
				mPendingFnSignature = sig;

				typeStack.push_back(StackValueType::PTR);
				structTypeStack.push_back(fnTypeStr);

				typeCheckAnonymousFunction(anonFunc, localVariables);
				break;
			}

			case IAstNode::Type::RETURN_STATEMENT: {
				// Validate that the stack has the expected number of return values
				if (mCurrentFunctionOutputCount > 0 && typeStack.size() < mCurrentFunctionOutputCount) {
					std::string errorMsg = "'return' in function expecting ";
					errorMsg += std::to_string(mCurrentFunctionOutputCount);
					errorMsg += " return value";
					if (mCurrentFunctionOutputCount > 1) {
						errorMsg += "s";
					}
					errorMsg += ", but stack has ";
					errorMsg += std::to_string(typeStack.size());
					reportError(child, errorMsg.c_str());
				}
				// Stop processing this block — code after return is unreachable
				return;
			}

			case IAstNode::Type::AS_CAST: {
				// Type narrowing cast: updates struct type on top of stack
				// No push/pop — just changes the type annotation
				AstNodeAsCast* asCast = static_cast<AstNodeAsCast*>(child);
				if (!structTypeStack.empty()) {
					structTypeStack.back() = asCast->typeName();
				}
				break;
			}

			case IAstNode::Type::BREAK_STATEMENT:
			case IAstNode::Type::CONTINUE_STATEMENT: {
				// Record where this jump leaves the stack; the enclosing loop checks it against
				// the depth at the loop head. Processing continues rather than returning, so
				// anything written after the jump is still checked as it was before.
				mLoopJumps.push_back(
						{child, child->type() == IAstNode::Type::BREAK_STATEMENT, typeStack, structTypeStack});
				break;
			}

			default:
				// Other node types don't affect the type stack
				break;
			}
		}
	}

	// typeCheckInstruction and typeCheckInstructionInternal are in semantic_validator_instructions.cc

} // namespace Qd
