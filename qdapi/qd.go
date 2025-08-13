package main

/*
#include <stdlib.h>
#include <stdint.h>

typedef struct {
	uintptr_t handle;;
} qd_module_t;

typedef struct {
	uintptr_t handle;;
} qd_context_t;

*/
import "C"
import (
	"sync"
	"unsafe"
)

type QdContext struct {
	Modules []QdModule
}

type QdModule struct {
	ctx     uintptr
	Name    string
	Scripts []string
}

var (
	mutex    = &sync.Mutex{}
	contexts = make(map[*C.qd_context_t]*QdContext)
)

//export qd_create_context
func qd_create_context() *C.qd_context_t {
	p := (*C.qd_context_t)(C.malloc(C.size_t(C.sizeof_qd_context_t)))
	if p == nil {
		return nil
	}
	ctx := &QdContext{}
	mutex.Lock()
	contexts[p] = ctx
	mutex.Unlock()
	return p
}

//export qd_free_context
func qd_free_context(ctx *C.qd_context_t) {
	if ctx == nil {
		return
	}
	mutex.Lock()
	delete(contexts, ctx)
	mutex.Unlock()
	C.free(unsafe.Pointer(ctx))
}

//export qd_build
func qd_build(mod *C.qd_module_t) {
}

//export qd_execute
func qd_execute(ctx *C.qd_context_t, fn *C.char) {
}

//export qd_get_module
func qd_get_module(ctx *C.qd_context_t, name *C.char) *C.qd_module_t {
	if ctx == nil {
		return nil
	}

	mutex.Lock()
	defer mutex.Unlock()

	return nil
}

//export qd_add_script
func qd_add_script(mod *C.qd_module_t, script *C.char) {
	if mod == nil {
		return
	}

	mutex.Lock()
	defer mutex.Unlock()
}

//export qd_register_function
func qd_register_function(mod *C.qd_module_t, name *C.char, fn unsafe.Pointer) {
}

func getContext(p *C.qd_context_t) *QdContext {
	mutex.Lock()
	defer mutex.Unlock()
	if ctx, ok := contexts[p]; ok {
		return ctx
	}
	return nil
}

//func getModule(p *C.qd_module_t) *QdModule {
//}
