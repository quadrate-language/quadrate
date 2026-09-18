// libunicode - Quadrate Standard Library - Unicode Module
//
// Operations on a single codepoint. `strings` answers the same questions about a whole
// string; both go through <quadrate/unicode/codepoint.h>, so the two layers cannot give
// different answers about the same character.
//
// This module used to be pure Quadrate implementing ASCII arithmetic -- to_upper was
// `c a - A +` -- which is correct for ASCII and a silent no-op above it. That was tolerable
// while `strings` was byte-based too, but once `strings` became properly UTF-8 the two
// disagreed: `"<e-acute>" strings::is_alpha` was true while `unicode::is_alpha` of the very
// same codepoint was false.
//
// Only the functions that were limited by that arithmetic moved here. The ones that are
// legitimately ASCII (hex digits, identifier characters, digit values) or pure encoding
// mechanics stay in unicode.qd, as do the character-code constants -- those are ordinary
// Unicode codepoints and needed no change.

#include <quadrate/unicode/unicode.h>
#include <quadrate/unicode/codepoint.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdio.h>
#include <stdlib.h>

// Pops one integer codepoint, aborting the way the other stdlib modules do on a type error.
static int64_t unicode_pop_cp(qd_context* ctx, const char* func_name) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in unicode::%s: Stack underflow\n", func_name);
		abort();
	}
	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in unicode::%s: Expected a character code, got type %d\n", func_name, elem.type);
		abort();
	}
	return elem.value.i;
}

// A codepoint is 0..0x10FFFF. Anything outside that is not a character, so every predicate
// answers false for it and both mappings return it unchanged -- the same answer the tables
// give for a character that simply has no mapping.
static int unicode_in_range(int64_t v) {
	return v >= 0 && v <= 0x10FFFF;
}

static int unicode_predicate(qd_context* ctx, const char* func_name, int (*test)(uint32_t)) {
	const int64_t value = unicode_pop_cp(ctx, func_name);
	const int result = unicode_in_range(value) ? test((uint32_t)value) : 0;
	qd_push_i(ctx, result ? 1 : 0);
	return (int){0};
}

static int unicode_mapping(qd_context* ctx, const char* func_name, uint32_t (*map)(uint32_t)) {
	const int64_t value = unicode_pop_cp(ctx, func_name);
	const int64_t result = unicode_in_range(value) ? (int64_t)map((uint32_t)value) : value;
	qd_push_i(ctx, result);
	return (int){0};
}

int usr_unicode_is_upper(qd_context* ctx) {
	return unicode_predicate(ctx, "is_upper", utf8_is_upper_cp);
}

int usr_unicode_is_lower(qd_context* ctx) {
	return unicode_predicate(ctx, "is_lower", utf8_is_lower_cp);
}

int usr_unicode_is_alpha(qd_context* ctx) {
	return unicode_predicate(ctx, "is_alpha", utf8_is_alpha_cp);
}

int usr_unicode_is_space(qd_context* ctx) {
	return unicode_predicate(ctx, "is_space", utf8_is_space_cp);
}

// Decimal digits only. Other scripts' digits are not interchangeable with these for parsing,
// so this stays exactly as narrow as it was; it is here to be provably the same test that
// strings::is_numeric applies, not because it needed widening.
int usr_unicode_is_digit(qd_context* ctx) {
	return unicode_predicate(ctx, "is_digit", utf8_is_digit_cp);
}

static int unicode_alnum(uint32_t cp) {
	return utf8_is_alpha_cp(cp) || utf8_is_digit_cp(cp);
}

int usr_unicode_is_alnum(qd_context* ctx) {
	return unicode_predicate(ctx, "is_alnum", unicode_alnum);
}

// C0 and C1 controls, plus delete. The ASCII version covered 0-31 and 127; C1 (0x80-0x9F) is
// the same category and was simply out of reach before.
static int unicode_control(uint32_t cp) {
	return cp < 0x20u || cp == 0x7Fu || (cp >= 0x80u && cp <= 0x9Fu);
}

int usr_unicode_is_control(qd_context* ctx) {
	return unicode_predicate(ctx, "is_control", unicode_control);
}

// Printable is the complement of control, which is what the ASCII version's "space through
// tilde" meant within its range. A space is printable; a newline is not.
static int unicode_print(uint32_t cp) {
	return !unicode_control(cp);
}

int usr_unicode_is_print(qd_context* ctx) {
	return unicode_predicate(ctx, "is_print", unicode_print);
}

// Punctuation and symbols: printable, and none of letter, digit or whitespace.
static int unicode_punct(uint32_t cp) {
	return unicode_print(cp) && !utf8_is_alpha_cp(cp) && !utf8_is_digit_cp(cp) && !utf8_is_space_cp(cp);
}

int usr_unicode_is_punct(qd_context* ctx) {
	return unicode_predicate(ctx, "is_punct", unicode_punct);
}

int usr_unicode_to_lower(qd_context* ctx) {
	return unicode_mapping(ctx, "to_lower", utf8_to_lower_cp);
}

int usr_unicode_to_upper(qd_context* ctx) {
	return unicode_mapping(ctx, "to_upper", utf8_to_upper_cp);
}
