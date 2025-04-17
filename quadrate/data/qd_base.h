// Copyright 2025 Joachim Klahr
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef QD_BASE_H
#define QD_BASE_H

#define QD_STACK_DEPTH 16384
#define QD_MARK_STACK_DEPTH 8
//#define QD_ENABLE_PANIC
#define __qd_real_t double

extern __qd_real_t __qd_stack[QD_STACK_DEPTH];
extern __qd_real_t __qd_mark_stacks[QD_MARK_STACK_DEPTH][QD_STACK_DEPTH];
extern __qd_real_t __qd_err;
extern int __qd_stack_ptr;
extern int __qd_mark_stack_ptr;
extern int __qd_mark_stacks_ptrs[QD_MARK_STACK_DEPTH];
extern int __qd_precision;

extern __qd_real_t __qd_ptr_to_real(void (*fn)(int, ...));
extern void (*__qd_real_to_ptr(__qd_real_t ptr))(int, ...);

extern void __qd_arg_push(__qd_real_t x);

extern void __qd_abs(int n, ...);
extern void __qd_acos(int n, ...);
extern void __qd_add(int n, ...);
extern void __qd_asin(int n, ...);
extern void __qd_atan(int n, ...);
extern void __qd_avg(int n, ...);
extern void __qd_call(int n, ...);
extern void __qd_cb(int n, ...);
extern void __qd_cbrt(int n, ...);
extern void __qd_ceil(int n, ...);
extern void __qd_clear(int n, ...);
extern void __qd_cos(int n, ...);
extern void __qd_dec(int n, ...);
extern void __qd_depth(int n, ...);
extern void __qd_div(int n, ...);
extern void __qd_dup(int n, ...);
extern void __qd_dup2(int n, ...);
extern void __qd_error(int n, ...);
extern void __qd_eval(int n, const char* expression, ...);
extern void __qd_fac(int n, ...);
extern void __qd_floor(int n, ...);
extern void __qd_inc(int n, ...);
extern void __qd_inv(int n, ...);
extern void __qd_ln(int n, ...);
extern void __qd_log10(int n, ...);
extern void __qd_mark(int n, ...);
extern void __qd_max(int n, ...);
extern void __qd_min(int n, ...);
extern void __qd_mod(int n, ...);
extern void __qd_mul(int n, ...);
extern void __qd_neg(int n, ...);
extern void __qd_nip(int n, ...);
extern void __qd_over(int n, ...);
extern void __qd_pick(int n, ...);
extern void __qd_pop(int n, ...);
extern void __qd_pow(int n, ...);
extern void __qd_print(int n, ...);
extern void __qd_push(int n, ...);
extern void __qd_read(int n, ...);
extern void __qd_reduce_add(int n, ...);
extern void __qd_reduce_div(int n, ...);
extern void __qd_reduce_mul(int n, ...);
extern void __qd_reduce_sub(int n, ...);
extern void __qd_revert(int n, ...);
extern void __qd_roll(int n, ...);
extern void __qd_rot(int n, ...);
extern void __qd_rot2(int n, ...);
extern void __qd_round(int n, ...);
extern void __qd_scale(int n, ...);
extern void __qd_sin(int n, ...);
extern void __qd_sq(int n, ...);
extern void __qd_sqrt(int n, ...);
extern void __qd_sub(int n, ...);
extern void __qd_sum(int n, ...);
extern void __qd_swap(int n, ...);
extern void __qd_swap2(int n, ...);
extern void __qd_tan(int n, ...);
extern void __qd_test(int n, ...);
extern void __qd_tuck(int n, ...);
extern void __qd_within(int n, ...);
extern void __qd_write(int n, ...);

extern void __qd___panic_stack_underflow(int n, ...);
extern void __qd___panic_stack_overflow(int n, ...);
extern void __qd_panic_mark_stack_overflow();
extern void __qd_panic_mark_stack_underflow();
extern void __qd_panic_stack_underflow();
extern void __qd_panic_stack_overflow();
extern void __qd_panic_value_infinity();
extern void __qd_panic_division_by_zero();
extern void __qd_panic_invalid_input();
extern void __qd_panic_invalid_data();
extern void __qd_panic_out_of_memory();

#endif

