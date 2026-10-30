#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
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
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/numeric_literal.h>
#include <quadrate/qc/semantic_validator.h>
#include <type_traits>
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
		case StackValueType::D128:
			return "d128";
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
		case AstNodeLiteral::LiteralType::D128:
			return StackValueType::D128;
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
	// `fmt::printf`/`sprintf`/`fprintf` pop one value per `%` in the format string, a count known only at
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
			return functionName == "printf" || functionName == "sprintf" || functionName == "fprintf";
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

	CastDirection SemanticValidator::argumentCast(const IAstNode* site, const char* callKind, const std::string& callee,
			size_t paramNumber, StackValueType expected, StackValueType& actual) {
		// A parameter of type any, or a generic one, takes whatever it is given
		if (expected == StackValueType::ANY || expected == StackValueType::UNKNOWN ||
				expected == StackValueType::TYPEVAR) {
			return CastDirection::NONE;
		}
		// An `any` is whatever it holds, which only the running program knows
		if (actual == StackValueType::ANY) {
			actual = expected;
			switch (expected) {
			case StackValueType::INT:
				return CastDirection::ANY_TO_INT;
			case StackValueType::FLOAT:
				return CastDirection::ANY_TO_FLOAT;
			case StackValueType::D128:
				return CastDirection::ANY_TO_D128;
			case StackValueType::STRING:
				return CastDirection::ANY_TO_STR;
			case StackValueType::PTR:
				return CastDirection::ANY_TO_PTR;
			default:
				return CastDirection::NONE;
			}
		}
		if (actual == StackValueType::UNKNOWN || actual == StackValueType::TYPEVAR || actual == expected) {
			return CastDirection::NONE;
		}
		// An i64 widens to a d128 exactly, so there is nothing to warn about
		if (actual == StackValueType::INT && expected == StackValueType::D128) {
			actual = expected;
			return CastDirection::INT_TO_D128;
		}

		std::string message = std::string(callKind) + " '" + callee + "': Parameter " + std::to_string(paramNumber) +
							  " expects " + stackValueTypeToString(expected) + ", but got " +
							  stackValueTypeToString(actual);
		if (!isImplicitCastAllowed(actual, expected)) {
			reportError(site, ("Type error in " + message).c_str());
			return CastDirection::NONE;
		}
		reportWarning(site, ("Implicit cast in " + message).c_str());
		CastDirection cast = CastDirection::NONE;
		if (actual == StackValueType::INT && expected == StackValueType::FLOAT) {
			cast = CastDirection::INT_TO_FLOAT;
		} else if (actual == StackValueType::FLOAT && expected == StackValueType::INT) {
			cast = CastDirection::FLOAT_TO_INT;
		}
		actual = expected;
		return cast;
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
							concrete == StackValueType::D128 || concrete == StackValueType::STRING) {
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

	std::string SemanticValidator::canonicalStructName(const std::string& written) const {
		// An alias is the type it names, so `[]Flag` unifies with `[]i64` when Flag is i64
		const std::string name = resolveTypeAliasChain(written);
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
		const std::string resolved = resolveTypeAliasChain(typeStr);
		std::vector<std::string> ins, outs;
		if (!splitFnType(resolved, ins, outs)) {
			return std::nullopt;
		}
		FunctionSignature sig;
		auto add = [&](const std::vector<std::string>& names, std::vector<StackValueType>& types,
						   std::unordered_map<size_t, std::string>& structTypes,
						   std::unordered_map<size_t, std::string>& typeNames) {
			for (size_t i = 0; i < names.size(); i++) {
				const std::string name = resolveTypeAliasChain(names[i]);
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
							concrete == StackValueType::D128 || concrete == StackValueType::STRING) {
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
				const std::string resolvedType = resolveTypeAliasChain(typeStr);

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
				case AstNodeLiteral::LiteralType::D128:
					return "d128";
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
				case StackValueType::D128:
					return "d128";
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

			const std::string resolvedType = resolveTypeAliasChain(param->typeString());
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

		if (isBuiltinTypeName(elem)) {
			return;
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

		const std::string resolved = resolveTypeAliasChain(elem);

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
				} else if (lit->literalType() == AstNodeLiteral::LiteralType::D128) {
					std::string problem = d128LiteralProblem(lit->value());
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

			case IAstNode::Type::INSTRUCTION:
				typeCheckInstructionNode(child, node, i, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::BLOCK: {
				// Recursively check nested blocks
				typeCheckBlock(child, typeStack, localVariables, structTypeStack);
				break;
			}

			case IAstNode::Type::IF_STATEMENT:
				typeCheckIf(child, node, i, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::DEFER_STATEMENT:
				typeCheckDefer(child, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::SWITCH_STATEMENT:
				typeCheckSwitch(child, node, i, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::FOR_STATEMENT:
				typeCheckFor(child, typeStack, localVariables, structTypeStack);
				break;

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

			case IAstNode::Type::WHILE_STATEMENT:
				typeCheckWhile(child, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::LOCAL:
				typeCheckLocal(child, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::STRUCT_CONSTRUCTION:
				typeCheckStructConstruction(child, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::IDENTIFIER:
				typeCheckIdentifier(child, node, i, typeStack, localVariables, structTypeStack);
				break;

			case IAstNode::Type::FIELD_ACCESS:
				typeCheckFieldAccess(child, typeStack, structTypeStack);
				break;

			case IAstNode::Type::FIELD_SET:
				typeCheckFieldSet(child, typeStack, structTypeStack);
				break;

			case IAstNode::Type::SCOPED_IDENTIFIER:
				typeCheckScopedIdentifier(child, node, i, typeStack, structTypeStack);
				break;

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

	// `!` and `?` only on a fallible callee, `?` only in a fallible caller, and a bare
	// fallible call tested by the `if` or `switch` right after it
	template <typename Call>
	void SemanticValidator::checkFallibleCall(Call* call, const FunctionSignature& sig, bool isMethod,
			const std::string& name, IAstNode* node, size_t i) {
		const std::string what = isMethod ? "method '" + name + "'" : "function '" + name + "'";
		if ((call->abortOnError() || call->propagateOnError()) && !sig.throws) {
			std::string errorMsg = std::string("Cannot use '") + (call->abortOnError() ? "!" : "?") + "' operator on " +
								   what + " which is not marked as fallible";
			if (!isMethod) {
				errorMsg += " (add '!' after signature)";
			}
			reportError(call, errorMsg.c_str());
		}
		if (call->propagateOnError() && !mCurrentFunctionFallible) {
			std::string errorMsg = "Cannot use '?' operator on " + what +
								   ": enclosing function must be fallible (add '!' to function signature)";
			reportError(call, errorMsg.c_str());
		}
		if (sig.throws && !call->abortOnError() && !call->propagateOnError()) {
			IAstNode* nextNode = (i + 1 < node->childCount()) ? node->child(i + 1) : nullptr;
			if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
									 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
				std::string errorMsg = std::string("Fallible ") + what +
									   " must be immediately followed by 'if' or 'switch' to check for "
									   "errors, or use '!' to abort on error, or '?' to propagate";
				reportError(call, errorMsg.c_str());
			}
		}
	}

	// A call's effect on the stack: its arguments off, its results on, and for a bare
	// fallible call the status the `if` or `switch` after it tests
	template <typename Call>
	void SemanticValidator::applyCallEffect(Call* call, const FunctionSignature& sig,
			const std::map<std::string, std::string>& typeBindings, const std::vector<std::string>& consumedStructTypes,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		for (size_t j = 0; j < sig.consumes.size(); j++) {
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
		}
		pushCallResults(sig, typeBindings, consumedStructTypes, typeStack, structTypeStack);
		if (sig.throws && !call->abortOnError() && !call->propagateOnError()) {
			typeStack.push_back(StackValueType::INT); // Error status
			structTypeStack.push_back("");
		}
	}

	// The struct types of the top `count` values, bottom to top: what a result that passes
	// an argument's struct type through reads. Empty when the model is shallower.
	static std::vector<std::string> topStructTypes(const std::vector<std::string>& structTypeStack, size_t count) {
		if (count == 0 || structTypeStack.size() < count) {
			return {};
		}
		return {structTypeStack.end() - static_cast<std::ptrdiff_t>(count), structTypeStack.end()};
	}

	// A struct argument against what the function reads of it: the struct it declares, and
	// the fields its body reads with their types
	void SemanticValidator::checkParameterFields(const IAstNode* site, const FunctionSignature& sig,
			const std::string& name, const std::vector<StackValueType>& typeStack,
			const std::vector<std::string>& structTypeStack) {
		if (sig.parameterFieldAccess.empty()) {
			return;
		}
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
				if (structTypeToRequiredFields.find(expectedStructType) == structTypeToRequiredFields.end()) {
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
					reportError(site, errorMsg.c_str());
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
									reportError(site, errorMsg.c_str());
								} else {
									StackValueType actualType = fieldIt->second;
									if (actualType != expectedType && expectedType != StackValueType::UNKNOWN &&
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
											reportError(site, errorMsg.c_str());
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

	// A call of a function, by bare or qualified name: its arguments against its
	// parameters, then its effect on the stack
	template <typename Call>
	void SemanticValidator::typeCheckFunctionCall(Call* call, const FunctionSignature& sig, const std::string& name,
			IAstNode* node, size_t i, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack) {
		checkFallibleCall(call, sig, false, name, node, i);

		if (typeStack.size() < sig.consumes.size()) {
			std::string errorMsg = "Type error in function call '" + name + "': Stack underflow (requires " +
								   std::to_string(sig.consumes.size()) + " values, have " +
								   std::to_string(typeStack.size()) + ")";
			reportError(call, errorMsg.c_str());
			return;
		}

		std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);
		for (size_t j = 0; j < sig.consumes.size(); j++) {
			const size_t stackIdx = typeStack.size() - sig.consumes.size() + j;
			paramCasts[j] = argumentCast(call, "function call", name, j + 1, sig.consumes[j], typeStack[stackIdx]);
		}
		call->setParameterCasts(paramCasts);

		checkParameterFields(call, sig, name, typeStack, structTypeStack);

		// Struct, array and fn-pointer arguments, binding generic type parameters
		std::map<std::string, std::string> typeBindings;
		bindCallTypeParams(sig, typeStack, structTypeStack, call, name, typeBindings);

		const bool consumesPtrParam =
				std::find(sig.consumes.begin(), sig.consumes.end(), StackValueType::PTR) != sig.consumes.end();
		applyCallEffect(call, sig, typeBindings, topStructTypes(structTypeStack, sig.consumes.size()), typeStack,
				structTypeStack);

		// A pending function signature belongs to a function pointer argument, so it goes
		// with one: kept, it would be taken for the signature of a returned function pointer.
		// A function that returns a closure without taking one keeps it.
		if (consumesPtrParam) {
			mPendingFnSignature.reset();
		}
	}

	template <typename Call>
	bool SemanticValidator::typeCheckMethodCall(Call* call, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		const std::string& name = call->name();

		// Any struct on the stack with a method of this name gives the signature, and with it
		// where the receiver has to be
		std::string registeredStructType;
		const size_t searchLimit = std::min(structTypeStack.size(), typeStack.size());
		for (size_t idx = searchLimit; idx > 0 && registeredStructType.empty(); idx--) {
			if (!structTypeStack[idx - 1].empty()) {
				registeredStructType = findMethodStructType(structTypeStack[idx - 1], name);
			}
		}
		if (registeredStructType.empty()) {
			return false;
		}
		auto methodSigIt = mFunctionSignatures.find(registeredStructType + "::" + name);
		if (methodSigIt == mFunctionSignatures.end()) {
			return false;
		}
		const FunctionSignature& sig = methodSigIt->second;

		checkFallibleCall(call, sig, true, name, node, i);

		// The receiver goes first, its arguments above it: `v idx elem set!`
		const size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;
		if (typeStack.size() <= additionalParams) {
			std::string errorMsg = "Type error in method call '";
			errorMsg += name;
			errorMsg += "': Stack underflow (requires receiver + ";
			errorMsg += std::to_string(additionalParams);
			errorMsg += " values)";
			reportError(call, errorMsg.c_str());
			return true;
		}
		const size_t receiverStackIdx = typeStack.size() - 1 - additionalParams;
		std::string receiverStructType;
		if (receiverStackIdx < structTypeStack.size()) {
			std::string actualRegisteredType = findMethodStructType(structTypeStack[receiverStackIdx], name);
			if (!actualRegisteredType.empty()) {
				receiverStructType = structTypeStack[receiverStackIdx];
				registeredStructType = actualRegisteredType;
			}
		}
		if (receiverStructType.empty()) {
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
			reportError(call, errorMsg.c_str());
			return true;
		}

		// Arguments, the receiver at index 0 skipped
		std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);
		for (size_t j = 1; j < sig.consumes.size() && receiverStackIdx + j < typeStack.size(); j++) {
			paramCasts[j] =
					argumentCast(call, "method call", name, j, sig.consumes[j], typeStack[receiverStackIdx + j]);
		}
		call->setParameterCasts(paramCasts);

		// The receiver's type arguments bind the method's type parameters
		applyCallEffect(call, sig, receiverTypeBindings(receiverStructType),
				topStructTypes(structTypeStack, sig.consumes.size()), typeStack, structTypeStack);

		// For code generation: the generic type registered the method, which names its function
		call->setIsMethodCall(true);
		call->setReceiverType(registeredStructType);
		call->setMethodInputParamCount(additionalParams);
		call->setMethodReceiverPositionFromTop(additionalParams);
		if constexpr (std::is_same_v<Call, AstNodeInstruction>) {
			// An identifier's callee is looked up by name; an instruction's is resolved here
			call->setCalleeFallible(sig.throws);
		}
		return true;
	}

	void SemanticValidator::typeCheckInstructionNode(IAstNode* child, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
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
			return;
		}

		// A method named like a builtin, such as `len`
		if (typeCheckMethodCall(instr, node, i, typeStack, structTypeStack)) {
			return;
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
	}

	void SemanticValidator::typeCheckIf(IAstNode* child, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		// Check that stack has a condition value
		if (typeStack.empty()) {
			reportErrorWithHint(child, "Type error in 'if': Stack underflow (requires 1 condition value)",
					"push a condition before 'if', e.g. 'x 0 > if { ... }'");
			return;
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
				if (!blockEndsDiverging(thenBody) && thenStack.size() != typeStack.size() && !mHasUnpredictableStack) {
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
								  ((delta == 1 || delta == -1) ? "" : "s") + (delta > 0 ? " deeper" : " shallower") +
								  " than it found it; the arm may not run, so what follows cannot "
								  "read a value whose presence depends on it";
				reportErrorWithHint(child, msg.c_str(),
						"consume what the arm produces inside it, or add an 'else' arm that leaves "
						"the same depth");
			}
			// The pre-`if` stack stands: the arm is neutral, or the error above was reported.
		}
	}

	void SemanticValidator::typeCheckDefer(IAstNode* child, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
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
	}

	void SemanticValidator::typeCheckSwitch(IAstNode* child, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		// Check that stack has a value to switch on
		if (typeStack.empty()) {
			reportErrorWithHint(child, "Type error in 'switch': Stack underflow (requires 1 value to match)",
					"push a value before 'switch', e.g. 'x switch { case 1 { ... } }'");
			return;
		}
		// Pop the switch value
		const StackValueType scrutineeType = typeStack.back();
		typeStack.pop_back();
		if (!structTypeStack.empty()) {
			structTypeStack.pop_back();
		}

		// Analyze all case branches to track stack effects
		AstNodeSwitchStatement* switchStmt = static_cast<AstNodeSwitchStatement*>(child);
		const auto& cases = switchStmt->cases();

		// A d128 matches only d128 labels, and a d128 label matches only a d128. The
		// other way round the arm could never be taken -- 1.5 and 1.5d are different
		// numbers -- and the program would say nothing about it.
		for (const auto* caseNode : cases) {
			if (caseNode->isDefault() || caseNode->value() == nullptr) {
				continue;
			}
			StackValueType labelType = StackValueType::UNKNOWN;
			IAstNode* label = caseNode->value();
			if (label->type() == IAstNode::Type::LITERAL) {
				labelType = getLiteralStackType(static_cast<AstNodeLiteral*>(label)->literalType());
			} else if (label->type() == IAstNode::Type::IDENTIFIER) {
				auto constIt = mConstantValues.find(static_cast<AstNodeIdentifier*>(label)->name());
				if (constIt != mConstantValues.end()) {
					labelType = getConstantType(constIt->second);
				}
			}
			const bool concreteScrutinee =
					scrutineeType == StackValueType::INT || scrutineeType == StackValueType::FLOAT ||
					scrutineeType == StackValueType::D128 || scrutineeType == StackValueType::STRING;
			if (labelType == StackValueType::UNKNOWN || !concreteScrutinee ||
					(labelType == StackValueType::D128) == (scrutineeType == StackValueType::D128)) {
				continue;
			}
			std::string msg = "Type error in 'switch': the case label is ";
			msg += typeToString(labelType);
			msg += " but the value switched on is ";
			msg += typeToString(scrutineeType);
			reportErrorWithHint(label, msg.c_str(),
					scrutineeType == StackValueType::D128 ? "write the label as a d128: '1.5d'"
														  : "a d128 label matches only a d128 value");
		}

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
			if (!caseNode->isDefault() && caseNode->value() && caseNode->value()->type() == IAstNode::Type::LITERAL) {
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
			return;
		}

		// The same rule as for `if`/`else`: every arm that reaches the merge point must leave
		// the stack at the same depth. Without a `_` arm the switch may match nothing, so
		// the arms must additionally leave it at the depth it had before the switch.
		if (!mHasUnpredictableStack && !allDiverge) {
			if (armsDisagree) {
				std::string msg = "switch arms leave different numbers of values on the stack (" +
								  std::to_string(referenceEffect) + " and " + std::to_string(disagreeingEffect) +
								  "); every arm must leave the same number, since whatever follows reads a "
								  "value whose identity would otherwise depend on which arm ran";
				reportError(child, msg.c_str());
			} else if (!hasDefault && haveReference && referenceEffect != 0) {
				std::string msg = "switch without a '_' arm changes the stack by " + std::to_string(referenceEffect) +
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
	}

	void SemanticValidator::typeCheckFor(IAstNode* child, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
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
	}

	void SemanticValidator::typeCheckWhile(IAstNode* child, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
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
	}

	void SemanticValidator::typeCheckLocal(IAstNode* child, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
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
					std::cerr << "[DEBUG TC] Store -> " << varName << " with struct type: " << structType << std::endl;
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
	}

	// What storing a value of type `actual` in a field declared `expected` converts: a
	// field holds its declared type's representation, whatever was pushed
	static CastDirection fieldCast(StackValueType actual, StackValueType expected) {
		if (actual == StackValueType::ANY) {
			switch (expected) {
			case StackValueType::INT:
				return CastDirection::ANY_TO_INT;
			case StackValueType::FLOAT:
				return CastDirection::ANY_TO_FLOAT;
			case StackValueType::D128:
				return CastDirection::ANY_TO_D128;
			case StackValueType::STRING:
				return CastDirection::ANY_TO_STR;
			case StackValueType::PTR:
				return CastDirection::ANY_TO_PTR;
			default:
				return CastDirection::NONE;
			}
		}
		if (actual == StackValueType::INT && expected == StackValueType::FLOAT) {
			return CastDirection::INT_TO_FLOAT;
		}
		if (actual == StackValueType::FLOAT && expected == StackValueType::INT) {
			return CastDirection::FLOAT_TO_INT;
		}
		if (actual == StackValueType::INT && expected == StackValueType::D128) {
			return CastDirection::INT_TO_D128;
		}
		return CastDirection::NONE;
	}

	namespace {
		// One field initializer's nodes as a block, so that they are checked as any other
		// code is. It owns nothing; the construction node does.
		class FieldValueBlock : public IAstNode {
		public:
			FieldValueBlock(IAstNode* construct, const StructFieldInit& init) : mConstruct(construct) {
				for (const auto& node : init.valueNodes) {
					mNodes.push_back(node.get());
				}
			}

			Type type() const override {
				return Type::BLOCK;
			}

			size_t childCount() const override {
				return mNodes.size();
			}

			IAstNode* child(size_t index) const override {
				return index < mNodes.size() ? mNodes[index] : nullptr;
			}

			IAstNode* parent() const override {
				return mConstruct;
			}

			void setParent(IAstNode*) override {
			}

			size_t line() const override {
				return mConstruct->line();
			}

			size_t column() const override {
				return mConstruct->column();
			}

			void setPosition(size_t, size_t) override {
			}

		private:
			IAstNode* mConstruct;
			std::vector<IAstNode*> mNodes;
		};
	}

	void SemanticValidator::typeCheckStructConstruction(IAstNode* child, std::vector<StackValueType>& typeStack,
			std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		// `Name { field = expr ... }`
		AstNodeStructConstruction* construct = static_cast<AstNodeStructConstruction*>(child);
		const std::string& name = construct->structName();

		const std::string lookupKey = constructedStructKey(name);
		if (lookupKey.empty()) {
			// An undefined struct, reported where references are resolved. Its fields are not
			// checked, but the functions written in them are.
			checkAnonymousFunctionsWithin(construct, localVariables);
		} else {
			std::unordered_set<std::string> providedFields;
			for (size_t index = 0; index < construct->fieldInits().size(); index++) {
				providedFields.insert(construct->fieldInits()[index].fieldName);
				typeCheckFieldInit(construct, lookupKey, index, localVariables);
			}
			checkMissingFields(construct, lookupKey, providedFields);
		}

		// An instantiated generic keeps its arguments (`Box<i64>`) so that its fields resolve
		// to concrete types later
		std::string typeArgSuffix;
		if (!construct->typeArgs().empty()) {
			typeArgSuffix = "<";
			for (size_t ta = 0; ta < construct->typeArgs().size(); ta++) {
				typeArgSuffix += (ta > 0 ? ", " : "") + construct->typeArgs()[ta];
			}
			typeArgSuffix += ">";
		}
		typeStack.push_back(StackValueType::PTR);
		// The qualified name, which methods resolve by
		structTypeStack.push_back((lookupKey.empty() ? qualifiedModuleStructName(name) : lookupKey) + typeArgSuffix);
	}

	// The key the tables hold a constructed struct under: its own name for a struct of this
	// file, the qualified one for a module's. Empty when no struct has the name.
	std::string SemanticValidator::constructedStructKey(const std::string& name) const {
		if (mStructDeclarations.count(name) || mModuleStructDeclarations.count(name)) {
			return name;
		}
		if (name.find("::") != std::string::npos) {
			return "";
		}
		for (const auto& [moduleName, structs] : mModuleStructs) {
			if (structs.count(name) && mModuleStructDeclarations.count(moduleName + "::" + name)) {
				return moduleName + "::" + name;
			}
		}
		return "";
	}

	// A struct a module defines, by its qualified name; any other name as it is
	std::string SemanticValidator::qualifiedModuleStructName(const std::string& name) const {
		if (name.find("::") == std::string::npos) {
			for (const auto& [moduleName, structs] : mModuleStructs) {
				auto it = structs.find(name);
				if (it != structs.end() && it->second) {
					return moduleName + "::" + name;
				}
			}
		}
		return name;
	}

	// One `field = expr`: the field exists, and its expression, checked on a stack of its
	// own, leaves one value the field can hold
	void SemanticValidator::typeCheckFieldInit(AstNodeStructConstruction* construct, const std::string& lookupKey,
			size_t index, const std::unordered_map<std::string, StackValueType>& localVariables) {
		const StructFieldInit& fieldInit = construct->fieldInits()[index];
		const std::string& name = construct->structName();
		const std::string& fieldName = fieldInit.fieldName;

		StackValueType expectedType = StackValueType::UNKNOWN;
		const bool fieldExists = fieldTypeOrReport(construct, lookupKey, fieldName, expectedType);
		std::vector<StackValueType> valueTypes;
		std::vector<std::string> valueStructTypes;
		const bool countable = typeCheckFieldValue(construct, fieldInit, localVariables, valueTypes, valueStructTypes);
		if (!fieldExists) {
			return;
		}
		if (valueTypes.size() != 1) {
			// The values go onto the stack the struct is built from, so one too many is taken
			// for another field, and one too few takes what was already there
			if (countable) {
				std::string errorMsg = "Field '" + fieldName + "' in struct construction '" + name +
									   "' must be given one value, but its expression leaves " +
									   std::to_string(valueTypes.size());
				reportError(construct, errorMsg.c_str());
			}
			return;
		}
		construct->setFieldCast(
				index, checkFieldValue(construct, name, lookupKey, fieldName, expectedType, valueTypes.back(),
							   valueStructTypes.empty() ? "" : valueStructTypes.back()));
	}

	// The declared type of a field the struct under `structKey` has. Reports a field it
	// lacks, which is false.
	bool SemanticValidator::fieldTypeOrReport(const AstNodeStructConstruction* construct, const std::string& structKey,
			const std::string& fieldName, StackValueType& fieldType) {
		// The field tables rather than the declaration, which for a module's struct may be gone
		const auto* fieldTypes = lookupStructFieldTypes(structKey);
		if (fieldTypes != nullptr) {
			if (auto it = fieldTypes->find(fieldName); it != fieldTypes->end()) {
				fieldType = it->second;
				return true;
			}
		}
		std::string errorMsg = "Unknown field '" + fieldName + "' in struct '" + construct->structName() + "'";
		if (fieldTypes != nullptr) {
			std::string suggestion = findSimilarNameInMap(fieldName, *fieldTypes);
			if (!suggestion.empty()) {
				errorMsg += "; did you mean '" + suggestion + "'?";
			}
		}
		reportError(construct, errorMsg.c_str());
		return false;
	}

	// A field's expression, checked as code on an empty stack: what it binds or leaves
	// pending belongs to it. False when what it leaves cannot be counted.
	bool SemanticValidator::typeCheckFieldValue(AstNodeStructConstruction* construct, const StructFieldInit& fieldInit,
			const std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<StackValueType>& valueTypes, std::vector<std::string>& valueStructTypes) {
		FieldValueBlock value(construct, fieldInit);
		std::unordered_map<std::string, StackValueType> scope = localVariables;
		const auto savedPendingSig = mPendingFnSignature;
		const bool savedUnpredictable = mHasUnpredictableStack;
		const size_t errorsBefore = mErrorCount;
		mHasUnpredictableStack = false;
		typeCheckBlock(&value, valueTypes, scope, valueStructTypes);
		const bool countable = !mHasUnpredictableStack && mErrorCount == errorsBefore;
		mHasUnpredictableStack = mHasUnpredictableStack || savedUnpredictable;
		mPendingFnSignature = savedPendingSig;
		return countable;
	}

	// The struct or type parameter a field of the struct under `structKey` declares, empty
	// for a field of a value type
	std::string SemanticValidator::fieldStructType(const std::string& structKey, const std::string& fieldName) const {
		if (auto it = mStructFieldStructTypes.find(structKey); it != mStructFieldStructTypes.end()) {
			if (auto fieldIt = it->second.find(fieldName); fieldIt != it->second.end()) {
				return fieldIt->second;
			}
		}
		return "";
	}

	// A value stored into a field of a struct being built, by name or by position: an error
	// when the field cannot hold it, and the cast the store needs
	CastDirection SemanticValidator::checkFieldValue(const IAstNode* site, const std::string& writtenName,
			const std::string& structKey, const std::string& fieldName, StackValueType expectedType,
			StackValueType actualType, const std::string& actualStructType) {
		const std::string expectedStructType = fieldStructType(structKey, fieldName);
		// A field of a type parameter takes whatever it is given
		if ((!expectedStructType.empty() && isCurrentTypeParam(expectedStructType)) ||
				actualType == StackValueType::UNKNOWN || expectedType == StackValueType::UNKNOWN) {
			return CastDirection::NONE;
		}
		const CastDirection cast = fieldCast(actualType, expectedType);
		// An i64 widens to a d128 exactly, and an `any` is checked when it is stored, as
		// for an argument
		if ((actualType == StackValueType::INT && expectedType == StackValueType::D128) ||
				actualType == StackValueType::ANY) {
			return cast;
		}
		const std::string prefix = "struct construction '" + writtenName + "': Field '" + fieldName + "' ";
		if (actualType != expectedType) {
			if (!isImplicitCastAllowed(actualType, expectedType)) {
				std::string errorMsg =
						"Type error in " + prefix + "expects " +
						(expectedStructType.empty() ? stackValueTypeToString(expectedType) : expectedStructType) +
						", but got " +
						(actualStructType.empty() ? stackValueTypeToString(actualType) : actualStructType);
				reportError(site, errorMsg.c_str());
			} else if (shouldWarnImplicitCast(actualType, expectedType)) {
				std::string warnMsg = "Implicit cast in " + prefix + "expects " + stackValueTypeToString(expectedType) +
									  ", but got " + stackValueTypeToString(actualType);
				reportWarning(site, warnMsg.c_str());
			}
		} else if (std::string why;
				fieldValueRejected(structKey, fieldName, expectedStructType, actualStructType, why)) {
			// Both are the coarse type the field declared, so what separates them is the
			// pointer's real type. Structural rather than by string: `fn( -- f64)` and the
			// `fn(-- f64)` buildFnTypeString emits are the same type, and so are `Point` and
			// `mod::Point`.
			std::string errorMsg = "Type error in " + prefix + why;
			reportError(site, errorMsg.c_str());
		}
		return cast;
	}

	// Every field without a default has to be given
	void SemanticValidator::checkMissingFields(AstNodeStructConstruction* construct, const std::string& lookupKey,
			const std::unordered_set<std::string>& providedFields) {
		const auto* fieldTypes = lookupStructFieldTypes(lookupKey);
		if (fieldTypes == nullptr) {
			return;
		}
		auto defaultsIt = mStructFieldsWithDefaults.find(lookupKey);
		for (const auto& fieldEntry : *fieldTypes) {
			const bool hasDefault =
					defaultsIt != mStructFieldsWithDefaults.end() && defaultsIt->second.count(fieldEntry.first);
			if (!providedFields.count(fieldEntry.first) && !hasDefault) {
				std::string errorMsg = "Missing field '" + fieldEntry.first + "' in struct construction '" +
									   construct->structName() + "'";
				reportError(construct, errorMsg.c_str());
			}
		}
	}

	std::vector<std::string> SemanticValidator::fieldNames(const AstNodeStructDeclaration* structDecl) {
		std::vector<std::string> names;
		for (const auto& field : structDecl->fields()) {
			names.push_back(field->name());
		}
		return names;
	}

	std::vector<CastDirection> SemanticValidator::typeCheckPositionalConstruction(const IAstNode* site,
			const std::string& writtenName, const std::string& structKey, const std::vector<std::string>* fieldOrder,
			const std::string& pushedType, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack) {
		std::vector<CastDirection> casts;
		if (fieldOrder != nullptr) {
			const size_t fieldCount = fieldOrder->size();
			if (typeStack.size() < fieldCount) {
				std::string errorMsg = "Type error in struct construction '" + writtenName +
									   "': Stack underflow (requires " + std::to_string(fieldCount) + " values, have " +
									   std::to_string(typeStack.size()) + ")";
				reportError(site, errorMsg.c_str());
			} else if (const auto* fieldTypes = lookupStructFieldTypes(structKey)) {
				// Fields are in declaration order, consumed bottom to top
				casts.assign(fieldCount, CastDirection::NONE);
				for (size_t fi = 0; fi < fieldCount; fi++) {
					const size_t stackIdx = typeStack.size() - fieldCount + fi;
					const std::string& fieldName = (*fieldOrder)[fi];
					auto fieldTypeIt = fieldTypes->find(fieldName);
					if (fieldTypeIt == fieldTypes->end()) {
						continue;
					}
					casts[fi] = checkFieldValue(site, writtenName, structKey, fieldName, fieldTypeIt->second,
							typeStack[stackIdx], stackIdx < structTypeStack.size() ? structTypeStack[stackIdx] : "");
				}
			}
			for (size_t fi = 0; fi < fieldCount && !typeStack.empty(); fi++) {
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
			}
		}
		typeStack.push_back(StackValueType::PTR);
		structTypeStack.push_back(pushedType);
		return casts;
	}

	void SemanticValidator::typeCheckIdentifier(IAstNode* child, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::unordered_map<std::string, StackValueType>& localVariables,
			std::vector<std::string>& structTypeStack) {
		AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(child);
		const std::string& name = ident->name();

		if (auto localIt = localVariables.find(name); localIt != localVariables.end()) {
			pushLocalVariable(name, localIt->second, typeStack, structTypeStack);
			return;
		}
		if (auto constIt = mConstantValues.find(name); constIt != mConstantValues.end()) {
			typeStack.push_back(getConstantType(constIt->second));
			structTypeStack.push_back("");
			return;
		}
		// A module-level `var`, carrying its struct type so that a following `<<field` resolves
		if (auto globalIt = mGlobalVarTypes.find(name); globalIt != mGlobalVarTypes.end()) {
			typeStack.push_back(stringToStackValueType(globalIt->second));
			structTypeStack.push_back(isStructTypeName(globalIt->second) ? globalIt->second : "");
			return;
		}
		// Positional construction, `x y Point`
		if (const std::string structKey = positionalStructKey(name); !structKey.empty()) {
			const std::optional<std::vector<std::string>> fieldOrder = declaredFieldOrder(structKey);
			ident->setParameterCasts(typeCheckPositionalConstruction(ident, name, structKey,
					fieldOrder ? &*fieldOrder : nullptr, structKey, typeStack, structTypeStack));
			return;
		}
		if (typeCheckMethodCall(ident, node, i, typeStack, structTypeStack)) {
			return;
		}
		if (auto sigIt = mFunctionSignatures.find(name); sigIt != mFunctionSignatures.end()) {
			typeCheckFunctionCall(ident, sigIt->second, name, node, i, typeStack, structTypeStack);
		} else {
			// Not a local, not a method, not a user function: unresolved, so its stack
			// effect is unknown and the function-level checks cannot be trusted.
			// This must stay inside the else: placed after the block it ran for every
			// call, which switched off the arity, if-arm and defer checks in any
			// function that called anything.
			mHasUnpredictableStack = true;
		}
	}

	// A local's type, and for a pointer the struct it holds and the signature of the
	// function it may hold, which a following `call` checks against
	void SemanticValidator::pushLocalVariable(const std::string& name, StackValueType type,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		typeStack.push_back(type);
		if (type != StackValueType::PTR) {
			structTypeStack.push_back("");
			return;
		}
		auto structTypeIt = mLocalVariableStructTypes.find(name);
		structTypeStack.push_back(structTypeIt != mLocalVariableStructTypes.end() ? structTypeIt->second : "");
		if (auto fnSigIt = mLocalVariableFnSignatures.find(name); fnSigIt != mLocalVariableFnSignatures.end()) {
			mPendingFnSignature = fnSigIt->second;
		}
	}

	// The struct `x y Name` builds: one of this file, or an imported module's by its bare
	// name, under its qualified one. Empty when no struct has the name.
	std::string SemanticValidator::positionalStructKey(const std::string& name) const {
		if (mDefinedStructs.count(name)) {
			return name;
		}
		// Once a `break` that sent the identifier on to the call checks, which found no
		// function and took it for an unresolved one
		for (const auto& [moduleName, structs] : mModuleStructs) {
			if (auto structIt = structs.find(name); structIt != structs.end() && structIt->second) {
				return moduleName + "::" + name;
			}
		}
		return "";
	}

	// A struct's fields in the order its declaration gives them; none when no declaration
	// is at hand
	std::optional<std::vector<std::string>> SemanticValidator::declaredFieldOrder(const std::string& structKey) const {
		if (auto it = mStructDeclarations.find(structKey); it != mStructDeclarations.end()) {
			return fieldNames(it->second);
		}
		if (auto it = mModuleStructDeclarations.find(structKey); it != mModuleStructDeclarations.end()) {
			return fieldNames(it->second);
		}
		return std::nullopt;
	}

	void SemanticValidator::typeCheckFieldAccess(
			IAstNode* child, std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		// `struct <<field`: pop the struct, push the field. The struct's type is what its
		// producer left on the struct-type stack -- a local, a call, a construction, `as`,
		// or a previous `<<` of a struct-typed field -- so nothing here needs to know what
		// the operand *was*, only what it is.
		AstNodeFieldAccess* fieldAccess = static_cast<AstNodeFieldAccess*>(child);
		const std::string& fieldName = fieldAccess->fieldName();

		if (typeStack.empty()) {
			std::string msg = "Type error in field access '<<" + fieldName + "': Stack underflow (requires a struct)";
			reportErrorWithHint(fieldAccess, msg.c_str(), "push the struct first, e.g. 'p <<x'");
			typeStack.push_back(StackValueType::UNKNOWN);
			structTypeStack.push_back("");
			return;
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
				operandType == StackValueType::D128 || operandType == StackValueType::STRING) {
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
				std::string msg = "Type error in field access '<<" + fieldName + "': no struct has a field named '" +
								  fieldName + "'";
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
	}

	void SemanticValidator::typeCheckFieldSet(
			IAstNode* child, std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		// `struct value >>field`: pop the value, check the field against the struct beneath
		// it, and leave the struct on the stack for chaining.
		AstNodeFieldSet* fieldSet = static_cast<AstNodeFieldSet*>(child);
		const std::string& fieldName = fieldSet->fieldName();

		if (typeStack.size() < 2) {
			std::string msg =
					"Type error in field set '>>" + fieldName + "': Stack underflow (requires struct and value)";
			reportErrorWithHint(fieldSet, msg.c_str(), "push the struct, then the value, e.g. 'p 42 >>x'");
			if (!typeStack.empty()) {
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
			}
			return;
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
				if (!genericField) {
					fieldSet->setCast(fieldCast(valueType, expected));
				}
				bool checkable = !genericField && expected != StackValueType::ANY &&
								 expected != StackValueType::UNKNOWN && expected != StackValueType::TYPEVAR &&
								 valueType != StackValueType::ANY && valueType != StackValueType::UNKNOWN &&
								 valueType != StackValueType::TYPEVAR;
				// An i64 widens to a d128 exactly, as an argument does
				const bool widens = valueType == StackValueType::INT && expected == StackValueType::D128;
				if (checkable && valueType != expected && !widens && !isImplicitCastAllowed(valueType, expected)) {
					std::string msg = "Type error in field set '>>" + fieldName + "': field '" + fieldName +
									  "' of struct '" + structType + "' is " + stackValueTypeToString(expected) +
									  ", but the value is " + stackValueTypeToString(valueType);
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
				std::string msg = "Type error in field set '>>" + fieldName + "': no struct has a field named '" +
								  fieldName + "'";
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
	}

	void SemanticValidator::typeCheckScopedIdentifier(IAstNode* child, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		AstNodeScopedIdentifier* scoped = static_cast<AstNodeScopedIdentifier*>(child);

		// Resolve sb::append_any at type-check time based on the type stack
		// This is generated by $"..." string interpolation
		// Use mResolvedInterpolations to only resolve each node once
		if (scoped->scope() == "sb" && scoped->name() == "append_any" &&
				mResolvedInterpolations.find(scoped) == mResolvedInterpolations.end()) {
			mResolvedInterpolations.insert(scoped);
			// An integer has a path of its own that builds the digits in place. Anything
			// else goes through cast<str>: sending an f64 to append_int died on the
			// first `mod` it reached.
			if (!typeStack.empty() && typeStack.back() == StackValueType::STRING) {
				scoped->setName("append");
			} else if (!typeStack.empty() && typeStack.back() == StackValueType::INT) {
				scoped->setName("append_int");
			} else {
				scoped->setName("append_value");
			}
		}

		const std::string& moduleName = scoped->scope();
		const std::string& functionName = scoped->name();
		const std::string qualifiedName = moduleName + "::" + functionName;

		// Imported C functions and variadic stdlib functions may have signatures
		// that don't reflect all consumed values. Mark stack as unpredictable.
		if (mImportedLibraryFunctions.find(qualifiedName) != mImportedLibraryFunctions.end() ||
				qualifiedName == "fmt::printf" || qualifiedName == "fmt::sprintf" || qualifiedName == "fmt::fprintf") {
			mHasUnpredictableStack = true;
		}

		if (typeCheckScopedConstant(moduleName, functionName, typeStack, structTypeStack)) {
			return;
		}

		// Positional construction of a module's struct, `x y mod::Point`
		auto moduleStructsIt = mModuleStructs.find(moduleName);
		if (moduleStructsIt != mModuleStructs.end()) {
			auto structIt = moduleStructsIt->second.find(functionName);
			if (structIt != moduleStructsIt->second.end() && structIt->second) {
				// Its field order from the tables, since the AST node may be invalid
				auto structFieldOrderIt = mStructFieldOrder.find(qualifiedName);
				scoped->setParameterCasts(typeCheckPositionalConstruction(scoped, qualifiedName, qualifiedName,
						structFieldOrderIt != mStructFieldOrder.end() ? &structFieldOrderIt->second : nullptr,
						functionName, typeStack, structTypeStack));
				return;
			}
		}

		if (typeCheckExplicitMethodCall(scoped, node, i, typeStack, structTypeStack) ||
				typeCheckModuleMethodCall(scoped, node, i, typeStack, structTypeStack)) {
			return;
		}

		auto sigIt = mFunctionSignatures.find(qualifiedName);
		if (sigIt != mFunctionSignatures.end()) {
			typeCheckFunctionCall(scoped, sigIt->second, qualifiedName, node, i, typeStack, structTypeStack);
		} else if (const std::string owner = moduleMethodOwner(moduleName, functionName); !owner.empty()) {
			// A method of one of the module's structs, with no receiver where it would want one.
			// Let through, it was compiled as a guess at which method was meant, and the guess
			// rolled a receiver out of whatever was on the stack.
			auto methodSigIt = mFunctionSignatures.find(owner + "::" + functionName);
			const size_t params = methodSigIt != mFunctionSignatures.end() && !methodSigIt->second.consumes.empty()
										  ? methodSigIt->second.consumes.size() - 1
										  : 0;
			std::string errorMsg = "Method call '" + qualifiedName + "': a '" + owner +
								   "' must be pushed first, then " + std::to_string(params) +
								   " parameter(s) on top (e.g., 'receiver";
			for (size_t p = 0; p < params; p++) {
				errorMsg += " param";
			}
			errorMsg += " " + qualifiedName + "')";
			reportError(scoped, errorMsg.c_str());
			mHasUnpredictableStack = true;
		} else {
			// Signature not found: the module was not loaded or analysed, so the stack
			// effect is unknown. Inside the else for the same reason as the IDENTIFIER case.
			mHasUnpredictableStack = true;
		}
	}

	// The struct of `module` that has a method `name`, by its qualified name; empty if none does
	std::string SemanticValidator::moduleMethodOwner(const std::string& module, const std::string& name) const {
		const std::string prefix = module + "::";
		for (const auto& [structType, methods] : mStructMethods) {
			if (structType.compare(0, prefix.size(), prefix) == 0 && methods.count(name)) {
				return structType;
			}
		}
		return "";
	}

	// `scope::name` as a constant or an enum variant: false when it is neither
	bool SemanticValidator::typeCheckScopedConstant(const std::string& scope, const std::string& name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		const std::string qualifiedName = scope + "::" + name;
		std::optional<StackValueType> type;

		if (auto it = mConstantValues.find(qualifiedName); it != mConstantValues.end()) {
			// An enum variant, or a constant of this file by its qualified name
			type = getConstantType(it->second);
		} else if (mImportedLibraries.count(scope) && mConstantValues.count(name)) {
			// Self-qualified module constant. `time.qd` declares
			// `import "libtime.a" as "time"` and reads its own `pub const Millisecond` as
			// `time::Millisecond`; the constant is registered under the bare name. Guarded
			// on the scope being an FFI namespace so this cannot swallow a genuinely
			// unresolved `othermodule::Name`.
			type = getConstantType(mConstantValues.at(name));
		} else if (auto moduleIt = mModuleConstants.find(scope);
				moduleIt != mModuleConstants.end() && moduleIt->second.count(name)) {
			auto valueIt = mModuleConstantValues.find(qualifiedName);
			type = valueIt != mModuleConstantValues.end() ? getConstantType(valueIt->second) : StackValueType::UNKNOWN;
		}

		if (!type) {
			return false;
		}
		typeStack.push_back(*type);
		structTypeStack.push_back("");
		return true;
	}

	// `Type::method`, the receiver on top of the stack and the arguments under it: false,
	// with nothing done, when the scope is no struct with that method
	bool SemanticValidator::typeCheckExplicitMethodCall(AstNodeScopedIdentifier* scoped, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		const std::string& typeName = scoped->scope();
		const std::string& methodName = scoped->name();
		auto methodsIt = mStructMethods.find(typeName);
		if (methodsIt == mStructMethods.end() || !methodsIt->second.count(methodName)) {
			return false;
		}
		const std::string qualifiedName = typeName + "::" + methodName;
		auto sigIt = mFunctionSignatures.find(qualifiedName);
		if (sigIt == mFunctionSignatures.end()) {
			return false;
		}
		const FunctionSignature& sig = sigIt->second;

		std::string receiverStructType;
		if (!typeStack.empty() && typeStack.back() == StackValueType::PTR && !structTypeStack.empty()) {
			receiverStructType = structTypeStack.back();
		}
		if (!receiverStructType.empty() && receiverStructType != typeName) {
			std::string errorMsg = "Type error: Explicit method call '" + qualifiedName +
								   "' expects receiver of type '" + typeName + "' but stack top is '" +
								   receiverStructType + "'";
			reportError(scoped, errorMsg.c_str());
		}

		checkFallibleCall(scoped, sig, true, methodName, node, i);

		const size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;
		if (typeStack.size() < 1 + additionalParams) {
			std::string errorMsg = "Type error in method call '" + methodName +
								   "': Stack underflow (requires receiver + " + std::to_string(additionalParams) +
								   " values)";
			reportError(scoped, errorMsg.c_str());
			return true;
		}

		// The stack holds [arg1 .. argN receiver] and the signature [receiver arg1 .. argN]
		std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);
		for (size_t j = 1; j < sig.consumes.size(); j++) {
			const size_t stackIdx = typeStack.size() - sig.consumes.size() + (j - 1);
			paramCasts[j - 1] =
					argumentCast(scoped, "method call", methodName, j, sig.consumes[j], typeStack[stackIdx]);
		}
		scoped->setParameterCasts(paramCasts);

		applyCallEffect(scoped, sig, receiverTypeBindings(receiverStructType), {}, typeStack, structTypeStack);

		scoped->setIsMethodCall(true);
		scoped->setMethodInputParamCount(additionalParams);
		scoped->setMethodReceiverPositionFromTop(0);
		return true;
	}

	// `module::method` on a struct of that module, the receiver under its arguments, as
	// `sb::append` on a StringBuilder: false, with nothing done, when no struct of the
	// module sits on the stack where a method of that name would want its receiver
	bool SemanticValidator::typeCheckModuleMethodCall(AstNodeScopedIdentifier* scoped, IAstNode* node, size_t i,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		const std::string& moduleName = scoped->scope();
		const std::string& methodName = scoped->name();
		for (size_t searchDepth = 0; searchDepth < typeStack.size(); searchDepth++) {
			const size_t stackIdx = typeStack.size() - 1 - searchDepth;
			if (typeStack[stackIdx] != StackValueType::PTR || stackIdx >= structTypeStack.size() ||
					structTypeStack[stackIdx].empty()) {
				continue;
			}
			const std::string& receiverStructType = structTypeStack[stackIdx];

			const bool isModuleStruct = receiverStructType.rfind(moduleName + "::", 0) == 0 ||
										mStructMethods.count(moduleName + "::" + receiverStructType) ||
										mStructMethods.count(receiverStructType);
			if (!isModuleStruct) {
				continue;
			}

			std::string registeredStructType = findMethodStructType(receiverStructType, methodName);
			if (registeredStructType.empty() && receiverStructType.find("::") == std::string::npos) {
				registeredStructType = findMethodStructType(moduleName + "::" + receiverStructType, methodName);
			}
			if (registeredStructType.empty()) {
				continue;
			}
			auto sigIt = mFunctionSignatures.find(registeredStructType + "::" + methodName);
			if (sigIt == mFunctionSignatures.end()) {
				continue;
			}
			const FunctionSignature& sig = sigIt->second;

			// The receiver goes first, its arguments above it
			const size_t additionalParams = sig.consumes.size() > 0 ? sig.consumes.size() - 1 : 0;
			if (searchDepth != additionalParams) {
				continue;
			}

			checkFallibleCall(scoped, sig, true, methodName, node, i);

			std::vector<CastDirection> paramCasts(sig.consumes.size(), CastDirection::NONE);
			for (size_t j = 1; j < sig.consumes.size(); j++) {
				paramCasts[j] =
						argumentCast(scoped, "method call", methodName, j, sig.consumes[j], typeStack[stackIdx + j]);
			}
			scoped->setParameterCasts(paramCasts);

			// Copied, since applying the call pops the slot it names
			const std::string receiver = receiverStructType;
			applyCallEffect(scoped, sig, receiverTypeBindings(receiver),
					topStructTypes(structTypeStack, sig.consumes.size()), typeStack, structTypeStack);

			scoped->setIsMethodCall(true);
			scoped->setReceiverType(registeredStructType);
			scoped->setMethodInputParamCount(additionalParams);
			scoped->setMethodReceiverPositionFromTop(additionalParams);
			return true;
		}
		return false;
	}

	// typeCheckInstruction and typeCheckInstructionInternal are in semantic_validator_instructions.cc

} // namespace Qd
