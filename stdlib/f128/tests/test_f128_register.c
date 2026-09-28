/**
 * @file test_f128_register.c
 * @brief The generated registration table, exercised through the interpreter
 *
 * The interpreter resolves f128's words by name, so qd_f128_register() has to put them on
 * the context first. An f128 reads back through qd_interp_peek as its exact decimal text,
 * which is what these compare against.
 */

#include <quadrate/f128/f128.h>
#include <quadrate/interp/interp.h>
#include <string.h>
#include <unit-check/uc.h>

// Top of stack rendered, after evaluating source; "" when the source fails
static const char* eval_text(qd_interp* interp, const char* source, qd_interp_value* value) {
	if (!qd_interp_eval(interp, source) || !qd_interp_peek(interp, 0, value)) {
		return "";
	}
	return value->text;
}

TEST(WordsAreUnknownBeforeRegistering) {
	qd_interp* interp = qd_interp_create(256);

	ASSERT(!qd_interp_eval(interp, "2q f128::sqrt"), "f128::sqrt is not built in");

	qd_interp_destroy(interp);
}

TEST(RegisterMakesWordsResolvable) {
	qd_interp* interp = qd_interp_create(256);
	ASSERT(qd_f128_register(qd_interp_context(interp)), "registration succeeds");

	qd_interp_value value;
	ASSERT(strcmp(eval_text(interp, "2q f128::sqrt", &value), "1.414213562373095048801688724209698") == 0,
			"sqrt(2) to 34 digits");
	ASSERT(value.type == QD_INTERP_VALUE_F128, "and it reads back as an f128");
	ASSERT(strcmp(eval_text(interp, "f128::pi", &value), "3.141592653589793238462643383279503") == 0,
			"pi to 34 digits");
	ASSERT(strcmp(eval_text(interp, "2.5q f128::round_even", &value), "2") == 0, "ties go to even");
	ASSERT(strcmp(eval_text(interp, "0.1q 0.2q +", &value), "0.3") == 0, "the interpreter adds decimally too");

	qd_interp_destroy(interp);
}

TEST(RegisterRejectsNullContext) {
	ASSERT(!qd_f128_register(NULL), "a NULL context is refused");
}

TEST(TableCoversTheImportBlock) {
	qd_interp* interp = qd_interp_create(256);
	ASSERT(qd_f128_register(qd_interp_context(interp)), "registration succeeds");

	// The ends of the import block: sqrt opens it and nan closes it. A partial walk
	// loses one or the other.
	ASSERT(qd_interp_eval(interp, "2q f128::sqrt"), "the first word of the block is present");
	ASSERT(qd_interp_eval(interp, "f128::nan"), "and so is the last");

	qd_interp_destroy(interp);
}

int main(void) {
	return UC_PrintResults();
}
