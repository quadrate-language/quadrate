#pragma once

#include <cstring>

namespace Qd {
	// Built-in runtime instructions
	// These are instructions that are directly compiled into the runtime executable
	static const char* BUILTIN_INSTRUCTIONS[] = {
			// Comparison operators (also available as symbols)
			"!=", "<", "<=", "==", ">", ">=",
			// Arithmetic operators (also available as symbols)
			"%", "*", "+", "-", ".", "/",
			// Arithmetic instructions
			"add", "dec", "div", "inc", "mod", "mul", "neg", "sub",
			// Logical operations
			"eq", "gt", "gte", "lt", "lte", "neq", "within",
			// Stack operations
			"call", "clear", "depth", "drop", "drop2", "dup", "dup2", "dupd", "nip", "nipd", "over", "over2",
			"overd", "pick", "roll", "rot", "swap", "swap2", "swapd", "tuck",
			// Type casting
			"castf", "casti", "casts",
			// I/O
			"nl", "print", "prints", "printsv", "printv", "read",
			// Threading
			"detach", "spawn", "wait",
			// Error handling
			"error"};

	static const size_t BUILTIN_INSTRUCTION_COUNT = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);

	// Extended instruction list for semantic validation
	// Includes built-in instructions PLUS commonly imported library functions
	// This prevents false "undefined function" errors when validating standard library modules
	static const char* VALIDATOR_INSTRUCTIONS[] = {
			// All built-in instructions
			"!=", "%", "*", "+", "-", ".", "/", "<", "<=", "==", ">", ">=", "add", "dec", "div", "eq", "gt", "gte",
			"inc", "lt", "lte", "mod", "mul", "neg", "neq", "sub", "within",
			// Math library functions (imported by stdlib modules)
			"abs", "acos", "asin", "atan", "cb", "cbrt", "ceil", "cos", "fac", "floor", "inv", "ln", "log10",
			"max", "min", "pow", "round", "sin", "sq", "sqrt", "tan",
			// Logical/bitwise operations
			"and", "lshift", "not", "or", "rshift", "xor",
			// Stack operations
			"call", "clear", "depth", "drop", "drop2", "dup", "dup2", "dupd", "nip", "nipd", "over", "over2",
			"overd", "pick", "roll", "rot", "swap", "swap2", "swapd", "tuck",
			// Type casting
			"castf", "casti", "casts",
			// I/O
			"nl", "print", "prints", "printsv", "printv", "read",
			// Threading
			"detach", "spawn", "wait",
			// Error handling
			"error"};

	static const size_t VALIDATOR_INSTRUCTION_COUNT = sizeof(VALIDATOR_INSTRUCTIONS) / sizeof(VALIDATOR_INSTRUCTIONS[0]);

	// Helper function to check if an identifier is a built-in instruction (for parsing)
	inline bool isBuiltInInstruction(const char* name) {
		for (size_t i = 0; i < BUILTIN_INSTRUCTION_COUNT; i++) {
			if (strcmp(name, BUILTIN_INSTRUCTIONS[i]) == 0) {
				return true;
			}
		}
		return false;
	}

	// Helper function to check if an identifier is a known instruction (for validation)
	inline bool isKnownInstruction(const char* name) {
		for (size_t i = 0; i < VALIDATOR_INSTRUCTION_COUNT; i++) {
			if (strcmp(name, VALIDATOR_INSTRUCTIONS[i]) == 0) {
				return true;
			}
		}
		return false;
	}
}
