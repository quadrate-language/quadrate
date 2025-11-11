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
 *   time::Second time::sleep  // Sleep for 1 second
 *   500 time::Millisecond mul time::sleep   // Sleep for 500 milliseconds
 *   time::Millisecond time::sleep     // Sleep for 1 millisecond
 */
qd_exec_result qd_stdqd_sleep(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_TIME_H
