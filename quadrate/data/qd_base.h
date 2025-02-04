#ifndef QD_BASE_H
#define QD_BASE_H

#define QD_STACK_DEPTH 1024
#define QD_STACK_ELEMENT_SIZE 4
#define __qd_real_t double

extern __qd_real_t __qd_stack[QD_STACK_DEPTH][QD_STACK_ELEMENT_SIZE];
extern int __qd_stack_ptr;

extern void __qd_arg_push(__qd_real_t a, __qd_real_t b, __qd_real_t c, __qd_real_t d);

extern void __qd_add();
extern void __qd_pop();
extern void __qd_push(int n, ...);

#endif

