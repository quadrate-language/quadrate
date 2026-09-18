/**
 * @file qd_string.h
 * @brief Reference-counted string implementation for Quadrate runtime
 *
 * Provides efficient string management with copy-on-write semantics through
 * reference counting. Strings are shared when possible (e.g., during stack
 * duplication) and only freed when all references are released.
 */

#ifndef QD_QUADRATE_RUNTIME_STRING_H
#define QD_QUADRATE_RUNTIME_STRING_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
#include <atomic>
#include <cstdint>
extern "C" {
typedef std::atomic<size_t> qd_atomic_size_t;
typedef std::atomic<uint64_t> qd_atomic_uint64;
#else
#include <stdatomic.h>
#include <stdint.h>
typedef atomic_size_t qd_atomic_size_t;
typedef _Atomic uint64_t qd_atomic_uint64;
#endif

/**
 * @brief Minimum capacity for small strings (0-7 bytes)
 */
#define QD_STRING_SMALL_CAPACITY 8

/**
 * @brief Minimum capacity for regular strings (8+ bytes)
 */
#define QD_STRING_MIN_CAPACITY 16

/**
 * @brief Default initial capacity for string builders
 */
#define QD_SB_DEFAULT_CAPACITY 64

/**
 * @brief Reference-counted string structure with capacity
 *
 * Strings use atomic reference counting for thread-safety.
 * The reference count starts at 1 when created.
 * When refcount==1, strings can be mutated in-place if capacity allows.
 */
/** @brief char_length has not been computed for this string yet */
#define QD_STRING_CHAR_LEN_UNKNOWN ((size_t)-1)

typedef struct qd_string {
	char* data;			///< Null-terminated string data (owned)
	size_t length;		///< Byte length (cached, excluding null terminator)
	size_t capacity;	///< Buffer capacity (excluding null terminator)
	size_t char_length; ///< Codepoint count, or QD_STRING_CHAR_LEN_UNKNOWN until asked
	/// Memo for character-index lookups: the packed (char index, byte offset) of the last one.
	/// Both halves move together in a single atomic word so a concurrent reader can never pair
	/// one update's index with another's offset. Zero means "start of string", which is always
	/// a valid place to resume from.
	qd_atomic_uint64 scan_cursor;
	qd_atomic_size_t refcount; ///< Atomic reference count
} qd_string_t;

/**
 * @brief Create a new reference-counted string
 *
 * Creates a new string with an initial reference count of 1.
 * The input string is copied.
 *
 * @param str Source string to copy (must not be NULL)
 * @return Pointer to new qd_string_t, or NULL on allocation failure
 *
 * @note The caller owns the initial reference and must call qd_string_release()
 * @note Returns NULL if str is NULL or allocation fails
 */
qd_string_t* qd_string_create(const char* str);

/**
 * @brief Create a reference-counted string with known length
 *
 * More efficient than qd_string_create() when length is already known.
 *
 * @param str Source string to copy (must not be NULL)
 * @param length Length of the string (excluding null terminator)
 * @return Pointer to new qd_string_t, or NULL on allocation failure
 *
 * @note The caller owns the initial reference and must call qd_string_release()
 */
qd_string_t* qd_string_create_with_length(const char* str, size_t length);

/**
 * @brief Increment the reference count of a string
 *
 * Increases the reference count by 1, indicating that another owner
 * now shares ownership of the string.
 *
 * @param str String to retain (can be NULL - no-op)
 * @return The same string pointer for convenience
 *
 * @note Thread-safe: uses atomic operations
 * @note If str is NULL, this is a no-op and returns NULL
 */
/**
 * @brief Number of codepoints in the string, computed once and cached
 *
 * `length` is bytes; this is characters. Counting is a walk, so the answer is memoised on
 * the string -- a loop calling strings::char_at would otherwise rescan from the start on
 * every character and turn an ordinary scan quadratic.
 *
 * When this equals `length` every character occupies one byte, which lets an index-based
 * operation skip the walk entirely and index bytes directly.
 */
size_t qd_string_char_length(qd_string_t* str);

/**
 * @brief Byte offset of character `index`, clamped to the byte length
 *
 * The walk that finding a character requires is what makes a loop reading a string one
 * character at a time quadratic. Two things stop that: when every character is one byte the
 * index *is* the offset, and otherwise the last lookup is memoised so a forward scan -- which
 * is what such loops do -- resumes from where it left off instead of from the start.
 */
size_t qd_string_char_offset(qd_string_t* str, size_t index);

/**
 * @brief Character index of byte offset `byte_offset`, the inverse of qd_string_char_offset
 *
 * A search returns where in the bytes it matched; the string functions report character
 * indices. Shares the same cursor, so scanning a string with repeated searches costs one walk
 * in total rather than one per call.
 */
size_t qd_string_char_index(qd_string_t* str, size_t byte_offset);

qd_string_t* qd_string_retain(qd_string_t* str);

/**
 * @brief Decrement the reference count and free if zero
 *
 * Decreases the reference count by 1. If the count reaches zero,
 * the string data and structure are freed.
 *
 * @param str String to release (can be NULL - no-op)
 *
 * @note Thread-safe: uses atomic operations
 * @note If str is NULL, this is a no-op
 * @note After calling this, the pointer should be considered invalid
 *       unless you know other references exist
 */
void qd_string_release(qd_string_t* str);

/**
 * @brief Get the current reference count
 *
 * @param str String to query (must not be NULL)
 * @return Current reference count
 *
 * @note For debugging/testing purposes only
 * @note The returned value may be stale in multithreaded contexts
 */
size_t qd_string_refcount(const qd_string_t* str);

/**
 * @brief Get the string data
 *
 * @param str String to query (must not be NULL)
 * @return Pointer to null-terminated string data
 *
 * @note The returned pointer is valid as long as the string is retained
 * @note Do not free the returned pointer
 */
const char* qd_string_data(const qd_string_t* str);

/**
 * @brief Get the string length
 *
 * @param str String to query (must not be NULL)
 * @return Length of the string (excluding null terminator)
 */
size_t qd_string_length(const qd_string_t* str);

/**
 * @brief Smart concatenation with in-place optimization
 *
 * If str1 has refcount==1 and enough capacity, appends in-place.
 * Otherwise creates a new string with extra capacity.
 * str1 and str2 are consumed (released).
 *
 * @param str1 First string (will be released)
 * @param str2 Second string (will be released)
 * @return New concatenated string, or NULL on allocation failure
 *
 * @note Both input strings are released regardless of success
 */
qd_string_t* qd_string_concat_smart(qd_string_t* str1, qd_string_t* str2);

/**
 * @brief String builder for efficient repeated concatenations
 *
 * Provides O(n) concatenation by using a growable buffer instead of
 * creating a new string on each append. Uses geometric growth (2x)
 * to amortize allocation costs.
 */
typedef struct qd_string_builder {
	char* data;		 ///< Buffer for building string (owned)
	size_t length;	 ///< Current string length
	size_t capacity; ///< Current buffer capacity
} qd_string_builder_t;

/**
 * @brief Create a new string builder
 *
 * @param initial_capacity Initial buffer capacity (0 = use default)
 * @return Pointer to new string builder, or NULL on allocation failure
 *
 * @note Caller must call qd_sb_free() when done
 */
qd_string_builder_t* qd_sb_create(size_t initial_capacity);

/**
 * @brief Append a C string to the builder
 *
 * @param sb String builder (must not be NULL)
 * @param str String to append (must not be NULL)
 * @param len Length of string to append
 * @return true on success, false on allocation failure
 */
bool qd_sb_append(qd_string_builder_t* sb, const char* str, size_t len);

/**
 * @brief Convert string builder to qd_string_t
 *
 * Creates a reference-counted string from the builder's contents.
 * The builder remains valid and can continue to be used.
 *
 * @param sb String builder (must not be NULL)
 * @return New qd_string_t, or NULL on allocation failure
 *
 * @note The returned string has refcount=1; caller must release it
 */
qd_string_t* qd_sb_to_string(qd_string_builder_t* sb);

/**
 * @brief Free a string builder
 *
 * @param sb String builder to free (can be NULL - no-op)
 */
void qd_sb_free(qd_string_builder_t* sb);

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_STRING_H
