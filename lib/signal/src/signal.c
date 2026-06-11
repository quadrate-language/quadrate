#define _POSIX_C_SOURCE 200809L

#include <quadrate/signal/signal.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
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

int usr_signal_trap(qd_context* ctx) {
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

	return (int){0};
}

int usr_signal_ignore(qd_context* ctx) {
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

	return (int){0};
}

int usr_signal_reset(qd_context* ctx) {
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

	return (int){0};
}

int usr_signal_pending(qd_context* ctx) {
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

	return (int){0};
}

int usr_signal_clear(qd_context* ctx) {
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

	return (int){0};
}

int usr_signal_wait(qd_context* ctx) {
	// Wait for any trapped signal. A naive "scan pending then pause()" loop has
	// a race: a signal delivered between the scan and pause() would be missed,
	// potentially hanging forever. Block signals around the pending check and
	// use sigsuspend() to atomically unblock-and-wait, which closes that window.
	sigset_t block_all, prev_mask;
	sigfillset(&block_all);
	sigprocmask(SIG_BLOCK, &block_all, &prev_mask);

	int result = 0;
	while (1) {
		int found = 0;
		for (int i = 1; i < MAX_SIGNALS; i++) {
			if (pending_signals[i]) {
				found = i;
				break;
			}
		}

		if (found) {
			qd_stack_error err = qd_stack_push_int(ctx->st, (int64_t)found);
			if (err != QD_STACK_OK) {
				// Restore the mask before aborting so the process state is sane.
				sigprocmask(SIG_SETMASK, &prev_mask, NULL);
				fprintf(stderr, "Fatal error in signal::wait: Failed to push signal number\n");
				qd_print_stack_trace(ctx);
				abort();
			}
			break;
		}

		// No signal pending: atomically restore the previous mask (so trapped
		// signals can be delivered), wait for one, then re-block on return.
		sigsuspend(&prev_mask);
	}

	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
	return (int){result};
}

int usr_signal_raise(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in signal::raise: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::raise: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::raise: Expected integer signal number, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	int signum = (int)elem.value.i;
	int result = raise(signum);

	err = qd_stack_push_int(ctx->st, (int64_t)result);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::raise: Failed to push result\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (int){0};
}

int usr_signal_SigHup(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGHUP);
	return 0;
}

int usr_signal_SigInt(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGINT);
	return 0;
}

int usr_signal_SigQuit(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGQUIT);
	return 0;
}

int usr_signal_SigIll(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGILL);
	return 0;
}

int usr_signal_SigAbrt(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGABRT);
	return 0;
}

int usr_signal_SigFpe(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGFPE);
	return 0;
}

int usr_signal_SigKill(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGKILL);
	return 0;
}

int usr_signal_SigSegv(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGSEGV);
	return 0;
}

int usr_signal_SigPipe(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGPIPE);
	return 0;
}

int usr_signal_SigAlrm(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGALRM);
	return 0;
}

int usr_signal_SigTerm(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGTERM);
	return 0;
}

int usr_signal_SigUsr1(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	return 0;
}

int usr_signal_SigUsr2(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	return 0;
}

int usr_signal_SigChld(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGCHLD);
	return 0;
}

int usr_signal_SigCont(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGCONT);
	return 0;
}

int usr_signal_SigStop(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGSTOP);
	return 0;
}

int usr_signal_SigTstp(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGTSTP);
	return 0;
}

int usr_signal_SigTtin(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGTTIN);
	return 0;
}

int usr_signal_SigTtou(qd_context* ctx) {
	qd_stack_push_int(ctx->st, (int64_t)SIGTTOU);
	return 0;
}

int usr_signal_kill(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in signal::kill: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop signal number
	qd_stack_element_t sig_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &sig_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::kill: Failed to pop signal number\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (sig_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::kill: Expected integer signal number, got type %d\n", sig_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop PID
	qd_stack_element_t pid_elem;
	err = qd_stack_pop(ctx->st, &pid_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::kill: Failed to pop PID\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (pid_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in signal::kill: Expected integer PID, got type %d\n", pid_elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	pid_t pid = (pid_t)pid_elem.value.i;
	int signum = (int)sig_elem.value.i;
	int result = kill(pid, signum);

	err = qd_stack_push_int(ctx->st, (int64_t)result);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in signal::kill: Failed to push result\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (int){0};
}
