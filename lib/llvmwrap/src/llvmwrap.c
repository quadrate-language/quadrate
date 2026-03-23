/**
 * @file llvmwrap.c
 * @brief LLVM C API wrappers for the Quadrate self-hosted compiler.
 *
 * Each function follows the Quadrate native calling convention:
 *   int usr_llvmwrap_funcname(qd_context* ctx)
 * Arguments are popped from the runtime stack, results are pushed back.
 */

#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: pop i64 from stack
static int pop_int(qd_context* ctx, int64_t* out) {
	qd_stack_element_t e;
	if (qd_stack_pop(ctx->st, &e) != QD_STACK_OK) return -1;
	*out = e.value.i;
	return 0;
}

// Helper: pop ptr from stack
static int pop_ptr(qd_context* ctx, void** out) {
	qd_stack_element_t e;
	if (qd_stack_pop(ctx->st, &e) != QD_STACK_OK) return -1;
	*out = e.value.p;
	return 0;
}

// Helper: pop f64 from stack
static int pop_float(qd_context* ctx, double* out) {
	qd_stack_element_t e;
	if (qd_stack_pop(ctx->st, &e) != QD_STACK_OK) return -1;
	*out = e.value.f;
	return 0;
}

// Helper: pop string from stack, return C string (caller must release)
static int pop_str(qd_context* ctx, const char** out, qd_string_t** sref) {
	qd_stack_element_t e;
	if (qd_stack_pop(ctx->st, &e) != QD_STACK_OK) return -1;
	if (e.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "llvmwrap: expected string, got type %d\n", e.type);
		return -1;
	}
	*out = qd_string_data(e.value.s);
	*sref = e.value.s;
	return 0;
}

// ============================================================
// Module/Context
// ============================================================

// ( -- ctx:ptr )
int usr_llvmwrap_context_create(qd_context* ctx) {
	LLVMContextRef c = LLVMContextCreate();
	qd_push_p(ctx, (void*)c);
	return 0;
}

// ( name:str ctx:ptr -- mod:ptr )
int usr_llvmwrap_module_create(qd_context* ctx) {
	void* llvm_ctx;
	const char* name; qd_string_t* sref;
	pop_ptr(ctx, &llvm_ctx);
	pop_str(ctx, &name, &sref);
	LLVMModuleRef mod = LLVMModuleCreateWithNameInContext(name, (LLVMContextRef)llvm_ctx);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)mod);
	return 0;
}

// ( mod:ptr -- ok:i64 )
int usr_llvmwrap_module_verify(qd_context* ctx) {
	void* mod;
	pop_ptr(ctx, &mod);
	char* error = NULL;
	LLVMBool failed = LLVMVerifyModule((LLVMModuleRef)mod, LLVMReturnStatusAction, &error);
	if (failed) {
		fprintf(stderr, "LLVM verification failed: %s\n", error ? error : "(unknown)");
	}
	if (error) LLVMDisposeMessage(error);
	qd_push_i(ctx, failed ? 0 : 1);
	return 0;
}

// ( mod:ptr -- ir:str )
int usr_llvmwrap_module_print(qd_context* ctx) {
	void* mod;
	pop_ptr(ctx, &mod);
	char* ir = LLVMPrintModuleToString((LLVMModuleRef)mod);
	qd_push_s(ctx, ir);
	LLVMDisposeMessage(ir);
	return 0;
}

// ( mod:ptr path:str -- ok:i64 )
int usr_llvmwrap_write_bitcode(qd_context* ctx) {
	const char* path; qd_string_t* sref;
	void* mod;
	pop_str(ctx, &path, &sref);
	pop_ptr(ctx, &mod);
	int result = LLVMWriteBitcodeToFile((LLVMModuleRef)mod, path);
	qd_string_release(sref);
	qd_push_i(ctx, result == 0 ? 1 : 0);
	return 0;
}

// ( mod:ptr -- )
int usr_llvmwrap_module_dispose(qd_context* ctx) {
	void* mod;
	pop_ptr(ctx, &mod);
	LLVMDisposeModule((LLVMModuleRef)mod);
	return 0;
}

// ( ctx:ptr -- )
int usr_llvmwrap_context_dispose(qd_context* ctx) {
	void* c;
	pop_ptr(ctx, &c);
	LLVMContextDispose((LLVMContextRef)c);
	return 0;
}

// ============================================================
// Types
// ============================================================

// ( ctx:ptr -- t:ptr )
int usr_llvmwrap_int1_type(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMInt1TypeInContext((LLVMContextRef)c));
	return 0;
}

int usr_llvmwrap_int8_type(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMInt8TypeInContext((LLVMContextRef)c));
	return 0;
}

int usr_llvmwrap_int32_type(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMInt32TypeInContext((LLVMContextRef)c));
	return 0;
}

int usr_llvmwrap_int64_type(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMInt64TypeInContext((LLVMContextRef)c));
	return 0;
}

int usr_llvmwrap_double_type(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMDoubleTypeInContext((LLVMContextRef)c));
	return 0;
}

int usr_llvmwrap_ptr_type(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMPointerTypeInContext((LLVMContextRef)c, 0));
	return 0;
}

// ( ret:ptr params:ptr count:i64 variadic:i64 -- t:ptr )
int usr_llvmwrap_fn_type(qd_context* ctx) {
	int64_t variadic, count;
	void *params, *ret;
	pop_int(ctx, &variadic);
	pop_int(ctx, &count);
	pop_ptr(ctx, &params);
	pop_ptr(ctx, &ret);
	LLVMTypeRef ft = LLVMFunctionType(
		(LLVMTypeRef)ret, (LLVMTypeRef*)params, (unsigned)count, variadic ? 1 : 0);
	qd_push_p(ctx, (void*)ft);
	return 0;
}

// ( ctx:ptr name:str fields:ptr count:i64 -- t:ptr )
int usr_llvmwrap_struct_type(qd_context* ctx) {
	int64_t count;
	void *fields, *c;
	const char* name; qd_string_t* sref;
	pop_int(ctx, &count);
	pop_ptr(ctx, &fields);
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &c);
	LLVMTypeRef st = LLVMStructCreateNamed((LLVMContextRef)c, name);
	LLVMStructSetBody(st, (LLVMTypeRef*)fields, (unsigned)count, 0);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)st);
	return 0;
}

// ============================================================
// Functions/Blocks
// ============================================================

// ( mod:ptr name:str ty:ptr -- fn:ptr )
int usr_llvmwrap_add_function(qd_context* ctx) {
	void *ty, *mod;
	const char* name; qd_string_t* sref;
	pop_ptr(ctx, &ty);
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &mod);
	LLVMValueRef fn = LLVMAddFunction((LLVMModuleRef)mod, name, (LLVMTypeRef)ty);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)fn);
	return 0;
}

// ( ctx:ptr fn:ptr name:str -- bb:ptr )
int usr_llvmwrap_append_block(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void *fn, *c;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &fn);
	pop_ptr(ctx, &c);
	LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext((LLVMContextRef)c, (LLVMValueRef)fn, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)bb);
	return 0;
}

// ( fn:ptr linkage:i64 -- )
int usr_llvmwrap_set_linkage(qd_context* ctx) {
	int64_t linkage;
	void* fn;
	pop_int(ctx, &linkage);
	pop_ptr(ctx, &fn);
	LLVMSetLinkage((LLVMValueRef)fn, (LLVMLinkage)linkage);
	return 0;
}

// ( fn:ptr idx:i64 -- val:ptr )
int usr_llvmwrap_get_param(qd_context* ctx) {
	int64_t idx;
	void* fn;
	pop_int(ctx, &idx);
	pop_ptr(ctx, &fn);
	qd_push_p(ctx, (void*)LLVMGetParam((LLVMValueRef)fn, (unsigned)idx));
	return 0;
}

// ( mod:ptr name:str -- fn:ptr )
int usr_llvmwrap_get_function(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void* mod;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &mod);
	LLVMValueRef fn = LLVMGetNamedFunction((LLVMModuleRef)mod, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)fn);
	return 0;
}

// ============================================================
// Builder
// ============================================================

// ( ctx:ptr -- b:ptr )
int usr_llvmwrap_builder_create(qd_context* ctx) {
	void* c; pop_ptr(ctx, &c);
	qd_push_p(ctx, (void*)LLVMCreateBuilderInContext((LLVMContextRef)c));
	return 0;
}

// ( b:ptr bb:ptr -- )
int usr_llvmwrap_position_at_end(qd_context* ctx) {
	void *bb, *b;
	pop_ptr(ctx, &bb);
	pop_ptr(ctx, &b);
	LLVMPositionBuilderAtEnd((LLVMBuilderRef)b, (LLVMBasicBlockRef)bb);
	return 0;
}

// ( b:ptr fnty:ptr fn:ptr args:ptr nargs:i64 name:str -- val:ptr )
int usr_llvmwrap_build_call(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	int64_t nargs;
	void *args, *fn, *fnty, *b;
	pop_str(ctx, &name, &sref);
	pop_int(ctx, &nargs);
	pop_ptr(ctx, &args);
	pop_ptr(ctx, &fn);
	pop_ptr(ctx, &fnty);
	pop_ptr(ctx, &b);
	LLVMValueRef val = LLVMBuildCall2(
		(LLVMBuilderRef)b, (LLVMTypeRef)fnty, (LLVMValueRef)fn,
		(LLVMValueRef*)args, (unsigned)nargs, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)val);
	return 0;
}

// ( b:ptr val:ptr -- inst:ptr )
int usr_llvmwrap_build_ret(qd_context* ctx) {
	void *val, *b;
	pop_ptr(ctx, &val);
	pop_ptr(ctx, &b);
	qd_push_p(ctx, (void*)LLVMBuildRet((LLVMBuilderRef)b, (LLVMValueRef)val));
	return 0;
}

// ( b:ptr -- inst:ptr )
int usr_llvmwrap_build_ret_void(qd_context* ctx) {
	void* b; pop_ptr(ctx, &b);
	qd_push_p(ctx, (void*)LLVMBuildRetVoid((LLVMBuilderRef)b));
	return 0;
}

// ( b:ptr bb:ptr -- inst:ptr )
int usr_llvmwrap_build_br(qd_context* ctx) {
	void *bb, *b;
	pop_ptr(ctx, &bb);
	pop_ptr(ctx, &b);
	qd_push_p(ctx, (void*)LLVMBuildBr((LLVMBuilderRef)b, (LLVMBasicBlockRef)bb));
	return 0;
}

// ( b:ptr cond:ptr then_bb:ptr else_bb:ptr -- inst:ptr )
int usr_llvmwrap_build_cond_br(qd_context* ctx) {
	void *else_bb, *then_bb, *cond, *b;
	pop_ptr(ctx, &else_bb);
	pop_ptr(ctx, &then_bb);
	pop_ptr(ctx, &cond);
	pop_ptr(ctx, &b);
	qd_push_p(ctx, (void*)LLVMBuildCondBr(
		(LLVMBuilderRef)b, (LLVMValueRef)cond,
		(LLVMBasicBlockRef)then_bb, (LLVMBasicBlockRef)else_bb));
	return 0;
}

// ( b:ptr ty:ptr name:str -- val:ptr )
int usr_llvmwrap_build_alloca(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void *ty, *b;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &ty);
	pop_ptr(ctx, &b);
	LLVMValueRef val = LLVMBuildAlloca((LLVMBuilderRef)b, (LLVMTypeRef)ty, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)val);
	return 0;
}

// ( b:ptr ty:ptr ptr:ptr name:str -- val:ptr )
int usr_llvmwrap_build_load(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void *ptr, *ty, *b;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &ptr);
	pop_ptr(ctx, &ty);
	pop_ptr(ctx, &b);
	LLVMValueRef val = LLVMBuildLoad2((LLVMBuilderRef)b, (LLVMTypeRef)ty, (LLVMValueRef)ptr, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)val);
	return 0;
}

// ( b:ptr val:ptr ptr:ptr -- inst:ptr )
int usr_llvmwrap_build_store(qd_context* ctx) {
	void *ptr, *val, *b;
	pop_ptr(ctx, &ptr);
	pop_ptr(ctx, &val);
	pop_ptr(ctx, &b);
	qd_push_p(ctx, (void*)LLVMBuildStore((LLVMBuilderRef)b, (LLVMValueRef)val, (LLVMValueRef)ptr));
	return 0;
}

// ( b:ptr ty:ptr ptr:ptr indices:ptr nidx:i64 name:str -- val:ptr )
int usr_llvmwrap_build_gep(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	int64_t nidx;
	void *indices, *ptr, *ty, *b;
	pop_str(ctx, &name, &sref);
	pop_int(ctx, &nidx);
	pop_ptr(ctx, &indices);
	pop_ptr(ctx, &ptr);
	pop_ptr(ctx, &ty);
	pop_ptr(ctx, &b);
	LLVMValueRef val = LLVMBuildGEP2(
		(LLVMBuilderRef)b, (LLVMTypeRef)ty, (LLVMValueRef)ptr,
		(LLVMValueRef*)indices, (unsigned)nidx, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)val);
	return 0;
}

// ( b:ptr pred:i64 lhs:ptr rhs:ptr name:str -- val:ptr )
int usr_llvmwrap_build_icmp(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void *rhs, *lhs, *b;
	int64_t pred;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &rhs);
	pop_ptr(ctx, &lhs);
	pop_int(ctx, &pred);
	pop_ptr(ctx, &b);
	LLVMValueRef val = LLVMBuildICmp(
		(LLVMBuilderRef)b, (LLVMIntPredicate)pred,
		(LLVMValueRef)lhs, (LLVMValueRef)rhs, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)val);
	return 0;
}

// ( b:ptr val:ptr ty:ptr name:str -- val:ptr )
int usr_llvmwrap_build_inttoptr(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void *ty, *val, *b;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &ty);
	pop_ptr(ctx, &val);
	pop_ptr(ctx, &b);
	LLVMValueRef r = LLVMBuildIntToPtr((LLVMBuilderRef)b, (LLVMValueRef)val, (LLVMTypeRef)ty, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)r);
	return 0;
}

// ( b:ptr -- )
int usr_llvmwrap_build_unreachable(qd_context* ctx) {
	void* b; pop_ptr(ctx, &b);
	LLVMBuildUnreachable((LLVMBuilderRef)b);
	return 0;
}

// ( b:ptr -- )
int usr_llvmwrap_builder_dispose(qd_context* ctx) {
	void* b; pop_ptr(ctx, &b);
	LLVMDisposeBuilder((LLVMBuilderRef)b);
	return 0;
}

// ============================================================
// Constants
// ============================================================

// ( ty:ptr val:i64 -- c:ptr )
int usr_llvmwrap_const_int(qd_context* ctx) {
	int64_t val;
	void* ty;
	pop_int(ctx, &val);
	pop_ptr(ctx, &ty);
	qd_push_p(ctx, (void*)LLVMConstInt((LLVMTypeRef)ty, (unsigned long long)val, 1));
	return 0;
}

// ( ty:ptr val:f64 -- c:ptr )
int usr_llvmwrap_const_real(qd_context* ctx) {
	double val;
	void* ty;
	pop_float(ctx, &val);
	pop_ptr(ctx, &ty);
	qd_push_p(ctx, (void*)LLVMConstReal((LLVMTypeRef)ty, val));
	return 0;
}

// ( ty:ptr -- c:ptr )
int usr_llvmwrap_const_null(qd_context* ctx) {
	void* ty; pop_ptr(ctx, &ty);
	qd_push_p(ctx, (void*)LLVMConstNull((LLVMTypeRef)ty));
	return 0;
}

// ( b:ptr str:str name:str -- val:ptr )
int usr_llvmwrap_build_global_string(qd_context* ctx) {
	const char *name, *str;
	qd_string_t *sref_name, *sref_str;
	void* b;
	pop_str(ctx, &name, &sref_name);
	pop_str(ctx, &str, &sref_str);
	pop_ptr(ctx, &b);
	LLVMValueRef val = LLVMBuildGlobalStringPtr((LLVMBuilderRef)b, str, name);
	qd_string_release(sref_name);
	qd_string_release(sref_str);
	qd_push_p(ctx, (void*)val);
	return 0;
}

// ============================================================
// Args array helpers (for build_call)
// ============================================================

// ( cap:i64 -- arr:ptr )
int usr_llvmwrap_args_new(qd_context* ctx) {
	int64_t cap;
	pop_int(ctx, &cap);
	LLVMValueRef* arr = (LLVMValueRef*)calloc((size_t)cap, sizeof(LLVMValueRef));
	qd_push_p(ctx, (void*)arr);
	return 0;
}

// ( arr:ptr idx:i64 val:ptr -- )
int usr_llvmwrap_args_set(qd_context* ctx) {
	void *val, *arr;
	int64_t idx;
	pop_ptr(ctx, &val);
	pop_int(ctx, &idx);
	pop_ptr(ctx, &arr);
	((LLVMValueRef*)arr)[idx] = (LLVMValueRef)val;
	return 0;
}

// ( arr:ptr idx:i64 -- val:ptr )
int usr_llvmwrap_args_get(qd_context* ctx) {
	int64_t idx;
	void* arr;
	pop_int(ctx, &idx);
	pop_ptr(ctx, &arr);
	qd_push_p(ctx, (void*)((LLVMValueRef*)arr)[idx]);
	return 0;
}

// ( arr:ptr -- )
int usr_llvmwrap_args_free(qd_context* ctx) {
	void* arr; pop_ptr(ctx, &arr);
	free(arr);
	return 0;
}

// ============================================================
// Globals
// ============================================================

// ( mod:ptr name:str ty:ptr -- global:ptr )
int usr_llvmwrap_add_global(qd_context* ctx) {
	void *ty, *mod;
	const char* name; qd_string_t* sref;
	pop_ptr(ctx, &ty);
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &mod);
	LLVMValueRef g = LLVMAddGlobal((LLVMModuleRef)mod, (LLVMTypeRef)ty, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)g);
	return 0;
}

// ( mod:ptr name:str -- global:ptr )
int usr_llvmwrap_get_global(qd_context* ctx) {
	const char* name; qd_string_t* sref;
	void* mod;
	pop_str(ctx, &name, &sref);
	pop_ptr(ctx, &mod);
	LLVMValueRef g = LLVMGetNamedGlobal((LLVMModuleRef)mod, name);
	qd_string_release(sref);
	qd_push_p(ctx, (void*)g);
	return 0;
}

// ( global:ptr val:ptr -- )
int usr_llvmwrap_set_initializer(qd_context* ctx) {
	void *val, *g;
	pop_ptr(ctx, &val);
	pop_ptr(ctx, &g);
	LLVMSetInitializer((LLVMValueRef)g, (LLVMValueRef)val);
	return 0;
}

// ( global:ptr -- )
int usr_llvmwrap_set_global_constant(qd_context* ctx) {
	void* g; pop_ptr(ctx, &g);
	LLVMSetGlobalConstant((LLVMValueRef)g, 1);
	return 0;
}

// ============================================================
// Type array helpers (for fn_type, struct_type)
// ============================================================

// ( cap:i64 -- arr:ptr )
int usr_llvmwrap_types_new(qd_context* ctx) {
	int64_t cap;
	pop_int(ctx, &cap);
	LLVMTypeRef* arr = (LLVMTypeRef*)calloc((size_t)cap, sizeof(LLVMTypeRef));
	qd_push_p(ctx, (void*)arr);
	return 0;
}

// ( arr:ptr idx:i64 ty:ptr -- )
int usr_llvmwrap_types_set(qd_context* ctx) {
	void *ty, *arr;
	int64_t idx;
	pop_ptr(ctx, &ty);
	pop_int(ctx, &idx);
	pop_ptr(ctx, &arr);
	((LLVMTypeRef*)arr)[idx] = (LLVMTypeRef)ty;
	return 0;
}

// ( arr:ptr -- )
int usr_llvmwrap_types_free(qd_context* ctx) {
	void* arr; pop_ptr(ctx, &arr);
	free(arr);
	return 0;
}
