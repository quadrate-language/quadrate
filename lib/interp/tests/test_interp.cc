#include <algorithm>
#include <cstring>
#include <quadrate/interp/interp.h>
#include <string>
#include <unit-check/uc.h>
#include <vector>

namespace {

	/** Evaluate and return the rendered top of stack. */
	const char* top(qd_interp* interp, const char* source) {
		static qd_interp_value value;
		if (!qd_interp_eval(interp, source)) {
			return "<error>";
		}
		if (!qd_interp_peek(interp, 0, &value)) {
			return "<empty>";
		}
		return value.text;
	}

} // namespace

TEST(Arithmetic) {
	qd_interp* interp = qd_interp_create(256);
	ASSERT(interp != nullptr, "interpreter should be created");

	ASSERT(std::strcmp(top(interp, "2 3 +"), "5") == 0, "2 3 + is 5");
	ASSERT(std::strcmp(top(interp, "clear 10 4 -"), "6") == 0, "10 4 - is 6");
	ASSERT(std::strcmp(top(interp, "clear 6 7 *"), "42") == 0, "6 7 * is 42");
	ASSERT(std::strcmp(top(interp, "clear 84 2 /"), "42") == 0, "84 2 / is 42");
	ASSERT(std::strcmp(top(interp, "clear 17 5 %"), "2") == 0, "17 5 % is 2");
	ASSERT(std::strcmp(top(interp, "clear 5 neg"), "-5") == 0, "5 neg is -5");

	qd_interp_destroy(interp);
}

TEST(Floats) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "3.5 2.0 *"), "7") == 0, "3.5 2.0 * is 7");
	ASSERT(std::strcmp(top(interp, "clear 1.0 4.0 /"), "0.25") == 0, "1.0 4.0 / is 0.25");

	qd_interp_destroy(interp);
}

TEST(StackOperations) {
	qd_interp* interp = qd_interp_create(256);

	// rot brings the third element up: 1 2 3 becomes 2 3 1
	ASSERT(std::strcmp(top(interp, "1 2 3 rot"), "1") == 0, "rot leaves 1 on top");
	ASSERT(qd_interp_depth(interp) == 3, "rot preserves depth");

	ASSERT(std::strcmp(top(interp, "clear 7 dup *"), "49") == 0, "dup then multiply squares");
	ASSERT(std::strcmp(top(interp, "clear 1 2 swap"), "1") == 0, "swap exchanges the top two");
	ASSERT(std::strcmp(top(interp, "clear 1 2 drop"), "1") == 0, "drop removes the top");
	ASSERT(std::strcmp(top(interp, "clear 1 2 over"), "1") == 0, "over copies the second up");

	qd_interp_destroy(interp);
}

TEST(StackPersistsAcrossCalls) {
	qd_interp* interp = qd_interp_create(256);

	// The property a prompt depends on: one call's values outlive it
	ASSERT(qd_interp_eval(interp, "2"), "push 2");
	ASSERT(qd_interp_eval(interp, "3"), "push 3");
	ASSERT(qd_interp_depth(interp) == 2, "both values are on the stack");
	ASSERT(std::strcmp(top(interp, "+"), "5") == 0, "a later call consumes them");
	ASSERT(qd_interp_depth(interp) == 1, "leaving one result");

	qd_interp_destroy(interp);
}

TEST(PeekIndexesFromTheTop) {
	qd_interp* interp = qd_interp_create(256);
	qd_interp_value value;

	ASSERT(qd_interp_eval(interp, "10 20 30"), "push three values");

	ASSERT(qd_interp_peek(interp, 0, &value), "index 0 is in range");
	ASSERT(std::strcmp(value.text, "30") == 0, "index 0 is the top");
	ASSERT(value.type == QD_INTERP_VALUE_INT, "and is an integer");
	ASSERT(value.i == 30, "with the right value");

	ASSERT(qd_interp_peek(interp, 2, &value), "index 2 is in range");
	ASSERT(std::strcmp(value.text, "10") == 0, "index 2 is the bottom");

	ASSERT(!qd_interp_peek(interp, 3, &value), "index 3 is out of range");

	qd_interp_destroy(interp);
}

TEST(Comparison) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "3 4 <"), "1") == 0, "3 < 4 is true");
	ASSERT(std::strcmp(top(interp, "clear 4 3 <"), "0") == 0, "4 < 3 is false");
	ASSERT(std::strcmp(top(interp, "clear 5 5 =="), "1") == 0, "5 == 5 is true");

	qd_interp_destroy(interp);
}

TEST(SyntaxErrorsAreReported) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "2 +++++ @@@"), "malformed source fails");
	ASSERT(qd_interp_error(interp)[0] != '\0', "with a message");

	qd_interp_destroy(interp);
}

TEST(UnsupportedConstructsNameThemselves) {
	qd_interp* interp = qd_interp_create(256);

	// A real instruction that only means something in generated code. The
	// message must name it, so the caller learns what is missing.
	ASSERT(!qd_interp_eval(interp, "1 __hlt"), "freestanding instruction is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "__hlt") != nullptr, "message names the instruction");

	qd_interp_destroy(interp);
}

TEST(UnderflowIsRefusedNotFatal) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "+"), "add with an empty stack fails");
	ASSERT(std::strstr(qd_interp_error(interp), "needs 2 values") != nullptr, "message explains the arity");

	ASSERT(!qd_interp_eval(interp, "1 +"), "add with one value fails");
	ASSERT(qd_interp_depth(interp) == 1, "the value is still there");

	qd_interp_destroy(interp);
}

TEST(CastIsTotalSoStringsAreRefused) {
	qd_interp* interp = qd_interp_create(256);

	// 'cast' must always produce a value, so the one direction that can fail is not offered.
	// It used to answer 0, which is also what "0" parses to, so a failure was undetectable.
	ASSERT(!qd_interp_eval(interp, "clear \"notanumber\" cast<i64>"), "string to int is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "cannot be cast to an integer") != nullptr, "and says why");
	ASSERT(std::strstr(qd_interp_error(interp), "strconv") != nullptr, "and points at strconv");

	ASSERT(!qd_interp_eval(interp, "clear \"1.5\" cast<f64>"), "string to float is refused too");
	ASSERT(std::strstr(qd_interp_error(interp), "cannot be cast to a float") != nullptr, "with the float wording");

	// A well-formed numeral is refused just the same: the point is that the operation has no
	// way to report failure, not that this particular string would have failed.
	ASSERT(!qd_interp_eval(interp, "clear \"42\" cast<i64>"), "a parseable string is refused as well");

	// The total directions are untouched.
	ASSERT(std::strcmp(top(interp, "clear 3.9 cast<i64>"), "3") == 0, "float to int still truncates");
	ASSERT(std::strcmp(top(interp, "clear 7 cast<f64>"), "7") == 0, "int to float still works");
	ASSERT(std::strcmp(top(interp, "clear 42 cast<str>"), "\"42\"") == 0, "any to string still works");

	qd_interp_destroy(interp);
}

TEST(FatalRuntimeErrorsAreRecovered) {
	// Each of these called _exit(1) before the runtime grew a recovery mode.
	// They are the reason interpreted code can be typed by a person.
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "1 0 /"), "integer division by zero fails");
	ASSERT(std::strstr(qd_interp_error(interp), "Division by zero") != nullptr, "with the runtime's message");

	// Float division by zero is NOT one of these: IEEE 754 defines it as +/-infinity,
	// and 0.0/0.0 as NaN. It used to abort here while the compiled path -- a bare fdiv --
	// returned infinity for the same expression.
	ASSERT(std::strcmp(top(interp, "clear 1.0 0.0 /"), "inf") == 0, "float division by zero is +infinity");
	ASSERT(std::strcmp(top(interp, "clear -1.0 0.0 /"), "-inf") == 0, "negative numerator gives -infinity");
	ASSERT(qd_interp_eval(interp, "clear 0.0 0.0 /"), "0.0/0.0 is NaN, not an error");

	ASSERT(!qd_interp_eval(interp, "clear \"abc\" 1 +"), "adding a string to an int fails");
	ASSERT(std::strstr(qd_interp_error(interp), "Type error") != nullptr, "with a type error");

	// pick takes its depth from a value popped at run time, so the arity check
	// cannot see this coming — only recovery catches it
	ASSERT(!qd_interp_eval(interp, "clear 1 5 pick"), "out-of-range pick fails");
	ASSERT(std::strstr(qd_interp_error(interp), "out of range") != nullptr, "with a range error");

	// And the interpreter still works afterwards
	ASSERT(std::strcmp(top(interp, "clear 2 3 +"), "5") == 0, "the context survives recovery");

	qd_interp_destroy(interp);
}

TEST(AttachSharesAContext) {
	qd_context* ctx = qd_create_context(256);
	ASSERT(ctx != nullptr, "context should be created");

	qd_interp* interp = qd_interp_attach(ctx);
	ASSERT(interp != nullptr, "interpreter should attach");
	ASSERT(qd_interp_context(interp) == ctx, "and report the borrowed context");

	// A value pushed through the runtime API is visible to interpreted code
	ASSERT(qd_push_i(ctx, 40) == 0, "push through the runtime API");
	ASSERT(std::strcmp(top(interp, "2 +"), "42") == 0, "interpreted code sees it");

	// Destroying a borrowing interpreter must leave the context alive
	qd_interp_destroy(interp);
	ASSERT(qd_stack_size(ctx->st) == 1, "the context outlives the interpreter");
	qd_free_context(ctx);
}

TEST(NullAndEmptyInputs) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, nullptr), "null source fails");
	ASSERT(qd_interp_eval(interp, ""), "empty source succeeds");
	ASSERT(qd_interp_depth(interp) == 0, "and does nothing");

	ASSERT(!qd_interp_eval(nullptr, "1"), "null interpreter fails");
	ASSERT(qd_interp_depth(nullptr) == 0, "null interpreter has no depth");
	ASSERT(qd_interp_context(nullptr) == nullptr, "null interpreter has no context");
	ASSERT(!qd_interp_attach(nullptr), "attaching a null context fails");

	qd_interp_destroy(interp);
	qd_interp_destroy(nullptr); // must not crash
}

namespace {

	// A native that pushes a constant, to prove registration reaches the stack
	int nativeAnswer(qd_context* ctx, void* userdata) {
		(void)userdata;
		return qd_push_i(ctx, 42);
	}

	// A native that consumes two values, to exercise the arity guard
	int nativeSum(qd_context* ctx, void* userdata) {
		(void)userdata;
		int64_t a = 0;
		int64_t b = 0;
		if (qd_pop_i(ctx, &b) != 0 || qd_pop_i(ctx, &a) != 0) {
			return 1;
		}
		return qd_push_i(ctx, a + b);
	}

	// A native that reports failure, to prove errors surface
	int nativeFails(qd_context* ctx, void* userdata) {
		(void)userdata;
		qd_set_error_msg(ctx, "the hardware said no");
		return 1;
	}

	// A native that reads its userdata
	int nativeFromUserdata(qd_context* ctx, void* userdata) {
		return qd_push_i(ctx, *static_cast<const int64_t*>(userdata));
	}

} // namespace

TEST(RegisteredFunctionIsCallable) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(qd_interp_register(interp, "answer", "( -- v:i64)", nativeAnswer, nullptr), "registration succeeds");
	ASSERT(qd_interp_registered_count(interp) == 1, "one function registered");

	ASSERT(std::strcmp(top(interp, "answer"), "42") == 0, "the native ran and pushed its result");

	// And composes with the rest of the language
	ASSERT(std::strcmp(top(interp, "clear answer answer +"), "84") == 0, "natives compose with builtins");

	qd_interp_destroy(interp);
}

TEST(RegisteredFunctionCanBeScoped) {
	qd_interp* interp = qd_interp_create(256);

	// The scoped spelling is how an embedder groups its capabilities
	ASSERT(qd_interp_register(interp, "hw::answer", "( -- v:i64)", nativeAnswer, nullptr), "scoped registration");
	ASSERT(std::strcmp(top(interp, "hw::answer"), "42") == 0, "scoped name resolves");

	// The unscoped spelling of the same leaf name must not resolve
	ASSERT(!qd_interp_eval(interp, "answer"), "unscoped name is not defined");

	qd_interp_destroy(interp);
}

TEST(RegisteredFunctionConsumesStack) {
	qd_interp* interp = qd_interp_create(256);
	qd_interp_register(interp, "sum2", "(a:i64 b:i64 -- r:i64)", nativeSum, nullptr);

	ASSERT(std::strcmp(top(interp, "20 22 sum2"), "42") == 0, "native consumed both values");
	ASSERT(qd_interp_depth(interp) == 1, "leaving one result");

	qd_interp_destroy(interp);
}

TEST(RegisteredFunctionArityIsChecked) {
	qd_interp* interp = qd_interp_create(256);
	qd_interp_register(interp, "sum2", "(a:i64 b:i64 -- r:i64)", nativeSum, nullptr);

	// The signature says two inputs, so one value on the stack must be refused
	// before the native runs rather than letting it underflow
	ASSERT(!qd_interp_eval(interp, "clear 1 sum2"), "too few values is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "needs 2 values") != nullptr, "message explains the arity");
	ASSERT(qd_interp_depth(interp) == 1, "and the stack is untouched");

	qd_interp_destroy(interp);
}

TEST(RegisteredFunctionErrorsSurface) {
	qd_interp* interp = qd_interp_create(256);
	qd_interp_register(interp, "beep", "( -- )", nativeFails, nullptr);

	ASSERT(!qd_interp_eval(interp, "beep"), "a failing native fails the evaluation");
	ASSERT(std::strstr(qd_interp_error(interp), "the hardware said no") != nullptr, "its message reaches the caller");

	qd_interp_destroy(interp);
}

TEST(RegisteredFunctionReceivesUserdata) {
	qd_interp* interp = qd_interp_create(256);
	int64_t value = 7;
	qd_interp_register(interp, "cfg", "( -- v:i64)", nativeFromUserdata, &value);

	ASSERT(std::strcmp(top(interp, "cfg"), "7") == 0, "userdata reached the native");

	value = 9;
	ASSERT(std::strcmp(top(interp, "clear cfg"), "9") == 0, "and is read at call time, not registration");

	qd_interp_destroy(interp);
}

TEST(RegisteringTwiceReplaces) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_register(interp, "f", "( -- v:i64)", nativeAnswer, nullptr);
	int64_t value = 5;
	qd_interp_register(interp, "f", "( -- v:i64)", nativeFromUserdata, &value);

	ASSERT(qd_interp_registered_count(interp) == 1, "still one entry");
	ASSERT(std::strcmp(top(interp, "f"), "5") == 0, "the later registration wins");

	qd_interp_destroy(interp);
}

TEST(UndefinedNameIsReported) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "no_such_word"), "an unregistered name fails");
	ASSERT(std::strstr(qd_interp_error(interp), "no_such_word") != nullptr, "the message names it");
	ASSERT(std::strstr(qd_interp_error(interp), "not defined") != nullptr, "and says what is wrong");

	qd_interp_destroy(interp);
}

TEST(RegisterRejectsBadInput) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_register(nullptr, "f", "( -- )", nativeAnswer, nullptr), "null interpreter");
	ASSERT(!qd_interp_register(interp, nullptr, "( -- )", nativeAnswer, nullptr), "null name");
	ASSERT(!qd_interp_register(interp, "", "( -- )", nativeAnswer, nullptr), "empty name");
	ASSERT(!qd_interp_register(interp, "f", "( -- )", nullptr, nullptr), "null function");
	ASSERT(qd_interp_registered_count(interp) == 0, "nothing was registered");
	ASSERT(qd_interp_registered_count(nullptr) == 0, "null interpreter has no registrations");

	// A signature that cannot be read disables the arity check rather than
	// refusing the registration
	ASSERT(qd_interp_register(interp, "g", nullptr, nativeAnswer, nullptr), "null signature is accepted");
	ASSERT(std::strcmp(top(interp, "g"), "42") == 0, "and the function still works");

	qd_interp_destroy(interp);
}

TEST(StringLiteralsDropTheirQuotes) {
	qd_interp* interp = qd_interp_create(256);

	// peek renders a string quoted, so a correctly stored "kept" reads back as
	// "kept" and a doubly quoted one would read as ""kept""
	ASSERT(std::strcmp(top(interp, "\"kept\""), "\"kept\"") == 0, "quotes are not part of the value");
	ASSERT(std::strcmp(top(interp, "clear \"\""), "\"\"") == 0, "an empty string stays empty");

	qd_interp_destroy(interp);
}

TEST(StringLiteralEscapes) {
	// The compiled tier decodes these in generator_nodes.cc; interpreted source
	// must mean the same thing
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "\"a\\nb\""), "\"a\nb\"") == 0, "newline");
	ASSERT(std::strcmp(top(interp, "clear \"a\\tb\""), "\"a\tb\"") == 0, "tab");
	ASSERT(std::strcmp(top(interp, "clear \"a\\\\b\""), "\"a\\b\"") == 0, "backslash");
	ASSERT(std::strcmp(top(interp, "clear \"a\\\"b\""), "\"a\"b\"") == 0, "escaped quote");
	ASSERT(std::strcmp(top(interp, "clear \"a\\x41b\""), "\"aAb\"") == 0, "hex escape");
	ASSERT(std::strcmp(top(interp, "clear \"a\\x4Zb\""), "\"a\\x4Zb\"") == 0, "invalid hex is kept verbatim");
	ASSERT(std::strcmp(top(interp, "clear \"a\\qb\""), "\"a\\qb\"") == 0, "unknown escape is kept verbatim");

	qd_interp_destroy(interp);
}

TEST(Conditionals) {
	qd_interp* interp = qd_interp_create(256);

	// The condition comes off the stack, as everything else does
	ASSERT(std::strcmp(top(interp, "1 if { 42 }"), "42") == 0, "a true condition runs the block");
	ASSERT(qd_interp_eval(interp, "clear 0 if { 42 }"), "a false condition succeeds");
	ASSERT(qd_interp_depth(interp) == 0, "and runs nothing");

	ASSERT(std::strcmp(top(interp, "clear 1 if { 1 } else { 2 }"), "1") == 0, "true takes the first branch");
	ASSERT(std::strcmp(top(interp, "clear 0 if { 1 } else { 2 }"), "2") == 0, "false takes the else branch");

	// Any non-zero is true
	ASSERT(std::strcmp(top(interp, "clear -1 if { 7 } else { 8 }"), "7") == 0, "negative is true");

	ASSERT(std::strcmp(top(interp, "clear 1 if { 1 if { 5 } }"), "5") == 0, "conditionals nest");

	qd_interp_destroy(interp);
}

TEST(ConditionalNeedsACondition) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "if { 1 }"), "an empty stack is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "condition") != nullptr, "and says why");

	qd_interp_destroy(interp);
}

TEST(Loops) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "0 loop { 1 + dup 5 >= if { break } }"), "5") == 0, "break leaves the loop");
	ASSERT(qd_interp_depth(interp) == 1, "and the stack is what the body left");

	// continue skips the rest of the body, break still ends it
	ASSERT(std::strcmp(top(interp, "clear 0 loop { 1 + dup 3 == if { continue } dup 6 >= if { break } }"), "6") == 0,
			"continue resumes the loop");

	// A break belongs to its own loop, not an outer one
	ASSERT(std::strcmp(top(interp, "clear 0 loop { 1 + dup 3 >= if { break } } 100 +"), "103") == 0,
			"execution resumes after the loop");

	qd_interp_destroy(interp);
}

TEST(Switch) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "2 switch { 1 { 10 } 2 { 20 } _ { 0 } }"), "20") == 0, "the matching case runs");

	ASSERT(std::strcmp(top(interp, "clear 9 switch { 1 { 10 } _ { 99 } }"), "99") == 0,
			"'_' catches what nothing else does");

	// The value is spent either way, as 'if' spends its condition
	ASSERT(qd_interp_eval(interp, "clear 7 switch { 1 { 10 } }"), "no case and no '_' is not an error");
	ASSERT(qd_interp_depth(interp) == 0, "and the value is gone");

	ASSERT(std::strcmp(top(interp, "clear \"b\" switch { \"a\" { 1 } \"b\" { 2 } _ { 0 } }"), "2") == 0,
			"a string matches a string case");

	ASSERT(std::strcmp(top(interp, "clear 1 switch { \"1\" { 10 } _ { 0 } }"), "0") == 0,
			"a case of another type is not a match");

	ASSERT(std::strcmp(top(interp, "clear 3 switch { 1 { 10 } _ { 5 } } 100 +"), "105") == 0,
			"execution resumes after the switch");

	ASSERT(!qd_interp_eval(interp, "clear switch { 1 { 10 } }"), "an empty stack is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "value") != nullptr, "and says why");

	qd_interp_destroy(interp);
}

TEST(Return) {
	qd_interp* interp = qd_interp_create(256);

	// At the top level a return ends the line, like a break does
	ASSERT(qd_interp_eval(interp, "1 return 2"), "a top-level return succeeds");
	ASSERT(qd_interp_depth(interp) == 1, "so the 2 is never pushed");

	// The guard clause the absence of 'return' used to cost a level of nesting
	qd_interp_eval(interp, "fn clamp(n:i64 -- r:i64) { n 0 < if { 0 return } n 2 * }");
	ASSERT(std::strcmp(top(interp, "clear -5 clamp"), "0") == 0, "the guard returns early");
	ASSERT(std::strcmp(top(interp, "clear 5 clamp"), "10") == 0, "and the rest runs otherwise");

	// A return leaves the loop it is written in, and the function with it
	qd_interp_eval(interp, "fn first_over(n:i64 -- r:i64) { n loop { 1 + dup 10 > if { return } } }");
	ASSERT(std::strcmp(top(interp, "clear 5 first_over"), "11") == 0, "return escapes a loop");

	// But not the call: the caller carries on
	qd_interp_eval(interp, "fn seven( -- r:i64) { 7 return 9 }");
	ASSERT(std::strcmp(top(interp, "clear seven 100 +"), "107") == 0, "the caller resumes");

	qd_interp_destroy(interp);
}

TEST(Constants) {
	qd_interp* interp = qd_interp_create(256);

	// A const used to parse and then report its own name as undefined
	ASSERT(qd_interp_eval(interp, "const Fire = 0xa3"), "a constant can be declared");
	ASSERT(qd_interp_depth(interp) == 0, "declaring runs nothing");
	ASSERT(std::strcmp(top(interp, "Fire"), "163") == 0, "and the name resolves to its value");

	ASSERT(qd_interp_eval(interp, "const Half = 0.5"), "a float constant");
	ASSERT(std::strcmp(top(interp, "clear Half"), "0.5") == 0, "keeps its type");

	ASSERT(qd_interp_eval(interp, "const Greeting = \"hi\""), "a string constant");
	ASSERT(std::strcmp(top(interp, "clear Greeting"), "\"hi\"") == 0, "likewise");

	// And composes with everything else
	ASSERT(std::strcmp(top(interp, "clear Fire 1 +"), "164") == 0, "constants are ordinary values");

	qd_interp_eval(interp, "const Limit = 10");
	const char* name = qd_interp_last_declared(interp);
	ASSERT(name != nullptr && std::strcmp(name, "Limit") == 0, "the declared name is reported");

	ASSERT(qd_interp_undeclare(interp, "Fire"), "a constant can be undeclared");
	ASSERT(!qd_interp_eval(interp, "clear Fire"), "and stops resolving");
	ASSERT(std::strstr(qd_interp_error(interp), "not defined") != nullptr, "with the usual message");

	qd_interp_destroy(interp);
}

TEST(Enums) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(qd_interp_eval(interp, "enum Color { Red, Green, Blue }"), "an enum can be declared");
	ASSERT(std::strcmp(top(interp, "Color::Red"), "0") == 0, "the first variant is 0");
	ASSERT(std::strcmp(top(interp, "clear Color::Blue"), "2") == 0, "and they count up");

	ASSERT(qd_interp_eval(interp, "enum Sig { Hup = 1, Int = 2, Kill = 9 }"), "explicit values");
	ASSERT(std::strcmp(top(interp, "clear Sig::Kill"), "9") == 0, "are used as written");

	// A switch over an enum is the reason to want one
	ASSERT(std::strcmp(top(interp, "clear Sig::Int switch { Sig::Hup { 10 } Sig::Int { 20 } _ { 0 } }"), "20") == 0,
			"an enum variant is a case label");

	ASSERT(qd_interp_undeclare(interp, "Color"), "an enum can be undeclared");
	ASSERT(!qd_interp_eval(interp, "clear Color::Red"), "and its variants go with it");

	qd_interp_destroy(interp);
}

TEST(NamedLocals) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "5 -> x x x +"), "10") == 0, "a name can be bound and read twice");
	ASSERT(qd_interp_depth(interp) == 1, "reading copies rather than moves");

	// Each arrow takes the top, so the bindings read in reverse
	ASSERT(qd_interp_eval(interp, "clear 1 2 3 -> a -> b -> c"), "three arrows bind three values");
	ASSERT(std::strcmp(top(interp, "a"), "3") == 0, "the top went to the first name");
	ASSERT(std::strcmp(top(interp, "clear b"), "2") == 0, "the next to the second");
	ASSERT(std::strcmp(top(interp, "clear c"), "1") == 0, "the bottom to the last");

	// '_' is the documented spelling of a discard
	ASSERT(qd_interp_eval(interp, "clear 1 2 -> _"), "a discard binds nothing");
	ASSERT(qd_interp_depth(interp) == 1, "and drops the value");

	ASSERT(std::strcmp(top(interp, "clear \"kept\" -> s s"), "\"kept\"") == 0, "a string binds too");
	ASSERT(std::strcmp(top(interp, "clear 1.5 -> f f"), "1.5") == 0, "and a float");

	ASSERT(!qd_interp_eval(interp, "clear -> a"), "binding with nothing on the stack is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "needs 1 value") != nullptr, "with the arity");

	qd_interp_destroy(interp);
}

TEST(NamedParameters) {
	qd_interp* interp = qd_interp_create(256);

	// All inputs named: they are bound on entry and leave the stack, which is
	// what the type checker models and what the compiled tier does
	qd_interp_eval(interp, "fn area(w:i64 h:i64 -- a:i64) { w h * }");
	ASSERT(std::strcmp(top(interp, "clear 3 4 area"), "12") == 0, "parameters bind by name");
	ASSERT(qd_interp_depth(interp) == 1, "and are consumed");

	// The last parameter is the one on top
	qd_interp_eval(interp, "fn difference(a:i64 b:i64 -- r:i64) { a b - }");
	ASSERT(std::strcmp(top(interp, "clear 10 3 difference"), "7") == 0, "in the order written");

	// An unnamed input stays on the stack for the body to read positionally
	qd_interp_eval(interp, "stack fn twice(i64 -- r:i64) { 2 * }");
	ASSERT(std::strcmp(top(interp, "clear 5 twice"), "10") == 0, "an unnamed parameter stays on the stack");

	// A frame belongs to its call: the caller's names are not visible inside it
	qd_interp_eval(interp, "fn peek( -- r:i64) { outer }");
	ASSERT(!qd_interp_eval(interp, "clear 1 -> outer peek"), "a callee cannot see the caller's names");
	ASSERT(std::strstr(qd_interp_error(interp), "not defined") != nullptr, "the name means nothing there");
	ASSERT(std::strcmp(top(interp, "clear 7 -> outer 3 4 area drop outer"), "7") == 0,
			"and the caller's binding survives the call");

	ASSERT(!qd_interp_eval(interp, "clear 1 area"), "too few arguments is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "needs 2 values") != nullptr, "with the arity");

	qd_interp_destroy(interp);
}

TEST(ForLoops) {
	qd_interp* interp = qd_interp_create(256);

	// start end step, with the end exclusive
	ASSERT(std::strcmp(top(interp, "0 0 5 1 for i { i + }"), "10") == 0, "0..5 sums to 10");
	ASSERT(std::strcmp(top(interp, "clear 0 0 10 2 for i { i + }"), "20") == 0, "a step of 2 skips");
	ASSERT(std::strcmp(top(interp, "clear 0 5 0 -1 for i { i + }"), "15") == 0, "a negative step counts down");
	ASSERT(qd_interp_eval(interp, "clear 0 5 5 1 for i { i + }"), "an empty range runs nothing");
	ASSERT(std::strcmp(top(interp, ""), "0") == 0, "leaving the accumulator alone");

	ASSERT(std::strcmp(top(interp, "clear 0 0 10 1 for i { i 3 == if { break } i + }"), "3") == 0, "break leaves it");
	ASSERT(std::strcmp(top(interp, "clear 0 0 5 1 for i { i 2 == if { continue } i + }"), "8") == 0,
			"continue skips the rest of the body");

	// The iterator is an ordinary local, so loops nest
	ASSERT(std::strcmp(top(interp, "clear 0 0 3 1 for i { 0 3 1 for j { i j * + } }"), "9") == 0, "for loops nest");

	// A float start makes a float iterator
	ASSERT(std::strcmp(top(interp, "clear 0.0 0.0 2.0 0.5 for x { x + }"), "3") == 0, "a float loop steps by 0.5");

	ASSERT(!qd_interp_eval(interp, "clear 1 2 for i { }"), "too few bounds is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "start") != nullptr, "and says what is missing");

	ASSERT(!qd_interp_eval(interp, "clear \"a\" 2 1 for i { }"), "a non-numeric bound is refused");

	qd_interp_destroy(interp);
}

TEST(Casts) {
	qd_interp* interp = qd_interp_create(256);

	// A calculator mixes the two numeric types constantly
	ASSERT(std::strcmp(top(interp, "3.7 cast<i64>"), "3") == 0, "float to int truncates");
	ASSERT(std::strcmp(top(interp, "clear 3 cast<f64> 2.0 /"), "1.5") == 0, "int to float divides as a float");
	ASSERT(std::strcmp(top(interp, "clear 65 cast<str>"), "\"65\"") == 0, "int to string renders it");

	ASSERT(!qd_interp_eval(interp, "clear 1 cast"), "a bare cast is refused");
	ASSERT(std::strstr(qd_interp_error(interp), "cast<i64>") != nullptr, "and shows the spelling");

	qd_interp_destroy(interp);
}

TEST(ArrayLiterals) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(std::strcmp(top(interp, "[1 2 3] -> a a len"), "3") == 0, "a literal builds an array");
	ASSERT(std::strcmp(top(interp, "clear a 1 nth"), "2") == 0, "which indexes with nth");
	ASSERT(qd_interp_eval(interp, "clear a free"), "and frees");

	ASSERT(std::strcmp(top(interp, "clear [] len"), "0") == 0, "an empty literal has no elements");
	ASSERT(std::strcmp(top(interp, "clear [1.5 2.5] 0 nth"), "1.5") == 0, "a float array keeps its type");
	ASSERT(std::strcmp(top(interp, "clear [\"a\" \"b\"] 1 nth"), "\"b\"") == 0, "a string array too");

	// A dispatch table is the reason to want one
	ASSERT(std::strcmp(top(interp, "clear [10 20 30] -> t 0 0 3 1 for i { t i nth + }"), "60") == 0,
			"an array reads as data");

	qd_interp_destroy(interp);
}

TEST(RunawayLoopIsStopped) {
	// Nothing can interrupt a running evaluation on a calculator, so an
	// unbounded loop has to stop itself
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "loop { }"), "an empty infinite loop fails");
	ASSERT(std::strstr(qd_interp_error(interp), "execution limit") != nullptr, "with a message naming the cause");

	ASSERT(!qd_interp_eval(interp, "clear loop { 1 drop }"), "so does one with a body");

	// And the interpreter still works afterwards
	ASSERT(std::strcmp(top(interp, "clear 2 3 +"), "5") == 0, "the interpreter survives");

	qd_interp_destroy(interp);
}

TEST(StepLimitIsConfigurable) {
	qd_interp* interp = qd_interp_create(256);
	ASSERT(qd_interp_step_limit(interp) > 0, "there is a default limit");

	qd_interp_set_step_limit(interp, 100);
	ASSERT(qd_interp_step_limit(interp) == 100, "the limit can be lowered");
	ASSERT(!qd_interp_eval(interp, "0 loop { 1 + }"), "a tight limit stops sooner");

	// Enough headroom for real work
	qd_interp_set_step_limit(interp, 1000000);
	ASSERT(std::strcmp(top(interp, "clear 0 loop { 1 + dup 100 >= if { break } }"), "100") == 0,
			"a raised limit lets real loops finish");

	qd_interp_set_step_limit(nullptr, 10); // must not crash
	ASSERT(qd_interp_step_limit(nullptr) == 0, "a null interpreter has no limit");

	qd_interp_destroy(interp);
}

TEST(FlowDoesNotLeakBetweenCalls) {
	qd_interp* interp = qd_interp_create(256);

	// A break outside a loop ends the line, and must not affect the next one
	ASSERT(qd_interp_eval(interp, "1 break 2"), "a stray break ends the line");
	ASSERT(qd_interp_depth(interp) == 1, "so the 2 is never pushed");
	ASSERT(std::strcmp(top(interp, "10 +"), "11") == 0, "the next call runs normally");

	qd_interp_destroy(interp);
}

TEST(DeclaredFunctions) {
	qd_interp* interp = qd_interp_create(256);

	// A declaration defines; it does not run
	ASSERT(qd_interp_eval(interp, "fn double(x:i64 -- r:i64) { x 2 * }"), "a function can be declared");
	ASSERT(qd_interp_depth(interp) == 0, "declaring runs nothing");

	ASSERT(std::strcmp(top(interp, "5 double"), "10") == 0, "and can then be called");
	ASSERT(std::strcmp(top(interp, "clear 21 double double"), "84") == 0, "calls compose");

	qd_interp_destroy(interp);
}

static bool collect_word(const char* name, void* userdata) {
	static_cast<std::vector<std::string>*>(userdata)->emplace_back(name);
	return true;
}

static bool has(const std::vector<std::string>& words, const char* name) {
	return std::find(words.begin(), words.end(), name) != words.end();
}

TEST(VisitWords) {
	qd_interp* interp = qd_interp_create(256);
	qd_interp_register(interp, "mynative", "( -- )", [](qd_context*, void*) { return 0; }, nullptr);
	qd_interp_eval(interp, "fn mine( -- r:i64) { 7 }");

	std::vector<std::string> words;
	qd_interp_visit_words(interp, collect_word, &words);

	ASSERT(has(words, "dup"), "builtins are visited");
	ASSERT(has(words, "mynative"), "registered natives are visited");
	ASSERT(has(words, "mine"), "declarations are visited");

	qd_interp_destroy(interp);
}

TEST(VisitWordsStops) {
	qd_interp* interp = qd_interp_create(256);

	int seen = 0;
	qd_interp_visit_words(
			interp,
			[](const char*, void* userdata) {
				(*static_cast<int*>(userdata))++;
				return false;
			},
			&seen);
	ASSERT(seen == 1, "returning false stops the walk");

	qd_interp_destroy(interp);
}

TEST(DeclarationAfterComment) {
	qd_interp* interp = qd_interp_create(256);

	// A stored program opens with a comment; it must still declare, not be
	// wrapped in an implicit main
	ASSERT(qd_interp_eval(interp, "// what it does\nfn f( -- r:i64) { 7 }"), "line comment first");
	ASSERT(qd_interp_eval(interp, "/* block */ fn g( -- r:i64) { 8 }"), "block comment first");
	ASSERT(std::strcmp(top(interp, "f g +"), "15") == 0, "both are callable");

	qd_interp_destroy(interp);
}

TEST(LastDeclared) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "1 2 +");
	ASSERT(qd_interp_last_declared(interp) == nullptr, "an expression declares nothing");

	qd_interp_eval(interp, "fn double(x:i64 -- r:i64) { x 2 * }");
	const char* name = qd_interp_last_declared(interp);
	ASSERT(name != nullptr && std::strcmp(name, "double") == 0, "the declared name is reported");

	qd_interp_eval(interp, "5 double");
	ASSERT(qd_interp_last_declared(interp) == nullptr, "and is cleared by the next eval");

	qd_interp_destroy(interp);
}

TEST(FunctionsCallFunctions) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn sq(x:i64 -- r:i64) { x x * }");
	qd_interp_eval(interp, "fn quad(x:i64 -- r:i64) { x sq sq }");

	ASSERT(std::strcmp(top(interp, "clear 3 quad"), "81") == 0, "a function may call another");

	qd_interp_destroy(interp);
}

TEST(FunctionsUseControlFlow) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn clamp(x:i64 -- r:i64) { x dup 100 > if { drop 100 } }");
	ASSERT(std::strcmp(top(interp, "clear 5 clamp"), "5") == 0, "below the limit passes through");
	ASSERT(std::strcmp(top(interp, "clear 500 clamp"), "100") == 0, "above it is clamped");

	qd_interp_destroy(interp);
}

TEST(FunctionsRecurse) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn countdown(n:i64 -- r:i64) { n dup 0 > if { 1 - countdown } }");
	ASSERT(std::strcmp(top(interp, "clear 5 countdown"), "0") == 0, "recursion terminates");

	qd_interp_destroy(interp);
}

TEST(RunawayRecursionIsStopped) {
	// Each level costs a C stack frame in the walk, so this has to be refused
	// rather than left to take the process down
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn forever(n:i64 -- r:i64) { n forever }");
	ASSERT(!qd_interp_eval(interp, "clear 1 forever"), "unbounded recursion fails");
	ASSERT(std::strstr(qd_interp_error(interp), "recursed too deeply") != nullptr, "and says why");

	ASSERT(std::strcmp(top(interp, "clear 2 3 +"), "5") == 0, "the interpreter survives");

	qd_interp_destroy(interp);
}

TEST(RedefiningAFunction) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn f(x:i64 -- r:i64) { x 1 + }");
	ASSERT(std::strcmp(top(interp, "clear 10 f"), "11") == 0, "the first definition applies");

	qd_interp_eval(interp, "fn f(x:i64 -- r:i64) { x 100 + }");
	ASSERT(std::strcmp(top(interp, "clear 10 f"), "110") == 0, "a later definition replaces it");

	qd_interp_destroy(interp);
}

TEST(DeclaredFunctionsShadowNatives) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_register(interp, "thing", "( -- v:i64)", nativeAnswer, nullptr);
	ASSERT(std::strcmp(top(interp, "thing"), "42") == 0, "the native answers first");

	// A program may deliberately replace a capability the host provided
	qd_interp_eval(interp, "fn thing( -- r:i64) { 7 }");
	ASSERT(std::strcmp(top(interp, "clear thing"), "7") == 0, "a declaration wins");

	qd_interp_destroy(interp);
}

TEST(DeclarationsSurviveTheParseThatMadeThem) {
	// The body belongs to the Ast that parsed it, so that Ast has to outlive
	// the call. Many later evaluations should not disturb it.
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn keep(x:i64 -- r:i64) { x 3 * }");
	for (int i = 0; i < 200; i++) {
		qd_interp_eval(interp, "clear 1 2 +");
	}
	ASSERT(std::strcmp(top(interp, "clear 5 keep"), "15") == 0, "the body is still valid");

	qd_interp_destroy(interp);
}

TEST(ExpressionsAndDeclarationsBothParse) {
	qd_interp* interp = qd_interp_create(256);

	// Which form the source takes decides how it is parsed; both must work, and
	// a malformed one of either kind must report its own error
	ASSERT(qd_interp_eval(interp, "1 2 +"), "an expression");
	ASSERT(qd_interp_eval(interp, "fn g( -- r:i64) { 1 }"), "a declaration");
	ASSERT(!qd_interp_eval(interp, "fn broken(("), "a malformed declaration fails");
	ASSERT(qd_interp_error(interp)[0] != '\0', "with a message");
	ASSERT(!qd_interp_eval(interp, "2 +++ @@@"), "a malformed expression fails");

	qd_interp_destroy(interp);
}

TEST(UndeclaringAFunction) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(qd_interp_declared_count(interp) == 0, "nothing declared yet");
	qd_interp_eval(interp, "fn sq(x:i64 -- r:i64) { x x * }");
	ASSERT(qd_interp_declared_count(interp) == 1, "one declaration");
	ASSERT(std::strcmp(top(interp, "7 sq"), "49") == 0, "it works");

	ASSERT(qd_interp_undeclare(interp, "sq"), "undeclaring reports success");
	ASSERT(qd_interp_declared_count(interp) == 0, "and the count drops");
	ASSERT(!qd_interp_eval(interp, "clear 7 sq"), "the name no longer resolves");
	ASSERT(std::strstr(qd_interp_error(interp), "not defined") != nullptr, "with the usual message");

	ASSERT(!qd_interp_undeclare(interp, "sq"), "undeclaring twice reports nothing to do");
	ASSERT(!qd_interp_undeclare(interp, "neverexisted"), "an unknown name likewise");
	ASSERT(!qd_interp_undeclare(nullptr, "sq"), "null interpreter");
	ASSERT(!qd_interp_undeclare(interp, nullptr), "null name");
	ASSERT(qd_interp_declared_count(nullptr) == 0, "null interpreter has no declarations");

	qd_interp_destroy(interp);
}

TEST(UndeclaringRestoresANative) {
	qd_interp* interp = qd_interp_create(256);

	qd_interp_register(interp, "thing", "( -- v:i64)", nativeAnswer, nullptr);
	qd_interp_eval(interp, "fn thing( -- r:i64) { 7 }");
	ASSERT(std::strcmp(top(interp, "thing"), "7") == 0, "the declaration shadows the native");

	// Removing the declaration uncovers what the host registered
	ASSERT(qd_interp_undeclare(interp, "thing"), "undeclared");
	ASSERT(std::strcmp(top(interp, "clear thing"), "42") == 0, "the native answers again");

	qd_interp_destroy(interp);
}

TEST(RedeclaringDoesNotAccumulate) {
	// Each declaration owns the parse its body lives in. Replacing one has to
	// release the previous parse, or an interpreter that is used for a while
	// grows without bound -- which is what a calculator is.
	qd_interp* interp = qd_interp_create(256);

	for (int i = 0; i < 500; i++) {
		ASSERT(qd_interp_eval(interp, "fn f(x:i64 -- r:i64) { x 2 * }") || false, "redeclare");
	}
	ASSERT(qd_interp_declared_count(interp) == 1, "500 redefinitions leave one declaration");
	ASSERT(std::strcmp(top(interp, "clear 21 f"), "42") == 0, "and the last one works");

	qd_interp_destroy(interp);
}

TEST(UndeclaredFunctionsStopBeingCallable) {
	// A body must not outlive its declaration: calling through a stale pointer
	// is the failure this ownership is meant to prevent
	qd_interp* interp = qd_interp_create(256);

	qd_interp_eval(interp, "fn outer(x:i64 -- r:i64) { x inner }");
	qd_interp_eval(interp, "fn inner(x:i64 -- r:i64) { x 3 * }");
	ASSERT(std::strcmp(top(interp, "clear 5 outer"), "15") == 0, "calls through");

	qd_interp_undeclare(interp, "inner");
	ASSERT(!qd_interp_eval(interp, "clear 5 outer"), "the caller now fails");
	ASSERT(std::strstr(qd_interp_error(interp), "inner") != nullptr, "naming the missing word");

	qd_interp_destroy(interp);
}

int main() {
	return UC_PrintResults();
}
