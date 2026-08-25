#ifndef QD_QC_INSTRUCTIONS_H
#define QD_QC_INSTRUCTIONS_H

#include <cstring>
#include <string>

namespace Qd {
	// Built-in runtime instructions
	// These are instructions that are directly compiled into the runtime executable
	inline constexpr const char* BUILTIN_INSTRUCTIONS[] = {
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

	inline constexpr size_t BUILTIN_INSTRUCTION_COUNT = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);

	// Builtins that used to exist and no longer do. Kept here only so their use
	// reports what happened instead of a generic "undefined identifier", the same
	// way the parser still recognises 'while'. The old stack effect is included
	// because it is the thing a reader porting old code needs to know, and the
	// replacement spells out the exact rewrite: '->' binds top-of-stack first, so
	// the binding order is the reverse of the effect's left-to-right names and is
	// too easy to get subtly wrong by hand.
	struct RemovedInstruction {
		const char* name;
		const char* effect;
		const char* replacement;
	};

	inline constexpr RemovedInstruction REMOVED_INSTRUCTIONS[] = {
			{"drop2", "( a b -- )", "drop drop"},
			{"dupd", "( a b -- a a b )", "-> b -> a  a a b"},
			{"nipd", "( a b c -- a c )", "-> c -> b -> a  a c"},
			{"over2", "( a b c d -- a b c d a b )", "-> d -> c -> b -> a  a b c d a b"},
			{"overd", "( a b c -- a b a c )", "-> c -> b -> a  a b a c"},
			{"swap2", "( a b c d -- c d a b )", "-> d -> c -> b -> a  c d a b"},
			{"swapd", "( a b c -- b a c )", "-> c -> b -> a  b a c"},
			{"tuck", "( a b -- b a b )", "-> b -> a  b a b"},
	};

	inline constexpr size_t REMOVED_INSTRUCTION_COUNT = sizeof(REMOVED_INSTRUCTIONS) / sizeof(REMOVED_INSTRUCTIONS[0]);

	// Returns the entry for 'name' if it is a removed builtin, else nullptr.
	inline const RemovedInstruction* findRemovedInstruction(const char* name) {
		for (size_t i = 0; i < REMOVED_INSTRUCTION_COUNT; i++) {
			if (strcmp(name, REMOVED_INSTRUCTIONS[i].name) == 0) {
				return &REMOVED_INSTRUCTIONS[i];
			}
		}
		return nullptr;
	}

	// Builds the diagnostic for a removed builtin: what it was, and the exact
	// rewrite. Shared so every reporting site says the same thing.
	inline std::string removedInstructionMessage(const RemovedInstruction& removed) {
		return std::string("'") + removed.name + "' has been removed (it was " + removed.effect +
			   "); use named locals instead: '" + removed.replacement + "'";
	}

	// Keywords that used to exist and no longer do. Unlike the removed builtins
	// these are no longer reserved at all, so a bare use is an ordinary identifier
	// and only reaches here once name resolution has failed. Reporting the removal
	// beats the fuzzy matcher's guess at what the "typo" was meant to be.
	struct RemovedKeyword {
		const char* name;
		const char* message;
	};

	// Shared so the parser (which sees 'ctx {' before name resolution) and the
	// semantic validator (which sees a bare 'ctx') say exactly the same thing.
	inline constexpr const char* REMOVED_CTX_MESSAGE =
			"'ctx' has been removed; it ran the block on a copy of the stack and appended only the block's "
			"top value to the parent, so inlining the body is not equivalent - bind what the body needs "
			"with named locals and push the result explicitly";

	inline constexpr RemovedKeyword REMOVED_KEYWORDS[] = {
			{"ctx", REMOVED_CTX_MESSAGE},
			{"while", "'while' has been removed; use 'loop' with 'if'/'break' instead"},
	};

	inline constexpr size_t REMOVED_KEYWORD_COUNT = sizeof(REMOVED_KEYWORDS) / sizeof(REMOVED_KEYWORDS[0]);

	// Returns the removal message if 'name' is a removed keyword, else nullptr.
	inline const char* removedKeywordMessage(const char* name) {
		for (size_t i = 0; i < REMOVED_KEYWORD_COUNT; i++) {
			if (strcmp(name, REMOVED_KEYWORDS[i].name) == 0) {
				return REMOVED_KEYWORDS[i].message;
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
