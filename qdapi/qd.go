package main

/*
#include <stdlib.h>

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

func main() {}
