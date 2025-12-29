/**
 * @file compress.h
 * @brief Compression library for Quadrate (gzip/zlib wrapper)
 */

#ifndef QDCOMPRESS_H
#define QDCOMPRESS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compress data using gzip format (default level 6).
 * Stack: (data:str -- compressed:str)!
 */
qd_exec_result usr_compress_gzip(qd_context* ctx);

/**
 * Compress data using gzip format with specific level.
 * Stack: (data:str level:i64 -- compressed:str)!
 */
qd_exec_result usr_compress_gzip_level(qd_context* ctx);

/**
 * Decompress gzip data.
 * Stack: (compressed:str -- data:str)!
 */
qd_exec_result usr_compress_gunzip(qd_context* ctx);

/**
 * Compress data using raw deflate (no header).
 * Stack: (data:str -- compressed:str)!
 */
qd_exec_result usr_compress_deflate(qd_context* ctx);

/**
 * Decompress raw deflate data.
 * Stack: (compressed:str -- data:str)!
 */
qd_exec_result usr_compress_inflate(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif /* QDCOMPRESS_H */
