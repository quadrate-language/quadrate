/**
 * @file tty.h
 * @brief Terminal detection and information for the Quadrate standard library
 *
 * This module provides functions for detecting whether file descriptors
 * are connected to terminals and getting terminal dimensions.
 */

#ifndef QD_QDTTY_TTY_H
#define QD_QDTTY_TTY_H

#include <quadrate/rt/context.h>

/**
 * @brief Check if stdout is connected to a terminal
 *
 * Stack effect: ( -- is_tty:i )
 *
 * Returns 1 if stdout is a terminal, 0 otherwise.
 * Useful for deciding whether to output colors.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_tty_is_stdout(qd_context* ctx);

/**
 * @brief Check if stderr is connected to a terminal
 *
 * Stack effect: ( -- is_tty:i )
 *
 * Returns 1 if stderr is a terminal, 0 otherwise.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_tty_is_stderr(qd_context* ctx);

/**
 * @brief Check if stdin is connected to a terminal
 *
 * Stack effect: ( -- is_tty:i )
 *
 * Returns 1 if stdin is a terminal, 0 otherwise.
 * Useful for detecting if input is piped.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_tty_is_stdin(qd_context* ctx);

/**
 * @brief Get terminal dimensions
 *
 * Stack effect: ( -- rows:i cols:i )
 *
 * Returns the terminal size in rows and columns.
 * Returns 0, 0 if not a terminal or size cannot be determined.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_tty_size(qd_context* ctx);

/**
 * @brief Get terminal width (columns)
 *
 * Stack effect: ( -- cols:i )
 *
 * Returns the terminal width in columns.
 * Returns 0 if not a terminal or width cannot be determined.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_tty_width(qd_context* ctx);

/**
 * @brief Get terminal height (rows)
 *
 * Stack effect: ( -- rows:i )
 *
 * Returns the terminal height in rows.
 * Returns 0 if not a terminal or height cannot be determined.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_tty_height(qd_context* ctx);

#endif // QD_QDTTY_TTY_H
