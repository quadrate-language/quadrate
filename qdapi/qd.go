package main

/*
#include <stdlib.h>

typedef struct {
} qd_module_t;

typedef struct {
	int dummy;
} qd_context_t;

*/
import "C"
import "unsafe"

//export qd_create_context
func qd_create_context() *C.qd_context_t {
	p := (*C.qd_context_t)(C.malloc(C.size_t(C.sizeof_qd_context_t)))
	return p
}

//export qd_free_context
func qd_free_context(ctx *C.qd_context_t) {
	C.free(unsafe.Pointer(ctx))
}

//export qd_build
func qd_build(ctx *C.qd_context_t) {
}

//export qd_create_module
func qd_create_module(ctx *C.qd_context_t, name *C.char) *C.qd_module_t {
	p := (*C.qd_module_t)(C.malloc(C.size_t(C.sizeof_qd_module_t)))
	return p
}
