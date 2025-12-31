/**
 * @file sqlite.h
 * @brief SQLite database driver for Quadrate
 */

#ifndef QDSQLITE_H
#define QDSQLITE_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open SQLite database.
 * Stack: (path:str -- db:ptr)!
 */
qd_exec_result usr_sqlite_open(qd_context* ctx);

/**
 * Close database.
 * Stack: (db:ptr -- )
 */
qd_exec_result usr_sqlite_close(qd_context* ctx);

/**
 * Execute SQL without results.
 * Stack: (sql:str db:ptr -- )!
 */
qd_exec_result usr_sqlite_exec(qd_context* ctx);

/**
 * Prepare a SQL statement.
 * Stack: (sql:str db:ptr -- stmt:ptr)!
 */
qd_exec_result usr_sqlite_prepare(qd_context* ctx);

/**
 * Bind string parameter to statement.
 * Stack: (value:str index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_text(qd_context* ctx);

/**
 * Bind integer parameter to statement.
 * Stack: (value:i64 index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_int(qd_context* ctx);

/**
 * Bind float parameter to statement.
 * Stack: (value:f64 index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_float(qd_context* ctx);

/**
 * Bind NULL parameter to statement.
 * Stack: (index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_null(qd_context* ctx);

/**
 * Execute prepared statement and step to next row.
 * Returns 1 if row available, 0 if done.
 * Stack: (stmt:ptr -- has_row:i64)!
 */
qd_exec_result usr_sqlite_step(qd_context* ctx);

/**
 * Reset statement for re-execution.
 * Stack: (stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_reset(qd_context* ctx);

/**
 * Finalize (free) prepared statement.
 * Stack: (stmt:ptr -- )
 */
qd_exec_result usr_sqlite_finalize(qd_context* ctx);

/**
 * Get column count from current row.
 * Stack: (stmt:ptr -- count:i64)
 */
qd_exec_result usr_sqlite_column_count(qd_context* ctx);

/**
 * Get column name by index.
 * Stack: (index:i64 stmt:ptr -- name:str)
 */
qd_exec_result usr_sqlite_column_name(qd_context* ctx);

/**
 * Get column type by index.
 * Returns: 1=INTEGER, 2=FLOAT, 3=TEXT, 4=BLOB, 5=NULL
 * Stack: (index:i64 stmt:ptr -- type:i64)
 */
qd_exec_result usr_sqlite_column_type(qd_context* ctx);

/**
 * Get integer column value.
 * Stack: (index:i64 stmt:ptr -- value:i64)
 */
qd_exec_result usr_sqlite_column_int(qd_context* ctx);

/**
 * Get float column value.
 * Stack: (index:i64 stmt:ptr -- value:f64)
 */
qd_exec_result usr_sqlite_column_float(qd_context* ctx);

/**
 * Get text column value.
 * Stack: (index:i64 stmt:ptr -- value:str)
 */
qd_exec_result usr_sqlite_column_text(qd_context* ctx);

/**
 * Get last insert rowid.
 * Stack: (db:ptr -- rowid:i64)
 */
qd_exec_result usr_sqlite_last_insert_rowid(qd_context* ctx);

/**
 * Get number of rows changed by last statement.
 * Stack: (db:ptr -- changes:i64)
 */
qd_exec_result usr_sqlite_changes(qd_context* ctx);

/**
 * Begin transaction.
 * Stack: (db:ptr -- )!
 */
qd_exec_result usr_sqlite_begin(qd_context* ctx);

/**
 * Commit transaction.
 * Stack: (db:ptr -- )!
 */
qd_exec_result usr_sqlite_commit(qd_context* ctx);

/**
 * Rollback transaction.
 * Stack: (db:ptr -- )!
 */
qd_exec_result usr_sqlite_rollback(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif /* QDSQLITE_H */
