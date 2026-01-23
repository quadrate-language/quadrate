/**
 * @file qd.h
 * @brief High-level Quadrate embedding API
 *
 * Provides a high-level API for embedding Quadrate into C/C++ applications.
 * This includes module management, script compilation, and function execution.
 *
 * @note For low-level runtime operations, see qdrt/runtime.h
 */

#ifndef QD_QD_QD_H
#define QD_QD_QD_H

#include <qdrt/exec_result.h>
#include <qdrt/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque module structure
 *
 * Represents a Quadrate module that can contain scripts and registered functions.
 * The internal structure is hidden to maintain encapsulation.
 */
typedef struct qd_module qd_module;

// Context management functions are now in qdrt/runtime.h (included above)

/**
 * @brief Get or create a module by name
 *
 * Retrieves an existing module with the given name, or creates a new one
 * if it doesn't exist.
 *
 * @param ctx Execution context
 * @param name Module name (must not be NULL)
 * @return Pointer to the module, or NULL on error
 *
 * @note Module lifetimes are managed internally
 */
qd_module* qd_get_module(qd_context* ctx, const char* name);

/**
 * @brief Add Quadrate source code to a module
 *
 * Adds script source code to the module. Multiple scripts can be added
 * to a single module. Scripts are compiled when qd_build() is called.
 *
 * @param mod Target module
 * @param script Quadrate source code (must not be NULL)
 *
 * @note The script string is copied; the caller retains ownership
 */
void qd_add_script(qd_module* mod, const char* script);

/**
 * @brief Register a native C function with the module
 *
 * Registers a C function that can be called from Quadrate code.
 * The function must follow Quadrate calling conventions (taking qd_context*
 * and returning int).
 *
 * @param mod Target module
 * @param name Function name as it appears in Quadrate code
 * @param fn Function pointer (must not be NULL)
 *
 * @note Function pointers are stored as void*, cast appropriately when calling
 */
void qd_register_function(qd_module* mod, const char* name, void (*fn)(void));

/**
 * @brief Compile all scripts added to the module
 *
 * Compiles all scripts that have been added to the module via qd_add_script().
 * This must be called before executing any functions from the module.
 *
 * @param mod Target module
 *
 * @note If compilation fails, errors are reported to stderr
 */
void qd_build(qd_module* mod);

/**
 * @brief Check if a module has been successfully compiled
 *
 * @param mod Target module
 * @return true if the module was compiled successfully, false otherwise
 */
bool qd_is_compiled(qd_module* mod);

/**
 * @brief Set minimum line for warnings during compilation
 *
 * Warnings for lines before the minimum line are suppressed during compilation.
 * This is useful for REPL-style incremental compilation where previous code
 * has already been validated and we don't want to re-show warnings for it.
 *
 * @param mod Target module
 * @param line Minimum line number (1-based, 0 means no suppression)
 */
void qd_set_warning_min_line(qd_module* mod, size_t line);

/**
 * @brief Execute Quadrate code
 *
 * Executes Quadrate code, which can include function calls, literals, and
 * stack operations. Supports both simple function calls and inline expressions.
 *
 * @param ctx Execution context
 * @param code Quadrate code to execute (e.g., "5 math::square print nl")
 *
 * @note Check ctx->error_code after execution to detect errors
 */
void qd_execute(qd_context* ctx, const char* code);

#ifdef __cplusplus
}
#endif

#endif // QUADRATE_QD_H
