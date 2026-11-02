#ifndef QD_QC_INSTRUCTIONS_H
#define QD_QC_INSTRUCTIONS_H

#include <cstring>
#include <string>

namespace Qd {
	// Built-in runtime instructions
	// These are instructions that are directly compiled into the runtime executable
	// Grouped by kind, starting with the comparison operators. The operators are spelled only
	// as symbols. The group comments sit between entries rather than directly after the brace:
	// clang-format 22 and 23 disagree irreconcilably about a comment in that position, so a file
	// containing one cannot satisfy both.
	inline constexpr const char* BUILTIN_INSTRUCTIONS[] = {"!=", "<", "<=", "==", ">", ">=",
			// Arithmetic operators
			"%", "*", "+", "++", "-", "--", "/",
			// Negation has no symbol of its own
			"neg",
			// Range test
			"within",
			// Logical operations on truth values: every non-zero value counts as true, so
			// `2 1 and` is 1. The bitwise forms live in the bits module.
			"and", "not", "or",
			// Bitwise operations, reached through the bits module: AND/OR/XOR/NOT would collide
			// with the logical words above, and the shifts are bitwise as much as they are. Internal:
			// use bits::and, bits::shl and friends.
			"__and", "__not", "__or", "__shl", "__shr", "__xor",
			// Stack operations
			"drop", "drop2", "dup", "dup2", "nip", "over", "pick", "rot", "rot4", "rrot", "rrot4", "swap", "tuck",
			// Combinators: run function values in the patterns stack code repeats. Typed by
			// following what each does on the modelled stack; lowered to locals, `for` and `call`
			// before code generation, with a function value written in place inlined. `call` is the
			// simplest, and runs one once.
			"all", "any", "bi", "bi_star", "both", "call", "dip", "dip2", "each", "filter", "find", "fold", "index",
			"keep", "keep2", "map", "scan", "times", "tri", "tri_star",
			// Arrays, and strings read as sequences of characters by len and nth. Creation is a
			// literal -- `[10]i64`, `[]i64` -- not an instruction; see REMOVED_INSTRUCTIONS below.
			"append", "len", "nth", "set",
			// Memory: a shallow copy of a struct or an array, and freeing one
			"clone", "free",
			// Type casting and introspection
			"cast", "sizeof",
			// Raw memory stores/loads — lower directly to LLVM store/load.
			// Zero overhead (single instruction). Useful in freestanding mode
			// where mem::* (libc-backed) is unavailable.
			// Stores: (addr:i64 offset:i64 value:i64 -- )
			// Loads:  (addr:i64 offset:i64 -- value:i64) (zero-extended)
			"__ld8", "__ld16", "__ld32", "__ld64", "__st8", "__st16", "__st32", "__st64",
			// x86 I/O port operations — lower to inline asm (outb/inb etc.), and named as
			// <sys/io.h> names them; the operands stay port first, as everywhere else here.
			// Freestanding-only, x86/x86_64 targets only. Internal: use sys module.
			// out: (port:i64 value:i64 --)   in: (port:i64 -- value:i64)
			"__inb", "__inw", "__inl", "__outb", "__outw", "__outl",
			// CPU control — lower to single inline asm. Freestanding-only.
			"__cli", "__hlt", "__sti",
			// I/O
			"dump", "nl", "print",
			// Error handling: a fallible call's status tested as a flag
			"err", "fail", "ok", "panic"};

	inline constexpr size_t BUILTIN_INSTRUCTION_COUNT = sizeof(BUILTIN_INSTRUCTIONS) / sizeof(BUILTIN_INSTRUCTIONS[0]);

	// Builtins that used to exist and no longer do. Kept here only so their use
	// reports what happened instead of a generic "undefined identifier". Only a name
	// nothing else answers to reaches here, so a removed name is free to be reused. The
	// old stack effect is included
	// because it is the thing a reader porting old code needs to know, and the
	// replacement spells out the exact rewrite: '->' binds top-of-stack first, so
	// the binding order is the reverse of the effect's left-to-right names and is
	// too easy to get subtly wrong by hand.
	struct RemovedInstruction {
		const char* name;
		const char* effect;
		const char* replacement;
		// The phrase introducing the replacement. Per-entry because not every removal is
		// answered by named locals: `read` is answered by a stdlib call.
		const char* advice = "use named locals instead";
	};

	inline constexpr RemovedInstruction REMOVED_INSTRUCTIONS[] = {
			{"add", "( a b -- sum )", "+", "operators are spelled as symbols"},
			{"dec", "( a -- a-1 )", "--", "operators are spelled as symbols"},
			{"detach", "( thread -- )", "t detach!",
					"threads are the thread module's; detach the Thread that thread::spawn gave"},
			{"div", "( a b -- quotient )", "/", "operators are spelled as symbols"},
			{"dupd", "( a b -- a a b )", "over swap", "write it with the stack words"},
			{"eq", "( a b -- flag )", "==", "operators are spelled as symbols"},
			{"gt", "( a b -- flag )", ">", "operators are spelled as symbols"},
			{"gte", "( a b -- flag )", ">=", "operators are spelled as symbols"},
			{"inc", "( a -- a+1 )", "++", "operators are spelled as symbols"},
			{"lnot", "( a -- b )", "not", "'not' is the logical negation now; 'bits::not' is the bitwise one"},
			{"lt", "( a b -- flag )", "<", "operators are spelled as symbols"},
			{"lte", "( a b -- flag )", "<=", "operators are spelled as symbols"},
			{"make", "( size -- arr )", "[N]T', and an empty one is '[]T",
					"array creation is a literal now, spelling the element type the way a signature does, so "
					"'N make<T>' is written"},
			{"makef", "( size -- arr )", "[N]f64", "array creation is a literal now, so 'N makef' is written"},
			{"makei", "( size -- arr )", "[N]i64", "array creation is a literal now, so 'N makei' is written"},
			{"makep", "( size -- arr )", "[N]ptr", "array creation is a literal now, so 'N makep' is written"},
			{"makes", "( size -- arr )", "[N]str", "array creation is a literal now, so 'N makes' is written"},
			{"mod", "( a b -- remainder )", "%", "operators are spelled as symbols"},
			{"mul", "( a b -- product )", "*", "operators are spelled as symbols"},
			{"neq", "( a b -- flag )", "!=", "operators are spelled as symbols"},
			{"nipd", "( a b c -- a c )", "swap drop", "write it with the stack words"},
			{"over2", "( a b c d -- a b c d a b )", "-> d -> c -> b -> a  a b c d a b"},
			{"overd", "( a b c -- a b a c )", "fn { over } dip", "write it with the stack words"},
			{"printv", "( val -- )", "dump", "it was renamed"},
			{"read", "( -- ... n )", "os::args -> args  args len -> argc",
					"the arguments are an array now; use os::args instead"},
			{"roll", "( ... n -- ... )", "-> c -> b -> a  b c a",
					"it took its depth at run time, which no static stack model can follow; name the "
					"values instead"},
			{"shl", "( a n -- result )", "bits::shl",
					"the shifts are in the bits module, with the other bitwise operations"},
			{"shr", "( a n -- result )", "bits::shr",
					"the shifts are in the bits module, with the other bitwise operations"},
			{"spawn", "( fn -- thread )", "use thread  &f thread::spawn! -> t", "threads are the thread module's"},
			{"sub", "( a b -- diff )", "-", "operators are spelled as symbols"},
			{"swap2", "( a b c d -- c d a b )", "-> d -> c -> b -> a  c d a b"},
			{"swapd", "( a b c -- b a c )", "fn { swap } dip", "write it with the stack words"},
			{"wait", "( thread -- )", "t join!",
					"threads are the thread module's; join the Thread that thread::spawn gave"},
			{"xor", "( a b -- c )", "bits::xor",
					"the bitwise operators moved to the bits module so 'and', 'or' and 'not' could take "
					"their logical meanings"},
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
		return std::string("'") + removed.name + "' has been removed (it was " + removed.effect + "); " +
			   removed.advice + ": '" + removed.replacement + "'";
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

	// Every keyword reference.def documents -- `fn`, `if`, `->`, `_` and the rest -- so the
	// tools that colour, complete or reserve keywords read the list the manual is made from
	inline constexpr const char* KEYWORDS[] = {
#define KEYWORD(name, description) #name,
#define BUILTIN(name, signature, description)
#include "reference.def"
#undef KEYWORD
#undef BUILTIN
	};

	inline bool isKeyword(const std::string& name) {
		for (const char* keyword : KEYWORDS) {
			if (name == keyword) {
				return true;
			}
		}
		return false;
	}

	// The combinators, with how many function values each takes from the top of the stack
	// and how many values below them
	struct Combinator {
		const char* name;
		size_t functionValues;
		size_t values;
	};

	inline constexpr Combinator COMBINATORS[] = {
			{"all", 1, 1},
			{"any", 1, 1},
			{"bi", 2, 1},
			{"bi_star", 2, 2},
			{"both", 1, 2},
			{"dip", 1, 1},
			{"dip2", 1, 2},
			{"each", 1, 1},
			{"filter", 1, 1},
			{"find", 1, 1},
			{"fold", 1, 2},
			{"index", 1, 1},
			{"keep", 1, 1},
			{"keep2", 1, 2},
			{"map", 1, 1},
			{"scan", 1, 2},
			{"times", 1, 1},
			{"tri", 3, 1},
			{"tri_star", 3, 3},
	};

	inline const Combinator* findCombinator(const std::string& name) {
		for (const Combinator& combinator : COMBINATORS) {
			if (name == combinator.name) {
				return &combinator;
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
}

#endif // QD_QC_INSTRUCTIONS_H
