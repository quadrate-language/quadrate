// Instruction type checking for semantic validator
// Extracted from semantic_validator_typecheck.cc for maintainability

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/numeric_literal.h>
#include <quadrate/qc/semantic_validator.h>

namespace Qd {

	namespace {
		// The node that follows this one in its parent block, or nullptr. A fallible call's
		// consumer is its next sibling, which is what the rule about `if`/`switch` is about.
		IAstNode* nextSiblingOf(IAstNode* node) {
			if (!node) {
				return nullptr;
			}
			IAstNode* parent = node->parent();
			if (!parent) {
				return nullptr;
			}
			for (size_t i = 0; i + 1 < parent->childCount(); i++) {
				if (parent->child(i) == node) {
					return parent->child(i + 1);
				}
			}
			return nullptr;
		}
	}

#include "semantic_validator_internal.h"

	void SemanticValidator::typeCheckInstruction(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack) {
		typeCheckInstructionInternal(node, name, typeStack, structTypeStack, true);
	}

	void SemanticValidator::typeCheckInstructionInternal(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, bool reportErrors) {
		// Handle instruction aliases
		if (strcmp(name, "/") == 0) {
			name = "div";
		} else if (strcmp(name, "*") == 0) {
			name = "mul";
		} else if (strcmp(name, "+") == 0) {
			name = "add";
		} else if (strcmp(name, "-") == 0) {
			name = "sub";
		} else if (strcmp(name, "%") == 0) {
			name = "mod";
		} else if (strcmp(name, "==") == 0) {
			name = "eq";
		} else if (strcmp(name, "!=") == 0) {
			name = "neq";
		} else if (strcmp(name, "<") == 0) {
			name = "lt";
		} else if (strcmp(name, ">") == 0) {
			name = "gt";
		} else if (strcmp(name, "<=") == 0) {
			name = "lte";
		} else if (strcmp(name, ">=") == 0) {
			name = "gte";
		} else if (strcmp(name, "++") == 0) {
			name = "inc";
		} else if (strcmp(name, "--") == 0) {
			name = "dec";
		}

		// Freestanding mode: reject builtins that need libc / hosted I/O.
		// Pure stack/arithmetic/control-flow ops are still allowed.
		if (mFreestandingMode) {
			static const char* kUnsafeBuiltins[] = {
					"print", "prints", "printv", "nl", "panic", "err", "spawn", "wait", "detach", nullptr};
			for (size_t i = 0; kUnsafeBuiltins[i] != nullptr; i++) {
				if (strcmp(name, kUnsafeBuiltins[i]) == 0) {
					std::string err = "builtin '" + std::string(name) + "' is not available in --freestanding mode";
					reportErrorConditional(node, err.c_str(), reportErrors);
					return;
				}
			}
		}

		// panic instruction: ( msg code -- ) sets error flag and returns from function
		// Can only be called inside fallible functions (marked with !)
		if (strcmp(name, "panic") == 0) {
			if (!mCurrentFunctionFallible) {
				reportErrorConditional(
						node, "'panic' can only be used inside fallible functions (marked with !)", reportErrors);
				return;
			}

			// Requires (msg code) on stack
			if (typeStack.size() < 2) {
				reportErrorConditional(
						node, "Type error in 'panic': Stack underflow (requires msg and code)", reportErrors);
				return;
			}
			// Pop msg and code
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			return;
		}

		// err instruction: ( -- msg code ) retrieves error info from last failed fallible call
		if (strcmp(name, "err") == 0) {
			// Push msg (string) and code (int)
			typeStack.push_back(StackValueType::STRING);
			structTypeStack.push_back("");
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}

		// Increment/Decrement functions: inc, dec (preserve type)
		if (strcmp(name, "inc") == 0 || strcmp(name, "dec") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Type remains the same (already on stack)
			return;
		}
		// Binary arithmetic operations: add, sub, mul, div
		else if (strcmp(name, "add") == 0 || strcmp(name, "sub") == 0 || strcmp(name, "mul") == 0 ||
				 strcmp(name, "div") == 0) {
			checkBuiltinArithmetic(node, name, typeStack, structTypeStack, reportErrors);
			return;
		}
		// Comparison operations: eq, neq, lt, gt, lte, gte (consume 2, produce int/bool)
		else if (strcmp(name, "eq") == 0 || strcmp(name, "neq") == 0 || strcmp(name, "lt") == 0 ||
				 strcmp(name, "gt") == 0 || strcmp(name, "lte") == 0 || strcmp(name, "gte") == 0) {
			checkBuiltinComparison(node, name, typeStack, structTypeStack, reportErrors);
			return;
		}
		// Logical operations: and, or, and the bitwise forms behind the bits module
		// (consume 2 values, produce int)
		else if (strcmp(name, "and") == 0 || strcmp(name, "or") == 0 || strcmp(name, "__and") == 0 ||
				 strcmp(name, "__or") == 0 || strcmp(name, "__xor") == 0) {
			if (typeStack.size() < 2) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 2 values)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop both operands
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			// Push result (always int/bool)
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Negation: the logical 'not', and the bitwise form behind the bits module
		// (consume 1, produce int)
		else if (strcmp(name, "not") == 0 || strcmp(name, "__not") == 0) {
			if (typeStack.empty()) {
				std::string msg = std::string("Type error in '") + name + "': Stack underflow (requires 1 value)";
				reportErrorConditional(node, msg.c_str(), reportErrors);
				return;
			}
			// Pop operand, push result (type stays int)
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Bitwise shift operations: shl, shr (consume 2 ints, produce int)
		else if (strcmp(name, "shl") == 0 || strcmp(name, "shr") == 0) {
			if (typeStack.size() < 2) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 2 values)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop both operands
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			// Push result (always int)
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Negation: neg (preserve numeric type)
		else if (strcmp(name, "neg") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(
						node, "Type error in 'neg': Stack underflow (requires 1 numeric value)", reportErrors);
				return;
			}
			// Type stays the same
			return;
		}
		// Type casting: cast<T> (convert to type T)
		else if (strcmp(name, "cast") == 0) {
			checkBuiltinCast(node, typeStack, structTypeStack, reportErrors);
			return;
		}
		// sizeof<T>: the size of a type, known at compile time
		else if (strcmp(name, "sizeof") == 0) {
			if (node->type() == IAstNode::Type::INSTRUCTION &&
					!static_cast<AstNodeInstruction*>(node)->hasTypeParam()) {
				reportErrorConditional(node, "'sizeof' takes a type, not a value: write 'sizeof<i64>'", reportErrors);
				return;
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Raw memory, x86 port I/O and CPU control
		else if (checkBuiltinMachine(node, name, typeStack, structTypeStack, reportErrors)) {
			return;
		}
		// Modulo: mod (consume 2 ints, produce int)
		else if (strcmp(name, "mod") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(
						node, "Type error in 'mod': Stack underflow (requires 2 integer values)", reportErrors);
				return;
			}
			if (typeStack[typeStack.size() - 1] == StackValueType::D128 ||
					typeStack[typeStack.size() - 2] == StackValueType::D128) {
				reportErrorConditionalWithHint(node, "Type error in 'mod': mod is integer-only and does not take d128",
						"use 'decimal::fmod' for the truncating remainder, or 'decimal::remainder' for IEEE's",
						reportErrors);
			}
			// Pop both operands
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			// Push result (always int)
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Print operations: print, printv
		else if (strcmp(name, "print") == 0 || strcmp(name, "printv") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 value)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back(); // Pop the value
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			return;
		}
		// Non-destructive print: prints
		else if (strcmp(name, "prints") == 0 || strcmp(name, "nl") == 0) {
			// These don't modify the stack
			return;
		}
		// Stack shuffles: dup, dup2, swap, over, pick, nip, drop, rot
		else if (checkBuiltinShuffle(node, name, typeStack, structTypeStack, reportErrors)) {
			return;
		}
		// call - invoke function pointer from stack
		else if (strcmp(name, "call") == 0) {
			checkBuiltinCall(node, typeStack, structTypeStack, reportErrors);
			return;
		}
		// Threading: spawn ( fn:ptr -- handle:i64 )
		//
		// The handle qd_spawn pushes is an i64 (the OS thread handle cast to an integer), not the
		// ptr that `&worker` put on the stack. Leaving this unmodelled meant the function
		// pointer's PTR stayed on the type stack, so `threads i &worker spawn set` reported
		// "Array is '[]i64' but value is ptr" and no array element type could satisfy it.
		else if (strcmp(name, "spawn") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(
						node, "Type error in 'spawn': Stack underflow (requires 1 function pointer)", reportErrors);
				return;
			}
			if (typeStack.back() != StackValueType::PTR && typeStack.back() != StackValueType::UNKNOWN) {
				reportErrorConditional(
						node, "Type error in 'spawn': Expected a function pointer (use &function_name)", reportErrors);
				return;
			}
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			// The spawned function runs on its own fresh stack, so its signature has no bearing
			// on this one.
			mPendingFnSignature.reset();
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Threading: wait, detach ( handle:i64 -- )
		else if (strcmp(name, "wait") == 0 || strcmp(name, "detach") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 thread handle)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			if (typeStack.back() != StackValueType::INT && typeStack.back() != StackValueType::UNKNOWN) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected a thread handle from 'spawn'";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			return;
		}
		// Array length: len ( arr -- len )
		// Returns the length of an array, consuming the array reference
		else if (strcmp(name, "len") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'len': Stack underflow (requires 1 array)", reportErrors);
				return;
			}
			// Verify top is a pointer (array)
			StackValueType top = typeStack.back();
			if (top != StackValueType::PTR) {
				std::string errorMsg = "Type error in 'len': Expected array (ptr), got ";
				errorMsg += typeToString(top);
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop array, push int (length)
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Array access: nth ( arr idx -- elem )
		// Returns element at index, consuming the array reference
		else if (strcmp(name, "nth") == 0) {
			checkBuiltinNth(node, typeStack, structTypeStack, reportErrors);
			return;
		}
		// Array set: set ( arr idx val -- )
		// Sets element at index
		else if (strcmp(name, "set") == 0) {
			checkBuiltinSet(node, typeStack, structTypeStack, reportErrors);
			return;
		}
		// Array append: append ( arr val -- arr' )
		// Appends value to array and returns new array
		else if (strcmp(name, "append") == 0) {
			checkBuiltinAppend(node, typeStack, structTypeStack, reportErrors);
			return;
		}
		// clone - a new struct of the same type, fields copied
		else if (strcmp(name, "clone") == 0) {
			checkBuiltinClone(node, typeStack, structTypeStack, reportErrors);
			return;
		}
		// free - deallocate memory pointed to by a pointer
		else if (strcmp(name, "free") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(
						node, "Type error in 'free': Stack underflow (requires 1 pointer)", reportErrors);
				return;
			}
			StackValueType top = typeStack.back();
			if (top != StackValueType::PTR) {
				std::string errorMsg = "Type error in 'free': Expected pointer type, got ";
				errorMsg += typeToString(top);
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop the pointer
			typeStack.pop_back();
			// Pop struct type as well
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			return;
		}
		// within: (value min max -- result:int)
		else if (strcmp(name, "within") == 0) {
			if (typeStack.size() < 3) {
				reportErrorConditional(
						node, "Type error in 'within': Stack underflow (requires 3 values)", reportErrors);
				return;
			}
			typeStack.pop_back();
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 3) {
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}

		// Unhandled instruction — mark stack as unpredictable
		// so the function output validation is skipped
		mHasUnpredictableStack = true;
	}

	// Binary arithmetic operations: add, sub, mul, div
	void SemanticValidator::checkBuiltinArithmetic(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.size() < 2) {
			std::string errorMsg = "Type error in '";
			errorMsg += name;
			errorMsg += "': Stack underflow (requires 2 numeric values)";
			reportErrorConditional(node, errorMsg.c_str(), reportErrors);
			return;
		}

		StackValueType b = typeStack.back();
		typeStack.pop_back();
		StackValueType a = typeStack.back();
		typeStack.pop_back();
		// Keep structTypeStack in sync
		if (structTypeStack.size() >= 2) {
			structTypeStack.pop_back();
			structTypeStack.pop_back();
		}

		if (a == StackValueType::D128 || b == StackValueType::D128) {
			// d128 meets d128, and an i64, which widens to it exactly: every i64 has at most
			// 19 digits of the 34. An f64 does not mix. Converting one on the quiet would
			// bring in exactly the binary rounding a decimal type is for keeping out:
			// `0.1 0.1d +` would not be 0.2.
			const StackValueType other = (a == StackValueType::D128) ? b : a;
			const bool otherOpen = other == StackValueType::ANY || other == StackValueType::UNKNOWN ||
								   other == StackValueType::TYPEVAR;
			if (other != StackValueType::D128 && other != StackValueType::INT && !otherOpen) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': d128 does not mix with ";
				errorMsg += typeToString(other);
				reportErrorConditionalWithHint(node, errorMsg.c_str(),
						"convert the other operand with 'cast<d128>', or write the literal with a 'd': '2.5d'",
						reportErrors);
			}
			typeStack.push_back(StackValueType::D128);
			structTypeStack.push_back("");
			return;
		}

		if (!isNumericType(a) || !isNumericType(b)) {
			std::string errorMsg = "Type error in '";
			errorMsg += name;
			errorMsg += "': Expected numeric types, got ";
			errorMsg += typeToString(a);
			errorMsg += " and ";
			errorMsg += typeToString(b);
			reportErrorConditional(node, errorMsg.c_str(), reportErrors);
			return;
		}

		// Result is float if either operand is float, otherwise int
		StackValueType result = (a == StackValueType::FLOAT || b == StackValueType::FLOAT) ? StackValueType::FLOAT
																						   : StackValueType::INT;
		typeStack.push_back(result);
		structTypeStack.push_back(""); // Arithmetic result is never a struct
		return;
	}

	// Comparison operations: eq, neq, lt, gt, lte, gte (consume 2, produce int/bool)
	void SemanticValidator::checkBuiltinComparison(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.size() < 2) {
			std::string errorMsg = "Type error in '";
			errorMsg += name;
			errorMsg += "': Stack underflow (requires 2 values)";
			reportErrorConditional(node, errorMsg.c_str(), reportErrors);
			return;
		}
		// A d128 compares with a d128, or an i64 widened to one. Against an f64 the answer
		// would come from a conversion nobody asked for, as with the arithmetic above.
		{
			const StackValueType b = typeStack[typeStack.size() - 1];
			const StackValueType a = typeStack[typeStack.size() - 2];
			if ((a == StackValueType::D128) != (b == StackValueType::D128)) {
				const StackValueType other = (a == StackValueType::D128) ? b : a;
				if (other != StackValueType::ANY && other != StackValueType::UNKNOWN &&
						other != StackValueType::TYPEVAR && other != StackValueType::INT) {
					std::string errorMsg = "Type error in '";
					errorMsg += name;
					errorMsg += "': a d128 compares only with a d128 or an i64, not with ";
					errorMsg += typeToString(other);
					reportErrorConditionalWithHint(node, errorMsg.c_str(),
							"convert the other operand with 'cast<d128>', or write the literal with a 'd': '2.5d'",
							reportErrors);
				}
			}
		}
		// Pop both operands
		typeStack.pop_back();
		typeStack.pop_back();
		if (structTypeStack.size() >= 2) {
			structTypeStack.pop_back();
			structTypeStack.pop_back();
		}
		// Push result (always int/bool)
		typeStack.push_back(StackValueType::INT);
		structTypeStack.push_back("");
		return;
	}

	// Type casting: cast<T> (convert to type T)
	void SemanticValidator::checkBuiltinCast(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.empty()) {
			reportErrorConditional(node, "Type error in 'cast': Stack underflow (requires 1 value)", reportErrors);
			return;
		}
		// Pop any type, remembering it: a string operand is the one case 'cast' cannot
		// handle honestly (see the rejection below).
		const StackValueType operandType = typeStack.back();
		typeStack.pop_back();
		if (!structTypeStack.empty()) {
			structTypeStack.pop_back();
		}
		// Determine result type from type parameter
		StackValueType resultType = StackValueType::STRING; // default
		if (node->type() == IAstNode::Type::INSTRUCTION) {
			AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(node);
			if (instr->hasTypeParam()) {
				const std::string& typeParam = instr->typeParam();
				// The sized-type rejection is read through `type` aliases -- `type Byte = u8`
				// used to slip past it. Only that check: the backend picks `qd_cast*` from
				// the written name, so an alias of `i64` is still not a cast target.
				const std::string sizedTarget = sizedIntTypeThroughAliases(typeParam);
				if (!sizedTarget.empty()) {
					// `300 cast<u8>` yielded 300: the target named a width and the cast did
					// nothing with it. A cast target is a stack value, which is always 64
					// bits, so there is no width to apply -- mask if that is what is wanted.
					resultType = StackValueType::INT;
					reportErrorConditionalWithHint(node,
							sizedTypeMisuseMessage(typeParam, sizedTarget, "a cast target").c_str(),
							"to keep the low bits, mask: 'value 255 and' for a byte", reportErrors);
				} else if (typeParam == "i64") {
					resultType = StackValueType::INT;
				} else if (typeParam == "f64") {
					resultType = StackValueType::FLOAT;
				} else if (typeParam == "d128") {
					resultType = StackValueType::D128;
				} else if (typeParam == "str") {
					resultType = StackValueType::STRING;
				} else if (typeParam == "ptr") {
					resultType = StackValueType::PTR;
				} else if (isCurrentTypeParam(typeParam)) {
					// `cast<T>` inside a generic: the target is not known here.
					resultType = StackValueType::TYPEVAR;
				} else {
					// Anything else used to fall through to the STRING default in silence:
					// `p cast<Node>` type-checked and produced a string, and the mistake
					// surfaced a long way away as "the value on the stack is string, not a
					// struct". 'cast' offers four targets and a struct is not one of them --
					// narrowing a pointer to a struct type is what `as` is for, and it is
					// free, where a cast implies a conversion.
					std::string err = "Type error in 'cast': '";
					err += typeParam;
					err += "' is not a type 'cast' converts to";
					if (isStructTypeName(typeParam)) {
						std::string hint = "use 'value as ";
						hint += typeParam;
						hint += "' to read a pointer as a struct -- 'as' is a compile-time annotation, "
								"not a conversion";
						reportErrorConditionalWithHint(node, err.c_str(), hint.c_str(), reportErrors);
					} else {
						reportErrorConditionalWithHint(
								node, err.c_str(), "'cast' converts between i64, f64, d128, ptr and str", reportErrors);
					}
				}
			}
		}
		// A string cannot be cast to a number. Every other direction 'cast' offers is
		// total -- an int to a float, a float to an int, anything to a string -- but
		// parsing can fail, and this one reported failure as the value 0: "notanumber"
		// cast<i64> and "0" cast<i64> both gave 0, with nothing to tell them apart. It
		// also stopped at the first bad character, so "42abc" silently became 42.
		//
		// strconv already has the honest version of every one of these, fallible, so
		// this direction was a quiet duplicate of an existing API rather than a feature.
		// Removing it rather than making 'cast' fallible keeps 'cast' what it is: a
		// total conversion that never needs its result checked.
		if (operandType == StackValueType::STRING &&
				(resultType == StackValueType::INT || resultType == StackValueType::FLOAT ||
						resultType == StackValueType::D128)) {
			const char* hint = (resultType == StackValueType::INT)
									   ? "use 'strconv::atoi' (or 'strconv::parse_int' for a base), which is fallible: "
										 "'s strconv::atoi if { -> n ... } else { ... }'"
							   : (resultType == StackValueType::FLOAT)
									   ? "use 'strconv::parse_float', which is fallible: "
										 "'s strconv::parse_float if { -> x ... } else { ... }'"
									   : "use 'strconv::parse_d128', which is fallible: "
										 "'s strconv::parse_d128 if { -> x ... } else { ... }'";
			std::string err = "Type error in 'cast': a string cannot be cast to ";
			err += (resultType == StackValueType::INT)	   ? "an integer"
				   : (resultType == StackValueType::FLOAT) ? "a float"
														   : "a d128";
			err += "; parsing can fail and 'cast' has no way to report it";
			reportErrorConditionalWithHint(node, err.c_str(), hint, reportErrors);
		}

		// A d128 is a number that happens to be boxed; the box is not something a program
		// can do anything with, so there is no pointer to cast it to.
		if (operandType == StackValueType::D128 && resultType == StackValueType::PTR) {
			reportErrorConditional(node, "Type error in 'cast': a d128 cannot be cast to ptr", reportErrors);
		}

		typeStack.push_back(resultType);
		structTypeStack.push_back("");
		return;
	}

	// call - invoke function pointer from stack
	void SemanticValidator::checkBuiltinCall(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.empty()) {
			reportErrorConditional(node, "Type error in 'call': Stack underflow (requires 1 value)", reportErrors);
			return;
		}
		// Pop the function pointer - runtime will verify it's a pointer type
		typeStack.pop_back();
		// Pop struct type as well, remembering it: for a pointer that came from a typed
		// parameter, a struct field, an array element or a local declared with a `fn(...)`
		// type, the type string is the only description of the callee there is.
		std::string fnTypeStr;
		if (!structTypeStack.empty()) {
			fnTypeStr = structTypeStack.back();
			structTypeStack.pop_back();
		}

		// A pending signature from `&f` or a lambda is preferred -- it is the real
		// signature, with struct types the type string cannot carry. Otherwise the declared
		// `fn(...)` type says what the call does.
		if (!mPendingFnSignature.has_value() && !fnTypeStr.empty()) {
			mPendingFnSignature = parseFnTypeString(fnTypeStr);
		}

		if (mPendingFnSignature.has_value()) {
			const FunctionSignature& sig = mPendingFnSignature.value();

			// A fallible callee is handled at the call site exactly as a fallible name is:
			// `!` aborts, `?` propagates, and a bare call has to be read by an `if` or a
			// `switch`. The site cannot see any of this for itself -- what it calls is a
			// value -- so the resolved signature is what answers, and the answer is recorded
			// on the node for code generation, which has no signature to consult.
			if (AstNodeInstruction* callInst = (node && node->type() == IAstNode::Type::INSTRUCTION)
													   ? static_cast<AstNodeInstruction*>(node)
													   : nullptr) {
				const bool abort = callInst->abortOnError();
				const bool propagate = callInst->propagateOnError();
				if ((abort || propagate) && !sig.throws) {
					std::string op = abort ? "!" : "?";
					std::string errorMsg =
							"Cannot use '" + op + "' operator on 'call': the function called is not fallible";
					reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				}
				if (propagate && !mCurrentFunctionFallible) {
					reportErrorConditional(node,
							"Cannot use '?' operator on 'call': enclosing function must be fallible "
							"(add '!' to function signature)",
							reportErrors);
				}
				if (sig.throws) {
					callInst->setCalleeFallible(true);
					mCallSiteSignatures[node] = sig;
					if (!abort && !propagate) {
						IAstNode* nextNode = nextSiblingOf(node);
						if (!nextNode || (nextNode->type() != IAstNode::Type::IF_STATEMENT &&
												 nextNode->type() != IAstNode::Type::SWITCH_STATEMENT)) {
							reportErrorConditional(node,
									"Fallible 'call' must be immediately followed by 'if' or 'switch' to "
									"check for errors, or use '!' to abort on error, or '?' to propagate",
									reportErrors);
						}
					}
				}
			}

			// Consume input parameters from stack
			if (typeStack.size() < sig.consumes.size()) {
				std::string errorMsg = "Type error in 'call': Stack underflow for function pointer (requires ";
				errorMsg += std::to_string(sig.consumes.size());
				errorMsg += " values, have ";
				errorMsg += std::to_string(typeStack.size());
				errorMsg += ")";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				mPendingFnSignature.reset();
				return;
			}

			// Pop consumed types
			for (size_t j = 0; j < sig.consumes.size(); j++) {
				typeStack.pop_back();
				if (!structTypeStack.empty()) {
					structTypeStack.pop_back();
				}
			}

			// Push produced types, with any struct/array/fn types the signature carries, so a
			// call that returns a struct or another function pointer can be chained.
			FunctionSignature applied = sig;
			const bool bareFallible = applied.throws && node && node->type() == IAstNode::Type::INSTRUCTION &&
									  !static_cast<AstNodeInstruction*>(node)->abortOnError() &&
									  !static_cast<AstNodeInstruction*>(node)->propagateOnError();
			mPendingFnSignature.reset();
			pushCallResults(applied, {}, {}, typeStack, structTypeStack);
			if (bareFallible) {
				// The status the following `if`/`switch` reads, as a bare fallible call by
				// name leaves.
				typeStack.push_back(StackValueType::INT);
				structTypeStack.push_back("");
			}
			// The effect is fully known, so the function-level checks stay in force. Falling
			// out of this branch instead would reach the "unhandled instruction" marker at the
			// end of this function -- a plain statement, not an `else` -- and switch off the
			// arity, if-arm and defer checks in every function containing a `call`.
			return;
		}
		// No signature: an untyped `ptr` was called and the effect really is unknown.
		mHasUnpredictableStack = true;
		return;
	}

	// Array access: nth ( arr idx -- elem )
	// Returns element at index, consuming the array reference
	void SemanticValidator::checkBuiltinNth(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.size() < 2) {
			reportErrorConditional(
					node, "Type error in 'nth': Stack underflow (requires array and index)", reportErrors);
			return;
		}
		// Pop index
		typeStack.pop_back();
		if (!structTypeStack.empty()) {
			structTypeStack.pop_back();
		}
		// Verify array is a pointer
		StackValueType arr = typeStack.back();
		if (arr != StackValueType::PTR) {
			std::string errorMsg = "Type error in 'nth': Expected array (ptr), got ";
			errorMsg += typeToString(arr);
			reportErrorConditional(node, errorMsg.c_str(), reportErrors);
			return;
		}
		// Pop array, push element type based on array's tracked element type
		std::string arrayType = "";
		if (!structTypeStack.empty()) {
			arrayType = structTypeStack.back();
			structTypeStack.pop_back();
		}
		typeStack.pop_back();
		std::string elemType;
		if (arrayType.size() > 2 && arrayType[0] == '[' && arrayType[1] == ']') {
			elemType = arrayType.substr(2);
			if (elemType == "i64") {
				typeStack.push_back(StackValueType::INT);
				structTypeStack.push_back("");
			} else if (elemType == "f64") {
				typeStack.push_back(StackValueType::FLOAT);
				structTypeStack.push_back("");
			} else if (elemType == "d128") {
				typeStack.push_back(StackValueType::D128);
				structTypeStack.push_back("");
			} else if (elemType == "str") {
				typeStack.push_back(StackValueType::STRING);
				structTypeStack.push_back("");
			} else if (elemType == "ptr" || elemType == "any") {
				typeStack.push_back(StackValueType::ANY);
				structTypeStack.push_back("");
			} else {
				// Struct element type
				typeStack.push_back(StackValueType::PTR);
				structTypeStack.push_back(elemType);
			}
		} else {
			// Unknown array type, fall back to ANY
			typeStack.push_back(StackValueType::ANY);
			structTypeStack.push_back("");
		}
		// Hand the element type to the backend, which has no way to reach it: see
		// AstNodeInstruction::resolvedType. Only on the reporting pass -- the isolated
		// signature analysis walks the same nodes with a stack it has less information
		// about, and its answer must not overwrite this one.
		if (reportErrors && node != nullptr && node->type() == IAstNode::Type::INSTRUCTION) {
			static_cast<AstNodeInstruction*>(node)->setResolvedType(elemType);
		}
		return;
	}

	// Array set: set ( arr idx val -- )
	// Sets element at index
	void SemanticValidator::checkBuiltinSet(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.size() < 3) {
			reportErrorConditional(
					node, "Type error in 'set': Stack underflow (requires array, index, and value)", reportErrors);
			return;
		}
		// Check element type against typed array
		StackValueType valType = typeStack.back();
		// Array is 3rd from top (arr idx val)
		size_t arrIdx = typeStack.size() - 3;
		if (arrIdx < structTypeStack.size()) {
			const std::string& arrayType = structTypeStack[arrIdx];
			if (arrayType.size() > 2 && arrayType[0] == '[' && arrayType[1] == ']') {
				std::string elemType = arrayType.substr(2);
				StackValueType expectedElemType = StackValueType::ANY;
				if (elemType == "i64") {
					expectedElemType = StackValueType::INT;
				} else if (elemType == "f64") {
					expectedElemType = StackValueType::FLOAT;
				} else if (elemType == "d128") {
					expectedElemType = StackValueType::D128;
				} else if (elemType == "str") {
					expectedElemType = StackValueType::STRING;
				}
				if (expectedElemType != StackValueType::ANY && valType != StackValueType::ANY &&
						valType != StackValueType::UNKNOWN && valType != expectedElemType) {
					std::string errorMsg = "Type error in 'set': Array is '";
					errorMsg += arrayType;
					errorMsg += "' but value is ";
					errorMsg += stackValueTypeToString(valType);
					reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				}
			}
		}
		// Pop value, index, array
		typeStack.pop_back(); // value
		typeStack.pop_back(); // index
		typeStack.pop_back(); // array
		if (structTypeStack.size() >= 3) {
			structTypeStack.pop_back();
			structTypeStack.pop_back();
			structTypeStack.pop_back();
		}
		return;
	}

	// Array append: append ( arr val -- arr' )
	// Appends value to array and returns new array
	void SemanticValidator::checkBuiltinAppend(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.size() < 2) {
			reportErrorConditional(
					node, "Type error in 'append': Stack underflow (requires array and value)", reportErrors);
			return;
		}
		// Check element type against typed array before popping
		StackValueType valType = typeStack.back();
		// Array is 2nd from top (arr val)
		size_t arrIdx = typeStack.size() - 2;
		if (arrIdx < structTypeStack.size()) {
			const std::string& arrayType = structTypeStack[arrIdx];
			if (arrayType.size() > 2 && arrayType[0] == '[' && arrayType[1] == ']') {
				std::string elemType = arrayType.substr(2);
				StackValueType expectedElemType = StackValueType::ANY;
				if (elemType == "i64") {
					expectedElemType = StackValueType::INT;
				} else if (elemType == "f64") {
					expectedElemType = StackValueType::FLOAT;
				} else if (elemType == "d128") {
					expectedElemType = StackValueType::D128;
				} else if (elemType == "str") {
					expectedElemType = StackValueType::STRING;
				}
				if (expectedElemType != StackValueType::ANY && valType != StackValueType::ANY &&
						valType != StackValueType::UNKNOWN && valType != expectedElemType) {
					std::string errorMsg = "Type error in 'append': Array is '";
					errorMsg += arrayType;
					errorMsg += "' but value is ";
					errorMsg += stackValueTypeToString(valType);
					reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				}
			}
		}
		// Pop value
		typeStack.pop_back();
		if (!structTypeStack.empty()) {
			structTypeStack.pop_back();
		}
		// Verify array is a pointer
		StackValueType arr = typeStack.back();
		if (arr != StackValueType::PTR) {
			std::string errorMsg = "Type error in 'append': Expected array (ptr), got ";
			errorMsg += typeToString(arr);
			reportErrorConditional(node, errorMsg.c_str(), reportErrors);
			return;
		}
		// Array stays on stack (as modified array), already PTR type
		return;
	}

	// clone - a new struct of the same type, fields copied
	void SemanticValidator::checkBuiltinClone(IAstNode* node, std::vector<StackValueType>& typeStack,
			std::vector<std::string>& structTypeStack, bool reportErrors) {
		if (typeStack.empty()) {
			reportErrorConditional(node, "Type error in 'clone': Stack underflow (requires 1 struct)", reportErrors);
			return;
		}
		// The struct type is what decides the size to allocate and the fields to retain, so
		// an untyped pointer cannot be cloned: nothing says how much of it there is. `as`
		// narrows one to a struct type and is the way through.
		const std::string structType = structTypeStack.empty() ? std::string() : structTypeStack.back();
		const bool known = typeStack.back() == StackValueType::PTR && !structType.empty() &&
						   lookupStructFieldTypes(structType) != nullptr;
		if (!known) {
			std::string errorMsg = "Type error in 'clone': Expected a struct, got ";
			errorMsg += structType.empty() ? typeToString(typeStack.back()) : structType;
			if (typeStack.back() == StackValueType::PTR && structType.empty()) {
				reportErrorConditionalWithHint(node, errorMsg.c_str(),
						"the struct type has to be known here; narrow the pointer with 'as', e.g. 'p as Point "
						"clone'",
						reportErrors);
			} else {
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
			}
			return;
		}
		// The copy has the type of the original, so the stack is unchanged in shape: the
		// struct goes off and one of the same type comes back.
		if (reportErrors && node != nullptr && node->type() == IAstNode::Type::INSTRUCTION) {
			static_cast<AstNodeInstruction*>(node)->setResolvedType(structType);
		}
		return;
	}

	// The machine instructions: raw memory loads and stores, x86 port I/O and CPU control. Each
	// takes i64 operands and leaves an i64 or nothing. False when `name` is none of them.
	bool SemanticValidator::checkBuiltinMachine(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, bool reportErrors) {
		struct Machine {
			const char* names[4];
			size_t consumes;
			bool producesValue;
			const char* operands;
		};

		static const Machine MACHINE[] = {
				{{"__st8", "__st16", "__st32", "__st64"}, 3, false, "addr offset value"},
				{{"__ld8", "__ld16", "__ld32", "__ld64"}, 2, true, "addr offset"},
				{{"__port_out8", "__port_out16", "__port_out32", nullptr}, 2, false, "port value"},
				{{"__port_in8", "__port_in16", "__port_in32", nullptr}, 1, true, "port"},
				{{"__cli", "__sti", "__hlt", nullptr}, 0, false, ""},
		};
		for (const Machine& machine : MACHINE) {
			if (std::none_of(std::begin(machine.names), std::end(machine.names),
						[name](const char* n) { return n != nullptr && strcmp(name, n) == 0; })) {
				continue;
			}
			if (typeStack.size() < machine.consumes) {
				std::string err = "Type error in '";
				err += name;
				err += "': Stack underflow (requires ";
				err += machine.operands;
				err += ")";
				reportErrorConditional(node, err.c_str(), reportErrors);
				return true;
			}
			typeStack.resize(typeStack.size() - machine.consumes);
			structTypeStack.resize(structTypeStack.size() - std::min(machine.consumes, structTypeStack.size()));
			if (machine.producesValue) {
				typeStack.push_back(StackValueType::INT);
				structTypeStack.push_back("");
			}
			return true;
		}
		return false;
	}

	// Stack shuffles: a fixed rearrangement of the top values, their struct types with them.
	// False when `name` is none of them.
	bool SemanticValidator::checkBuiltinShuffle(IAstNode* node, const char* name,
			std::vector<StackValueType>& typeStack, std::vector<std::string>& structTypeStack, bool reportErrors) {
		struct Shuffle {
			const char* name;
			size_t consumes;
			std::vector<size_t> produces; // indices into what it consumes, deepest first
		};

		static const Shuffle SHUFFLES[] = {
				{"dup", 1, {0, 0}},		   // ( a -- a a )
				{"dup2", 2, {0, 1, 0, 1}}, // ( a b -- a b a b )
				{"swap", 2, {1, 0}},	   // ( a b -- b a )
				{"over", 2, {0, 1, 0}},	   // ( a b -- a b a )
				{"pick", 3, {0, 1, 2, 0}}, // ( x y z -- x y z x )
				{"nip", 2, {1}},		   // ( a b -- b )
				{"drop", 1, {}},		   // ( a -- )
				{"rot", 3, {1, 2, 0}},	   // ( a b c -- b c a )
		};
		for (const Shuffle& shuffle : SHUFFLES) {
			if (strcmp(name, shuffle.name) != 0) {
				continue;
			}
			const size_t n = shuffle.consumes;
			if (typeStack.size() < n) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires ";
				errorMsg += std::to_string(n);
				errorMsg += n == 1 ? " value)" : " values)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return true;
			}
			const std::vector<StackValueType> types(typeStack.end() - static_cast<std::ptrdiff_t>(n), typeStack.end());
			typeStack.resize(typeStack.size() - n);
			for (size_t index : shuffle.produces) {
				typeStack.push_back(types[index]);
			}
			// The struct types move with their values. Where fewer are tracked than there are
			// values, the untracked ones count as none.
			std::vector<std::string> structs(n);
			const size_t tracked = std::min(n, structTypeStack.size());
			std::copy(structTypeStack.end() - static_cast<std::ptrdiff_t>(tracked), structTypeStack.end(),
					structs.end() - static_cast<std::ptrdiff_t>(tracked));
			structTypeStack.resize(structTypeStack.size() - tracked);
			for (size_t index : shuffle.produces) {
				structTypeStack.push_back(structs[index]);
			}
			return true;
		}
		return false;
	}

} // namespace Qd
