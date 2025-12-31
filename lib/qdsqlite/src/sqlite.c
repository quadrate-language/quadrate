/**
 * @file sqlite.c
 * @brief SQLite database driver implementation for Quadrate
 */

#include "qdsqlite/sqlite.h"
#include <qdrt/context.h>
#include <qdrt/qd_string.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes matching module.qd */
#define SQLITE_ERR_OK 1
#define SQLITE_ERR_OPEN 2
#define SQLITE_ERR_EXEC 3
#define SQLITE_ERR_PREPARE 4
#define SQLITE_ERR_BIND 5
#define SQLITE_ERR_STEP 6
#define SQLITE_ERR_INVALID_ARG 7

/** Helper to safely set error message */
static void set_error_msg(qd_context* ctx, const char* msg) {
	if (ctx->error_msg) free(ctx->error_msg);
	ctx->error_msg = strdup(msg);
}

/** Helper to set error with SQLite message */
static void set_sqlite_error(qd_context* ctx, const char* prefix, sqlite3* db) {
	if (ctx->error_msg) free(ctx->error_msg);
	const char* sqlite_msg = db ? sqlite3_errmsg(db) : "unknown error";
	size_t len = strlen(prefix) + strlen(sqlite_msg) + 3;
	ctx->error_msg = malloc(len);
	if (ctx->error_msg) {
		snprintf(ctx->error_msg, len, "%s: %s", prefix, sqlite_msg);
	}
}

/**
 * open - Open SQLite database
 * Stack: (path:str -- db:ptr)!
 */
qd_exec_result usr_sqlite_open(qd_context* ctx) {
	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);

	if (err != QD_STACK_OK || path_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "sqlite::open: expected string path");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	const char* path = qd_string_data(path_elem.value.s);

	sqlite3* db = NULL;
	int rc = sqlite3_open(path, &db);
	qd_string_release(path_elem.value.s);

	if (rc != SQLITE_OK) {
		set_sqlite_error(ctx, "sqlite::open", db);
		ctx->error_code = SQLITE_ERR_OPEN;
		if (db) sqlite3_close(db);
		return (qd_exec_result){SQLITE_ERR_OPEN};
	}

	qd_push_p(ctx, db);
	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * close - Close database
 * Stack: (db:ptr -- )
 */
qd_exec_result usr_sqlite_close(qd_context* ctx) {
	qd_stack_element_t db_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);

	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::close: expected pointer");
		return (qd_exec_result){0};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	if (db) {
		sqlite3_close(db);
	}

	return (qd_exec_result){0};
}

/**
 * exec - Execute SQL without results
 * Stack: (sql:str db:ptr -- )!
 */
qd_exec_result usr_sqlite_exec(qd_context* ctx) {
	qd_stack_element_t db_elem, sql_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::exec: expected database pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &sql_elem);
	if (err != QD_STACK_OK || sql_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "sqlite::exec: expected SQL string");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	const char* sql = qd_string_data(sql_elem.value.s);

	char* errmsg = NULL;
	int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	qd_string_release(sql_elem.value.s);

	if (rc != SQLITE_OK) {
		if (ctx->error_msg) free(ctx->error_msg);
		if (errmsg) {
			size_t len = strlen("sqlite::exec: ") + strlen(errmsg) + 1;
			ctx->error_msg = malloc(len);
			if (ctx->error_msg) {
				snprintf(ctx->error_msg, len, "sqlite::exec: %s", errmsg);
			}
			sqlite3_free(errmsg);
		} else {
			ctx->error_msg = strdup("sqlite::exec: execution failed");
		}
		ctx->error_code = SQLITE_ERR_EXEC;
		return (qd_exec_result){SQLITE_ERR_EXEC};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * prepare - Prepare a SQL statement
 * Stack: (sql:str db:ptr -- stmt:ptr)!
 */
qd_exec_result usr_sqlite_prepare(qd_context* ctx) {
	qd_stack_element_t db_elem, sql_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::prepare: expected database pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &sql_elem);
	if (err != QD_STACK_OK || sql_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "sqlite::prepare: expected SQL string");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	const char* sql = qd_string_data(sql_elem.value.s);

	sqlite3_stmt* stmt = NULL;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	qd_string_release(sql_elem.value.s);

	if (rc != SQLITE_OK) {
		set_sqlite_error(ctx, "sqlite::prepare", db);
		ctx->error_code = SQLITE_ERR_PREPARE;
		return (qd_exec_result){SQLITE_ERR_PREPARE};
	}

	qd_push_p(ctx, stmt);
	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * bind_text - Bind string parameter
 * Stack: (value:str index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_text(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem, value_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::bind_text: expected statement pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "sqlite::bind_text: expected integer index");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK || value_elem.type != QD_STACK_TYPE_STR) {
		set_error_msg(ctx, "sqlite::bind_text: expected string value");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;
	const char* value = qd_string_data(value_elem.value.s);
	size_t len = qd_string_length(value_elem.value.s);

	int rc = sqlite3_bind_text(stmt, index, value, (int)len, SQLITE_TRANSIENT);
	qd_string_release(value_elem.value.s);

	if (rc != SQLITE_OK) {
		set_error_msg(ctx, "sqlite::bind_text: bind failed");
		ctx->error_code = SQLITE_ERR_BIND;
		return (qd_exec_result){SQLITE_ERR_BIND};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * bind_int - Bind integer parameter
 * Stack: (value:i64 index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_int(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem, value_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::bind_int: expected statement pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "sqlite::bind_int: expected integer index");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK || value_elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "sqlite::bind_int: expected integer value");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;
	int64_t value = value_elem.value.i;

	int rc = sqlite3_bind_int64(stmt, index, value);

	if (rc != SQLITE_OK) {
		set_error_msg(ctx, "sqlite::bind_int: bind failed");
		ctx->error_code = SQLITE_ERR_BIND;
		return (qd_exec_result){SQLITE_ERR_BIND};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * bind_float - Bind float parameter
 * Stack: (value:f64 index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_float(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem, value_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::bind_float: expected statement pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "sqlite::bind_float: expected integer index");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK || value_elem.type != QD_STACK_TYPE_FLOAT) {
		set_error_msg(ctx, "sqlite::bind_float: expected float value");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;
	double value = value_elem.value.f;

	int rc = sqlite3_bind_double(stmt, index, value);

	if (rc != SQLITE_OK) {
		set_error_msg(ctx, "sqlite::bind_float: bind failed");
		ctx->error_code = SQLITE_ERR_BIND;
		return (qd_exec_result){SQLITE_ERR_BIND};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * bind_null - Bind NULL parameter
 * Stack: (index:i64 stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_bind_null(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::bind_null: expected statement pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		set_error_msg(ctx, "sqlite::bind_null: expected integer index");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;

	int rc = sqlite3_bind_null(stmt, index);

	if (rc != SQLITE_OK) {
		set_error_msg(ctx, "sqlite::bind_null: bind failed");
		ctx->error_code = SQLITE_ERR_BIND;
		return (qd_exec_result){SQLITE_ERR_BIND};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * step - Execute and step to next row
 * Stack: (stmt:ptr -- has_row:i64)!
 */
qd_exec_result usr_sqlite_step(qd_context* ctx) {
	qd_stack_element_t stmt_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::step: expected statement pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		qd_push_i(ctx, 1);  /* has row */
		qd_push_i(ctx, SQLITE_ERR_OK);
		return (qd_exec_result){0};
	} else if (rc == SQLITE_DONE) {
		qd_push_i(ctx, 0);  /* no more rows */
		qd_push_i(ctx, SQLITE_ERR_OK);
		return (qd_exec_result){0};
	} else {
		set_error_msg(ctx, "sqlite::step: execution failed");
		ctx->error_code = SQLITE_ERR_STEP;
		return (qd_exec_result){SQLITE_ERR_STEP};
	}
}

/**
 * reset - Reset statement for re-execution
 * Stack: (stmt:ptr -- )!
 */
qd_exec_result usr_sqlite_reset(qd_context* ctx) {
	qd_stack_element_t stmt_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::reset: expected statement pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * finalize - Free prepared statement
 * Stack: (stmt:ptr -- )
 */
qd_exec_result usr_sqlite_finalize(qd_context* ctx) {
	qd_stack_element_t stmt_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	if (stmt) {
		sqlite3_finalize(stmt);
	}

	return (qd_exec_result){0};
}

/**
 * column_count - Get number of columns
 * Stack: (stmt:ptr -- count:i64)
 */
qd_exec_result usr_sqlite_column_count(qd_context* ctx) {
	qd_stack_element_t stmt_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int count = sqlite3_column_count(stmt);
	qd_push_i(ctx, count);
	return (qd_exec_result){0};
}

/**
 * column_name - Get column name
 * Stack: (index:i64 stmt:ptr -- name:str)
 */
qd_exec_result usr_sqlite_column_name(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_s(ctx, "");
		return (qd_exec_result){0};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		qd_push_s(ctx, "");
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;

	const char* name = sqlite3_column_name(stmt, index);
	qd_push_s(ctx, name ? name : "");
	return (qd_exec_result){0};
}

/**
 * column_type - Get column type
 * Stack: (index:i64 stmt:ptr -- type:i64)
 */
qd_exec_result usr_sqlite_column_type(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;

	int type = sqlite3_column_type(stmt, index);
	qd_push_i(ctx, type);
	return (qd_exec_result){0};
}

/**
 * column_int - Get integer column
 * Stack: (index:i64 stmt:ptr -- value:i64)
 */
qd_exec_result usr_sqlite_column_int(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;

	int64_t value = sqlite3_column_int64(stmt, index);
	qd_push_i(ctx, value);
	return (qd_exec_result){0};
}

/**
 * column_float - Get float column
 * Stack: (index:i64 stmt:ptr -- value:f64)
 */
qd_exec_result usr_sqlite_column_float(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_f(ctx, 0.0);
		return (qd_exec_result){0};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		qd_push_f(ctx, 0.0);
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;

	double value = sqlite3_column_double(stmt, index);
	qd_push_f(ctx, value);
	return (qd_exec_result){0};
}

/**
 * column_text - Get text column
 * Stack: (index:i64 stmt:ptr -- value:str)
 */
qd_exec_result usr_sqlite_column_text(qd_context* ctx) {
	qd_stack_element_t stmt_elem, index_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &stmt_elem);
	if (err != QD_STACK_OK || stmt_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_s(ctx, "");
		return (qd_exec_result){0};
	}

	err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK || index_elem.type != QD_STACK_TYPE_INT) {
		qd_push_s(ctx, "");
		return (qd_exec_result){0};
	}

	sqlite3_stmt* stmt = (sqlite3_stmt*)stmt_elem.value.p;
	int index = (int)index_elem.value.i;

	const unsigned char* text = sqlite3_column_text(stmt, index);
	int len = sqlite3_column_bytes(stmt, index);

	if (text && len > 0) {
		qd_string_t* s = qd_string_create_with_length((const char*)text, (size_t)len);
		if (s) {
			qd_push_s_ref(ctx, s);
			qd_string_release(s);
		} else {
			qd_push_s(ctx, "");
		}
	} else {
		qd_push_s(ctx, "");
	}

	return (qd_exec_result){0};
}

/**
 * last_insert_rowid - Get last insert rowid
 * Stack: (db:ptr -- rowid:i64)
 */
qd_exec_result usr_sqlite_last_insert_rowid(qd_context* ctx) {
	qd_stack_element_t db_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	int64_t rowid = sqlite3_last_insert_rowid(db);
	qd_push_i(ctx, rowid);
	return (qd_exec_result){0};
}

/**
 * changes - Get rows changed
 * Stack: (db:ptr -- changes:i64)
 */
qd_exec_result usr_sqlite_changes(qd_context* ctx) {
	qd_stack_element_t db_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	int changes = sqlite3_changes(db);
	qd_push_i(ctx, changes);
	return (qd_exec_result){0};
}

/**
 * begin - Begin transaction
 * Stack: (db:ptr -- )!
 */
qd_exec_result usr_sqlite_begin(qd_context* ctx) {
	qd_stack_element_t db_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::begin: expected database pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	char* errmsg = NULL;
	int rc = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &errmsg);

	if (rc != SQLITE_OK) {
		if (ctx->error_msg) free(ctx->error_msg);
		if (errmsg) {
			size_t len = strlen("sqlite::begin: ") + strlen(errmsg) + 1;
			ctx->error_msg = malloc(len);
			if (ctx->error_msg) {
				snprintf(ctx->error_msg, len, "sqlite::begin: %s", errmsg);
			}
			sqlite3_free(errmsg);
		} else {
			ctx->error_msg = strdup("sqlite::begin: failed");
		}
		ctx->error_code = SQLITE_ERR_EXEC;
		return (qd_exec_result){SQLITE_ERR_EXEC};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * commit - Commit transaction
 * Stack: (db:ptr -- )!
 */
qd_exec_result usr_sqlite_commit(qd_context* ctx) {
	qd_stack_element_t db_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::commit: expected database pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	char* errmsg = NULL;
	int rc = sqlite3_exec(db, "COMMIT", NULL, NULL, &errmsg);

	if (rc != SQLITE_OK) {
		if (ctx->error_msg) free(ctx->error_msg);
		if (errmsg) {
			size_t len = strlen("sqlite::commit: ") + strlen(errmsg) + 1;
			ctx->error_msg = malloc(len);
			if (ctx->error_msg) {
				snprintf(ctx->error_msg, len, "sqlite::commit: %s", errmsg);
			}
			sqlite3_free(errmsg);
		} else {
			ctx->error_msg = strdup("sqlite::commit: failed");
		}
		ctx->error_code = SQLITE_ERR_EXEC;
		return (qd_exec_result){SQLITE_ERR_EXEC};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}

/**
 * rollback - Rollback transaction
 * Stack: (db:ptr -- )!
 */
qd_exec_result usr_sqlite_rollback(qd_context* ctx) {
	qd_stack_element_t db_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &db_elem);
	if (err != QD_STACK_OK || db_elem.type != QD_STACK_TYPE_PTR) {
		set_error_msg(ctx, "sqlite::rollback: expected database pointer");
		ctx->error_code = SQLITE_ERR_INVALID_ARG;
		return (qd_exec_result){SQLITE_ERR_INVALID_ARG};
	}

	sqlite3* db = (sqlite3*)db_elem.value.p;
	char* errmsg = NULL;
	int rc = sqlite3_exec(db, "ROLLBACK", NULL, NULL, &errmsg);

	if (rc != SQLITE_OK) {
		if (ctx->error_msg) free(ctx->error_msg);
		if (errmsg) {
			size_t len = strlen("sqlite::rollback: ") + strlen(errmsg) + 1;
			ctx->error_msg = malloc(len);
			if (ctx->error_msg) {
				snprintf(ctx->error_msg, len, "sqlite::rollback: %s", errmsg);
			}
			sqlite3_free(errmsg);
		} else {
			ctx->error_msg = strdup("sqlite::rollback: failed");
		}
		ctx->error_code = SQLITE_ERR_EXEC;
		return (qd_exec_result){SQLITE_ERR_EXEC};
	}

	qd_push_i(ctx, SQLITE_ERR_OK);
	return (qd_exec_result){0};
}
