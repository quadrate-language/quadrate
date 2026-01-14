#ifndef QD_QC_INSTRUCTIONS_H
#define QD_QC_INSTRUCTIONS_H

#include <cstring>

namespace Qd {
	// Built-in runtime instructions
	// These are instructions that are directly compiled into the runtime executable
	static const char* BUILTIN_INSTRUCTIONS[] = {
			// Comparison operators (also available as symbols)
			"!=", "<", "<=", "==", ">", ">=",
			// Arithmetic operators (also available as symbols)
			"%", "*", "+", "++", "-", "--", "/",
			// Bitwise operators (also available as symbols)
			"<<", ">>",
			// Arithmetic instructions
			"add", "dec", "div", "inc", "mod", "mul", "neg", "sub",
			// Bitwise operations
			"and", "not", "or", "shl", "shr", "xor",
			// Logical operations
			"eq", "gt", "gte", "lt", "lte", "neq", "within",
			// Stack operations
			"call", "clear", "depth", "drop", "drop2", "dup", "dup2", "dupd", "free", "len", "nip", "nipd", "nth",
			"over", "over2", "overd", "pick", "roll", "rot", "swap", "swap2", "swapd", "tuck",
			// Array operations
			"append", "make", "makef", "makei", "makep", "makes", "set",
			// Type casting and introspection
			"cast", "sizeof",
			// I/O
			"nl", "print", "prints", "printsv", "printv", "read",
			// Threading
			"detach", "spawn", "wait",
			// Error handling
			"err", "panic"};

	static const size_t BUILTIN_INSTRUCTION_COUNT = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);

	// Helper function to check if an identifier is a built-in instruction
	inline bool isBuiltInInstruction(const char* name) {
		for (size_t i = 0; i < BUILTIN_INSTRUCTION_COUNT; i++) {
			if (strcmp(name, BUILTIN_INSTRUCTIONS[i]) == 0) {
				return true;
			}
		}
		return false;
	}

	// Alias for semantic validation (currently same as built-in check)
	inline bool isKnownInstruction(const char* name) {
		return isBuiltInInstruction(name);
	}
}

#endif // QD_QC_INSTRUCTIONS_H
