/**
 * @file d128.c
 * @brief IEEE 754-2008 decimal128 values, on libmpdec
 *
 * A value is 128 bits of base-10^9 limbs. Each operation takes its operands apart into
 * libmpdec numbers on the C stack and puts the result back together, so nothing is
 * allocated for a value. libmpdec does the arithmetic, every operation in the IEEE
 * decimal128 context -- 34 digits, emax 6144, clamped, roundTiesToEven -- so each result
 * is the decimal128 value the standard says it is. The elementary functions libmpdec
 * lacks, or does slowly, are built here at a higher working precision.
 */

#include <quadrate/rt/d128.h>

#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <mpdecimal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "d128_pi.h"
#include "runtime_internal.h"

// Digits carried beyond decimal128's 34 while evaluating a function libmpdec lacks.
#define QDRT_GUARD_DIGITS 10
#define QDRT_WORK_PREC (34 + QDRT_GUARD_DIGITS)

// A libmpdec value on the C stack, with room for a decimal128 coefficient, for the
// double-width intermediate a product rounds from, and for the working precision. One
// that outgrows it moves to the heap by itself, so it still wants mpd_del.
#define QDRT_MPD_WORDS 12
#define QDRT_MPD_LOCAL(name)                                                                                           \
	mpd_uint_t name##_data[QDRT_MPD_WORDS];                                                                            \
	mpd_t name = {MPD_STATIC | MPD_STATIC_DATA, 0, 0, 0, QDRT_MPD_WORDS, name##_data}

// ---------------------------------------------------------------------------
// The encoding
//
// A coefficient of up to 34 digits is four base-10^9 limbs, the words libmpdec itself
// computes in on a 32-bit host, so taking a value apart and putting it back is moving
// words rather than regrouping digits. The top limb has at most seven digits and shares
// a 37-bit head with the exponent; the rest of the 128 bits are three 30-bit limbs and
// the sign:
//
//   w[1]: sign (1) | head (37) | limb 2, top 26 bits
//   w[0]: limb 2, low 4 bits | limb 1 (30) | limb 0 (30)
//
// A finite value's head is top * 12288 + e, where e is the exponent modulo 12288 -- the
// 12288 exponents from -6176 to 6111 each have one -- so that all-zero bits are a plain
// 0. 12288 is 3 * 2^12, which makes splitting the head a shift and a division by 3.
// Heads from 10^7 * 12288 up mark infinity and NaN, a NaN's payload in its limbs.
// ---------------------------------------------------------------------------

#define D128_SIGN (UINT64_C(1) << 63)
#define D128_HEAD_SHIFT 26
#define D128_HEAD_MASK ((UINT64_C(1) << 37) - 1)
#define D128_LIMB_MASK ((UINT64_C(1) << 30) - 1)
#define D128_EXPONENTS 12288
#define D128_TOP_LIMIT 10000000u // 10^7: the top limb's bound, and the stride of the special kinds
#define D128_HEAD_SPECIAL (UINT64_C(10000000) * D128_EXPONENTS)

enum {
	D128_KIND_INF,
	D128_KIND_NAN,
	D128_KIND_SNAN
};

static uint64_t head(qd_d128_t v) {
	return (v.w[1] >> D128_HEAD_SHIFT) & D128_HEAD_MASK;
}

static qd_d128_t encode(bool negative, uint64_t h, const uint32_t limbs[3]) {
	qd_d128_t r;
	r.w[1] = (negative ? D128_SIGN : 0) | (h << D128_HEAD_SHIFT) | (limbs[2] >> 4);
	r.w[0] = ((uint64_t)(limbs[2] & 0xF) << 60) | ((uint64_t)limbs[1] << 30) | limbs[0];
	return r;
}

static qd_d128_t special(bool negative, unsigned kind) {
	static const uint32_t none[3] = {0, 0, 0};
	return encode(negative, D128_HEAD_SPECIAL + (uint64_t)kind * D128_TOP_LIMIT, none);
}

// The context every result is rounded in: what mpd_ieee_context(ctx, 128) sets, as a
// constant to copy rather than a call on every operation
static const mpd_context_t decimal128_context = {
		34,
		6144,
		-6143,
		0,
		0,
		0,
		MPD_ROUND_HALF_EVEN,
		1,
		1,
};

static void ctx128(mpd_context_t* ctx) {
	*ctx = decimal128_context;
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

// Running out of memory ends the process, as an allocation failure does across the
// runtime: there is no value left to hand back.
static _Noreturn void out_of_memory(void) {
	static const char message[] = "fatal: out of memory computing a d128\n";
	(void)!write(2, message, sizeof message - 1);
	qdrt_fatal_exit();
}

static void check_memory(uint32_t status) {
	if (status & MPD_Malloc_error) {
		out_of_memory();
	}
}

// A coefficient of up to 36 digits as four base-10^9 words, least significant first,
// from libmpdec's words and back. Both are powers of ten, so this is regrouping digits.
static void chunks_from_mpd(const mpd_t* v, uint32_t c[4]) {
#if MPD_RDIGITS == 9
	for (mpd_ssize_t i = 0; i < 4; i++) {
		c[i] = i < v->len ? (uint32_t)v->data[i] : 0;
	}
#else
	// A special value libmpdec made may have no words at all: a NaN from 0/0 has length 0
	const uint64_t w0 = v->len > 0 ? (uint64_t)v->data[0] : 0;
	const uint64_t w1 = v->len > 1 ? (uint64_t)v->data[1] : 0;
	c[0] = (uint32_t)(w0 % 1000000000u);
	c[1] = (uint32_t)(w0 / 1000000000u % 1000000000u);
	c[2] = (uint32_t)(w0 / UINT64_C(1000000000000000000) + w1 % 100000000u * 10u);
	c[3] = (uint32_t)(w1 / 100000000u);
#endif
}

static void mpd_from_chunks(mpd_t* v, const uint32_t c[4], uint32_t* status) {
#if MPD_RDIGITS == 9
	const mpd_ssize_t words = 4;
#else
	const mpd_ssize_t words = 2;
#endif
	if (v->alloc < words && !mpd_qresize(v, words, status)) {
		return;
	}
#if MPD_RDIGITS == 9
	for (mpd_ssize_t i = 0; i < 4; i++) {
		v->data[i] = c[i];
	}
#else
	v->data[0] = c[0] + (uint64_t)c[1] * 1000000000u + (uint64_t)(c[2] % 10u) * UINT64_C(1000000000000000000);
	v->data[1] = c[2] / 10u + (uint64_t)c[3] * 100000000u;
#endif
	v->len = words;
	while (v->len > 1 && v->data[v->len - 1] == 0) {
		v->len--;
	}
	mpd_setdigits(v);
}

static mpd_ssize_t digits_in(mpd_uint_t word) {
	mpd_ssize_t n = 1;
	while (word >= 10) {
		word /= 10;
		n++;
	}
	return n;
}

// The coefficient from four limbs, written straight into libmpdec's words: what
// mpd_from_chunks does, without the calls, as every operand of every operation comes
// through here
static void set_coefficient(mpd_t* v, const uint32_t c[4]) {
#if MPD_RDIGITS == 9
	const mpd_ssize_t words = 4;
#else
	const mpd_ssize_t words = 2;
#endif
	if (v->alloc < words) {
		uint32_t status = 0;
		mpd_from_chunks(v, c, &status);
		check_memory(status);
		return;
	}
#if MPD_RDIGITS == 9
	v->data[0] = c[0];
	v->data[1] = c[1];
	v->data[2] = c[2];
	v->data[3] = c[3];
	v->len = c[3] != 0 ? 4 : (c[2] != 0 ? 3 : (c[1] != 0 ? 2 : 1));
#else
	v->data[0] = c[0] + (uint64_t)c[1] * 1000000000u + (uint64_t)(c[2] % 10u) * UINT64_C(1000000000000000000);
	v->data[1] = c[2] / 10u + (uint64_t)c[3] * 100000000u;
	v->len = v->data[1] != 0 ? 2 : 1;
#endif
	v->digits = (v->len - 1) * MPD_RDIGITS + digits_in(v->data[v->len - 1]);
}

// A local the operations wrote into goes back to the heap only if it went there
static void release_local(mpd_t* v) {
	if (!(v->flags & MPD_STATIC_DATA)) {
		mpd_del(v);
	}
}

// The value an encoding holds, into `out`
static void unpack(qd_d128_t v, mpd_t* out) {
	const uint8_t sign = (v.w[1] & D128_SIGN) ? MPD_NEG : MPD_POS;
	const uint64_t h = head(v);
	uint32_t c[4] = {
			(uint32_t)(v.w[0] & D128_LIMB_MASK),
			(uint32_t)((v.w[0] >> 30) & D128_LIMB_MASK),
			(uint32_t)(((v.w[1] & ((UINT64_C(1) << D128_HEAD_SHIFT) - 1)) << 4) | (v.w[0] >> 60)),
			0,
	};
	uint8_t kind = 0;
	mpd_ssize_t exp = 0;
	if (h >= D128_HEAD_SPECIAL) {
		const uint64_t rest = h - D128_HEAD_SPECIAL;
		if (rest < D128_TOP_LIMIT) {
			mpd_setspecial(out, sign, MPD_INF);
			return;
		}
		kind = rest < 2 * (uint64_t)D128_TOP_LIMIT ? MPD_NAN : MPD_SNAN;
		c[3] = (uint32_t)(rest % D128_TOP_LIMIT);
		// A payload has at most 33 digits; one longer is not canonical, and reads as none
		if (c[3] >= D128_TOP_LIMIT / 10) {
			c[0] = c[1] = c[2] = c[3] = 0;
		}
	} else {
		c[3] = (uint32_t)(h >> 12) / 3;
		const uint32_t e = (uint32_t)(h - (uint64_t)c[3] * D128_EXPONENTS);
		exp = e < 6112 ? (mpd_ssize_t)e : (mpd_ssize_t)e - D128_EXPONENTS;
	}
	// Thirty bits hold more than a limb can: a value with a larger one is not canonical,
	// and its coefficient reads as zero
	if (c[0] >= 1000000000u || c[1] >= 1000000000u || c[2] >= 1000000000u) {
		c[0] = c[1] = c[2] = c[3] = 0;
	}
	set_coefficient(out, c);
	out->flags = (uint8_t)((out->flags & (MPD_STATIC | MPD_DATAFLAGS)) | sign | kind);
	out->exp = exp;
}

// The encoding of `v`, which must already be a decimal128 value: at most 34 digits, an
// exponent in range, a NaN payload of at most 33.
static qd_d128_t pack(const mpd_t* v) {
	const bool negative = mpd_isnegative(v);
	if (mpd_isinfinite(v)) {
		return special(negative, D128_KIND_INF);
	}
	uint32_t c[4];
	chunks_from_mpd(v, c);
	uint64_t h;
	if (mpd_isnan(v)) {
		h = D128_HEAD_SPECIAL + (uint64_t)(mpd_issnan(v) ? D128_KIND_SNAN : D128_KIND_NAN) * D128_TOP_LIMIT + c[3];
	} else {
		const mpd_ssize_t e = v->exp < 0 ? v->exp + D128_EXPONENTS : v->exp;
		h = (uint64_t)c[3] * D128_EXPONENTS + (uint64_t)e;
	}
	return encode(negative, h, c);
}

// A result libmpdec computed in the decimal128 context, encoded
static qd_d128_t pack_result(mpd_t* v, uint32_t status) {
	check_memory(status);
	const qd_d128_t r = pack(v);
	release_local(v);
	return r;
}

// A working-precision value rounded once into decimal128 and encoded. mpd_qfinalize
// rather than mpd_qplus, which would turn -0 into +0.
static qd_d128_t pack_work(const mpd_t* value, uint32_t status) {
	mpd_context_t ctx;
	ctx128(&ctx);
	QDRT_MPD_LOCAL(r);
	mpd_qcopy(&r, value, &status);
	mpd_qfinalize(&r, &ctx, &status);
	return pack_result(&r, status);
}

// ---------------------------------------------------------------------------
// Conversions
// ---------------------------------------------------------------------------

bool qd_d128_parse(const char* text, qd_d128_t* out) {
	if (text == NULL || out == NULL) {
		return false;
	}
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(v);
	mpd_qset_string(&v, text, &ctx, &status);
	// A signaling NaN is a value no operation here produces or tests for, so reading one
	// would only hand the program something it cannot deal with.
	const bool ok = !(status & MPD_Conversion_syntax) && !mpd_issnan(&v);
	if (ok) {
		*out = pack_result(&v, status);
	} else {
		mpd_del(&v);
	}
	return ok;
}

qd_d128_t qd_d128_from_i64(int64_t v) {
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(r);
	mpd_qset_i64(&r, v, &ctx, &status);
	return pack_result(&r, status);
}

qd_d128_t qd_d128_from_f64(double v) {
	if (isnan(v)) {
		return special(signbit(v), D128_KIND_NAN);
	}
	if (isinf(v)) {
		return special(v < 0, D128_KIND_INF);
	}

	// %.33e writes the binary value correctly rounded to 34 significant digits -- glibc and
	// musl both format exactly -- which is the conversion IEEE asks for.
	char digits[64];
	snprintf(digits, sizeof digits, "%.33e", v);

	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(exact);
	QDRT_MPD_LOCAL(tidy);
	mpd_qset_string(&exact, digits, &ctx, &status);

	// That spelling always carries 34 digits, so 1.0 would arrive as 1.000...000 and print
	// that way. Give it the quantum a literal would have had: no trailing fractional zeros,
	// and an integer written as an integer.
	mpd_qreduce(&tidy, &exact, &ctx, &status);
	if (tidy.exp > 0 && tidy.digits + tidy.exp <= ctx.prec) {
		mpd_qrescale(&exact, &tidy, 0, &ctx, &status);
	} else {
		mpd_qcopy(&exact, &tidy, &status);
	}
	mpd_del(&tidy);
	const qd_d128_t r = pack_work(&exact, status);
	mpd_del(&exact);
	return r;
}

int64_t qd_d128_to_i64(qd_d128_t v) {
	QDRT_MPD_LOCAL(a);
	unpack(v, &a);
	if (mpd_isnan(&a)) {
		return 0;
	}
	const bool negative = mpd_isnegative(&a);
	if (mpd_isinfinite(&a) || mpd_adjexp(&a) > 18) {
		return negative ? INT64_MIN : INT64_MAX;
	}
	mpd_context_t ctx;
	ctx_work(&ctx, 40);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(whole);
	mpd_qtrunc(&whole, &a, &ctx, &status);
	check_memory(status);
	status = 0;
	const int64_t result = mpd_qget_i64(&whole, &status);
	mpd_del(&whole);
	if (status & MPD_Invalid_operation) {
		return negative ? INT64_MIN : INT64_MAX;
	}
	return result;
}

double qd_d128_to_f64(qd_d128_t v) {
	// Formatting and reading back is correctly rounded: at most 34 digits go in, and
	// strtod rounds whatever it reads to the nearest double.
	char text[QD_D128_STRING_MAX];
	qd_d128_format(v, text, sizeof text);
	return strtod(text, NULL);
}

// The IEEE 754 to-scientific-string rule: the coefficient's digits with the point placed
// by the exponent while the number reads plainly -- a non-positive exponent and an
// adjusted exponent of -6 or more -- and otherwise one digit, the rest after a point,
// and the adjusted exponent.
size_t qd_d128_format(qd_d128_t v, char* buf, size_t size) {
	if (buf == NULL || size == 0) {
		return 0;
	}
	const bool negative = (v.w[1] & D128_SIGN) != 0;
	if (qd_d128_is_nan(v) || qd_d128_is_inf(v)) {
		const char* name = qd_d128_is_nan(v) ? "nan" : "inf";
		const int n = snprintf(buf, size, "%s%s", negative ? "-" : "", name);
		return n < 0 ? 0 : ((size_t)n < size ? (size_t)n : size - 1);
	}

	QDRT_MPD_LOCAL(a);
	unpack(v, &a);
	uint32_t c[4];
	chunks_from_mpd(&a, c);
	char digits[40];
	if (c[3] != 0) {
		snprintf(digits, sizeof digits, "%" PRIu32 "%09" PRIu32 "%09" PRIu32 "%09" PRIu32, c[3], c[2], c[1], c[0]);
	} else if (c[2] != 0) {
		snprintf(digits, sizeof digits, "%" PRIu32 "%09" PRIu32 "%09" PRIu32, c[2], c[1], c[0]);
	} else if (c[1] != 0) {
		snprintf(digits, sizeof digits, "%" PRIu32 "%09" PRIu32, c[1], c[0]);
	} else {
		snprintf(digits, sizeof digits, "%" PRIu32, c[0]);
	}
	const int len = (int)strlen(digits);
	const long exp = (long)a.exp;
	const long adjusted = exp + len - 1;

	char text[QD_D128_STRING_MAX];
	int n;
	if (exp <= 0 && adjusted >= -6) {
		if (exp == 0) {
			n = snprintf(text, sizeof text, "%s%s", negative ? "-" : "", digits);
		} else if (len > -exp) {
			const int whole = len + (int)exp;
			n = snprintf(text, sizeof text, "%s%.*s.%s", negative ? "-" : "", whole, digits, digits + whole);
		} else {
			// At most five zeros: the adjusted exponent is -6 or more
			n = snprintf(text, sizeof text, "%s0.%.*s%s", negative ? "-" : "", (int)(-exp - len), "00000", digits);
		}
	} else {
		n = snprintf(text, sizeof text,
				"%s%c%s%s"
				"e%c%ld",
				negative ? "-" : "", digits[0], len > 1 ? "." : "", digits + 1, adjusted < 0 ? '-' : '+',
				adjusted < 0 ? -adjusted : adjusted);
	}
	if (n < 0) {
		buf[0] = '\0';
		return 0;
	}
	const size_t written = (size_t)n < size ? (size_t)n : size - 1;
	memcpy(buf, text, written);
	buf[written] = '\0';
	return written;
}

// ---------------------------------------------------------------------------
// Classification and comparison
// ---------------------------------------------------------------------------

bool qd_d128_is_nan(qd_d128_t v) {
	return head(v) >= D128_HEAD_SPECIAL + D128_TOP_LIMIT;
}

bool qd_d128_is_inf(qd_d128_t v) {
	return head(v) >= D128_HEAD_SPECIAL && head(v) < D128_HEAD_SPECIAL + D128_TOP_LIMIT;
}

bool qd_d128_is_finite(qd_d128_t v) {
	return head(v) < D128_HEAD_SPECIAL;
}

bool qd_d128_is_zero(qd_d128_t v) {
	QDRT_MPD_LOCAL(a);
	unpack(v, &a);
	return mpd_iszero(&a);
}

bool qd_d128_is_normal(qd_d128_t v) {
	mpd_context_t ctx;
	ctx128(&ctx);
	QDRT_MPD_LOCAL(a);
	unpack(v, &a);
	return mpd_isnormal(&a, &ctx);
}

bool qd_d128_is_subnormal(qd_d128_t v) {
	mpd_context_t ctx;
	ctx128(&ctx);
	QDRT_MPD_LOCAL(a);
	unpack(v, &a);
	return mpd_issubnormal(&a, &ctx);
}

bool qd_d128_is_signed(qd_d128_t v) {
	return (v.w[1] & D128_SIGN) != 0;
}

int qd_d128_compare(qd_d128_t a, qd_d128_t b) {
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(y);
	unpack(a, &x);
	unpack(b, &y);
	uint32_t status = 0;
	const int c = mpd_qcmp(&x, &y, &status);
	return c == INT_MAX ? 2 : c;
}

int qd_d128_total_order(qd_d128_t a, qd_d128_t b) {
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(y);
	unpack(a, &x);
	unpack(b, &y);
	return mpd_cmp_total(&x, &y);
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

#define QDRT_D128_UNARY(name, call)                                                                                    \
	qd_d128_t qd_d128_##name(qd_d128_t a) {                                                                            \
		mpd_context_t ctx;                                                                                             \
		ctx128(&ctx);                                                                                                  \
		uint32_t status = 0;                                                                                           \
		QDRT_MPD_LOCAL(x);                                                                                             \
		QDRT_MPD_LOCAL(r);                                                                                             \
		unpack(a, &x);                                                                                                 \
		call(&r, &x, &ctx, &status);                                                                                   \
		return pack_result(&r, status);                                                                                \
	}

#define QDRT_D128_BINARY(name, call)                                                                                   \
	qd_d128_t qd_d128_##name(qd_d128_t a, qd_d128_t b) {                                                               \
		mpd_context_t ctx;                                                                                             \
		ctx128(&ctx);                                                                                                  \
		uint32_t status = 0;                                                                                           \
		QDRT_MPD_LOCAL(x);                                                                                             \
		QDRT_MPD_LOCAL(y);                                                                                             \
		QDRT_MPD_LOCAL(r);                                                                                             \
		unpack(a, &x);                                                                                                 \
		unpack(b, &y);                                                                                                 \
		call(&r, &x, &y, &ctx, &status);                                                                               \
		return pack_result(&r, status);                                                                                \
	}

QDRT_D128_BINARY(add, mpd_qadd)
QDRT_D128_BINARY(sub, mpd_qsub)
QDRT_D128_BINARY(mul, mpd_qmul)
QDRT_D128_BINARY(div, mpd_qdiv)
QDRT_D128_BINARY(remainder, mpd_qrem_near)
QDRT_D128_BINARY(fmod, mpd_qrem)
QDRT_D128_BINARY(min, mpd_qmin)
QDRT_D128_BINARY(max, mpd_qmax)
QDRT_D128_BINARY(quantize, mpd_qquantize)

QDRT_D128_UNARY(logb, mpd_qlogb)
QDRT_D128_UNARY(next_up, mpd_qnext_plus)
QDRT_D128_UNARY(next_down, mpd_qnext_minus)
QDRT_D128_UNARY(reduce, mpd_qreduce)
QDRT_D128_UNARY(trunc, mpd_qtrunc)
QDRT_D128_UNARY(floor, mpd_qfloor)
QDRT_D128_UNARY(ceil, mpd_qceil)
QDRT_D128_UNARY(sqrt, mpd_qsqrt)

qd_d128_t qd_d128_fma(qd_d128_t a, qd_d128_t b, qd_d128_t c) {
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(y);
	QDRT_MPD_LOCAL(z);
	QDRT_MPD_LOCAL(r);
	unpack(a, &x);
	unpack(b, &y);
	unpack(c, &z);
	mpd_qfma(&r, &x, &y, &z, &ctx, &status);
	return pack_result(&r, status);
}

// negate, abs and copySign are sign-bit operations in IEEE 754 -- quiet, and exact even
// for NaN and zero -- and here they are the top bit and nothing else.
qd_d128_t qd_d128_neg(qd_d128_t a) {
	a.w[1] ^= D128_SIGN;
	return a;
}

qd_d128_t qd_d128_abs(qd_d128_t a) {
	a.w[1] &= ~D128_SIGN;
	return a;
}

qd_d128_t qd_d128_copysign(qd_d128_t a, qd_d128_t sign) {
	a.w[1] = (a.w[1] & ~D128_SIGN) | (sign.w[1] & D128_SIGN);
	return a;
}

qd_d128_t qd_d128_scaleb(qd_d128_t a, int64_t n) {
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(scale);
	QDRT_MPD_LOCAL(r);
	unpack(a, &x);
	// scaleb wants its operand within +-2*(emax+prec), and anything past that overflows or
	// underflows completely anyway, so clamping loses nothing.
	const int64_t limit = 2 * (6144 + 34) + 1;
	mpd_qset_i64(&scale, n > limit ? limit : (n < -limit ? -limit : n), &ctx, &status);
	mpd_qscaleb(&r, &x, &scale, &ctx, &status);
	return pack_result(&r, status);
}

static qd_d128_t round_with(qd_d128_t a, int mode) {
	mpd_context_t ctx;
	ctx128(&ctx);
	ctx.round = mode;
	uint32_t status = 0;
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(r);
	unpack(a, &x);
	mpd_qround_to_int(&r, &x, &ctx, &status);
	return pack_result(&r, status);
}

qd_d128_t qd_d128_round_even(qd_d128_t a) {
	return round_with(a, MPD_ROUND_HALF_EVEN);
}

qd_d128_t qd_d128_round(qd_d128_t a) {
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
	// "3." and then the digits after the 3, one more than are kept. Only reducing a very
	// large trigonometric argument wants more than fit here.
	const size_t want = (size_t)prec + 1;
	char small[128];
	char* text = want + 2 <= sizeof small ? small : malloc(want + 2);
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
	if (text != small) {
		free(text);
	}
	return !(*status & MPD_Malloc_error);
}

qd_d128_t qd_d128_pi(void) {
	QDRT_MPD_LOCAL(pi);
	uint32_t status = 0;
	if (!pi_to(&pi, 34, &status)) {
		out_of_memory();
	}
	const qd_d128_t r = pack_work(&pi, status);
	mpd_del(&pi);
	return r;
}

qd_d128_t qd_d128_e(void) {
	return qd_d128_exp(qd_d128_from_i64(1));
}

qd_d128_t qd_d128_inf(void) {
	return special(false, D128_KIND_INF);
}

qd_d128_t qd_d128_nan(void) {
	return special(false, D128_KIND_NAN);
}

// ---------------------------------------------------------------------------
// Trigonometry
//
// Everything below works at a working precision, in fixed-point decimals or bare mpd_t,
// and rounds once, at the end, into decimal128. Each helper returns false only when
// memory runs out.
// ---------------------------------------------------------------------------

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

static const uint32_t pow10_u32[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

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
	QDRT_MPD_LOCAL(halfpi_v);
	QDRT_MPD_LOCAL(k_v);
	QDRT_MPD_LOCAL(tmp_v);
	mpd_t* halfpi = &halfpi_v;
	mpd_t* k = &k_v;
	mpd_t* tmp = &tmp_v;
	bool ok = true;
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

enum trig_kind {
	TRIG_SIN,
	TRIG_COS,
	TRIG_TAN
};

static qd_d128_t trig(qd_d128_t a, enum trig_kind kind) {
	QDRT_MPD_LOCAL(xa);
	unpack(a, &xa);
	const mpd_t* x = &xa;
	if (mpd_isnan(x) || mpd_isinfinite(x)) {
		// sin(inf) has no value: invalid, and a quiet NaN.
		return qd_d128_nan();
	}
	if (mpd_iszero(x) && kind != TRIG_COS) {
		// sin and tan are odd: -0 stays -0.
		return a;
	}

	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(r);
	QDRT_MPD_LOCAL(t);
	QDRT_MPD_LOCAL(sn);
	QDRT_MPD_LOCAL(cn);
	QDRT_MPD_LOCAL(den);
	qd_d128_t result = {{0, 0}};
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
		result = pack_work(out, status);
	}
	mpd_del(&r);
	mpd_del(&t);
	mpd_del(&sn);
	mpd_del(&cn);
	mpd_del(&den);
	if (!ok) {
		out_of_memory();
	}
	return result;
}

qd_d128_t qd_d128_sin(qd_d128_t a) {
	return trig(a, TRIG_SIN);
}

qd_d128_t qd_d128_cos(qd_d128_t a) {
	return trig(a, TRIG_COS);
}

qd_d128_t qd_d128_tan(qd_d128_t a) {
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
	QDRT_MPD_LOCAL(tmp_v);
	mpd_t* tmp = &tmp_v;
	if (!pi_to(out, QDRT_WORK_PREC + 2, status)) {
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

qd_d128_t qd_d128_atan(qd_d128_t a) {
	QDRT_MPD_LOCAL(xa);
	unpack(a, &xa);
	const mpd_t* x = &xa;
	if (mpd_isnan(x)) {
		return qd_d128_nan();
	}
	if (mpd_iszero(x)) {
		return a;
	}
	uint32_t status = 0;
	QDRT_MPD_LOCAL(out);
	const bool ok =
			mpd_isinfinite(x) ? pi_fraction(&out, 1, 2, mpd_isnegative(x), &status) : atan_work(&out, x, &status);
	if (!ok) {
		out_of_memory();
	}
	const qd_d128_t result = pack_work(&out, status);
	mpd_del(&out);
	return result;
}

// asin(x) = atan(x / sqrt((1-x)(1+x))). (1-x)(1+x) rather than 1-x^2: near |x| = 1
// the latter cancels away the digits the answer depends on.
qd_d128_t qd_d128_asin(qd_d128_t a) {
	QDRT_MPD_LOCAL(xa);
	unpack(a, &xa);
	const mpd_t* x = &xa;
	if (mpd_isnan(x)) {
		return qd_d128_nan();
	}
	if (mpd_iszero(x)) {
		return a;
	}
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(one);
	QDRT_MPD_LOCAL(p);
	QDRT_MPD_LOCAL(q);
	qd_d128_t result;
	bool ok = true;
	mpd_qset_i64(&one, 1, &ctx, &status);
	mpd_qcopy_abs(&p, x, &status);
	const int against_one = mpd_qcmp(&p, &one, &status);
	if (against_one > 0) {
		result = qd_d128_nan();
	} else if (against_one == 0) {
		ok = pi_fraction(&p, 1, 2, mpd_isnegative(x), &status);
		result = pack_work(&p, status);
	} else {
		mpd_qsub(&p, &one, x, &ctx, &status);
		mpd_qadd(&q, &one, x, &ctx, &status);
		mpd_qmul(&p, &p, &q, &ctx, &status);
		mpd_qsqrt(&p, &p, &ctx, &status);
		mpd_qdiv(&q, x, &p, &ctx, &status);
		ok = atan_work(&p, &q, &status);
		result = pack_work(&p, status);
	}
	mpd_del(&one);
	mpd_del(&p);
	mpd_del(&q);
	if (!ok) {
		out_of_memory();
	}
	return result;
}

// acos(x) = 2*atan(sqrt((1-x)/(1+x))), which keeps its digits near x = 1 where
// pi/2 - asin(x) would cancel them.
qd_d128_t qd_d128_acos(qd_d128_t a) {
	QDRT_MPD_LOCAL(xa);
	unpack(a, &xa);
	const mpd_t* x = &xa;
	if (mpd_isnan(x)) {
		return qd_d128_nan();
	}
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(one);
	QDRT_MPD_LOCAL(p);
	QDRT_MPD_LOCAL(q);
	qd_d128_t result;
	bool ok = true;
	mpd_qset_i64(&one, 1, &ctx, &status);
	mpd_qcopy_abs(&p, x, &status);
	const int against_one = mpd_qcmp(&p, &one, &status);
	if (against_one > 0) {
		result = qd_d128_nan();
	} else if (against_one == 0 && mpd_isnegative(x)) {
		ok = pi_fraction(&p, 1, 1, false, &status);
		result = pack_work(&p, status);
	} else if (against_one == 0) {
		result = qd_d128_from_i64(0);
	} else {
		mpd_qsub(&p, &one, x, &ctx, &status);
		mpd_qadd(&q, &one, x, &ctx, &status);
		mpd_qdiv(&p, &p, &q, &ctx, &status);
		mpd_qsqrt(&p, &p, &ctx, &status);
		ok = atan_work(&q, &p, &status);
		mpd_qadd(&q, &q, &q, &ctx, &status);
		result = pack_work(&q, status);
	}
	mpd_del(&one);
	mpd_del(&p);
	mpd_del(&q);
	if (!ok) {
		out_of_memory();
	}
	return result;
}

// atan2 with the special cases of C99 Annex F, which IEEE 754-2008 section 9.2.1 follows.
qd_d128_t qd_d128_atan2(qd_d128_t ya, qd_d128_t xa) {
	QDRT_MPD_LOCAL(yv);
	QDRT_MPD_LOCAL(xv);
	unpack(ya, &yv);
	unpack(xa, &xv);
	const mpd_t* y = &yv;
	const mpd_t* x = &xv;
	if (mpd_isnan(y) || mpd_isnan(x)) {
		return qd_d128_nan();
	}
	const bool yneg = mpd_isnegative(y);
	const bool xneg = mpd_isnegative(x);
	if (mpd_iszero(y) && !xneg) {
		// atan2(+-0, +0 or positive) is +-0
		return ya;
	}
	if (mpd_isinfinite(x) && !xneg && !mpd_isinfinite(y)) {
		// atan2(finite, +inf) is +-0
		const qd_d128_t zero = qd_d128_from_i64(0);
		return yneg ? qd_d128_neg(zero) : zero;
	}

	uint32_t status = 0;
	QDRT_MPD_LOCAL(out);
	bool ok = true;
	if (mpd_iszero(y)) {
		ok = pi_fraction(&out, 1, 1, yneg, &status);
	} else if (mpd_iszero(x)) {
		ok = pi_fraction(&out, 1, 2, yneg, &status);
	} else if (mpd_isinfinite(y)) {
		if (mpd_isinfinite(x)) {
			ok = pi_fraction(&out, xneg ? 3 : 1, 4, yneg, &status);
		} else {
			ok = pi_fraction(&out, 1, 2, yneg, &status);
		}
	} else if (mpd_isinfinite(x)) {
		ok = pi_fraction(&out, 1, 1, yneg, &status);
	} else {
		mpd_context_t ctx;
		ctx_work(&ctx, QDRT_WORK_PREC + 2);
		QDRT_MPD_LOCAL(ratio);
		QDRT_MPD_LOCAL(pi);
		mpd_qdiv(&ratio, y, x, &ctx, &status);
		mpd_qcopy_abs(&ratio, &ratio, &status);
		ok = atan_work(&out, &ratio, &status);
		if (ok && xneg) {
			// The angle lies in the left half-plane: pi - atan(|y/x|)
			ok = pi_to(&pi, QDRT_WORK_PREC + 2, &status);
			mpd_qsub(&out, &pi, &out, &ctx, &status);
		}
		if (ok && yneg) {
			mpd_qcopy_negate(&out, &out, &status);
		}
		mpd_del(&ratio);
		mpd_del(&pi);
	}
	if (!ok) {
		out_of_memory();
	}
	const qd_d128_t result = pack_work(&out, status);
	mpd_del(&out);
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

// r rounded into decimal128 in *out, when no value within 10^-42 of r rounds
// differently. False otherwise, with *out untouched.
//
// *out is the rounded bound rather than r: the functions that come here are
// inexact, so their result has all 34 digits, even where r happens to be a short
// value such as 1E+3.
static bool round_if_decided(const mpd_t* r, uint32_t status, qd_d128_t* out) {
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
	check_memory(status);
	const bool decided = mpd_cmp_total(&lo, &hi) == 0;
	if (decided) {
		*out = pack(&lo);
	}
	mpd_del(&slack);
	mpd_del(&lo);
	mpd_del(&hi);
	return decided;
}

// An inexact result given every digit decimal128 has, as IEEE 754 section 5.2 asks and
// round_if_decided does. libmpdec leaves some short where its own working happened to
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

static qd_d128_t correctly_rounded(qd_d128_t a, void (*call)(mpd_t*, const mpd_t*, const mpd_context_t*, uint32_t*)) {
	mpd_context_t ctx;
	ctx128(&ctx);
	uint32_t status = 0;
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(r);
	unpack(a, &x);
	call(&r, &x, &ctx, &status);
	pad_inexact(&r, &status);
	return pack_result(&r, status);
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

qd_d128_t qd_d128_exp(qd_d128_t a) {
	uint32_t status = 0;
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(r);
	unpack(a, &x);
	qd_d128_t result;
	bool decided = false;
	// exp(0) is exactly 1
	if (!mpd_iszero(&x) && exp_work(&r, &x, &status)) {
		decided = round_if_decided(&r, status, &result);
	}
	mpd_del(&r);
	return decided ? result : correctly_rounded(a, mpd_qexp);
}

qd_d128_t qd_d128_ln(qd_d128_t a) {
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(r);
	unpack(a, &x);
	qd_d128_t result;
	bool decided = false;
	// ln 1 is exactly 0
	mpd_qset_i64(&r, 1, &ctx, &status);
	if (is_positive_finite(&x) && mpd_qcmp(&x, &r, &status) != 0 && ln_work(&r, &x, &status)) {
		decided = round_if_decided(&r, status, &result);
	}
	mpd_del(&r);
	return decided ? result : correctly_rounded(a, mpd_qln);
}

qd_d128_t qd_d128_log10(qd_d128_t a) {
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 16);
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(r);
	QDRT_MPD_LOCAL(ln10);
	unpack(a, &x);
	qd_d128_t result;
	bool decided = false;
	if (is_positive_finite(&x)) {
		// The log of a power of ten is an exact integer, which libmpdec gives as one
		mpd_qreduce(&r, &x, &ctx, &status);
		const bool power_of_ten = r.digits == 1 && r.data[0] == 1;
		if (!power_of_ten && ln_work(&r, &x, &status)) {
			mpd_qset_string(&ln10, QDRT_LN10, &wide, &status);
			mpd_qdiv(&r, &r, &ln10, &wide, &status);
			decided = round_if_decided(&r, status, &result);
		}
	}
	mpd_del(&r);
	mpd_del(&ln10);
	return decided ? result : correctly_rounded(a, mpd_qlog10);
}

qd_d128_t qd_d128_pow(qd_d128_t base, qd_d128_t exponent) {
	uint32_t status = 0;
	mpd_context_t ctx;
	ctx_work(&ctx, QDRT_WORK_PREC + 2);
	mpd_context_t wide;
	ctx_work(&wide, QDRT_WORK_PREC + 16);
	QDRT_MPD_LOCAL(x);
	QDRT_MPD_LOCAL(y);
	QDRT_MPD_LOCAL(r);
	unpack(base, &x);
	unpack(exponent, &y);
	qd_d128_t result;
	bool decided = false;
	// x^y = exp(y ln x), for x > 0 and y not an integer. Integer powers are exact where
	// they can be, which libmpdec works out by multiplying, and 1^y is exactly 1.
	mpd_qset_i64(&r, 1, &ctx, &status);
	if (is_positive_finite(&x) && mpd_qcmp(&x, &r, &status) != 0 && !mpd_isspecial(&y) && !mpd_isinteger(&y) &&
			ln_work(&r, &x, &status)) {
		mpd_qmul(&r, &r, &y, &wide, &status);
		if (exp_work(&r, &r, &status)) {
			decided = round_if_decided(&r, status, &result);
		}
	}
	if (!decided) {
		ctx128(&ctx);
		status = 0;
		mpd_qpow(&r, &x, &y, &ctx, &status);
		pad_inexact(&r, &status);
		return pack_result(&r, status);
	}
	mpd_del(&r);
	return result;
}
