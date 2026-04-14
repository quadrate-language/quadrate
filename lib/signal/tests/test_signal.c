/**
 * @file test_signal.c
 * @brief Unit tests for the qdsignal signal handling library
 */

#define _POSIX_C_SOURCE 200809L

#include <quadrate/signal/signal.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <signal.h>
#include <unistd.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


/* ===================================================================
 * Signal constant tests - verify each constant pushes the right value
 * =================================================================== */

TEST(SignalConstSigHup) {
	qd_context* ctx = create_test_context();
	usr_signal_SigHup(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGHUP, (int)elem.value.i, "SigHup should equal SIGHUP");
	destroy_test_context(ctx);
}

TEST(SignalConstSigInt) {
	qd_context* ctx = create_test_context();
	usr_signal_SigInt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGINT, (int)elem.value.i, "SigInt should equal SIGINT");
	destroy_test_context(ctx);
}

TEST(SignalConstSigQuit) {
	qd_context* ctx = create_test_context();
	usr_signal_SigQuit(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGQUIT, (int)elem.value.i, "SigQuit should equal SIGQUIT");
	destroy_test_context(ctx);
}

TEST(SignalConstSigIll) {
	qd_context* ctx = create_test_context();
	usr_signal_SigIll(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGILL, (int)elem.value.i, "SigIll should equal SIGILL");
	destroy_test_context(ctx);
}

TEST(SignalConstSigAbrt) {
	qd_context* ctx = create_test_context();
	usr_signal_SigAbrt(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGABRT, (int)elem.value.i, "SigAbrt should equal SIGABRT");
	destroy_test_context(ctx);
}

TEST(SignalConstSigFpe) {
	qd_context* ctx = create_test_context();
	usr_signal_SigFpe(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGFPE, (int)elem.value.i, "SigFpe should equal SIGFPE");
	destroy_test_context(ctx);
}

TEST(SignalConstSigKill) {
	qd_context* ctx = create_test_context();
	usr_signal_SigKill(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGKILL, (int)elem.value.i, "SigKill should equal SIGKILL");
	destroy_test_context(ctx);
}

TEST(SignalConstSigSegv) {
	qd_context* ctx = create_test_context();
	usr_signal_SigSegv(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGSEGV, (int)elem.value.i, "SigSegv should equal SIGSEGV");
	destroy_test_context(ctx);
}

TEST(SignalConstSigPipe) {
	qd_context* ctx = create_test_context();
	usr_signal_SigPipe(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGPIPE, (int)elem.value.i, "SigPipe should equal SIGPIPE");
	destroy_test_context(ctx);
}

TEST(SignalConstSigAlrm) {
	qd_context* ctx = create_test_context();
	usr_signal_SigAlrm(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGALRM, (int)elem.value.i, "SigAlrm should equal SIGALRM");
	destroy_test_context(ctx);
}

TEST(SignalConstSigTerm) {
	qd_context* ctx = create_test_context();
	usr_signal_SigTerm(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGTERM, (int)elem.value.i, "SigTerm should equal SIGTERM");
	destroy_test_context(ctx);
}

TEST(SignalConstSigUsr1) {
	qd_context* ctx = create_test_context();
	usr_signal_SigUsr1(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGUSR1, (int)elem.value.i, "SigUsr1 should equal SIGUSR1");
	destroy_test_context(ctx);
}

TEST(SignalConstSigUsr2) {
	qd_context* ctx = create_test_context();
	usr_signal_SigUsr2(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGUSR2, (int)elem.value.i, "SigUsr2 should equal SIGUSR2");
	destroy_test_context(ctx);
}

TEST(SignalConstSigChld) {
	qd_context* ctx = create_test_context();
	usr_signal_SigChld(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGCHLD, (int)elem.value.i, "SigChld should equal SIGCHLD");
	destroy_test_context(ctx);
}

TEST(SignalConstSigCont) {
	qd_context* ctx = create_test_context();
	usr_signal_SigCont(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGCONT, (int)elem.value.i, "SigCont should equal SIGCONT");
	destroy_test_context(ctx);
}

TEST(SignalConstSigStop) {
	qd_context* ctx = create_test_context();
	usr_signal_SigStop(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGSTOP, (int)elem.value.i, "SigStop should equal SIGSTOP");
	destroy_test_context(ctx);
}

TEST(SignalConstSigTstp) {
	qd_context* ctx = create_test_context();
	usr_signal_SigTstp(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGTSTP, (int)elem.value.i, "SigTstp should equal SIGTSTP");
	destroy_test_context(ctx);
}

TEST(SignalConstSigTtin) {
	qd_context* ctx = create_test_context();
	usr_signal_SigTtin(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGTTIN, (int)elem.value.i, "SigTtin should equal SIGTTIN");
	destroy_test_context(ctx);
}

TEST(SignalConstSigTtou) {
	qd_context* ctx = create_test_context();
	usr_signal_SigTtou(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(SIGTTOU, (int)elem.value.i, "SigTtou should equal SIGTTOU");
	destroy_test_context(ctx);
}


/* ===================================================================
 * Trap + raise + pending cycle (SIGUSR1)
 * =================================================================== */

TEST(SignalTrapRaisePendingUsr1) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Pending should be 0 before raise */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "SIGUSR1 should not be pending before raise");

	/* Raise SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "raise SIGUSR1 should return 0");

	/* Pending should be 1 after raise */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR1 should be pending after raise");

	/* Reset signal handler to avoid side effects */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}

TEST(SignalTrapRaisePendingUsr2) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR2 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_trap(ctx);

	/* Raise SIGUSR2 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "raise SIGUSR2 should return 0");

	/* Pending should be 1 after raise */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR2 should be pending after raise");

	/* Reset signal handler */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Clear clears the pending flag
 * =================================================================== */

TEST(SignalClearPending) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise it */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard raise result */

	/* Verify it is pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR1 should be pending before clear");

	/* Clear it */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_clear(ctx);

	/* Verify it is no longer pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "SIGUSR1 should not be pending after clear");

	/* Reset signal handler */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}

TEST(SignalClearDoesNotAffectOtherSignals) {
	qd_context* ctx = create_test_context();

	/* Trap both SIGUSR1 and SIGUSR2 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_trap(ctx);

	/* Raise both */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard raise result */

	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_raise(ctx);
	qd_stack_pop(ctx->st, &elem); /* discard raise result */

	/* Clear only SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_clear(ctx);

	/* SIGUSR1 should be cleared */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "SIGUSR1 should be cleared");

	/* SIGUSR2 should still be pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR2 should still be pending");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Ignore + raise does not crash
 * =================================================================== */

TEST(SignalIgnoreRaiseDoesNotCrash) {
	qd_context* ctx = create_test_context();

	/* Ignore SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_ignore(ctx);

	/* Raise it - should not crash since it's ignored */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "raise on ignored signal should return 0");

	/* Reset signal handler */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}

TEST(SignalIgnoreRaiseUsr2DoesNotCrash) {
	qd_context* ctx = create_test_context();

	/* Ignore SIGUSR2 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_ignore(ctx);

	/* Raise it */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "raise on ignored SIGUSR2 should return 0");

	/* Reset signal handler */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Reset restores default behavior
 * =================================================================== */

TEST(SignalResetClearsPendingFlag) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise it */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard raise result */

	/* Verify pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR1 should be pending before reset");

	/* Reset restores default and clears pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	/* Pending should be cleared by reset */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "SIGUSR1 should not be pending after reset");

	destroy_test_context(ctx);
}

TEST(SignalResetThenRetrap) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Reset it */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	/* Trap again - should work without error */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise and check pending to verify trap is working */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "raise should return 0");

	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR1 should be pending after re-trap and raise");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Raise returns 0 on success
 * =================================================================== */

TEST(SignalRaiseReturnsZero) {
	qd_context* ctx = create_test_context();

	/* Trap first so raise doesn't kill us */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise and check return value */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "raise result should be integer");
	ASSERT_EQ(0, (int)elem.value.i, "raise should return 0 on success");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Trap clears pending flag on install
 * =================================================================== */

TEST(SignalTrapClearsPendingOnInstall) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise to set pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard raise result */

	/* Re-trap should clear the pending flag */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "re-trapping should clear pending flag");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Kill - send signal to self via PID
 * =================================================================== */

TEST(SignalKillSelfUsr1) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Push PID then signal number (stack: pid signum) */
	qd_stack_push_int(ctx->st, (int64_t)getpid());
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_kill(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "kill self with SIGUSR1 should return 0");

	/* Verify signal was received */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR1 should be pending after kill");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Multiple raise calls keep pending set
 * =================================================================== */

TEST(SignalMultipleRaisesStayPending) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise twice */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard result */

	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_pop(ctx->st, &elem); /* discard result */

	/* Should still be pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "SIGUSR1 should still be pending after multiple raises");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Pending check does not consume the flag
 * =================================================================== */

TEST(SignalPendingDoesNotConsume) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR2 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_trap(ctx);

	/* Raise */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard result */

	/* Check pending twice - should be 1 both times */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "first pending check should return 1");

	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "second pending check should still return 1");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR2);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


/* ===================================================================
 * Ignore clears pending flag
 * =================================================================== */

TEST(SignalIgnoreClearsPending) {
	qd_context* ctx = create_test_context();

	/* Trap SIGUSR1 */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_trap(ctx);

	/* Raise to set pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_raise(ctx);
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem); /* discard result */

	/* Ignore should clear pending */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_ignore(ctx);

	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_pending(ctx);
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "pending should be cleared after ignore");

	/* Clean up */
	qd_stack_push_int(ctx->st, (int64_t)SIGUSR1);
	usr_signal_reset(ctx);

	destroy_test_context(ctx);
}


int main(void) {
	return UC_PrintResults();
}
