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

#include "instructions.h"
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/semantic_validator.h>

namespace Qd {

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
			static const char* kUnsafeBuiltins[] = {"print", "prints", "printv", "printsv", "nl", "read", "panic",
					"err", "spawn", "wait", "detach", nullptr};
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

		// read instruction: reads command-line arguments
		// Stack: [...] -> [...] arg0 arg1 ... argN argc
		// Since we don't know argc at compile-time, we push multiple values
		// to allow reasonable operations after read (assumes up to 16 arguments)
		// At runtime, arguments are parsed as int, float, or string based on content.
		// At compile-time, we use STRING type for arguments since that's the most common case.
		static const int READ_INSTRUCTION_MAX_ARGS = 16; // Maximum expected command-line arguments
		if (strcmp(name, "read") == 0) {
			mHasUnpredictableStack = true;
			typeStack.clear();
			// Push 16 STRING-typed arguments (actual types determined at runtime)
			for (int i = 0; i < READ_INSTRUCTION_MAX_ARGS; i++) {
				typeStack.push_back(StackValueType::STRING);
			}
			// Push argc as integer (on top of stack)
			typeStack.push_back(StackValueType::INT);
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
		// Logical negation: not (consume 1, produce int/bool)
		else if (strcmp(name, "not") == 0) {
			if (typeStack.empty()) {
				reportErrorConditional(node, "Type error in 'not': Stack underflow (requires 1 value)", reportErrors);
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
			// Pop any type
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
					}
				}
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
		// Stack operations: dupd ( a b -- a a b )
		else if (strcmp(name, "dupd") == 0) {
			if (typeStack.size() < 2) {
				reportError(node, "Type error in 'dupd': Stack underflow (requires 2 values)");
				return;
			}
			// Insert copy of second element before top: [a, b] -> [a, a, b]
			StackValueType second = typeStack[typeStack.size() - 2];
			typeStack.insert(typeStack.end() - 1, second);
			if (structTypeStack.size() >= 2) {
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				structTypeStack.insert(structTypeStack.end() - 1, secondStruct);
			}
			return;
		}
		// Stack operations: swapd ( a b c -- b a c )
		else if (strcmp(name, "swapd") == 0) {
			if (typeStack.size() < 3) {
				reportError(node, "Type error in 'swapd': Stack underflow (requires 3 values)");
				return;
			}
			// Get third, second, and top elements
			StackValueType third = typeStack[typeStack.size() - 3];
			StackValueType second = typeStack[typeStack.size() - 2];
			StackValueType top = typeStack.back();
			// Remove all three
			typeStack.pop_back();
			typeStack.pop_back();
			typeStack.pop_back();
			// Push: second, third, top (swapped second and third)
			typeStack.push_back(second);
			typeStack.push_back(third);
			typeStack.push_back(top);
			// Swap struct types as well
			if (structTypeStack.size() >= 3) {
				std::string thirdStruct = structTypeStack[structTypeStack.size() - 3];
				std::string secondStruct = structTypeStack[structTypeStack.size() - 2];
				std::string topStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
				structTypeStack.push_back(secondStruct);
				structTypeStack.push_back(thirdStruct);
				structTypeStack.push_back(topStruct);
				return;
			}
			return;
		}
		// Stack operations: overd ( a b c -- a b a c )
		else if (strcmp(name, "overd") == 0) {
			if (typeStack.size() < 3) {
				reportError(node, "Type error in 'overd': Stack underflow (requires 3 values)");
				return;
			}
			// Get the third element
			StackValueType third = typeStack[typeStack.size() - 3];
			// Push a copy of it to the top
			typeStack.push_back(third);
			// Copy struct type as well
			if (structTypeStack.size() >= 3) {
				std::string thirdStruct = structTypeStack[structTypeStack.size() - 3];
				structTypeStack.push_back(thirdStruct);
				return;
			}
			return;
		}
		// Stack operations: nipd ( a b c -- a c )
		else if (strcmp(name, "nipd") == 0) {
			if (typeStack.size() < 3) {
				reportError(node, "Type error in 'nipd': Stack underflow (requires 3 values)");
				return;
			}
			// Get top element
			StackValueType top = typeStack.back();
			typeStack.pop_back();
			// Remove second element
			typeStack.pop_back();
			// Push top back
			typeStack.push_back(top);
			// Remove second struct type as well
			if (structTypeStack.size() >= 3) {
				std::string topStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.pop_back();
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
		// Stack operations: drop2 ( a b -- )
		else if (strcmp(name, "drop2") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(
						node, "Type error in 'drop2': Stack underflow (requires 2 values)", reportErrors);
				return;
			}
			typeStack.pop_back();
			typeStack.pop_back();
			if (structTypeStack.size() >= 2) {
				structTypeStack.pop_back();
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
		// Stack operations: tuck ( a b -- b a b )
		else if (strcmp(name, "tuck") == 0) {
			if (typeStack.size() < 2) {
				reportErrorConditional(node, "Type error in 'tuck': Stack underflow (requires 2 values)", reportErrors);
				return;
			}
			StackValueType b = typeStack.back();
			typeStack.pop_back();
			StackValueType a = typeStack.back();
			typeStack.pop_back();
			typeStack.push_back(b);
			typeStack.push_back(a);
			typeStack.push_back(b);
			// Handle struct type stack
			if (structTypeStack.size() >= 2) {
				std::string bStruct = structTypeStack.back();
				structTypeStack.pop_back();
				std::string aStruct = structTypeStack.back();
				structTypeStack.pop_back();
				structTypeStack.push_back(bStruct);
				structTypeStack.push_back(aStruct);
				structTypeStack.push_back(bStruct);
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
			// Pop struct type as well
			if (!structTypeStack.empty()) {
				structTypeStack.pop_back();
			}

			// If we have a pending function signature from a known function pointer,
			// apply its stack effect
			if (mPendingFnSignature.has_value()) {
				const FunctionSignature& sig = mPendingFnSignature.value();

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

				// Push produced types
				for (const auto& type : sig.produces) {
					typeStack.push_back(type);
					structTypeStack.push_back(""); // Don't track struct types for now
				}

				mPendingFnSignature.reset();
			}
			// Otherwise, we don't know what the called function will do to the stack
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
