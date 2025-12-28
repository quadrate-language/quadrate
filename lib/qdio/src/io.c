/**
 * @file io.c
 * @brief Implementation of file I/O operations
 */

#include <qdio/io.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/** Helper to safely set error message (handles strdup failure) */
static void set_error_msg(qd_context* ctx, const char* msg) {
    if (ctx->error_msg) free(ctx->error_msg);
    ctx->error_msg = strdup(msg);
    // If strdup fails, error_msg will be NULL, which is acceptable
    // (error_code still indicates the error condition)
}

qd_exec_result usr_io_open(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 2) {
        fprintf(stderr, "Fatal error in io::open: Stack underflow (need 2, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop mode (top of stack)
    qd_stack_element_t mode_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &mode_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::open: Failed to pop mode\n");
        abort();
    }
    if (mode_elem.type != QD_STACK_TYPE_STR) {
        fprintf(stderr, "Fatal error in io::open: Expected string for mode, got %d\n", mode_elem.type);
        abort();
    }

    // Pop path
    qd_stack_element_t path_elem;
    err = qd_stack_pop(ctx->st, &path_elem);
    if (err != QD_STACK_OK) {
        qd_string_release(mode_elem.value.s);
        fprintf(stderr, "Fatal error in io::open: Failed to pop path\n");
        abort();
    }
    if (path_elem.type != QD_STACK_TYPE_STR) {
        qd_string_release(mode_elem.value.s);
        fprintf(stderr, "Fatal error in io::open: Expected string for path, got %d\n", path_elem.type);
        abort();
    }

    // Open file
    FILE* fp = fopen(qd_string_data(path_elem.value.s), qd_string_data(mode_elem.value.s));

    // Clean up strings
    qd_string_release(path_elem.value.s);
    qd_string_release(mode_elem.value.s);

    if (!fp) {
        // On failure, only push the error code
        ctx->error_code = IO_ERR_NOT_FOUND;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_NOT_FOUND);
        return (qd_exec_result){IO_ERR_NOT_FOUND};
    }

    // On success, push the result then Ok
    qd_exec_result push_result = qd_push_p(ctx, fp);
    if (push_result.code != 0) {
        fprintf(stderr, "Fatal error in io::open: Failed to push pointer to stack\n");
        abort();
    }
    qd_push_i(ctx, IO_ERR_OK);
    return (qd_exec_result){0};
}

qd_exec_result usr_io_close(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 1) {
        fprintf(stderr, "Fatal error in io::close: Stack underflow\n");
        qd_print_stack_trace(ctx);
        abort();
    }

    qd_stack_element_t elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::close: Failed to pop handle\n");
        abort();
    }
    if (elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::close: Expected pointer for handle, got %d\n", elem.type);
        abort();
    }

    FILE* fp = (FILE*)elem.value.p;
    if (fp) {
        fclose(fp); // Ignore errors on close
    }

    return (qd_exec_result){0};
}

qd_exec_result usr_io_read_string(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 2) {
        fprintf(stderr, "Fatal error in io::read: Stack underflow (need 2, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop count
    qd_stack_element_t count_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::read: Failed to pop count\n");
        abort();
    }
    if (count_elem.type != QD_STACK_TYPE_INT) {
        fprintf(stderr, "Fatal error in io::read: Expected integer for count, got %d\n", count_elem.type);
        abort();
    }

    // Pop handle
    qd_stack_element_t handle_elem;
    err = qd_stack_pop(ctx->st, &handle_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::read: Failed to pop handle\n");
        abort();
    }
    if (handle_elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::read: Expected pointer for handle, got %d\n", handle_elem.type);
        abort();
    }

    FILE* fp = (FILE*)handle_elem.value.p;
    int64_t count = count_elem.value.i;

    // Validate count is within reasonable bounds (max 1GB to prevent DoS)
    #define IO_MAX_READ_SIZE (1024 * 1024 * 1024)
    if (!fp || count < 0 || count > IO_MAX_READ_SIZE) {
        ctx->error_code = IO_ERR_INVALID_HANDLE;
        set_error_msg(ctx, "io::read_string: invalid file handle or count (max 1GB)");
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_INVALID_HANDLE};
    }

    if (count == 0) {
        qd_push_s(ctx, "");
        qd_push_i(ctx, 0);
        qd_push_i(ctx, IO_ERR_OK);
        return (qd_exec_result){0};
    }

    // Allocate buffer
    char* buffer = malloc((size_t)count + 1);
    if (!buffer) {
        ctx->error_code = IO_ERR_READ;
        set_error_msg(ctx, "io::read_string: allocation failed");
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_READ};
    }

    // Read from file
    size_t bytes_read = fread(buffer, 1, (size_t)count, fp);
    buffer[bytes_read] = '\0';

    if (bytes_read < (size_t)count && ferror(fp)) {
        ctx->error_code = IO_ERR_READ;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        free(buffer);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_READ};
    }

    // On success, push results then Ok
    qd_push_s(ctx, buffer);
    qd_push_i(ctx, (int64_t)bytes_read);
    qd_push_i(ctx, IO_ERR_OK);

    free(buffer);

    return (qd_exec_result){0};
}

qd_exec_result usr_io_write_string(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 2) {
        fprintf(stderr, "Fatal error in io::write: Stack underflow (need 2, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop data
    qd_stack_element_t data_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::write: Failed to pop data\n");
        abort();
    }
    if (data_elem.type != QD_STACK_TYPE_STR) {
        fprintf(stderr, "Fatal error in io::write: Expected string for data, got %d\n", data_elem.type);
        abort();
    }

    // Pop handle
    qd_stack_element_t handle_elem;
    err = qd_stack_pop(ctx->st, &handle_elem);
    if (err != QD_STACK_OK) {
        qd_string_release(data_elem.value.s);
        fprintf(stderr, "Fatal error in io::write: Failed to pop handle\n");
        abort();
    }
    if (handle_elem.type != QD_STACK_TYPE_PTR) {
        qd_string_release(data_elem.value.s);
        fprintf(stderr, "Fatal error in io::write: Expected pointer for handle, got %d\n", handle_elem.type);
        abort();
    }

    FILE* fp = (FILE*)handle_elem.value.p;
    const char* data = qd_string_data(data_elem.value.s);
    size_t len = strlen(data);

    if (!fp) {
        ctx->error_code = IO_ERR_INVALID_HANDLE;
        set_error_msg(ctx, "io::write_string: invalid file handle");
        qd_string_release(data_elem.value.s);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_INVALID_HANDLE};
    }

    size_t written = fwrite(data, 1, len, fp);
    qd_string_release(data_elem.value.s);

    if (written < len) {
        ctx->error_code = IO_ERR_WRITE;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_WRITE};
    }

    // On success, push result then Ok
    qd_push_i(ctx, (int64_t)written);
    qd_push_i(ctx, IO_ERR_OK);
    return (qd_exec_result){0};
}

qd_exec_result usr_io_seekg(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 3) {
        fprintf(stderr, "Fatal error in io::seekg: Stack underflow (need 3, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop whence
    qd_stack_element_t whence_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &whence_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::seekg: Failed to pop whence\n");
        abort();
    }
    if (whence_elem.type != QD_STACK_TYPE_INT) {
        fprintf(stderr, "Fatal error in io::seekg: Expected integer for whence, got %d\n", whence_elem.type);
        abort();
    }

    // Pop offset
    qd_stack_element_t offset_elem;
    err = qd_stack_pop(ctx->st, &offset_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::seekg: Failed to pop offset\n");
        abort();
    }
    if (offset_elem.type != QD_STACK_TYPE_INT) {
        fprintf(stderr, "Fatal error in io::seekg: Expected integer for offset, got %d\n", offset_elem.type);
        abort();
    }

    // Pop handle
    qd_stack_element_t handle_elem;
    err = qd_stack_pop(ctx->st, &handle_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::seekg: Failed to pop handle\n");
        abort();
    }
    if (handle_elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::seekg: Expected pointer for handle, got %d\n", handle_elem.type);
        abort();
    }

    FILE* fp = (FILE*)handle_elem.value.p;
    int64_t offset = offset_elem.value.i;
    int whence = (int)whence_elem.value.i;

    if (!fp) {
        ctx->error_code = IO_ERR_INVALID_HANDLE;
        set_error_msg(ctx, "io::seek: invalid file handle");
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_INVALID_HANDLE};
    }

    // Map whence values: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
    int seek_whence;
    switch (whence) {
        case 0: seek_whence = SEEK_SET; break;
        case 1: seek_whence = SEEK_CUR; break;
        case 2: seek_whence = SEEK_END; break;
        default:
            ctx->error_code = IO_ERR_INVALID_ARG;
            set_error_msg(ctx, "io::seek: invalid whence value");
            qd_push_i(ctx, ctx->error_code);
            return (qd_exec_result){IO_ERR_INVALID_ARG};
    }

    if (fseek(fp, offset, seek_whence) != 0) {
        ctx->error_code = IO_ERR_SEEK;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_SEEK};
    }

    int64_t position = ftell(fp);
    if (position < 0) {
        ctx->error_code = IO_ERR_SEEK;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_SEEK};
    }

    // On success, push result then Ok
    qd_push_i(ctx, position);
    qd_push_i(ctx, IO_ERR_OK);

    return (qd_exec_result){0};
}

qd_exec_result usr_io_eof(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 1) {
        fprintf(stderr, "Fatal error in io::eof: Stack underflow\n");
        qd_print_stack_trace(ctx);
        abort();
    }

    qd_stack_element_t elem;
    qd_stack_error err = qd_stack_peek(ctx->st, &elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::eof: Failed to peek handle\n");
        abort();
    }
    if (elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::eof: Expected pointer for handle, got %d\n", elem.type);
        abort();
    }

    FILE* fp = (FILE*)elem.value.p;
    int64_t is_eof = 0;

    if (fp) {
        is_eof = feof(fp) ? 1 : 0;
    }

    qd_push_i(ctx, is_eof);

    return (qd_exec_result){0};
}

// New unified API names
qd_exec_result usr_io_seek(qd_context* ctx) {
    return usr_io_seekg(ctx);  // Just call the existing implementation
}

qd_exec_result usr_io_tell(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 1) {
        fprintf(stderr, "Fatal error in io::tell: Stack underflow\n");
        qd_print_stack_trace(ctx);
        abort();
    }

    qd_stack_element_t elem;
    // Peek at the handle (don't pop it)
    qd_stack_error err = qd_stack_element(ctx->st, qd_stack_size(ctx->st) - 1, &elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::tell: Failed to peek handle\n");
        abort();
    }
    if (elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::tell: Expected pointer for handle, got %d\n", elem.type);
        abort();
    }

    FILE* fp = (FILE*)elem.value.p;
    if (!fp) {
        ctx->error_code = IO_ERR_INVALID_HANDLE;
        set_error_msg(ctx, "io::tell: invalid file handle");
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_INVALID_HANDLE};
    }

    int64_t position = ftell(fp);
    if (position < 0) {
        ctx->error_code = IO_ERR_SEEK;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_SEEK};
    }

    // On success, push result then Ok
    qd_push_i(ctx, position);
    qd_push_i(ctx, IO_ERR_OK);

    return (qd_exec_result){0};
}

// Unified buffer-based read (new primary API)
qd_exec_result usr_io_read(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 3) {
        fprintf(stderr, "Fatal error in io::read_bytes: Stack underflow (need 3, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop count
    qd_stack_element_t count_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::read_bytes: Failed to pop count\n");
        abort();
    }
    if (count_elem.type != QD_STACK_TYPE_INT) {
        fprintf(stderr, "Fatal error in io::read_bytes: Expected integer for count, got %d\n", count_elem.type);
        abort();
    }

    // Pop buffer
    qd_stack_element_t buffer_elem;
    err = qd_stack_pop(ctx->st, &buffer_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::read_bytes: Failed to pop buffer\n");
        abort();
    }
    if (buffer_elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::read_bytes: Expected pointer for buffer, got %d\n", buffer_elem.type);
        abort();
    }

    // Pop handle
    qd_stack_element_t handle_elem;
    err = qd_stack_pop(ctx->st, &handle_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::read_bytes: Failed to pop handle\n");
        abort();
    }
    if (handle_elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::read_bytes: Expected pointer for handle, got %d\n", handle_elem.type);
        abort();
    }

    FILE* fp = (FILE*)handle_elem.value.p;
    void* buffer = buffer_elem.value.p;
    int64_t count = count_elem.value.i;

    if (!fp || !buffer || count < 0) {
        ctx->error_code = IO_ERR_INVALID_ARG;
        set_error_msg(ctx, "io::read: invalid file handle, buffer, or count");
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_INVALID_ARG};
    }

    if (count == 0) {
        qd_push_i(ctx, 0);
        qd_push_i(ctx, IO_ERR_OK);
        return (qd_exec_result){0};
    }

    // Read from file
    size_t bytes_read = fread(buffer, 1, (size_t)count, fp);

    if (bytes_read < (size_t)count && ferror(fp)) {
        ctx->error_code = IO_ERR_READ;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_READ};
    }

    // On success, push result then Ok
    qd_push_i(ctx, (int64_t)bytes_read);
    qd_push_i(ctx, IO_ERR_OK);

    return (qd_exec_result){0};
}

// Read a line from stdin
qd_exec_result usr_io_readline(qd_context* ctx) {
    char* line = NULL;
    size_t len = 0;
    ssize_t nread = getline(&line, &len, stdin);

    if (nread == -1) {
        ctx->error_code = IO_ERR_EOF;
        set_error_msg(ctx, "io::readline: read error or EOF");
        free(line);
        qd_push_i(ctx, IO_ERR_EOF);
        return (qd_exec_result){IO_ERR_EOF};
    }

    // Remove trailing newline if present
    if (nread > 0 && line[nread - 1] == '\n') {
        line[nread - 1] = '\0';
        nread--;
    }

    // On success, push result then Ok
    qd_push_s(ctx, line);
    qd_push_i(ctx, IO_ERR_OK);
    free(line);

    return (qd_exec_result){0};
}

// Unified buffer-based write (new primary API)
qd_exec_result usr_io_write(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 3) {
        fprintf(stderr, "Fatal error in io::write_bytes: Stack underflow (need 3, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop count
    qd_stack_element_t count_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::write_bytes: Failed to pop count\n");
        abort();
    }
    if (count_elem.type != QD_STACK_TYPE_INT) {
        fprintf(stderr, "Fatal error in io::write_bytes: Expected integer for count, got %d\n", count_elem.type);
        abort();
    }

    // Pop buffer
    qd_stack_element_t buffer_elem;
    err = qd_stack_pop(ctx->st, &buffer_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::write_bytes: Failed to pop buffer\n");
        abort();
    }
    if (buffer_elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::write_bytes: Expected pointer for buffer, got %d\n", buffer_elem.type);
        abort();
    }

    // Pop handle
    qd_stack_element_t handle_elem;
    err = qd_stack_pop(ctx->st, &handle_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::write_bytes: Failed to pop handle\n");
        abort();
    }
    if (handle_elem.type != QD_STACK_TYPE_PTR) {
        fprintf(stderr, "Fatal error in io::write_bytes: Expected pointer for handle, got %d\n", handle_elem.type);
        abort();
    }

    FILE* fp = (FILE*)handle_elem.value.p;
    void* buffer = buffer_elem.value.p;
    int64_t count = count_elem.value.i;

    if (!fp || !buffer || count < 0) {
        ctx->error_code = IO_ERR_INVALID_ARG;
        set_error_msg(ctx, "io::write: invalid file handle, buffer, or count");
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_INVALID_ARG};
    }

    if (count == 0) {
        qd_push_i(ctx, 0);
        qd_push_i(ctx, IO_ERR_OK);
        return (qd_exec_result){0};
    }

    // Write to file
    size_t bytes_written = fwrite(buffer, 1, (size_t)count, fp);

    if (bytes_written < (size_t)count && ferror(fp)) {
        ctx->error_code = IO_ERR_WRITE;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, ctx->error_code);
        return (qd_exec_result){IO_ERR_WRITE};
    }

    // On success, push result then Ok
    qd_push_i(ctx, (int64_t)bytes_written);
    qd_push_i(ctx, IO_ERR_OK);

    return (qd_exec_result){0};
}

qd_exec_result usr_io_read_file(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 1) {
        fprintf(stderr, "Fatal error in io::read_file: Stack underflow (need 1, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop path
    qd_stack_element_t path_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::read_file: Failed to pop path\n");
        abort();
    }
    if (path_elem.type != QD_STACK_TYPE_STR) {
        fprintf(stderr, "Fatal error in io::read_file: Expected string for path, got %d\n", path_elem.type);
        abort();
    }

    const char* path = qd_string_data(path_elem.value.s);

    // Open file
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        int saved_errno = errno;
        qd_string_release(path_elem.value.s);
        ctx->error_code = IO_ERR_NOT_FOUND;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_NOT_FOUND);
        return (qd_exec_result){IO_ERR_NOT_FOUND};
    }

    // Get file size
    if (fseek(fp, 0, SEEK_END) != 0) {
        int saved_errno = errno;
        fclose(fp);
        qd_string_release(path_elem.value.s);
        ctx->error_code = IO_ERR_SEEK;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_SEEK);
        return (qd_exec_result){IO_ERR_SEEK};
    }

    long file_size = ftell(fp);
    if (file_size < 0) {
        int saved_errno = errno;
        fclose(fp);
        qd_string_release(path_elem.value.s);
        ctx->error_code = IO_ERR_SEEK;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_SEEK);
        return (qd_exec_result){IO_ERR_SEEK};
    }

    // Seek back to start
    if (fseek(fp, 0, SEEK_SET) != 0) {
        int saved_errno = errno;
        fclose(fp);
        qd_string_release(path_elem.value.s);
        ctx->error_code = IO_ERR_SEEK;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_SEEK);
        return (qd_exec_result){IO_ERR_SEEK};
    }

    // Allocate buffer
    char* buffer = malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(fp);
        qd_string_release(path_elem.value.s);
        ctx->error_code = IO_ERR_READ;
        set_error_msg(ctx, "io::read_file: allocation failed");
        qd_push_i(ctx, IO_ERR_READ);
        return (qd_exec_result){IO_ERR_READ};
    }

    // Read entire file
    size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);
    buffer[bytes_read] = '\0';

    // Check for read error before closing file (ferror requires open file)
    int had_read_error = (bytes_read < (size_t)file_size && ferror(fp));
    int saved_errno = errno;
    fclose(fp);
    qd_string_release(path_elem.value.s);

    if (had_read_error) {
        free(buffer);
        ctx->error_code = IO_ERR_READ;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_READ);
        return (qd_exec_result){IO_ERR_READ};
    }

    // On success, push result then Ok
    qd_push_s(ctx, buffer);
    qd_push_i(ctx, IO_ERR_OK);

    free(buffer);

    return (qd_exec_result){0};
}

qd_exec_result usr_io_write_file(qd_context* ctx) {
    size_t stack_size = qd_stack_size(ctx->st);
    if (stack_size < 2) {
        fprintf(stderr, "Fatal error in io::write_file: Stack underflow (need 2, have %zu)\n", stack_size);
        qd_print_stack_trace(ctx);
        abort();
    }

    // Pop contents (top of stack)
    qd_stack_element_t contents_elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &contents_elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "Fatal error in io::write_file: Failed to pop contents\n");
        abort();
    }
    if (contents_elem.type != QD_STACK_TYPE_STR) {
        fprintf(stderr, "Fatal error in io::write_file: Expected string for contents, got %d\n", contents_elem.type);
        abort();
    }

    // Pop path
    qd_stack_element_t path_elem;
    err = qd_stack_pop(ctx->st, &path_elem);
    if (err != QD_STACK_OK) {
        qd_string_release(contents_elem.value.s);
        fprintf(stderr, "Fatal error in io::write_file: Failed to pop path\n");
        abort();
    }
    if (path_elem.type != QD_STACK_TYPE_STR) {
        qd_string_release(contents_elem.value.s);
        fprintf(stderr, "Fatal error in io::write_file: Expected string for path, got %d\n", path_elem.type);
        abort();
    }

    const char* path = qd_string_data(path_elem.value.s);
    const char* contents = qd_string_data(contents_elem.value.s);
    size_t len = strlen(contents);

    // Open file for writing (truncate if exists)
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        int saved_errno = errno;
        qd_string_release(path_elem.value.s);
        qd_string_release(contents_elem.value.s);
        ctx->error_code = IO_ERR_PERMISSION;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_PERMISSION);
        return (qd_exec_result){IO_ERR_PERMISSION};
    }

    // Write contents
    size_t written = fwrite(contents, 1, len, fp);
    int saved_errno = errno;
    fclose(fp);

    qd_string_release(path_elem.value.s);
    qd_string_release(contents_elem.value.s);

    if (written < len) {
        ctx->error_code = IO_ERR_WRITE;
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "%s", strerror(saved_errno));
        set_error_msg(ctx, err_buf);
        qd_push_i(ctx, IO_ERR_WRITE);
        return (qd_exec_result){IO_ERR_WRITE};
    }

    // On success, push Ok
    qd_push_i(ctx, IO_ERR_OK);
    return (qd_exec_result){0};
}
