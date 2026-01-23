/**
 * @file signal.h
 * @brief Signal handling for Quadrate (signal:: module)
 *
 * Provides Unix signal handling with a polling-based API.
 * Signals are caught and stored as flags, which can be checked
 * and cleared by the application at safe points.
 */

#ifndef QD_QDSIGNAL_SIGNAL_H
#define QD_QDSIGNAL_SIGNAL_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Install a signal handler to catch the specified signal
 *
 * @par Stack Effect: ( signum:i -- )
 *
 * After calling trap, the signal will be caught instead of causing
 * the default action (e.g., termination). Use pending() to check
 * if the signal has been received.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * signal::SIGINT signal::trap  // Catch Ctrl+C
 * @endcode
 */
int usr_signal_trap(qd_context* ctx);

/**
 * @brief Ignore the specified signal
 *
 * @par Stack Effect: ( signum:i -- )
 *
 * The signal will be completely ignored (SIG_IGN).
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * signal::SIGPIPE signal::ignore  // Ignore broken pipe
 * @endcode
 */
int usr_signal_ignore(qd_context* ctx);

/**
 * @brief Reset signal to default behavior
 *
 * @par Stack Effect: ( signum:i -- )
 *
 * Restores the default action for the signal (SIG_DFL).
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * signal::SIGINT signal::reset  // Ctrl+C terminates again
 * @endcode
 */
int usr_signal_reset(qd_context* ctx);

/**
 * @brief Check if a signal is pending
 *
 * @par Stack Effect: ( signum:i -- flag:i )
 *
 * Returns 1 if the signal has been received since the last clear,
 * 0 otherwise. Does not clear the pending flag.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * signal::SIGINT signal::pending if {
 *     "Interrupted!" . nl
 * }
 * @endcode
 */
int usr_signal_pending(qd_context* ctx);

/**
 * @brief Clear the pending flag for a signal
 *
 * @par Stack Effect: ( signum:i -- )
 *
 * Clears the pending flag so that pending() returns 0 until
 * the signal is received again.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * signal::SIGINT signal::clear
 * @endcode
 */
int usr_signal_clear(qd_context* ctx);

/**
 * @brief Block until any trapped signal is received
 *
 * @par Stack Effect: ( -- signum:i )
 *
 * Blocks execution until one of the trapped signals is received.
 * Returns the signal number that was received.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * signal::wait -> sig
 * "Received signal " . sig . nl
 * @endcode
 */
int usr_signal_wait(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDSIGNAL_SIGNAL_H
