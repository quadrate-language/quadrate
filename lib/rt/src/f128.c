/**
 * @file f128.c
 * @brief IEEE 754-2008 decimal128 values, on libmpdec
 *
 * libmpdec does the arithmetic. Every operation is carried out in the IEEE decimal128
 * context -- 34 digits, emax 6144, clamped, roundTiesToEven -- so each result is the
 * decimal128 value the standard says it is. The trigonometric functions, which libmpdec
 * does not have, are built here from its arithmetic at a higher working precision.
 */

#include <quadrate/rt/f128.h>

#include <limits.h>
#include <math.h>
#include <mpdecimal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f128_pi.h"

// Four words is 76 digits on a 64-bit host: room for a full decimal128 coefficient and
// for the double-width intermediate a product rounds from, so ordinary arithmetic never
// has to allocate beyond the box. libmpdec moves a value that outgrows it to the heap by
// itself, and mpd_del hands that back.
#define QDRT_F128_WORDS 4

struct qd_f128 {
	atomic_size_t refcount;
	mpd_t num;
	mpd_uint_t data[QDRT_F128_WORDS];
};

// Digits carried beyond decimal128's 34 while evaluating a function libmpdec lacks.
#define QDRT_GUARD_DIGITS 10
#define QDRT_WORK_PREC (34 + QDRT_GUARD_DIGITS)

// ---------------------------------------------------------------------------
// Boxes and contexts
// ---------------------------------------------------------------------------

static qd_f128_t* box_new(void) {
	qd_f128_t* v = malloc(sizeof *v);
	if (v == NULL) {
		return NULL;
	}
	atomic_init(&v->refcount, 1);
	v->num.flags = MPD_STATIC | MPD_STATIC_DATA;
	v->num.exp = 0;
	v->num.digits = 1;
	v->num.len = 1;
	v->num.alloc = QDRT_F128_WORDS;
	v->num.data = v->data;
	v->data[0] = 0;
	return v;
}

static void box_free(qd_f128_t* v) {
	mpd_del(&v->num);
	free(v);
}

// A result is only lost to running out of memory; every other condition an operation
// raises -- inexact, overflow, invalid -- is part of the answer it gives.
static qd_f128_t* box_finish(qd_f128_t* v, uint32_t status) {
	if (status & MPD_Malloc_error) {
		box_free(v);
		return NULL;
	}
	return v;
}

static void ctx128(mpd_context_t* ctx) {
	mpd_ieee_context(ctx, 128);
}

// Unbounded exponent range, so intermediates never overflow on the way to a result that
// fits; the final rounding to decimal128 is what decides overflow.
static void ctx_work(mpd_context_t* ctx, mpd_ssize_t prec) {
	mpd_maxcontext(ctx);
	ctx->prec = prec;
	ctx->round = MPD_ROUND_HALF_EVEN;
	ctx->traps = 0;
	ctx->status = 0;
	ctx->clamp = 0;
	ctx->allcr = 1;
}

// Round a working-precision value into a new decimal128 box. mpd_qfinalize rather than
// mpd_qplus, which would turn -0 into +0.
static qd_f128_t* box_from_work(const mpd_t* value, uint32_t status) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	mpd_qcopy(&r->num, value, &status);
	mpd_qfinalize(&r->num, &ctx, &status);
	return box_finish(r, status);
}

static qd_f128_t* box_special(uint8_t flags) {
	qd_f128_t* r = box_new();
	if (r != NULL) {
		mpd_setspecial(&r->num, flags & MPD_NEG, flags & MPD_SPECIAL);
	}
	return r;
}

qd_f128_t* qd_f128_retain(qd_f128_t* v) {
	if (v != NULL) {
		atomic_fetch_add_explicit(&v->refcount, 1, memory_order_relaxed);
	}
	return v;
}

void qd_f128_release(qd_f128_t* v) {
	if (v != NULL && atomic_fetch_sub_explicit(&v->refcount, 1, memory_order_acq_rel) == 1) {
		box_free(v);
	}
}

size_t qd_f128_refcount(const qd_f128_t* v) {
	return v != NULL ? atomic_load(&v->refcount) : 0;
}

// ---------------------------------------------------------------------------
// Conversions
// ---------------------------------------------------------------------------

qd_f128_t* qd_f128_parse(const char* text) {
	if (text == NULL) {
		return NULL;
	}
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	mpd_qset_string(&r->num, text, &ctx, &status);
	// A signaling NaN is a value no operation here produces or tests for, so reading one
	// would only hand the program something it cannot deal with.
	if ((status & MPD_Conversion_syntax) || mpd_issnan(&r->num)) {
		box_free(r);
		return NULL;
	}
	return box_finish(r, status);
}

qd_f128_t* qd_f128_from_i64(int64_t v) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	mpd_qset_i64(&r->num, v, &ctx, &status);
	return box_finish(r, status);
}

qd_f128_t* qd_f128_from_f64(double v) {
	if (isnan(v)) {
		return box_special((uint8_t)(MPD_NAN | (signbit(v) ? MPD_NEG : MPD_POS)));
	}
	if (isinf(v)) {
		return box_special((uint8_t)(MPD_INF | (v < 0 ? MPD_NEG : MPD_POS)));
	}

	// %.33e writes the binary value correctly rounded to 34 significant digits -- glibc and
	// musl both format exactly -- which is the conversion IEEE asks for.
	char digits[64];
	snprintf(digits, sizeof digits, "%.33e", v);

	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	mpd_t* exact = mpd_qnew();
	if (exact == NULL) {
		return NULL;
	}
	mpd_qset_string(exact, digits, &ctx, &status);

	// That spelling always carries 34 digits, so 1.0 would arrive as 1.000...000 and print
	// that way. Give it the quantum a literal would have had: no trailing fractional zeros,
	// and an integer written as an integer.
	mpd_t* tidy = mpd_qnew();
	if (tidy == NULL) {
		mpd_del(exact);
		return NULL;
	}
	mpd_qreduce(tidy, exact, &ctx, &status);
	if (tidy->exp > 0 && tidy->digits + tidy->exp <= ctx.prec) {
		mpd_qrescale(exact, tidy, 0, &ctx, &status);
	} else {
		mpd_qcopy(exact, tidy, &status);
	}
	qd_f128_t* r = box_from_work(exact, status);
	mpd_del(tidy);
	mpd_del(exact);
	return r;
}

int64_t qd_f128_to_i64(const qd_f128_t* v) {
	const mpd_t* a = &v->num;
	if (mpd_isnan(a)) {
		return 0;
	}
	const bool negative = mpd_isnegative(a);
	if (mpd_isinfinite(a) || mpd_adjexp(a) > 18) {
		return negative ? INT64_MIN : INT64_MAX;
	}
	mpd_context_t ctx;
	ctx_work(&ctx, 40);
	uint32_t status = 0;
	mpd_t* whole = mpd_qnew();
	if (whole == NULL) {
		return 0;
	}
	mpd_qtrunc(whole, a, &ctx, &status);
	status = 0;
	int64_t result = mpd_qget_i64(whole, &status);
	mpd_del(whole);
	if (status & MPD_Invalid_operation) {
		return negative ? INT64_MIN : INT64_MAX;
	}
	return result;
}

double qd_f128_to_f64(const qd_f128_t* v) {
	// Formatting and reading back is correctly rounded: at most 34 digits go in, and
	// strtod rounds whatever it reads to the nearest double.
	char text[QD_F128_STRING_MAX];
	qd_f128_format(v, text, sizeof text);
	return strtod(text, NULL);
}

size_t qd_f128_format(const qd_f128_t* v, char* buf, size_t size) {
	if (buf == NULL || size == 0) {
		return 0;
	}
	const mpd_t* a = &v->num;
	const char* special = NULL;
	if (mpd_isnan(a)) {
		special = mpd_isnegative(a) ? "-nan" : "nan";
	} else if (mpd_isinfinite(a)) {
		special = mpd_isnegative(a) ? "-inf" : "inf";
	}
	if (special != NULL) {
		int n = snprintf(buf, size, "%s", special);
		return n < 0 ? 0 : (size_t)n;
	}

	char* text = NULL;
	mpd_ssize_t len = mpd_to_sci_size(&text, a, 0);
	if (len < 0 || text == NULL) {
		buf[0] = '\0';
		return 0;
	}
	size_t n = (size_t)len < size ? (size_t)len : size - 1;
	memcpy(buf, text, n);
	buf[n] = '\0';
	mpd_free(text);
	return n;
}

// ---------------------------------------------------------------------------
// Classification and comparison
// ---------------------------------------------------------------------------

bool qd_f128_is_nan(const qd_f128_t* v) {
	return mpd_isnan(&v->num);
}

bool qd_f128_is_inf(const qd_f128_t* v) {
	return mpd_isinfinite(&v->num);
}

bool qd_f128_is_finite(const qd_f128_t* v) {
	return mpd_isfinite(&v->num);
}

bool qd_f128_is_zero(const qd_f128_t* v) {
	return mpd_iszero(&v->num);
}

bool qd_f128_is_normal(const qd_f128_t* v) {
	mpd_context_t ctx;
	ctx128(&ctx);
	return mpd_isnormal(&v->num, &ctx);
}

bool qd_f128_is_subnormal(const qd_f128_t* v) {
	mpd_context_t ctx;
	ctx128(&ctx);
	return mpd_issubnormal(&v->num, &ctx);
}

bool qd_f128_is_signed(const qd_f128_t* v) {
	return mpd_isnegative(&v->num);
}

int qd_f128_compare(const qd_f128_t* a, const qd_f128_t* b) {
	uint32_t status = 0;
	int c = mpd_qcmp(&a->num, &b->num, &status);
	return c == INT_MAX ? 2 : c;
}

int qd_f128_total_order(const qd_f128_t* a, const qd_f128_t* b) {
	return mpd_cmp_total(&a->num, &b->num);
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

#define QDRT_F128_UNARY(name, call)                                                                                    \
	qd_f128_t* qd_f128_##name(const qd_f128_t* a) {                                                                    \
		qd_f128_t* r = box_new();                                                                                      \
		if (r == NULL) {                                                                                               \
			return NULL;                                                                                               \
		}                                                                                                              \
		mpd_context_t ctx;                                                                                             \
		ctx128(&ctx);                                                                                                  \
		uint32_t status = 0;                                                                                           \
		call(&r->num, &a->num, &ctx, &status);                                                                         \
		return box_finish(r, status);                                                                                  \
	}

#define QDRT_F128_BINARY(name, call)                                                                                   \
	qd_f128_t* qd_f128_##name(const qd_f128_t* a, const qd_f128_t* b) {                                                \
		qd_f128_t* r = box_new();                                                                                      \
		if (r == NULL) {                                                                                               \
			return NULL;                                                                                               \
		}                                                                                                              \
		mpd_context_t ctx;                                                                                             \
		ctx128(&ctx);                                                                                                  \
		uint32_t status = 0;                                                                                           \
		call(&r->num, &a->num, &b->num, &ctx, &status);                                                                \
		return box_finish(r, status);                                                                                  \
	}

QDRT_F128_BINARY(add, mpd_qadd)
QDRT_F128_BINARY(sub, mpd_qsub)
QDRT_F128_BINARY(mul, mpd_qmul)
QDRT_F128_BINARY(div, mpd_qdiv)
QDRT_F128_BINARY(remainder, mpd_qrem_near)
QDRT_F128_BINARY(fmod, mpd_qrem)
QDRT_F128_BINARY(min, mpd_qmin)
QDRT_F128_BINARY(max, mpd_qmax)
QDRT_F128_BINARY(quantize, mpd_qquantize)

QDRT_F128_UNARY(logb, mpd_qlogb)
QDRT_F128_UNARY(next_up, mpd_qnext_plus)
QDRT_F128_UNARY(next_down, mpd_qnext_minus)
QDRT_F128_UNARY(reduce, mpd_qreduce)
QDRT_F128_UNARY(trunc, mpd_qtrunc)
QDRT_F128_UNARY(floor, mpd_qfloor)
QDRT_F128_UNARY(ceil, mpd_qceil)
QDRT_F128_UNARY(sqrt, mpd_qsqrt)

qd_f128_t* qd_f128_fma(const qd_f128_t* a, const qd_f128_t* b, const qd_f128_t* c) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	mpd_qfma(&r->num, &a->num, &b->num, &c->num, &ctx, &status);
	return box_finish(r, status);
}

// negate, abs and copySign are sign-bit operations in IEEE 754 -- quiet, and exact even
// for NaN and zero -- which is what the mpd_qcopy_* family does. mpd_qminus and mpd_qabs
// are the arithmetic versions and would turn -0 - 0 into +0.
qd_f128_t* qd_f128_neg(const qd_f128_t* a) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	uint32_t status = 0;
	mpd_qcopy_negate(&r->num, &a->num, &status);
	return box_finish(r, status);
}

qd_f128_t* qd_f128_abs(const qd_f128_t* a) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	uint32_t status = 0;
	mpd_qcopy_abs(&r->num, &a->num, &status);
	return box_finish(r, status);
}

qd_f128_t* qd_f128_copysign(const qd_f128_t* a, const qd_f128_t* sign) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	uint32_t status = 0;
	mpd_qcopy_sign(&r->num, &a->num, &sign->num, &status);
	return box_finish(r, status);
}

qd_f128_t* qd_f128_scaleb(const qd_f128_t* a, int64_t n) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	mpd_t* scale = mpd_qnew();
	if (scale == NULL) {
		box_free(r);
		return NULL;
	}
	// scaleb wants its operand within +-2*(emax+prec), and anything past that overflows or
	// underflows completely anyway, so clamping loses nothing.
	const int64_t limit = 2 * (6144 + 34) + 1;
	mpd_qset_i64(scale, n > limit ? limit : (n < -limit ? -limit : n), &ctx, &status);
	mpd_qscaleb(&r->num, &a->num, scale, &ctx, &status);
	mpd_del(scale);
	return box_finish(r, status);
}

static qd_f128_t* round_with(const qd_f128_t* a, int mode) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	ctx.round = mode;
	uint32_t status = 0;
	mpd_qround_to_int(&r->num, &a->num, &ctx, &status);
	return box_finish(r, status);
}

qd_f128_t* qd_f128_round_even(const qd_f128_t* a) {
	return round_with(a, MPD_ROUND_HALF_EVEN);
}

qd_f128_t* qd_f128_round(const qd_f128_t* a) {
	// MPD_ROUND_HALF_UP is half away from zero, whatever its name suggests.
	return round_with(a, MPD_ROUND_HALF_UP);
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Pi to `prec` digits, correctly rounded: the digits are read one past what is kept and
// the context rounds on the last. Returns false only when memory runs out.
static bool pi_to(mpd_t* out, mpd_ssize_t prec, uint32_t* status) {
	if (prec > QDRT_PI_DIGITS - 1) {
		prec = QDRT_PI_DIGITS - 1;
	}
	// "3." and then the digits after the 3, one more than are kept
	const size_t want = (size_t)prec + 1;
	char* text = malloc(want + 2);
	if (text == NULL) {
		return false;
	}
	text[0] = '3';
	text[1] = '.';
	size_t at = 1; // digits of pi copied so far, the 3 included
	for (size_t i = 0; at < want; i++) {
		const char* chunk = qdrt_pi_chunks[i] + (i == 0 ? 1 : 0);
		size_t take = strlen(chunk);
		if (take > want - at) {
			take = want - at;
		}
		memcpy(text + 1 + at, chunk, take);
		at += take;
	}
	text[1 + at] = '\0';

	mpd_context_t ctx;
	ctx_work(&ctx, prec);
	mpd_qset_string(out, text, &ctx, status);
	free(text);
	return !(*status & MPD_Malloc_error);
}

qd_f128_t* qd_f128_pi(void) {
	mpd_t* pi = mpd_qnew();
	if (pi == NULL) {
		return NULL;
	}
	uint32_t status = 0;
	qd_f128_t* r = pi_to(pi, 34, &status) ? box_from_work(pi, status) : NULL;
	mpd_del(pi);
	return r;
}

qd_f128_t* qd_f128_e(void) {
	qd_f128_t* one = qd_f128_from_i64(1);
	if (one == NULL) {
		return NULL;
	}
	qd_f128_t* r = qd_f128_exp(one);
	qd_f128_release(one);
	return r;
}

qd_f128_t* qd_f128_inf(void) {
	return box_special(MPD_INF);
}

qd_f128_t* qd_f128_nan(void) {
	return box_special(MPD_NAN);
}

// ---------------------------------------------------------------------------
// Trigonometry
//
// Everything below works at a working precision, in fixed-point decimals or bare mpd_t,
// and rounds once, at the end, into decimal128. Each helper returns false only when
// memory runs out.
// ---------------------------------------------------------------------------

// A libmpdec value on the stack, with room for the working precision. One that outgrows
// it moves to the heap by itself, so it still wants mpd_del.
#define QDRT_MPD_WORDS 12
#define QDRT_MPD_LOCAL(name)                                                                                           \
	mpd_uint_t name##_data[QDRT_MPD_WORDS];                                                                            \
	mpd_t name = {MPD_STATIC | MPD_STATIC_DATA, 0, 0, 0, QDRT_MPD_WORDS, name##_data}

// Decimal CORDIC, as the HP-35 did it: an angle is taken apart into whole multiples of
// atan(10^-k), and turning a vector through one of them is a shift and an add. The
// steps run on fixed-point decimals of a few words rather than through libmpdec, as
// each is the same handful of word operations. They stop below 10^-11, and the angle
// left over is finished in libmpdec with one turn through its tangent, t + t^3/3, which
// is out by 2t^5/15, under 10^-55.

#define FX_LIMBS 8 // one whole word and seven of fraction, base 10^9
#define FX_FRAC_DIGITS 63
#define FX_BASE 1000000000u
#define CORDIC_LEVELS 12

typedef struct {
	uint32_t d[FX_LIMBS]; // most significant first; d[0] is the whole part
} fx_t;

// atan(10^-k) to 63 places, rounded
static const fx_t cordic_angles[CORDIC_LEVELS] = {
		{{0, 785398163, 397448309, 615660845, 819875721, 49292349, 843776455, 243736148}},
		{{0, 99668652, 491162027, 378446119, 878020590, 243278322, 504314648, 15508777}},
		{{0, 9999666, 686665238, 206340116, 209279548, 561369352, 544376639, 627939418}},
		{{0, 999999, 666666866, 666523809, 634920544, 11620934, 554268013, 91431048}},
		{{0, 99999, 999666666, 668666666, 652380952, 492063491, 154401162, 93462027}},
		{{0, 9999, 999999666, 666666686, 666666665, 238095238, 206349206, 340115440}},
		{{0, 999, 999999999, 666666666, 666866666, 666666523, 809523809, 634920635}},
		{{0, 99, 999999999, 999666666, 666666668, 666666666, 666652380, 952380952}},
		{{0, 9, 999999999, 999999666, 666666666, 666686666, 666666666, 665238095}},
		{{0, 0, 999999999, 999999999, 666666666, 666666666, 866666666, 666666667}},
		{{0, 0, 99999999, 999999999, 999666666, 666666666, 666668666, 666666667}},
		{{0, 0, 9999999, 999999999, 999999666, 666666666, 666666666, 686666667}},
};

static const uint32_t pow10_u32[10] = {
		1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

static void fx_set_one(fx_t* a) {
	memset(a, 0, sizeof *a);
	a->d[0] = 1;
}

// out = a * 10^-k, the digits shifted out dropped. Each digit offset is its own case so
// that the divisor is a constant, which compiles to a multiplication: the steps do
// little else.
#define FX_SHIFT_BY(DIV)                                                                                               \
	for (int i = FX_LIMBS - 1; i >= 0; i--) {                                                                          \
		const int from = i - words;                                                                                    \
		uint32_t v = from >= 0 ? a->d[from] / (DIV) : 0;                                                               \
		if (from >= 1) {                                                                                               \
			v += (a->d[from - 1] % (DIV)) * (FX_BASE / (DIV));                                                         \
		}                                                                                                              \
		out->d[i] = v;                                                                                                 \
	}

static void fx_shift(fx_t* out, const fx_t* a, int k) {
	const int words = k / 9;
	switch (k % 9) {
	case 0:
		FX_SHIFT_BY(1u)
		break;
	case 1:
		FX_SHIFT_BY(10u)
		break;
	case 2:
		FX_SHIFT_BY(100u)
		break;
	case 3:
		FX_SHIFT_BY(1000u)
		break;
	case 4:
		FX_SHIFT_BY(10000u)
		break;
	case 5:
		FX_SHIFT_BY(100000u)
		break;
	case 6:
		FX_SHIFT_BY(1000000u)
		break;
	case 7:
		FX_SHIFT_BY(10000000u)
		break;
	default:
		FX_SHIFT_BY(100000000u)
		break;
	}
}

#undef FX_SHIFT_BY

// a += b
static void fx_add(fx_t* a, const fx_t* b) {
	uint32_t carry = 0;
	for (int i = FX_LIMBS - 1; i >= 0; i--) {
		const uint32_t v = a->d[i] + b->d[i] + carry;
		carry = v >= FX_BASE;
		a->d[i] = carry ? v - FX_BASE : v;
	}
}

// out = a - b, unless b > a: then false and out is untouched
static bool fx_sub(fx_t* out, const fx_t* a, const fx_t* b) {
	fx_t r;
	uint32_t borrow = 0;
	for (int i = FX_LIMBS - 1; i >= 0; i--) {
		const uint32_t s = b->d[i] + borrow;
		borrow = a->d[i] < s;
		r.d[i] = borrow ? a->d[i] + FX_BASE - s : a->d[i] - s;
	}
	if (borrow) {
		return false;
	}
	*out = r;
	return true;
}

// A non-negative value under 10^9, rounded to 63 places. Exact for every value the
// steps are given, which have 46 digits at most and are no smaller than 10^-11.
static bool fx_from_mpd(fx_t* out, const mpd_t* v, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, FX_LIMBS * 9);
	QDRT_MPD_LOCAL(t);
	mpd_qrescale(&t, v, -FX_FRAC_DIGITS, &ctx, status);
	memset(out, 0, sizeof *out);
	// The coefficient's digits, least significant first, regrouped into base 10^9
	size_t n = 0;
	for (mpd_ssize_t i = 0; i < t.len && n < FX_LIMBS * 9; i++) {
		mpd_uint_t w = t.data[i];
		for (int j = 0; j < MPD_RDIGITS && n < FX_LIMBS * 9; j++, n++) {
			out->d[FX_LIMBS - 1 - n / 9] += (uint32_t)(w % 10) * pow10_u32[n % 9];
			w /= 10;
		}
	}
	mpd_del(&t);
	return !(*status & MPD_Malloc_error);
}

// The digits regrouped into libmpdec's words, exactly.
static bool fx_to_mpd(mpd_t* out, const fx_t* a, uint32_t* status) {
	const mpd_ssize_t words = (FX_LIMBS * 9 + MPD_RDIGITS - 1) / MPD_RDIGITS;
	if (!mpd_qresize(out, words, status)) {
		return false;
	}
	memset(out->data, 0, (size_t)words * sizeof *out->data);
	mpd_ssize_t at = 0;
	int used = 0;
	mpd_uint_t place = 1;
	for (int i = FX_LIMBS - 1; i >= 0; i--) {
		uint32_t w = a->d[i];
		for (int j = 0; j < 9; j++) {
			out->data[at] += (mpd_uint_t)(w % 10) * place;
			w /= 10;
			place *= 10;
			if (++used == MPD_RDIGITS) {
				at++;
				used = 0;
				place = 1;
			}
		}
	}
	out->len = words;
	while (out->len > 1 && out->data[out->len - 1] == 0) {
		out->len--;
	}
	mpd_set_flags(out, MPD_POS);
	out->exp = -FX_FRAC_DIGITS;
	mpd_setdigits(out);
	return true;
}

// Turn (1, 0) through theta, 0 <= theta <= pi/4, by every whole step that fits. Leaves
// the turned vector, stretched by the steps, and what of theta they did not cover.
static void cordic_rotate(fx_t* x, fx_t* y, fx_t* theta) {
	fx_set_one(x);
	memset(y, 0, sizeof *y);
	fx_t xs;
	fx_t ys;
	for (int k = 0; k < CORDIC_LEVELS; k++) {
		while (fx_sub(theta, theta, &cordic_angles[k])) {
			fx_shift(&xs, x, k);
			fx_shift(&ys, y, k);
			fx_sub(x, x, &ys); // y <= x throughout, so this never borrows
			fx_add(y, &xs);
		}
	}
}

// Turn (x, y), 0 <= y <= x, toward the x axis by every whole step that keeps y
// non-negative, adding up the angle turned through.
static void cordic_vector(fx_t* x, fx_t* y, fx_t* angle) {
	memset(angle, 0, sizeof *angle);
	fx_t xs;
	fx_t ys;
	fx_t next;
	for (int k = 0; k < CORDIC_LEVELS; k++) {
		for (;;) {
			fx_shift(&xs, x, k);
			if (!fx_sub(&next, y, &xs)) {
				break;
			}
			fx_shift(&ys, y, k);
			fx_add(x, &ys);
			*y = next;
			fx_add(angle, &cordic_angles[k]);
		}
	}
}

// Whether a value is large enough for the steps, which cover angles down to 10^-11.
// Below that the finishing turn is the whole of it, and is done in libmpdec at full
// relative precision rather than in fixed point.
static bool cordic_applies(const mpd_t* v) {
	return !mpd_iszero(v) && mpd_adjexp(v) >= 1 - CORDIC_LEVELS;
}

// tan(r), 0 <= r <= pi/4, at the working precision. The steps leave a vector at an
// angle a, whose tangent is y/x whatever they stretched it by, and the angle b left
// over; tan(a + b) = (y + x tan b) / (x - y tan b).
static bool cordic_tan(mpd_t* out, const mpd_t* r, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(y);
	QDRT_MPD_LOCAL(b);
	QDRT_MPD_LOCAL(tb);
	bool ok = true;
	const bool stepped = cordic_applies(r);
	if (stepped) {
		fx_t fx;
		fx_t fy;
		fx_t theta;
		ok = fx_from_mpd(&theta, r, status);
		if (ok) {
			cordic_rotate(&fx, &fy, &theta);
			ok = fx_to_mpd(&x, &fx, status) && fx_to_mpd(&y, &fy, status) && fx_to_mpd(&b, &theta, status);
		}
	} else {
		mpd_qcopy(&b, r, status);
	}

	if (ok) {
		// tan b = b + b^3/3
		mpd_qmul(&tb, &b, &b, &ctx, status);
		mpd_qmul(&tb, &tb, &b, &ctx, status);
		mpd_qset_i64(out, 3, &ctx, status);
		mpd_qdiv(&tb, &tb, out, &ctx, status);
		mpd_qadd(&tb, &tb, &b, &ctx, status);
		if (stepped) {
			mpd_qmul(&b, &x, &tb, &ctx, status);
			mpd_qadd(&b, &y, &b, &ctx, status);
			mpd_qmul(&tb, &y, &tb, &ctx, status);
			mpd_qsub(&tb, &x, &tb, &ctx, status);
			mpd_qdiv(out, &b, &tb, &ctx, status);
		} else {
			mpd_qcopy(out, &tb, status);
		}
	}
	mpd_del(&x);
	mpd_del(&y);
	mpd_del(&b);
	mpd_del(&tb);
	return ok && !(*status & MPD_Malloc_error);
}

// Reduce x to r in [-pi/4, pi/4] with x = r + n*pi/2, and n's quadrant in 0..3.
//
// Pi is taken to as many digits as x has integer digits plus the working precision, so
// the subtraction loses nothing to x's size. When x lies close to a multiple of pi/2 the
// subtraction cancels leading digits as well; those are detected and bought back by
// trying again with that many more.
static bool trig_reduce(mpd_t* r, int* quadrant, const mpd_t* x, uint32_t* status) {
	const mpd_ssize_t whole = mpd_adjexp(x) > 0 ? mpd_adjexp(x) + 1 : 0;
	mpd_t* halfpi = mpd_qnew();
	mpd_t* k = mpd_qnew();
	mpd_t* tmp = mpd_qnew();
	bool ok = halfpi != NULL && k != NULL && tmp != NULL;
	mpd_ssize_t loss = 0;
	*quadrant = 0;
	while (ok) {
		mpd_ssize_t prec = QDRT_WORK_PREC + whole + loss;
		if (prec > QDRT_PI_DIGITS - 2) {
			prec = QDRT_PI_DIGITS - 2;
		}
		mpd_context_t ctx;
		ctx_work(&ctx, prec);
		if (!pi_to(halfpi, prec + 1, status)) {
			ok = false;
			break;
		}
		mpd_qset_i64(tmp, 2, &ctx, status);
		mpd_qdiv(halfpi, halfpi, tmp, &ctx, status);

		// k = x / (pi/2) rounded to an integer; r = x - k*(pi/2)
		mpd_qdiv(k, x, halfpi, &ctx, status);
		ctx.round = MPD_ROUND_HALF_EVEN;
		mpd_qround_to_int(k, k, &ctx, status);
		mpd_qmul(tmp, k, halfpi, &ctx, status);
		mpd_qsub(r, x, tmp, &ctx, status);
		if (*status & MPD_Malloc_error) {
			ok = false;
			break;
		}

		// With k = 0 there was nothing to subtract and r is x exactly. Otherwise r carries
		// an absolute error of about 10^-(working precision + loss), so every leading digit
		// the subtraction cancelled has to be paid for with one more digit of pi.
		const mpd_ssize_t cancelled = mpd_iszero(k) ? 0 : (mpd_iszero(r) ? loss + 50 : -mpd_adjexp(r) - 1);
		if (cancelled > loss && prec < QDRT_PI_DIGITS - 2) {
			loss = cancelled + 2;
			continue;
		}

		mpd_qset_i64(tmp, 4, &ctx, status);
		mpd_qrem(tmp, k, tmp, &ctx, status);
		uint32_t get = 0;
		const int64_t n = mpd_qget_i64(tmp, &get);
		*quadrant = (int)(((n % 4) + 4) % 4);
		break;
	}
	mpd_del(halfpi);
	mpd_del(k);
	mpd_del(tmp);
	return ok && !(*status & MPD_Malloc_error);
}

enum trig_kind { TRIG_SIN, TRIG_COS, TRIG_TAN };

static qd_f128_t* trig(const qd_f128_t* a, enum trig_kind kind) {
	const mpd_t* x = &a->num;
	if (mpd_isnan(x) || mpd_isinfinite(x)) {
		// sin(inf) has no value: invalid, and a quiet NaN.
		return box_special(MPD_NAN);
	}
	if (mpd_iszero(x) && kind != TRIG_COS) {
		// sin and tan are odd: -0 stays -0.
		return qd_f128_retain((qd_f128_t*)a);
	}

	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(r);
	QDRT_MPD_LOCAL(t);
	QDRT_MPD_LOCAL(sn);
	QDRT_MPD_LOCAL(cn);
	QDRT_MPD_LOCAL(den);
	qd_f128_t* result = NULL;
	int quadrant = 0;

	// Under pi/4 there is nothing to reduce
	mpd_qset_string(&t, "0.785398163397448309", &ctx, &status);
	mpd_qcopy_abs(&r, x, &status);
	bool ok = mpd_qcmp(&r, &t, &status) < 0 ? mpd_qcopy(&r, x, &status) : trig_reduce(&r, &quadrant, x, &status);
	if (ok) {
		// r was reduced at whatever precision the reduction needed; the steps only
		// need the working precision.
		mpd_qfinalize(&r, &ctx, &status);
		const bool negative = mpd_isnegative(&r);
		mpd_qcopy_abs(&r, &r, &status);

		// t = tan(r/2) gives sin r = 2t/(1+t^2), cos r = (1-t^2)/(1+t^2) and
		// tan r = 2t/(1-t^2): a division each and no square root, and as t is at most
		// tan(pi/8) none of them cancels.
		mpd_qset_i64(&den, 2, &ctx, &status);
		mpd_qdiv(&r, &r, &den, &ctx, &status);
		ok = cordic_tan(&t, &r, &status);
		if (ok && negative) {
			mpd_qcopy_negate(&t, &t, &status);
		}
	}
	if (ok) {
		mpd_qadd(&sn, &t, &t, &ctx, &status);
		mpd_qmul(&t, &t, &t, &ctx, &status);
		mpd_qset_i64(&den, 1, &ctx, &status);
		mpd_qsub(&cn, &den, &t, &ctx, &status);
		mpd_qadd(&den, &den, &t, &ctx, &status);

		// sin(r + n*pi/2) and cos(r + n*pi/2) by quadrant
		mpd_t* out = &r;
		switch (kind) {
		case TRIG_SIN:
			mpd_qdiv(out, (quadrant & 1) ? &cn : &sn, &den, &ctx, &status);
			if (quadrant >= 2) {
				mpd_qcopy_negate(out, out, &status);
			}
			break;
		case TRIG_COS:
			mpd_qdiv(out, (quadrant & 1) ? &sn : &cn, &den, &ctx, &status);
			if (quadrant == 1 || quadrant == 2) {
				mpd_qcopy_negate(out, out, &status);
			}
			break;
		case TRIG_TAN:
			if (quadrant & 1) {
				mpd_qdiv(out, &cn, &sn, &ctx, &status);
				mpd_qcopy_negate(out, out, &status);
			} else {
				mpd_qdiv(out, &sn, &cn, &ctx, &status);
			}
			break;
		}
		result = box_from_work(out, status);
	}
	mpd_del(&r);
	mpd_del(&t);
	mpd_del(&sn);
	mpd_del(&cn);
	mpd_del(&den);
	return result;
}

qd_f128_t* qd_f128_sin(const qd_f128_t* a) {
	return trig(a, TRIG_SIN);
}

qd_f128_t* qd_f128_cos(const qd_f128_t* a) {
	return trig(a, TRIG_COS);
}

qd_f128_t* qd_f128_tan(const qd_f128_t* a) {
	return trig(a, TRIG_TAN);
}

// atan of a finite value, at the working precision.
//
// |x| > 1 is folded to pi/2 - atan(1/|x|). The vector (1, |x|) is then turned down to
// the axis by CORDIC steps, and the angle u = y/x it is left at, under 10^-11, is
// finished with u - u^3/3, which is out by u^5/5.
static bool atan_work(mpd_t* out, const mpd_t* x, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(y);
	QDRT_MPD_LOCAL(u);
	QDRT_MPD_LOCAL(tmp);
	bool ok = true;

	mpd_qset_i64(&tmp, 1, &ctx, status);
	mpd_qcopy_abs(&y, x, status);
	const bool invert = mpd_qcmp(&y, &tmp, status) > 0;
	if (invert) {
		mpd_qdiv(&y, &tmp, &y, &ctx, status);
	}

	if (cordic_applies(&y)) {
		fx_t fx;
		fx_t fy;
		fx_t angle;
		fx_set_one(&fx);
		ok = fx_from_mpd(&fy, &y, status);
		if (ok) {
			cordic_vector(&fx, &fy, &angle);
			ok = fx_to_mpd(out, &angle, status) && fx_to_mpd(&u, &fy, status) && fx_to_mpd(&tmp, &fx, status);
			mpd_qdiv(&u, &u, &tmp, &ctx, status);
		}
	} else {
		mpd_qset_i64(out, 0, &ctx, status);
		mpd_qcopy(&u, &y, status);
	}

	if (ok) {
		mpd_qmul(&tmp, &u, &u, &ctx, status);
		mpd_qmul(&tmp, &tmp, &u, &ctx, status);
		mpd_qset_i64(&y, 3, &ctx, status);
		mpd_qdiv(&tmp, &tmp, &y, &ctx, status);
		mpd_qsub(&u, &u, &tmp, &ctx, status);
		mpd_qadd(out, out, &u, &ctx, status);

		if (invert) {
			ok = pi_to(&tmp, QDRT_WORK_PREC + 2, status);
			mpd_qset_i64(&y, 2, &ctx, status);
			mpd_qdiv(&tmp, &tmp, &y, &ctx, status);
			mpd_qsub(out, &tmp, out, &ctx, status);
		}
		if (mpd_isnegative(x)) {
			mpd_qcopy_negate(out, out, status);
		}
	}
	mpd_del(&y);
	mpd_del(&u);
	mpd_del(&tmp);
	return ok && !(*status & MPD_Malloc_error);
}

// pi * num / den at the working precision, with the sign of `negative`.
static bool pi_fraction(mpd_t* out, int64_t num, int64_t den, bool negative, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC);
	mpd_t* tmp = mpd_qnew();
	if (tmp == NULL || !pi_to(out, QDRT_WORK_PREC + 2, status)) {
		mpd_del(tmp);
		return false;
	}
	mpd_qset_i64(tmp, num, &ctx, status);
	mpd_qmul(out, out, tmp, &ctx, status);
	mpd_qset_i64(tmp, den, &ctx, status);
	mpd_qdiv(out, out, tmp, &ctx, status);
	if (negative) {
		mpd_qcopy_negate(out, out, status);
	}
	mpd_del(tmp);
	return !(*status & MPD_Malloc_error);
}

qd_f128_t* qd_f128_atan(const qd_f128_t* a) {
	const mpd_t* x = &a->num;
	if (mpd_isnan(x)) {
		return box_special(MPD_NAN);
	}
	if (mpd_iszero(x)) {
		return qd_f128_retain((qd_f128_t*)a);
	}
	uint32_t status = 0;
	mpd_t* out = mpd_qnew();
	qd_f128_t* result = NULL;
	if (out != NULL) {
		bool ok = mpd_isinfinite(x) ? pi_fraction(out, 1, 2, mpd_isnegative(x), &status)
									: atan_work(out, x, &status);
		if (ok) {
			result = box_from_work(out, status);
		}
	}
	mpd_del(out);
	return result;
}

// asin(x) = atan(x / sqrt((1-x)(1+x))). (1-x)(1+x) rather than 1-x^2: near |x| = 1
// the latter cancels away the digits the answer depends on.
qd_f128_t* qd_f128_asin(const qd_f128_t* a) {
	const mpd_t* x = &a->num;
	if (mpd_isnan(x)) {
		return box_special(MPD_NAN);
	}
	if (mpd_iszero(x)) {
		return qd_f128_retain((qd_f128_t*)a);
	}
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_t* one = mpd_qnew();
	mpd_t* p = mpd_qnew();
	mpd_t* q = mpd_qnew();
	qd_f128_t* result = NULL;
	if (one != NULL && p != NULL && q != NULL) {
		mpd_qset_i64(one, 1, &ctx, &status);
		mpd_qcopy_abs(p, x, &status);
		const int against_one = mpd_qcmp(p, one, &status);
		if (against_one > 0) {
			result = box_special(MPD_NAN);
		} else if (against_one == 0) {
			if (pi_fraction(p, 1, 2, mpd_isnegative(x), &status)) {
				result = box_from_work(p, status);
			}
		} else {
			mpd_qsub(p, one, x, &ctx, &status);
			mpd_qadd(q, one, x, &ctx, &status);
			mpd_qmul(p, p, q, &ctx, &status);
			mpd_qsqrt(p, p, &ctx, &status);
			mpd_qdiv(q, x, p, &ctx, &status);
			if (atan_work(p, q, &status)) {
				result = box_from_work(p, status);
			}
		}
	}
	mpd_del(one);
	mpd_del(p);
	mpd_del(q);
	return result;
}

// acos(x) = 2*atan(sqrt((1-x)/(1+x))), which keeps its digits near x = 1 where
// pi/2 - asin(x) would cancel them.
qd_f128_t* qd_f128_acos(const qd_f128_t* a) {
	const mpd_t* x = &a->num;
	if (mpd_isnan(x)) {
		return box_special(MPD_NAN);
	}
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_t* one = mpd_qnew();
	mpd_t* p = mpd_qnew();
	mpd_t* q = mpd_qnew();
	qd_f128_t* result = NULL;
	if (one != NULL && p != NULL && q != NULL) {
		mpd_qset_i64(one, 1, &ctx, &status);
		mpd_qcopy_abs(p, x, &status);
		const int against_one = mpd_qcmp(p, one, &status);
		if (against_one > 0) {
			result = box_special(MPD_NAN);
		} else if (against_one == 0 && mpd_isnegative(x)) {
			if (pi_fraction(p, 1, 1, false, &status)) {
				result = box_from_work(p, status);
			}
		} else if (against_one == 0) {
			result = qd_f128_from_i64(0);
		} else {
			mpd_qsub(p, one, x, &ctx, &status);
			mpd_qadd(q, one, x, &ctx, &status);
			mpd_qdiv(p, p, q, &ctx, &status);
			mpd_qsqrt(p, p, &ctx, &status);
			if (atan_work(q, p, &status)) {
				mpd_qadd(q, q, q, &ctx, &status);
				result = box_from_work(q, status);
			}
		}
	}
	mpd_del(one);
	mpd_del(p);
	mpd_del(q);
	return result;
}

// atan2 with the special cases of C99 Annex F, which IEEE 754-2008 section 9.2.1 follows.
qd_f128_t* qd_f128_atan2(const qd_f128_t* ya, const qd_f128_t* xa) {
	const mpd_t* y = &ya->num;
	const mpd_t* x = &xa->num;
	if (mpd_isnan(y) || mpd_isnan(x)) {
		return box_special(MPD_NAN);
	}
	const bool yneg = mpd_isnegative(y);
	const bool xneg = mpd_isnegative(x);
	uint32_t status = 0;
	mpd_t* out = mpd_qnew();
	if (out == NULL) {
		return NULL;
	}
	bool ok = true;
	qd_f128_t* result = NULL;
	if (mpd_iszero(y)) {
		if (!xneg) {
			// atan2(+-0, +0 or positive) is +-0
			mpd_del(out);
			return qd_f128_retain((qd_f128_t*)ya);
		}
		ok = pi_fraction(out, 1, 1, yneg, &status);
	} else if (mpd_iszero(x)) {
		ok = pi_fraction(out, 1, 2, yneg, &status);
	} else if (mpd_isinfinite(y)) {
		if (mpd_isinfinite(x)) {
			ok = pi_fraction(out, xneg ? 3 : 1, 4, yneg, &status);
		} else {
			ok = pi_fraction(out, 1, 2, yneg, &status);
		}
	} else if (mpd_isinfinite(x)) {
		if (!xneg) {
			// atan2(finite, +inf) is +-0
			mpd_del(out);
			result = qd_f128_from_i64(0);
			if (result != NULL && yneg) {
				qd_f128_t* negated = qd_f128_neg(result);
				qd_f128_release(result);
				result = negated;
			}
			return result;
		}
		ok = pi_fraction(out, 1, 1, yneg, &status);
	} else {
		mpd_context_t ctx;
		ctx_work(&ctx, QDRT_WORK_PREC + 2);
		mpd_t* ratio = mpd_qnew();
		mpd_t* pi = mpd_qnew();
		ok = ratio != NULL && pi != NULL;
		if (ok) {
			mpd_qdiv(ratio, y, x, &ctx, &status);
			mpd_qcopy_abs(ratio, ratio, &status);
			ok = atan_work(out, ratio, &status);
			if (ok && xneg) {
				// The angle lies in the left half-plane: pi - atan(|y/x|)
				ok = pi_to(pi, QDRT_WORK_PREC + 2, &status);
				mpd_qsub(out, pi, out, &ctx, &status);
			}
			if (ok && yneg) {
				mpd_qcopy_negate(out, out, &status);
			}
		}
		mpd_del(ratio);
		mpd_del(pi);
	}
	if (ok && !(status & MPD_Malloc_error)) {
		result = box_from_work(out, status);
	}
	mpd_del(out);
	return result;
}

// ---------------------------------------------------------------------------
// Exponential and logarithm
//
// HP's pseudo-multiplication and pseudo-division, on the fixed-point decimals above: a
// value is taken apart into factors 1 + 10^-k, and multiplying by one is a shift and an
// add, its logarithm a constant from a table. What the factors leave, under 10^-11, is
// finished with a few terms of a series.
//
// Unlike the trigonometric functions these are promised correctly rounded, which a
// result 46 digits close does not prove on its own. So, as in Ziv's method, it is used
// only where its error cannot change the rounding, and libmpdec's correctly rounded
// function decides the rest: about one call in 10^8.
// ---------------------------------------------------------------------------

// ln(1 + 10^-k) to 63 places, rounded
static const fx_t ln_up[CORDIC_LEVELS] = {
		{{0, 693147180, 559945309, 417232121, 458176568, 75500134, 360255254, 120680009}},
		{{0, 95310179, 804324860, 43952123, 280765092, 220605365, 308644199, 185239808}},
		{{0, 9950330, 853168082, 848215357, 544260741, 688679609, 940058797, 864609560}},
		{{0, 999500, 333083533, 166809398, 920535011, 460755062, 393166551, 997019667}},
		{{0, 99995, 333308, 335333166, 680951131, 63482064, 401071075, 512661294}},
		{{0, 9999, 950000333, 330833353, 333166668, 95225595, 349205349, 215440032}},
		{{0, 999, 999500000, 333333083, 333533333, 166666809, 523684523, 920634821}},
		{{0, 99, 999995000, 333333, 308333335, 333333166, 666680952, 379702381}},
		{{0, 9, 999999950, 333, 333330833, 333353333, 333166666, 668095238}},
		{{0, 0, 999999999, 500000000, 333333333, 83333333, 533333333, 166666667}},
		{{0, 0, 99999999, 995000000, 333333, 333308333, 333335333, 333333167}},
		{{0, 0, 9999999, 999950000, 333, 333333330, 833333333, 353333333}},
};

// -ln(1 - 10^-k) for k from 1, to 63 places, rounded
static const fx_t ln_down[CORDIC_LEVELS - 1] = {
		{{0, 105360515, 657826301, 227500980, 839312798, 306120372, 983274072, 563939234}},
		{{0, 10050335, 853501441, 183548857, 558547706, 85515007, 674629873, 378699426}},
		{{0, 1000500, 333583533, 500142982, 254068344, 960755205, 250434409, 250988021}},
		{{0, 100005, 333358, 335333500, 14286964, 396835397, 734571075, 514089866}},
		{{0, 10000, 50000333, 335833353, 333500001, 428583928, 682540682, 548773532}},
		{{0, 1000, 500000, 333333583, 333533333, 500000142, 857267857, 253968354}},
		{{0, 100, 5000, 333333, 358333335, 333333500, 14285, 715535714}},
		{{0, 10, 50, 333, 333335833, 333353333, 333500000, 1428571}},
		{{0, 1, 0, 500000000, 333333333, 583333333, 533333333, 500000000}},
		{{0, 0, 100000000, 5000000, 333333, 333358333, 333335333, 333333500}},
		{{0, 0, 10000000, 50000, 333, 333333335, 833333333, 353333333}},
};

#define QDRT_LN10 "2.302585092994045684017991454684364207601101488628772976033327901"

// A bound on a working result's relative error, a hundred times what it is
#define QDRT_ZIV_DIGITS 42

// Below this |x|, exp(x) is a normal decimal128 either way. Beyond it libmpdec deals with
// overflow, underflow and subnormals.
#define QDRT_EXP_LIMIT 14000

// r rounded into a new decimal128 box, when no value within 10^-42 of r rounds
// differently. NULL otherwise, with *decided false.
//
// The box holds the rounded bound rather than r: the functions that come here are
// inexact, so their result has all 34 digits, even where r happens to be a short
// value such as 1E+3.
static qd_f128_t* box_if_decided(const mpd_t* r, uint32_t status, bool* decided) {
	mpd_context_t ctx;
	ctx128(&ctx);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 4);
	QDRT_MPD_LOCAL(slack);
	QDRT_MPD_LOCAL(lo);
	QDRT_MPD_LOCAL(hi);
	mpd_qcopy_abs(&slack, r, &status);
	slack.exp -= QDRT_ZIV_DIGITS;
	mpd_qsub(&lo, r, &slack, &wide, &status);
	mpd_qadd(&hi, r, &slack, &wide, &status);
	mpd_qfinalize(&lo, &ctx, &status);
	mpd_qfinalize(&hi, &ctx, &status);
	*decided = !(status & MPD_Malloc_error) && mpd_cmp_total(&lo, &hi) == 0;
	qd_f128_t* result = *decided ? box_from_work(&lo, status) : NULL;
	mpd_del(&slack);
	mpd_del(&lo);
	mpd_del(&hi);
	return result;
}

// An inexact result given every digit decimal128 has, as IEEE 754 section 5.2 asks and
// box_if_decided does. libmpdec leaves some short where its own working happened to
// come out exact: exp(2E-26) as 1.00000000000000000000000002.
static void pad_inexact(mpd_t* v, uint32_t* status) {
	if (!(*status & MPD_Inexact) || mpd_isspecial(v) || mpd_iszero(v)) {
		return;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	mpd_ssize_t n = ctx.prec - v->digits;
	if (n > v->exp - mpd_etiny(&ctx)) {
		n = v->exp - mpd_etiny(&ctx);
	}
	if (n > 0 && mpd_qshiftl(v, v, n, status)) {
		v->exp -= n;
	}
}

static qd_f128_t* correctly_rounded(const qd_f128_t* a,
		void (*call)(mpd_t*, const mpd_t*, const mpd_context_t*, uint32_t*)) {
	qd_f128_t* r = box_new();
	if (r == NULL) {
		return NULL;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	call(&r->num, &a->num, &ctx, &status);
	pad_inexact(&r->num, &status);
	return box_finish(r, status);
}

// exp(x) at the working precision, for finite x of magnitude under QDRT_EXP_LIMIT. False
// for any other x, and when memory runs out. out may be x.
static bool exp_work(mpd_t* out, const mpd_t* x, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 16);
	QDRT_MPD_LOCAL(ln10);
	QDRT_MPD_LOCAL(n);
	QDRT_MPD_LOCAL(r);
	QDRT_MPD_LOCAL(sum);
	QDRT_MPD_LOCAL(term);
	bool ok = false;

	mpd_qcopy_abs(&sum, x, status);
	mpd_qset_i64(&r, QDRT_EXP_LIMIT, &ctx, status);
	if (!mpd_isspecial(x) && mpd_qcmp(&sum, &r, status) < 0) {
		// x = n ln 10 + r with 0 <= r < ln 10, so exp(x) = 10^n exp(r). Most x a calculator
		// sees already lie there. Otherwise the quotient is rounded, so n can come out one
		// too large or too small.
		mpd_qset_string(&term, "2.302585092994", &ctx, status);
		if (!mpd_isnegative(x) && mpd_qcmp(x, &term, status) < 0) {
			mpd_qset_i64(&n, 0, &ctx, status);
			mpd_qcopy(&r, x, status);
		} else {
			mpd_qset_string(&ln10, QDRT_LN10, &wide, status);
			mpd_qdiv(&n, x, &ln10, &ctx, status);
			mpd_qfloor(&n, &n, &ctx, status);
			mpd_qmul(&r, &n, &ln10, &wide, status);
			mpd_qsub(&r, x, &r, &wide, status);
			mpd_qset_i64(&term, 1, &ctx, status);
			if (mpd_isnegative(&r)) {
				mpd_qsub(&n, &n, &term, &ctx, status);
				mpd_qadd(&r, &r, &ln10, &wide, status);
			} else if (mpd_qcmp(&r, &ln10, status) >= 0) {
				mpd_qadd(&n, &n, &term, &ctx, status);
				mpd_qsub(&r, &r, &ln10, &wide, status);
			}
		}
		mpd_qfinalize(&r, &ctx, status);

		// out = the product of every factor 1 + 10^-k whose logarithm fits in r
		ok = true;
		if (cordic_applies(&r)) {
			fx_t fr;
			fx_t fy;
			fx_t shifted;
			ok = fx_from_mpd(&fr, &r, status);
			fx_set_one(&fy);
			for (int k = 0; ok && k < CORDIC_LEVELS; k++) {
				while (fx_sub(&fr, &fr, &ln_up[k])) {
					fx_shift(&shifted, &fy, k);
					fx_add(&fy, &shifted);
				}
			}
			ok = ok && fx_to_mpd(out, &fy, status) && fx_to_mpd(&r, &fr, status);
		} else {
			mpd_qset_i64(out, 1, &ctx, status);
		}

		if (ok) {
			// exp(r) = 1 + r + r^2/2! + ..., for what r has left
			mpd_qset_i64(&sum, 1, &ctx, status);
			mpd_qset_i64(&term, 1, &ctx, status);
			for (int64_t j = 1; !(*status & MPD_Malloc_error); j++) {
				mpd_qmul(&term, &term, &r, &ctx, status);
				mpd_qset_i64(&ln10, j, &ctx, status);
				mpd_qdiv(&term, &term, &ln10, &ctx, status);
				if (mpd_iszero(&term) || mpd_adjexp(&term) < -(QDRT_WORK_PREC + 4)) {
					break;
				}
				mpd_qadd(&sum, &sum, &term, &ctx, status);
			}
			mpd_qmul(out, out, &sum, &ctx, status);
			uint32_t got = 0;
			out->exp += mpd_qget_ssize(&n, &got);
		}
	}
	mpd_del(&ln10);
	mpd_del(&n);
	mpd_del(&r);
	mpd_del(&sum);
	mpd_del(&term);
	return ok && !(*status & MPD_Malloc_error);
}

// ln(x) for finite x > 0, to some sixty digits: the factors and their logarithms are held
// to 63 places, and only the short series is at the working precision. pow needs the
// extra, as it multiplies the logarithm, error and all, by its exponent.
static bool ln_work(mpd_t* out, const mpd_t* x, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 16);
	QDRT_MPD_LOCAL(m);
	QDRT_MPD_LOCAL(y);
	QDRT_MPD_LOCAL(d);
	QDRT_MPD_LOCAL(power);
	QDRT_MPD_LOCAL(term);

	// x = m 10^e with m in [1, 10), so ln x = e ln 10 + ln m. From a half up to 1, m is x
	// itself and the factors are 1 - 10^-k: there e = -1 would make ln x the difference of
	// ln 10 and a logarithm nearly as large, which cancels the digits it is made of.
	mpd_qset_string(&term, "0.5", &ctx, status);
	const bool down = mpd_adjexp(x) == -1 && mpd_qcmp(x, &term, status) >= 0;
	const mpd_ssize_t e = down ? 0 : mpd_adjexp(x);
	mpd_qcopy(&m, x, status);
	m.exp -= e;

	// y = the product of every factor that keeps it on its side of m, and out the sum of
	// their logarithms
	fx_t fm;
	fx_t fy;
	fx_t facc;
	fx_t shifted;
	fx_t next;
	fx_t spare;
	bool ok = fx_from_mpd(&fm, &m, status);
	fx_set_one(&fy);
	memset(&facc, 0, sizeof facc);
	for (int k = down ? 1 : 0; ok && k < CORDIC_LEVELS; k++) {
		for (;;) {
			fx_shift(&shifted, &fy, k);
			if (down) {
				fx_sub(&next, &fy, &shifted);
				if (!fx_sub(&spare, &next, &fm)) {
					break;
				}
			} else {
				next = fy;
				fx_add(&next, &shifted);
				if (!fx_sub(&spare, &fm, &next)) {
					break;
				}
			}
			fy = next;
			fx_add(&facc, down ? &ln_down[k - 1] : &ln_up[k]);
		}
	}
	ok = ok && fx_to_mpd(&y, &fy, status) && fx_to_mpd(out, &facc, status);

	if (ok) {
		if (down) {
			mpd_qcopy_negate(out, out, status);
		}
		// ln(m / y) = ln(1 + d) = d - d^2/2 + d^3/3 - ..., |d| under 10^-11
		mpd_qsub(&d, &m, &y, &ctx, status);
		mpd_qdiv(&d, &d, &y, &ctx, status);
		mpd_qcopy(&power, &d, status);
		mpd_qadd(out, out, &d, &wide, status);
		for (int64_t j = 2; !mpd_iszero(&d) && !(*status & MPD_Malloc_error); j++) {
			mpd_qmul(&power, &power, &d, &ctx, status);
			mpd_qcopy_negate(&power, &power, status);
			mpd_qset_i64(&term, j, &ctx, status);
			mpd_qdiv(&term, &power, &term, &ctx, status);
			if (mpd_iszero(&term) || mpd_adjexp(&term) < mpd_adjexp(&d) - (QDRT_WORK_PREC + 4)) {
				break;
			}
			mpd_qadd(out, out, &term, &wide, status);
		}

		if (e != 0) {
			mpd_qset_string(&y, QDRT_LN10, &wide, status);
			mpd_qset_ssize(&term, e, &ctx, status);
			mpd_qmul(&term, &term, &y, &wide, status);
			mpd_qadd(out, out, &term, &wide, status);
		}
	}
	mpd_del(&m);
	mpd_del(&y);
	mpd_del(&d);
	mpd_del(&power);
	mpd_del(&term);
	return ok && !(*status & MPD_Malloc_error);
}

static bool is_positive_finite(const mpd_t* x) {
	return !mpd_isspecial(x) && !mpd_iszero(x) && !mpd_isnegative(x);
}

qd_f128_t* qd_f128_exp(const qd_f128_t* a) {
	uint32_t status = 0;
	QDRT_MPD_LOCAL(r);
	qd_f128_t* result = NULL;
	bool decided = false;
	// exp(0) is exactly 1
	if (!mpd_iszero(&a->num) && exp_work(&r, &a->num, &status)) {
		result = box_if_decided(&r, status, &decided);
	}
	mpd_del(&r);
	return decided ? result : correctly_rounded(a, mpd_qexp);
}

qd_f128_t* qd_f128_ln(const qd_f128_t* a) {
	const mpd_t* x = &a->num;
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(r);
	qd_f128_t* result = NULL;
	bool decided = false;
	// ln 1 is exactly 0
	mpd_qset_i64(&r, 1, &ctx, &status);
	if (is_positive_finite(x) && mpd_qcmp(x, &r, &status) != 0 && ln_work(&r, x, &status)) {
		result = box_if_decided(&r, status, &decided);
	}
	mpd_del(&r);
	return decided ? result : correctly_rounded(a, mpd_qln);
}

qd_f128_t* qd_f128_log10(const qd_f128_t* a) {
	const mpd_t* x = &a->num;
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 16);
	QDRT_MPD_LOCAL(r);
	QDRT_MPD_LOCAL(ln10);
	qd_f128_t* result = NULL;
	bool decided = false;
	if (is_positive_finite(x)) {
		// The log of a power of ten is an exact integer, which libmpdec gives as one
		mpd_qreduce(&r, x, &ctx, &status);
		const bool power_of_ten = r.digits == 1 && r.data[0] == 1;
		if (!power_of_ten && ln_work(&r, x, &status)) {
			mpd_qset_string(&ln10, QDRT_LN10, &wide, &status);
			mpd_qdiv(&r, &r, &ln10, &wide, &status);
			result = box_if_decided(&r, status, &decided);
		}
	}
	mpd_del(&r);
	mpd_del(&ln10);
	return decided ? result : correctly_rounded(a, mpd_qlog10);
}

qd_f128_t* qd_f128_pow(const qd_f128_t* base, const qd_f128_t* exponent) {
	const mpd_t* x = &base->num;
	const mpd_t* y = &exponent->num;
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 16);
	QDRT_MPD_LOCAL(r);
	qd_f128_t* result = NULL;
	bool decided = false;
	// x^y = exp(y ln x), for x > 0 and y not an integer. Integer powers are exact where
	// they can be, which libmpdec works out by multiplying, and 1^y is exactly 1.
	mpd_qset_i64(&r, 1, &ctx, &status);
	if (is_positive_finite(x) && mpd_qcmp(x, &r, &status) != 0 && !mpd_isspecial(y) && !mpd_isinteger(y) &&
			ln_work(&r, x, &status)) {
		mpd_qmul(&r, &r, y, &wide, &status);
		if (exp_work(&r, &r, &status)) {
			result = box_if_decided(&r, status, &decided);
		}
	}
	mpd_del(&r);
	if (decided) {
		return result;
	}
	qd_f128_t* fallback = box_new();
	if (fallback == NULL) {
		return NULL;
	}
	ctx128(&ctx);
	status = 0;
	mpd_qpow(&fallback->num, x, y, &ctx, &status);
	pad_inexact(&fallback->num, &status);
	return box_finish(fallback, status);
}
