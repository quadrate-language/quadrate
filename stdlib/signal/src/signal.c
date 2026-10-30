#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/signal/signal.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

// Maximum signal number we track (covers all standard signals)
#define MAX_SIGNALS 64

// Pending flags for each signal (set by handler, cleared by user)
// Using volatile sig_atomic_t for signal-safety
static volatile sig_atomic_t pending_signals[MAX_SIGNALS];

// Which signals trap() has installed the handler for, so wait() knows whether anything
// can ever wake it
static volatile sig_atomic_t trapped[MAX_SIGNALS];

// Signal handler - just sets the pending flag
static void signal_handler(int signum) {
	if (signum >= 0 && signum < MAX_SIGNALS) {
		pending_signals[signum] = 1;
	}
}

// Pop a signal number, from `lowest` up
static int signum_arg(qd_context* ctx, const char* op, int64_t lowest) {
	int64_t signum;
	qd_args(ctx, op, "i", &signum);
	if (signum < lowest || signum >= MAX_SIGNALS) {
		qd_fatal_raise(ctx, op, "Invalid signal number %" PRId64, signum);
	}
	return (int)signum;
}

static void set_action(qd_context* ctx, const char* op, int signum, void (*handler)(int)) {
	struct sigaction sa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(signum, &sa, NULL) == -1) {
		qd_fatal_raise(ctx, op, "Cannot change the action for signal %d: %s", signum, strerror(errno));
	}
	trapped[signum] = handler == signal_handler;
	pending_signals[signum] = 0;
}

int usr_signal_trap(qd_context* ctx) {
	const int signum = signum_arg(ctx, "signal::trap", 1);
	set_action(ctx, "signal::trap", signum, signal_handler);
	return 0;
}

int usr_signal_ignore(qd_context* ctx) {
	const int signum = signum_arg(ctx, "signal::ignore", 1);
	set_action(ctx, "signal::ignore", signum, SIG_IGN);
	return 0;
}

int usr_signal_reset(qd_context* ctx) {
	const int signum = signum_arg(ctx, "signal::reset", 1);
	set_action(ctx, "signal::reset", signum, SIG_DFL);
	return 0;
}

int usr_signal_pending(qd_context* ctx) {
	const int signum = signum_arg(ctx, "signal::pending", 0);
	return qd_push_i(ctx, pending_signals[signum] ? 1 : 0);
}

int usr_signal_clear(qd_context* ctx) {
	const int signum = signum_arg(ctx, "signal::clear", 0);
	pending_signals[signum] = 0;
	return 0;
}

// Wait for any trapped signal, and take it: its pending flag is cleared, so the next wait
// waits for the next one. It used to stay set, and every later wait returned the same
// signal at once.
//
// A naive "scan pending then pause()" loop has a race: a signal delivered between the scan
// and pause() would be missed, potentially hanging forever. Block signals around the
// pending check and use sigsuspend() to atomically unblock-and-wait, which closes that
// window.
int usr_signal_wait(qd_context* ctx) {
	bool any_trapped = false;
	for (int i = 1; i < MAX_SIGNALS; i++) {
		any_trapped = any_trapped || trapped[i];
	}
	if (!any_trapped) {
		qd_fatal_raise(ctx, "signal::wait", "No signal is trapped, so nothing could end the wait");
	}

	sigset_t block_all;
	sigset_t prev_mask;
	sigfillset(&block_all);
	sigprocmask(SIG_BLOCK, &block_all, &prev_mask);
	int found = 0;
	while (!found) {
		for (int i = 1; i < MAX_SIGNALS && !found; i++) {
			if (pending_signals[i]) {
				pending_signals[i] = 0;
				found = i;
			}
		}
		if (!found) {
			// Atomically restore the previous mask (so trapped signals can be
			// delivered), wait for one, then re-block on return.
			sigsuspend(&prev_mask);
		}
	}
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
	return qd_push_i(ctx, found);
}

int usr_signal_raise(qd_context* ctx) {
	int64_t signum;
	qd_args(ctx, "signal::raise", "i", &signum);
	return qd_push_i(ctx, raise((int)signum));
}

int usr_signal_SigHup(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGHUP);
	return 0;
}

int usr_signal_SigInt(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGINT);
	return 0;
}

int usr_signal_SigQuit(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGQUIT);
	return 0;
}

int usr_signal_SigIll(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGILL);
	return 0;
}

int usr_signal_SigAbrt(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGABRT);
	return 0;
}

int usr_signal_SigFpe(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGFPE);
	return 0;
}

int usr_signal_SigKill(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGKILL);
	return 0;
}

int usr_signal_SigSegv(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGSEGV);
	return 0;
}

int usr_signal_SigPipe(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGPIPE);
	return 0;
}

int usr_signal_SigAlrm(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGALRM);
	return 0;
}

int usr_signal_SigTerm(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGTERM);
	return 0;
}

int usr_signal_SigUsr1(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGUSR1);
	return 0;
}

int usr_signal_SigUsr2(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGUSR2);
	return 0;
}

int usr_signal_SigChld(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGCHLD);
	return 0;
}

int usr_signal_SigCont(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGCONT);
	return 0;
}

int usr_signal_SigStop(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGSTOP);
	return 0;
}

int usr_signal_SigTstp(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGTSTP);
	return 0;
}

int usr_signal_SigTtin(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGTTIN);
	return 0;
}

int usr_signal_SigTtou(qd_context* ctx) {
	qd_push_i(ctx, (int64_t)SIGTTOU);
	return 0;
}

int usr_signal_kill(qd_context* ctx) {
	int64_t pid;
	int64_t signum;
	qd_args(ctx, "signal::kill", "ii", &pid, &signum);
	return qd_push_i(ctx, kill((pid_t)pid, (int)signum));
}
