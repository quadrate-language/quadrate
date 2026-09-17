/**
 * @file test_math_register.c
 * @brief The generated registration table, exercised through the interpreter
 *
 * Covers the path an embedder takes: the compiler resolves math's words through
 * the archive, but the interpreter looks them up by name, so qd_math_register()
 * has to put them on the context first. Math stands in for every module here --
 * all sixteen tables come out of the same generator.
 */

#include <math.h>
#include <quadrate/interp/interp.h>
#include <quadrate/math/math.h>
#include <unit-check/uc.h>

// Top of stack as a double, after evaluating source
static double eval_top(qd_interp* interp, const char* source) {
	if (!qd_interp_eval(interp, source)) {
		return (double)NAN;
	}
	qd_interp_value value;
	if (!qd_interp_peek(interp, 0, &value)) {
		return (double)NAN;
	}
	return value.f;
}

TEST(WordsAreUnknownBeforeRegistering) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "0.0 math::sin"), "math::sin is not built in");

	qd_interp_destroy(interp);
}

TEST(RegisterMakesWordsResolvable) {
	qd_interp* interp = qd_interp_create(256);
	ASSERT(qd_math_register(qd_interp_context(interp)), "registration succeeds");

	ASSERT(fabs(eval_top(interp, "0.0 math::sin")) < 1e-9, "sin(0) is 0");
	ASSERT(fabs(eval_top(interp, "0.0 math::cos") - 1.0) < 1e-9, "cos(0) is 1");
	ASSERT(fabs(eval_top(interp, "9.0 math::sqrt") - 3.0) < 1e-9, "sqrt(9) is 3");
	ASSERT(fabs(eval_top(interp, "2.0 3.0 math::pow") - 8.0) < 1e-9, "pow(2,3) is 8");

	qd_interp_destroy(interp);
}

TEST(RegisterRejectsNullContext) {
	ASSERT(!qd_math_register(NULL), "a NULL context is refused");
}

TEST(TableCoversTheImportBlock) {
	qd_interp* interp = qd_interp_create(256);
	const size_t before = qd_native_count(qd_interp_context(interp));
	ASSERT(qd_math_register(qd_interp_context(interp)), "registration succeeds");
	const size_t after = qd_native_count(qd_interp_context(interp));

	// A count alone would not catch the generator dropping a declaration it
	// could not parse, so check the ends of the import block: sin opens it and
	// fac closes it. A partial walk loses one or the other.
	ASSERT(after - before > 1, "the table is not a single entry");
	ASSERT(qd_interp_eval(interp, "0.0 math::sin"), "the first word of the block is present");
	ASSERT(qd_interp_eval(interp, "5 math::fac"), "and so is the last");

	qd_interp_destroy(interp);
}

int main(void) {
	return UC_PrintResults();
}
