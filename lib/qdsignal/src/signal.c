#define _POSIX_C_SOURCE 200809L

#include <qdsignal/signal.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Maximum signal number we track (covers all standard signals)
#define MAX_SIGNALS 64

// Pending flags for each signal (set by handler, cleared by user)
// Using volatile sig_atomic_t for signal-safety
static volatile sig_atomic_t pending_signals[MAX_SIGNALS];

// Track which signals we've trapped (to restore later if needed)
static struct sigaction old_actions[MAX_SIGNALS];
static int trapped[MAX_SIGNALS];

// Signal handler - just sets the pending flag
static void signal_handler(int signum) {
	if (signum >= 0 && signum < MAX_SIGNALS) {
		pending_signals[signum] = 1;
	}
}

qd_exec_result usr_signal_trap(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in signal::trap: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::trap: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::trap: Expected integer signal number, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	int signum = (int)elem.value.i;
	if (signum < 1 || signum >= MAX_SIGNALS) {
		fprintf(stderr, "Fatal error in signal::trap: Invalid signal number %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Can't trap SIGKILL or SIGSTOP
	if (signum == SIGKILL || signum == SIGSTOP) {
		fprintf(stderr, "Fatal error in signal::trap: Cannot trap signal %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Install our handler
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(signum, &sa, &old_actions[signum]) == -1) {
		fprintf(stderr, "Fatal error in signal::trap: sigaction failed for signal %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	trapped[signum] = 1;
	pending_signals[signum] = 0;  // Clear any pending state

	return (qd_exec_result){0};
}

qd_exec_result usr_signal_ignore(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in signal::ignore: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::ignore: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::ignore: Expected integer signal number, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	int signum = (int)elem.value.i;
	if (signum < 1 || signum >= MAX_SIGNALS) {
		fprintf(stderr, "Fatal error in signal::ignore: Invalid signal number %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(signum, &sa, &old_actions[signum]) == -1) {
		fprintf(stderr, "Fatal error in signal::ignore: sigaction failed for signal %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	trapped[signum] = 0;
	pending_signals[signum] = 0;

	return (qd_exec_result){0};
}

qd_exec_result usr_signal_reset(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in signal::reset: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::reset: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::reset: Expected integer signal number, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	int signum = (int)elem.value.i;
	if (signum < 1 || signum >= MAX_SIGNALS) {
		fprintf(stderr, "Fatal error in signal::reset: Invalid signal number %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Restore to default
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(signum, &sa, NULL) == -1) {
		fprintf(stderr, "Fatal error in signal::reset: sigaction failed for signal %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	trapped[signum] = 0;
	pending_signals[signum] = 0;

	return (qd_exec_result){0};
}

qd_exec_result usr_signal_pending(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in signal::pending: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::pending: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::pending: Expected integer signal number, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	int signum = (int)elem.value.i;
	if (signum < 0 || signum >= MAX_SIGNALS) {
		fprintf(stderr, "Fatal error in signal::pending: Invalid signal number %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push 1 if pending, 0 otherwise
	int64_t is_pending = pending_signals[signum] ? 1 : 0;
	err = qd_stack_push_int(ctx->st, is_pending);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::pending: Failed to push result\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result usr_signal_clear(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in signal::clear: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::clear: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::clear: Expected integer signal number, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	int signum = (int)elem.value.i;
	if (signum < 0 || signum >= MAX_SIGNALS) {
		fprintf(stderr, "Fatal error in signal::clear: Invalid signal number %d\n", signum);
		qd_print_stack_trace(ctx);
		abort();
	}

	pending_signals[signum] = 0;

	return (qd_exec_result){0};
}

qd_exec_result usr_signal_wait(qd_context* ctx) {
	// Wait for any trapped signal using pause()
	// pause() returns when a signal is caught

	// First check if any signal is already pending
	for (int i = 1; i < MAX_SIGNALS; i++) {
		if (pending_signals[i]) {
			qd_stack_error err = qd_stack_push_int(ctx->st, (int64_t)i);
			if (err != QD_STACK_OK) {
				fprintf(stderr, "Fatal error in signal::wait: Failed to push signal number\n");
				qd_print_stack_trace(ctx);
				abort();
			}
			return (qd_exec_result){0};
		}
	}

	// No signal pending, wait for one
	while (1) {
		pause();  // Sleep until a signal arrives

		// Check which signal was received
		for (int i = 1; i < MAX_SIGNALS; i++) {
			if (pending_signals[i]) {
				qd_stack_error err = qd_stack_push_int(ctx->st, (int64_t)i);
				if (err != QD_STACK_OK) {
					fprintf(stderr, "Fatal error in signal::wait: Failed to push signal number\n");
					qd_print_stack_trace(ctx);
					abort();
				}
				return (qd_exec_result){0};
			}
		}
		// If we get here, it was a signal we're not tracking, keep waiting
	}

	// Never reached
	return (qd_exec_result){0};
}
