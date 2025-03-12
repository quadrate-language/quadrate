#ifndef QD_BASE_H
#define QD_BASE_H

#define QD_STACK_DEPTH 1024
#define __qd_real_t double

extern __qd_real_t __qd_stack[QD_STACK_DEPTH];
extern int __qd_stack_ptr;

extern void __qd_arg_push(__qd_real_t x);

extern void __qd_add(int n);
extern void __qd_pop(int n);
extern void __qd_push(int n, ...);
extern void __qd_depth(int n);
extern void __qd_sub(int n);
extern void __qd_mul(int n);
extern void __qd_div(int n);
extern void __qd_swap(int n);
extern void __qd_abs(int n);
extern void __qd_acos(int n);
extern void __qd_asin(int n);
extern void __qd_atan(int n);
extern void __qd_cos(int n);
extern void __qd_sin(int n);
extern void __qd_tan(int n);
extern void __qd_cbrt(int n);
extern void __qd_ceil(int n);
extern void __qd_floor(int n);
extern void __qd_clear(int n);
extern void __qd_dec(int n);
extern void __qd_inc(int n);
extern void __qd_dup(int n);
extern void __qd_inv(int n);
extern void __qd_ln(int n);
extern void __qd_log10(int n);
extern void __qd_neg(int n);
extern void __qd_over(int n);
extern void __qd_pow(int n);
extern void __qd_read(int n);
extern void __qd_rot(int n);
extern void __qd_rot2(int n);
extern void __qd_sq(int n);
extern void __qd_sqrt(int n);
extern void __qd_write(int n);
extern void __qd_print(int n);
extern void __qd_eval(int n, const char* expression);

extern void __qd_panic_stack_underflow();
extern void __qd_panic_stack_overflow();
extern void __qd_panic_division_by_zero();
extern void __qd_panic_invalid_input();

#endif

