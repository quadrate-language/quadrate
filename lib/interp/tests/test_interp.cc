#include <cstring>
#include <quadrate/interp/interp.h>
#include <unit-check/uc.h>

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

TEST(FatalRuntimeErrorsAreRecovered) {
	// Each of these called _exit(1) before the runtime grew a recovery mode.
	// They are the reason interpreted code can be typed by a person.
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "1 0 /"), "integer division by zero fails");
	ASSERT(std::strstr(qd_interp_error(interp), "Division by zero") != nullptr, "with the runtime's message");

	ASSERT(!qd_interp_eval(interp, "clear 1.0 0.0 /"), "float division by zero fails");

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

int main() {
	return UC_PrintResults();
}
