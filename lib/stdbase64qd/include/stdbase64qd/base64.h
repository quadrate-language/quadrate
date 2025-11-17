#ifndef STDBASE64QD_BASE64_H
#define STDBASE64QD_BASE64_H

#include <qdrt/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

// Base64 encoding/decoding functions
// Named with usr_ prefix for import mechanism

// Encode binary data to base64 string
// Stack: data:p len:i -- encoded:s
qd_exec_result usr_base64_encode(qd_context* ctx);

// Decode base64 string to binary data
// Stack: encoded:s -- data:p data_len:i
qd_exec_result usr_base64_decode(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDBASE64QD_BASE64_H
