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

int main() {
	return UC_PrintResults();
}
