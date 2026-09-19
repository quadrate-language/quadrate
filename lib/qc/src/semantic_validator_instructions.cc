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
		if (strcmp(name, ".") == 0) {
			name = "print";
		} else if (strcmp(name, "/") == 0) {
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
					"print", "prints", "printv", "printsv", "nl", "panic", "err", "spawn", "wait", "detach", nullptr};
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

		// Arithmetic operations: abs, sq (preserve type)
		if (strcmp(name, "abs") == 0 || strcmp(name, "sq") == 0) {
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
		// Trigonometric functions: sin, cos, tan, asin, acos, atan (always return float)
		else if (strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 ||
				 strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 || strcmp(name, "atan") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportError(node, errorMsg.c_str());
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Pop and push float (trig functions always return float)
			typeStack.pop_back();
			typeStack.push_back(StackValueType::FLOAT);
			return;
		}
		// Math functions: sqrt, cb, cbrt, ceil, floor, ln, log10, round (always return float)
		else if (strcmp(name, "sqrt") == 0 || strcmp(name, "cb") == 0 || strcmp(name, "cbrt") == 0 ||
				 strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0 || strcmp(name, "ln") == 0 ||
				 strcmp(name, "log10") == 0 || strcmp(name, "round") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportError(node, errorMsg.c_str());
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Pop and push float (math functions always return float)
			typeStack.pop_back();
			typeStack.push_back(StackValueType::FLOAT);
			return;
		}
		// Factorial function: fac (integer only, returns integer)
		else if (strcmp(name, "fac") == 0) {
			if (typeStack.empty()) {
				reportError(node, "Type error in 'fac': Stack underflow (requires 1 integer value)");
				return;
			}

			StackValueType top = typeStack.back();
			if (top != StackValueType::INT) {
				std::string errorMsg = "Type error in 'fac': Expected integer type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Type remains integer (already on stack)
			return;
		}
		// Increment/Decrement functions: inc, dec (preserve type)
		else if (strcmp(name, "inc") == 0 || strcmp(name, "dec") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 numeric value)";
				reportError(node, errorMsg.c_str());
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Type remains the same (already on stack)
			return;
		}
		// Inverse function: inv (numeric input, returns float)
		else if (strcmp(name, "inv") == 0) {
			if (typeStack.empty()) {
				reportError(node, "Type error in 'inv': Stack underflow (requires 1 numeric value)");
				return;
			}

			StackValueType top = typeStack.back();
			if (!isNumericType(top)) {
				std::string errorMsg = "Type error in 'inv': Expected numeric type, got ";
				errorMsg += typeToString(top);
				reportError(node, errorMsg.c_str());
				return;
			}
			// Pop and push float (inv always returns float)
			typeStack.pop_back();
			typeStack.push_back(StackValueType::FLOAT);
			return;
		}
		// Binary arithmetic operations: add, sub, mul, div, pow
		else if (strcmp(name, "add") == 0 || strcmp(name, "sub") == 0 || strcmp(name, "mul") == 0 ||
				 strcmp(name, "div") == 0 || strcmp(name, "pow") == 0) {
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
		else if (strcmp(name, "eq") == 0 || strcmp(name, "neq") == 0 || strcmp(name, "lt") == 0 ||
				 strcmp(name, "gt") == 0 || strcmp(name, "lte") == 0 || strcmp(name, "gte") == 0) {
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
		// Logical operations: and, or (consume 2 bools, produce int/bool)
		else if (strcmp(name, "and") == 0 || strcmp(name, "or") == 0) {
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
		// Bitwise / logical negation: not, lnot (consume 1, produce int/bool)
		else if (strcmp(name, "not") == 0 || strcmp(name, "lnot") == 0) {
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
		// Bitwise shift and XOR operations: shl, shr, xor (consume 2 ints, produce int)
		else if (strcmp(name, "shl") == 0 || strcmp(name, "shr") == 0 || strcmp(name, "xor") == 0) {
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
					if (typeParam == "i64" || typeParam == "i32" || typeParam == "i16" || typeParam == "i8" ||
							typeParam == "u64" || typeParam == "u32" || typeParam == "u16" || typeParam == "u8") {
						resultType = StackValueType::INT;
					} else if (typeParam == "f64" || typeParam == "f32") {
						resultType = StackValueType::FLOAT;
					} else if (typeParam == "str" || typeParam == "string") {
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
							reportErrorConditionalWithHint(node, err.c_str(),
									"'cast' converts between i64, f64, ptr and str; sized integer types are "
									"accepted as i64",
									reportErrors);
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
					(resultType == StackValueType::INT || resultType == StackValueType::FLOAT)) {
				const char* hint =
						(resultType == StackValueType::INT)
								? "use 'strconv::atoi' (or 'strconv::parse_int' for a base), which is fallible: "
								  "'s strconv::atoi if { -> n ... } else { ... }'"
								: "use 'strconv::parse_float', which is fallible: "
								  "'s strconv::parse_float if { -> x ... } else { ... }'";
				std::string err = "Type error in 'cast': a string cannot be cast to ";
				err += (resultType == StackValueType::INT) ? "an integer" : "a float";
				err += "; parsing can fail and 'cast' has no way to report it";
				reportErrorConditionalWithHint(node, err.c_str(), hint, reportErrors);
			}

			typeStack.push_back(resultType);
			structTypeStack.push_back("");
			return;
		}
		// Type size introspection: sizeof<T> or sizeof (on value)
		else if (strcmp(name, "sizeof") == 0) {
			if (node->type() == IAstNode::Type::INSTRUCTION) {
				AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(node);
				if (instr->hasTypeParam()) {
					// sizeof<T> - compile-time, just push an int
					typeStack.push_back(StackValueType::INT);
					structTypeStack.push_back("");
				} else {
					// sizeof on value - pop value, push int
					if (typeStack.empty()) {
						reportErrorConditional(
								node, "Type error in 'sizeof': Stack underflow (requires 1 value)", reportErrors);
						return;
					}
					typeStack.pop_back();
					if (!structTypeStack.empty()) {
						structTypeStack.pop_back();
					}
					typeStack.push_back(StackValueType::INT);
					structTypeStack.push_back("");
					return;
				}
			}
			return;
		}
		// Raw memory stores: __st8/__st16/__st32/__st64
		// (addr:i64 offset:i64 value:i64 -- )
		else if (strcmp(name, "__st8") == 0 || strcmp(name, "__st16") == 0 || strcmp(name, "__st32") == 0 ||
				 strcmp(name, "__st64") == 0) {
			if (typeStack.size() < 3) {
				std::string err = "Type error in '";
				err += name;
				err += "': Stack underflow (requires addr offset value)";
				reportErrorConditional(node, err.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back(); // value
			typeStack.pop_back(); // offset
			typeStack.pop_back(); // addr
			for (int i = 0; i < 3 && !structTypeStack.empty(); i++) {
				structTypeStack.pop_back();
			}
			return;
		}
		// Raw memory loads: __ld8/__ld16/__ld32/__ld64
		// (addr:i64 offset:i64 -- value:i64) — value is zero-extended.
		else if (strcmp(name, "__ld8") == 0 || strcmp(name, "__ld16") == 0 || strcmp(name, "__ld32") == 0 ||
				 strcmp(name, "__ld64") == 0) {
			if (typeStack.size() < 2) {
				std::string err = "Type error in '";
				err += name;
				err += "': Stack underflow (requires addr offset)";
				reportErrorConditional(node, err.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back(); // offset
			typeStack.pop_back(); // addr
			for (int i = 0; i < 2 && !structTypeStack.empty(); i++) {
				structTypeStack.pop_back();
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// Port I/O output: __port_out8/__port_out16/__port_out32
		// (port:i64 value:i64 --)
		else if (strcmp(name, "__port_out8") == 0 || strcmp(name, "__port_out16") == 0 ||
				 strcmp(name, "__port_out32") == 0) {
			if (typeStack.size() < 2) {
				std::string err = "Type error in '";
				err += name;
				err += "': Stack underflow (requires port value)";
				reportErrorConditional(node, err.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back(); // value
			typeStack.pop_back(); // port
			for (int i = 0; i < 2 && !structTypeStack.empty(); i++) {
				structTypeStack.pop_back();
			}
			return;
		}
		// Port I/O input: __port_in8/__port_in16/__port_in32
		// (port:i64 -- value:i64)
		else if (strcmp(name, "__port_in8") == 0 || strcmp(name, "__port_in16") == 0 ||
				 strcmp(name, "__port_in32") == 0) {
			if (typeStack.empty()) {
				std::string err = "Type error in '";
				err += name;
				err += "': Stack underflow (requires port)";
				reportErrorConditional(node, err.c_str(), reportErrors);
				return;
			}
			typeStack.pop_back(); // port
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			typeStack.push_back(StackValueType::INT);
			structTypeStack.push_back("");
			return;
		}
		// CPU control: __cli, __sti, __hlt ( -- )
		else if (strcmp(name, "__cli") == 0 || strcmp(name, "__sti") == 0 || strcmp(name, "__hlt") == 0) {
			// No stack effect
			return;
		}
		// Modulo: mod (consume 2 ints, produce int)
		else if (strcmp(name, "mod") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(
						node, "Type error in 'mod': Stack underflow (requires 2 integer values)", reportErrors);
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
		// Non-destructive print: prints, printsv
		else if (strcmp(name, "prints") == 0 || strcmp(name, "printsv") == 0 || strcmp(name, "nl") == 0) {
			// These don't modify the stack
			return;
		}
		// Stack operations: dup
		else if (strcmp(name, "dup") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'dup': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			StackValueType top = typeStack.back();
			typeStack.push_back(top); // Duplicate
			// Duplicate struct type as well
			if (!structTypeStack.empty()) {
				std::string topStruct = structTypeStack.back();
				structTypeStack.push_back(topStruct);
				return;
			}
			return;
		}
		// Stack operations: dup2 ( a b -- a b a b )
		else if (strcmp(name, "dup2") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'dup2': Stack underflow (requires 2 values)");
				return;
			}
			// Get the second and top elements
			StackValueType second = typeStack[typeStack.size() - 2];
			StackValueType top = typeStack.back();
			// Push copies of both
			typeStack.push_back(second);
			typeStack.push_back(top);
			// Duplicate struct types as well
			if (structTypeStack.size() >= 2) {
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				std::string topStruct = structTypeStack.back();
				structTypeStack.push_back(secondStruct);
				structTypeStack.push_back(topStruct);
				return;
			}
			return;
		}
		// Stack operations: swap
		else if (strcmp(name, "swap") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'swap': Stack underflow (requires 2 values)", reportErrors);
				return;
			}
			StackValueType a = typeStack.back();
			typeStack.pop_back();
			StackValueType b = typeStack.back();
			typeStack.pop_back();
			typeStack.push_back(a);
			typeStack.push_back(b);
			// Swap struct types as well
			if (structTypeStack.size() >= 2) {
				std::string aStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string bStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.push_back(aStruct);
				structTypeStack.push_back(bStruct);
				return;
			}
			return;
		}
		// Stack operations: over ( a b -- a b a )
		else if (strcmp(name, "over") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'over': Stack underflow (requires 2 values)");
				return;
			}
			// Get the second element
			StackValueType second = typeStack[typeStack.size() - 2];
			// Push a copy of it to the top
			typeStack.push_back(second);
			// Copy struct type as well
			if (structTypeStack.size() >= 2) {
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				structTypeStack.push_back(secondStruct);
				return;
			}
			return;
		}
		// Stack operations: pick ( x y z -- x y z x )
		else if (strcmp(name, "pick") == 0) {
			if (typeStack.size() < 3) {
				reportErrorConditional(node, "Type error in 'pick': Stack underflow (requires 3 values)", reportErrors);
				return;
			}
			typeStack.push_back(typeStack[typeStack.size() - 3]);
			if (structTypeStack.size() >= 3) {
				structTypeStack.push_back(structTypeStack[structTypeStack.size() - 3]);
			} else {
				structTypeStack.push_back("");
			}
			return;
		}
		// Stack operations: nip ( a b -- b )
		else if (strcmp(name, "nip") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'nip': Stack underflow (requires 2 values)");
				return;
			}
			StackValueType top = typeStack.back();
			typeStack.pop_back();
			typeStack.pop_back();	  // Remove second element
			typeStack.push_back(top); // Push top back
			// Remove second struct type as well
			if (structTypeStack.size() >= 2) {
				std::string topStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.push_back(topStruct);
				return;
			}
			return;
		}
		// Stack operations: drop ( a -- )
		else if (strcmp(name, "drop") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'drop': Stack underflow (requires 1 value)", reportErrors);
				return;
			}
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			return;
		}
		// Stack operations: rot ( a b c -- b c a )
		else if (strcmp(name, "rot") == 0) {
			if (typeStack.size() < 3) {
				reportErrorConditional(node, "Type error in 'rot': Stack underflow (requires 3 values)", reportErrors);
				return;
			}
			StackValueType c = typeStack.back();
			typeStack.pop_back();
			StackValueType b = typeStack.back();
			typeStack.pop_back();
			StackValueType a = typeStack.back();
			typeStack.pop_back();
			typeStack.push_back(b);
			typeStack.push_back(c);
			typeStack.push_back(a);
			// Handle struct type stack
			if (structTypeStack.size() >= 3) {
				std::string cStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string bStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string aStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.push_back(bStruct);
				structTypeStack.push_back(cStruct);
				structTypeStack.push_back(aStruct);
				return;
			}
			return;
		}
		// Stack operations: clear (empties the entire stack)
		else if (strcmp(name, "clear") == 0) {
			// Clear all elements from the type stack
			typeStack.clear();
			// Clear struct type stack as well
			structTypeStack.clear();
			return;
		}
		// Stack operations: depth (pushes the current stack depth as an integer)
		else if (strcmp(name, "depth") == 0) {
			// Push an int type onto the stack (depth is always an integer)
			typeStack.push_back(StackValueType::INT);
			// Push empty struct type (int is not a struct)
			structTypeStack.push_back("");
			return;
		}
		// call - invoke function pointer from stack
		else if (strcmp(name, "call") == 0) {
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
		// Array creation: make, makei, makef, makes, makep ( size -- arr )
		// All create typed arrays, always return pointer
		else if (strcmp(name, "make") == 0 || strcmp(name, "makei") == 0 || strcmp(name, "makef") == 0 ||
				 strcmp(name, "makes") == 0 || strcmp(name, "makep") == 0) {
			if (typeStack.empty()) {
				std::string errorMsg = "Type error in '";
				errorMsg += name;
				errorMsg += "': Stack underflow (requires 1 integer for size)";
				reportErrorConditional(node, errorMsg.c_str(), reportErrors);
				return;
			}
			// Pop size argument
			typeStack.pop_back();
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}
			// Push pointer (array) with element type tracking
			typeStack.push_back(StackValueType::PTR);
			if (strcmp(name, "makei") == 0) {
				structTypeStack.push_back("[]i64");
			} else if (strcmp(name, "makef") == 0) {
				structTypeStack.push_back("[]f64");
			} else if (strcmp(name, "makes") == 0) {
				structTypeStack.push_back("[]str");
			} else if (strcmp(name, "makep") == 0) {
				structTypeStack.push_back("[]ptr");
			} else if (strcmp(name, "make") == 0) {
				// Generic make<T>: read type param from instruction node
				auto* inst = static_cast<AstNodeInstruction*>(node);
				if (!inst->typeParam().empty()) {
					structTypeStack.push_back("[]" + inst->typeParam());
				} else {
					structTypeStack.push_back("[]i64"); // default
				}
			} else {
				structTypeStack.push_back("");
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
			if (arrayType.size() > 2 && arrayType[0] == '[' && arrayType[1] == ']') {
				std::string elemType = arrayType.substr(2);
				if (elemType == "i64") {
					typeStack.push_back(StackValueType::INT);
					structTypeStack.push_back("");
				} else if (elemType == "f64") {
					typeStack.push_back(StackValueType::FLOAT);
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
			return;
		}
		// Array set: set ( arr idx val -- )
		// Sets element at index
		else if (strcmp(name, "set") == 0) {
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
		else if (strcmp(name, "append") == 0) {
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

} // namespace Qd
