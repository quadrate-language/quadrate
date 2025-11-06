#ifndef STDQD_TIME_H
#define STDQD_TIME_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Time Functions
// ============================================================================

/**
 * qd_stdqd_sleep - Sleep for a specified duration in nanoseconds
 *
 * Stack signature: ( nanoseconds:i -- )
 *
 * Suspends execution for the specified duration in nanoseconds.
 *
 * Example in Quadrate:
 *   1000000000 time::sleep  // Sleep for 1 second (1 billion nanoseconds)
 *   500000000 time::sleep   // Sleep for 500 milliseconds
 *   1000000 time::sleep     // Sleep for 1 millisecond
 */
qd_exec_result qd_stdqd_sleep(qd_context* ctx);
qd_exec_result qd_stdqd_Nanosecond(qd_context* ctx);
qd_exec_result qd_stdqd_Microsecond(qd_context* ctx);
qd_exec_result qd_stdqd_Millisecond(qd_context* ctx);
qd_exec_result qd_stdqd_Second(qd_context* ctx);
qd_exec_result qd_stdqd_Minute(qd_context* ctx);
qd_exec_result qd_stdqd_Hour(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_TIME_H
