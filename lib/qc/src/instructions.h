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
			// Arithmetic instructions
			"add", "dec", "div", "inc", "mod", "mul", "neg", "sub",
			// Bitwise operations
			"and", "lnot", "not", "or", "shl", "shr", "xor",
			// Logical operations
			"eq", "gt", "gte", "lt", "lte", "neq", "within",
			// Stack operations
			"call", "clear", "depth", "drop", "dup", "dup2", "free", "len", "nip", "nth", "over", "pick", "roll", "rot",
			"swap",
			// Array operations
			"append", "make", "makef", "makei", "makep", "makes", "set",
			// Type casting and introspection
			"cast", "sizeof",
			// Raw memory stores/loads — lower directly to LLVM store/load.
			// Zero overhead (single instruction). Useful in freestanding mode
			// where mem::* (libc-backed) is unavailable.
			// Stores: (addr:i64 offset:i64 value:i64 -- )
			// Loads:  (addr:i64 offset:i64 -- value:i64) (zero-extended)
			"__ld8", "__ld16", "__ld32", "__ld64", "__st8", "__st16", "__st32", "__st64",
			// x86 I/O port operations — lower to inline asm (outb/inb etc.).
			// Freestanding-only, x86/x86_64 targets only. Internal: use sys module.
			// port_out: (port:i64 value:i64 --)   port_in: (port:i64 -- value:i64)
			"__port_in8", "__port_in16", "__port_in32", "__port_out8", "__port_out16", "__port_out32",
			// CPU control — lower to single inline asm. Freestanding-only.
			"__cli", "__hlt", "__sti",
			// I/O
			"nl", "print", "prints", "printsv", "printv", "read",
			// Threading
			"detach", "spawn", "wait",
			// Error handling
			"err", "panic"};

	static const size_t BUILTIN_INSTRUCTION_COUNT = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);

	// Builtins that used to exist and no longer do. Kept here only so their use
	// reports what happened instead of a generic "undefined identifier", the same
	// way the parser still recognises 'while'. The old stack effect is included
	// because it is the thing a reader porting old code needs to know.
	struct RemovedInstruction {
		const char* name;
		const char* effect;
	};

	static const RemovedInstruction REMOVED_INSTRUCTIONS[] = {
			{"drop2", "( a b -- )"},
			{"dupd", "( a b -- a a b )"},
			{"nipd", "( a b c -- a c )"},
			{"over2", "( a b c d -- a b c d a b )"},
			{"overd", "( a b c -- a b a c )"},
			{"swap2", "( a b c d -- c d a b )"},
			{"swapd", "( a b c -- b a c )"},
			{"tuck", "( a b -- b a b )"},
	};

	static const size_t REMOVED_INSTRUCTION_COUNT = sizeof(REMOVED_INSTRUCTIONS) / sizeof(REMOVED_INSTRUCTIONS[0]);

	// Returns the old stack effect if 'name' is a removed builtin, else nullptr.
	inline const char* removedInstructionEffect(const char* name) {
		for (size_t i = 0; i < REMOVED_INSTRUCTION_COUNT; i++) {
			if (strcmp(name, REMOVED_INSTRUCTIONS[i].name) == 0) {
				return REMOVED_INSTRUCTIONS[i].effect;
			}
		}
		return nullptr;
	}

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
