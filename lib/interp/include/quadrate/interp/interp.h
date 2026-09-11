/**
 * @file interp.h
 * @brief Interpreted execution of Quadrate source (lib/interp)
 *
 * Parses with lib/qc and walks the AST, calling lib/rt operations directly.
 * No LLVM and no toolchain at run time. See lib/interp/README.md.
 */

#ifndef QD_INTERP_INTERP_H
#define QD_INTERP_INTERP_H

#include <quadrate/rt/runtime.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief An interpreter and the stack it executes against
 */
typedef struct qd_interp qd_interp;

/**
 * @brief Type of a value read back from the stack
 */
typedef enum {
	QD_INTERP_VALUE_INT,   ///< 64-bit signed integer
	QD_INTERP_VALUE_FLOAT, ///< Double-precision float
	QD_INTERP_VALUE_STR,   ///< String
	QD_INTERP_VALUE_PTR	   ///< Pointer
} qd_interp_value_type;

/** @brief Capacity of qd_interp_value::text, including the terminator */
#define QD_INTERP_VALUE_TEXT_MAX 128

/**
 * @brief A stack value, with a rendering suitable for display
 */
typedef struct {
	qd_interp_value_type type;			 ///< Which member below is meaningful
	int64_t i;							 ///< Set when type is QD_INTERP_VALUE_INT
	double f;							 ///< Set when type is QD_INTERP_VALUE_FLOAT
	char text[QD_INTERP_VALUE_TEXT_MAX]; ///< Rendered form, always populated
} qd_interp_value;

/**
 * @brief Create an interpreter with a context of its own
 *
 * @param stack_size Stack capacity in elements
 * @return New interpreter, or NULL on allocation failure
 *
 * @note Destroy with qd_interp_destroy(), which also frees the context
 */
qd_interp* qd_interp_create(size_t stack_size);

/**
 * @brief Create an interpreter over an existing context
 *
 * Lets interpreted and compiled code share one stack.
 *
 * @param ctx Context to borrow; must outlive the interpreter
 * @return New interpreter, or NULL on allocation failure
 *
 * @note qd_interp_destroy() will not free a borrowed context
 */
qd_interp* qd_interp_attach(qd_context* ctx);

/**
 * @brief Destroy an interpreter
 *
 * Frees the context if it was created by qd_interp_create(). NULL is ignored.
 */
void qd_interp_destroy(qd_interp* interp);

/**
 * @brief Native function callable from interpreted Quadrate
 *
 * Structurally identical to lib/qd's qd_native_fn, so one C function can be
 * registered with either API without a cast.
 *
 * @param ctx Execution context; arguments are on its stack, results go back on it
 * @param userdata Pointer supplied at registration
 * @return 0 on success, non-zero to raise an error
 */
typedef int (*qd_interp_native_fn)(qd_context* ctx, void* userdata);

/**
 * @brief Make a C function callable by name from interpreted source
 *
 * The interpreter resolves a name it does not recognise as a builtin against
 * the functions registered here, so `display::clear` or `beep` in evaluated
 * source calls straight into @p fn. This is how an embedder exposes its own
 * capabilities — hardware, storage, anything — to code typed by a user.
 *
 * Registering the same name twice replaces the earlier entry.
 *
 * @param name      Name as written in source. May be scoped, e.g. "display::clear".
 * @param signature Stack effect in Quadrate syntax, e.g. "( -- )", "(x:f64 -- s:str)".
 *                  Only the inputs are read, to check the stack holds enough
 *                  before calling; pass NULL to skip that check.
 * @param fn        Function to call; must not be NULL
 * @param userdata  Passed to @p fn on every call
 * @return true if registered
 */
bool qd_interp_register(
		qd_interp* interp, const char* name, const char* signature, qd_interp_native_fn fn, void* userdata);

/**
 * @brief Number of native functions registered
 */
size_t qd_interp_registered_count(const qd_interp* interp);

/**
 * @brief Parse and execute Quadrate source
 *
 * @p source is a sequence of instructions and literals, written as it would
 * appear inside a function body — `2 3 +`, not a whole program. It executes
 * against the interpreter's persistent stack, so values left by one call are
 * visible to the next.
 *
 * On failure the stack is left as the error found it rather than rolled back:
 * a partially applied line is what a user needs to see to understand what went
 * wrong.
 *
 * @param source Null-terminated Quadrate source
 * @return true on success; on false see qd_interp_error()
 */
bool qd_interp_eval(qd_interp* interp, const char* source);

/**
 * @brief Message describing the most recent failure
 *
 * @return Message, or an empty string if the last call succeeded. Valid until
 *         the next qd_interp_eval() call, and never NULL.
 */
const char* qd_interp_error(const qd_interp* interp);

/**
 * @brief Number of values on the stack
 */
size_t qd_interp_depth(const qd_interp* interp);

/**
 * @brief Read a stack value without removing it
 *
 * @param index 0 is the top of the stack, 1 the element below it, and so on
 * @param[out] out Receives the value
 * @return true if @p index is in range
 */
bool qd_interp_peek(const qd_interp* interp, size_t index, qd_interp_value* out);

/**
 * @brief The context this interpreter executes against
 *
 * For embedders that need the runtime API directly, or that want to hand the
 * same context to lib/qd.
 *
 * @return The context, or NULL if @p interp is NULL
 */
qd_context* qd_interp_context(const qd_interp* interp);

#ifdef __cplusplus
}
#endif

#endif // QD_INTERP_INTERP_H
