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
QDRT_F128_BINARY(pow, mpd_qpow)

QDRT_F128_UNARY(logb, mpd_qlogb)
QDRT_F128_UNARY(next_up, mpd_qnext_plus)
QDRT_F128_UNARY(next_down, mpd_qnext_minus)
QDRT_F128_UNARY(reduce, mpd_qreduce)
QDRT_F128_UNARY(trunc, mpd_qtrunc)
QDRT_F128_UNARY(floor, mpd_qfloor)
QDRT_F128_UNARY(ceil, mpd_qceil)
QDRT_F128_UNARY(sqrt, mpd_qsqrt)
QDRT_F128_UNARY(exp, mpd_qexp)
QDRT_F128_UNARY(ln, mpd_qln)
QDRT_F128_UNARY(log10, mpd_qlog10)

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
// Everything below works on bare mpd_t at a working precision and rounds once, at the
// end, into decimal128. Each helper returns false only when memory runs out.
// ---------------------------------------------------------------------------

// Taylor series for sin (odd) or cos (even) at |r| <= pi/4, where both converge fast.
static bool trig_series(mpd_t* out, const mpd_t* r, bool odd, mpd_ssize_t prec, uint32_t* status) {
	mpd_context_t ctx;
	ctx_work(&ctx, prec + 2);
	mpd_t* r2 = mpd_qnew();
	mpd_t* term = mpd_qnew();
	mpd_t* denom = mpd_qnew();
	bool ok = r2 != NULL && term != NULL && denom != NULL;
	if (ok) {
		mpd_qmul(r2, r, r, &ctx, status);
		if (odd) {
			mpd_qcopy(term, r, status);
		} else {
			mpd_qset_i64(term, 1, &ctx, status);
		}
		mpd_qcopy(out, term, status);
		for (int64_t k = odd ? 2 : 1; !(*status & MPD_Malloc_error); k += 2) {
			// term *= -r^2 / (k * (k + 1))
			mpd_qset_i64(denom, -(k * (k + 1)), &ctx, status);
			mpd_qmul(term, term, r2, &ctx, status);
			mpd_qdiv(term, term, denom, &ctx, status);
			if (mpd_iszero(term) || mpd_adjexp(term) < mpd_adjexp(out) - prec - 2) {
				break;
			}
			mpd_qadd(out, out, term, &ctx, status);
		}
		ok = !(*status & MPD_Malloc_error);
	}
	mpd_del(r2);
	mpd_del(term);
	mpd_del(denom);
	return ok;
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
	mpd_t* r = mpd_qnew();
	mpd_t* s = mpd_qnew();
	mpd_t* c = mpd_qnew();
	qd_f128_t* result = NULL;
	int quadrant = 0;
	if (r != NULL && s != NULL && c != NULL && trig_reduce(r, &quadrant, x, &status)) {
		mpd_context_t ctx;
		ctx_work(&ctx, QDRT_WORK_PREC);
		// r was reduced at whatever precision the reduction needed; the series only
		// needs the working precision, and at x's size that is a great deal cheaper.
		mpd_qfinalize(r, &ctx, &status);
		const bool need_sin = kind != TRIG_COS || (quadrant & 1);
		const bool need_cos = kind != TRIG_SIN || (quadrant & 1);
		bool ok = (!need_sin || trig_series(s, r, true, QDRT_WORK_PREC, &status)) &&
				  (!need_cos || trig_series(c, r, false, QDRT_WORK_PREC, &status));
		if (ok) {
			// sin(r + n*pi/2) and cos(r + n*pi/2) by quadrant
			mpd_t* out = r;
			switch (kind) {
			case TRIG_SIN:
				mpd_qcopy(out, (quadrant & 1) ? c : s, &status);
				if (quadrant >= 2) {
					mpd_qcopy_negate(out, out, &status);
				}
				break;
			case TRIG_COS:
				mpd_qcopy(out, (quadrant & 1) ? s : c, &status);
				if (quadrant == 1 || quadrant == 2) {
					mpd_qcopy_negate(out, out, &status);
				}
				break;
			case TRIG_TAN:
				if (quadrant & 1) {
					mpd_qdiv(out, c, s, &ctx, &status);
					mpd_qcopy_negate(out, out, &status);
				} else {
					mpd_qdiv(out, s, c, &ctx, &status);
				}
				break;
			}
			result = box_from_work(out, status);
		}
	}
	mpd_del(r);
	mpd_del(s);
	mpd_del(c);
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
// |x| > 1 is folded to pi/2 - atan(1/|x|), then halved with
// atan(y) = 2*atan(y / (1 + sqrt(1 + y^2))) until the series converges quickly.
static bool atan_work(mpd_t* out, const mpd_t* x, uint32_t* status) {
	const mpd_ssize_t prec = QDRT_WORK_PREC;
	mpd_context_t ctx;
	ctx_work(&ctx, prec + 2);
	mpd_t* y = mpd_qnew();
	mpd_t* y2 = mpd_qnew();
	mpd_t* term = mpd_qnew();
	mpd_t* tmp = mpd_qnew();
	mpd_t* one = mpd_qnew();
	mpd_t* tenth = mpd_qnew();
	bool ok = y != NULL && y2 != NULL && term != NULL && tmp != NULL && one != NULL && tenth != NULL;
	if (ok) {
		mpd_qset_i64(one, 1, &ctx, status);
		mpd_qset_string(tenth, "0.1", &ctx, status);
		mpd_qcopy_abs(y, x, status);
		const bool invert = mpd_qcmp(y, one, status) > 0;
		if (invert) {
			mpd_qdiv(y, one, y, &ctx, status);
		}

		int halvings = 0;
		while (mpd_qcmp(y, tenth, status) > 0 && !(*status & MPD_Malloc_error)) {
			mpd_qmul(tmp, y, y, &ctx, status);
			mpd_qadd(tmp, tmp, one, &ctx, status);
			mpd_qsqrt(tmp, tmp, &ctx, status);
			mpd_qadd(tmp, tmp, one, &ctx, status);
			mpd_qdiv(y, y, tmp, &ctx, status);
			halvings++;
		}

		// y - y^3/3 + y^5/5 - ...
		mpd_qmul(y2, y, y, &ctx, status);
		mpd_qcopy(term, y, status);
		mpd_qcopy(out, y, status);
		for (int64_t k = 3; !(*status & MPD_Malloc_error); k += 2) {
			mpd_qmul(term, term, y2, &ctx, status);
			mpd_qcopy_negate(term, term, status);
			mpd_qset_i64(tmp, k, &ctx, status);
			mpd_qdiv(tmp, term, tmp, &ctx, status);
			if (mpd_iszero(tmp) || mpd_adjexp(tmp) < mpd_adjexp(out) - prec - 2) {
				break;
			}
			mpd_qadd(out, out, tmp, &ctx, status);
		}
		for (int i = 0; i < halvings; i++) {
			mpd_qadd(out, out, out, &ctx, status);
		}

		if (invert) {
			ok = pi_to(tmp, prec + 2, status);
			mpd_qset_i64(y, 2, &ctx, status);
			mpd_qdiv(tmp, tmp, y, &ctx, status);
			mpd_qsub(out, tmp, out, &ctx, status);
		}
		if (mpd_isnegative(x)) {
			mpd_qcopy_negate(out, out, status);
		}
		ok = ok && !(*status & MPD_Malloc_error);
	}
	mpd_del(y);
	mpd_del(y2);
	mpd_del(term);
	mpd_del(tmp);
	mpd_del(one);
	mpd_del(tenth);
	return ok;
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
