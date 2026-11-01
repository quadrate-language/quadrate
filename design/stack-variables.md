# Stack variables in function types

Status: **implemented**, 2026-11-01. See `CHANGELOG.md` for the shipped description; this
document is the reasoning behind it.

A function type can say that a function value takes or leaves *however many values its caller
gives it*, by naming that part of the stack with a stack variable, `..a`. This is what lets one
`dip`, `keep`, `bi` or `call` work for function values of every arity, where today each is fixed
to one shape.

```qd
pub stack fn dip<..a, ..b, T>(..a x:T q:fn(..a -- ..b) -- ..b x:T) {
	-> q -> x
	q call
	x
}

fn main() {
	10 20 5 fn (a:i64 b:i64 -- s:i64) { a b + } dip   // 30 5
	1 2 3 9 stack fn (i64 i64 i64 -- i64) { + + } dip // 6 9
}
```

## Why

`hof` has the combinators, typed as tightly as today's function types allow:

```qd
pub fn dip<T, U>(x:T y:U f:fn(T -- T) -- r:T top:U)
pub fn keep<T, U>(x:T f:fn(T -- U) -- r:U orig:T)
pub stack fn when<T>(x:T cond:i64 f:fn(T -- T) -- r:T)
```

Each works for exactly one shape of function value. A `dip` that should run `+` under the top
value needs a function taking two values, and `fn(T -- T)` takes one, so the program is
rejected. The combinators are therefore barely used -- about twenty uses in the whole tree, all in
`examples/hof` -- and code reaches for named locals instead (R55).

Two facts make the fix smaller than it looks.

**The runtime already does it.** `call` pops a function pointer and runs it on the shared stack;
the function takes what it takes and leaves what it leaves. A `dip` written for any arity runs
correctly today. Only the type checker refuses it, because it unifies function types by requiring
the same number of inputs and outputs. This is a change to the type checker and the syntax, and
none to code generation.

**A plain function type is already polymorphic in the rest of the stack.** `fn(i64 -- i64)`
means "takes one `i64` from the top, leaves one, and leaves everything below as it was" --
whatever that is. Written out, it is `fn(..r i64 -- ..r i64)` for a fresh `..r`. That is why
`map`, `filter`, `fold` and `both` are already as general as they need to be: their function
values have a fixed arity, and the rest of the stack takes care of itself. Stack variables are
needed only where the *arity* of a function value depends on the call site.

## Syntax

A stack variable is `..` followed by a lowercase name. It is declared with the type parameters
and written as the **first** element of a parameter list:

```qd
fn name<..a, ..b, T>(..a x:T q:fn(..a -- ..b) -- ..b x:T)
```

- **Where.** At the start of a signature's inputs or outputs, and at the start of a function
  type's inputs or outputs. Nowhere else: not after another parameter, not as a struct field, not
  in `[]T`.
- **How many.** At most one per list.
- **Declared.** In the `<...>` list, like a type parameter, so a reader sees at the head of the
  signature that the function reaches into its caller's stack.

One variable per list, always at the bottom, is enough for every combinator in Factor's core
vocabulary once their quotations are threaded (see `bi` below), and it keeps unification to
matching one list against another.

## Meaning

In a **function type**, `fn(..a x:T -- ..b)` is a function value that consumes the values `..a`
stands for and an `x`, and leaves the values `..b` stands for.

In a **signature**, `(..a x:T q:fn(..a -- ..b) -- ..b x:T)` says the function reaches below its
named inputs exactly as far as `..a` -- a depth fixed at each call by the function value passed
for `q` -- and leaves `..b` there instead.

Every function type **without** a stack variable has an implicit one, shared by its inputs and
outputs: `fn(T -- U)` is `fn(..r T -- ..r U)` for a fresh `..r`. Nothing written today changes
meaning.

## Typing a call

At a call of a function with stack variables, after the ordinary type parameters are bound as
today:

1. **Bind from the function arguments.** For each parameter whose type is a function type,
   unify its declared input list and output list with the argument's. A list is some types with
   an optional stack variable at the bottom. Unifying `..a e1 … ek` with `t1 … tn`:
   - match the top `k` types pairwise, `ei` with `t(n-k+i)`, as types are matched today;
   - bind `..a` to what is left below them, `t1 … t(n-k)`, which may itself end in a stack
     variable -- the argument's own, or the implicit one of a plain function type.
   A stack variable bound twice must be bound to the same list.

2. **Substitute.** Replace each stack variable in the signature with its list. The signature is
   now an ordinary stack effect, possibly with stack variables left at the bottom of its inputs
   and outputs.

3. **Cancel.** A stack variable still unbound that sits at the bottom of both the inputs and the
   outputs stands for the part of the stack the call does not touch, and is dropped from both.
   One left on one side only is an error: the call's reach cannot be known.

4. **Apply.** The result is a fixed effect, applied to the stack as any call's is.

Worked through for `10 20 5 add2 dip`, with `add2` of type `fn(i64 i64 -- i64)`:

```
dip       (..a x:T q:fn(..a -- ..b) -- ..b x:T)
argument  fn(..r i64 i64 -- ..r i64)          the implicit ..r made explicit
inputs    ..a  ~  ..r i64 i64      ..a = ..r i64 i64
outputs   ..b  ~  ..r i64          ..b = ..r i64
T                                   T = i64, from the 5
effect    (..r i64 i64 i64 q -- ..r i64 i64)
cancel    (i64 i64 i64 q -- i64 i64)
```

**Errors** say what could not be decided, at the call:

```
'dip' needs to know how many values ..a is, and no function argument says
'keep': ..b is 'i64' from 'q', but 'p' leaves 'str i64' there
```

When a function argument's type is not known at all -- a bare `ptr` called through -- the
function's stack effect is unknown, as it is for such a `call` today, and the checks that depend
on it are switched off for that function.

## Typing a body

Inside a function with stack variables, a stack variable is an opaque element of the modelled
stack: the body may move values above it, but not look into it.

- **On entry** the stack is the input variable, then the named inputs (for a `stack fn`; a plain
  `fn` binds them).
- **Calling a function value** whose type is `fn(..a … -- ..b …)` requires the stack to be the
  variable `..a` with the function's other inputs on top of it, and replaces that with `..b` and
  its other outputs. A call to another function with stack variables binds them as above, now
  with lists that may contain the caller's own variables.
- **Reaching below** a stack variable -- a `drop`, `swap` or call that would consume it -- is an
  error. The body's modelled stack starts above the variable, so it is reported as the stack
  underflow it is.
- **On exit** the stack must be the output variable with the declared outputs on top.

## The combinators

`hof` becomes:

```qd
// call, as a word
pub stack fn apply<..a, ..b>(..a q:fn(..a -- ..b) -- ..b) {
	call
}

// run q on what is under x, then put x back
pub stack fn dip<..a, ..b, T>(..a x:T q:fn(..a -- ..b) -- ..b x:T) {
	-> q -> x  q call  x
}

// run q on x, and keep x
pub stack fn keep<..a, ..b, T>(..a x:T q:fn(..a x:T -- ..b) -- ..b x:T) {
	-> q -> x  x q call  x
}

// run p on x, then q on x, each on what the last left
pub stack fn bi<..a, ..b, ..c, T>(..a x:T p:fn(..a x:T -- ..b) q:fn(..b x:T -- ..c) -- ..c) {
	-> q -> p -> x  x p call  x q call
}

// run q when the flag is set; either way the stack has one shape after
pub stack fn when<..a>(..a flag:bool q:fn(..a -- ..a) -- ..a) {
	swap if { call } else { drop }
}

// run q n times
pub stack fn times<..a>(..a n:i64 q:fn(..a -- ..a) -- ..a) {
	-> q -> n  0 n 1 for i { q call }
}
```

`bi` shows why one variable per list is enough. Factor types it as running `p` and `q` on `x`
side by side; threading them instead -- `q` runs on whatever `p` left, with `x` pushed again --
needs only the bottom variable, and with plain function values it reduces to today's shape:

```
p, q : fn(..r T -- ..r U)      implicit rows
..b = ..r U   ..c = ..r U U    from p, then q
effect (T p q -- U U)           what hof::bi is now
```

`map`, `filter`, `fold`, `find`, `any`, `all`, `both` and `bi_star` keep their types: their
function values have a fixed arity already.

Every existing call of the rewritten combinators keeps its meaning. `10 20 f dip` with
`f:fn(i64 -- i64)` binds `..a = ..r i64`, so `f` runs on `10` under `20`, as now.

## What does not change

- **Generic type parameters** bind and substitute as now; stack variables are bound after them.
- **Fallible function values.** `fn(..a -- ..b)!` is a fallible function type, and calling one
  is a fallible call like any other: `!`, `?`, or a bare call leaving a status. A combinator
  generic over whether its function value is fallible is out of scope; `hof` keeps the
  non-fallible forms.
- **`stack fn` and `fn`.** Either may have stack variables. A plain `fn` binds its named inputs
  on entry, and the stack variable stays where it was, below them.
- **Code generation.** None. Function values are called on the runtime stack already. A
  function using stack variables contains a `call`, which already keeps it off the compile-time
  stack path, where depths must be known when compiling.
- **The interpreter.** It does not type-check, and runs these as it runs any `call`.

## Surfaces

- Parser: `..name` in a type-parameter list and as the first element of a parameter list or a
  function type's lists.
- Validator: lists with a bottom variable in `FunctionSignature` and in function type strings;
  list unification; binding, substitution and cancellation at calls; the opaque element in
  body checking; the errors above.
- `hof`: the rewrites above, and its tests.
- Spec §3.4 (function types), §5.7 (signatures), Appendix A.
- Tooling: the LSP's hover and signature help print the variables; the formatter keeps
  `..a`; the tree-sitter grammar and queries; `quaddoc`.

## Decisions

- **Spelling.** `..a`, Factor's spelling, read as "and the rest". `...a` was the alternative;
  `*a` clashes with `*T`, the nullable type.
- **Declared.** In the `<...>` list, as type parameters are, so the head of a signature shows
  that the function reaches into its caller's stack.
- **Fallibility.** Out of scope. `hof` keeps non-fallible combinators, and a fallible function
  value is called with `!`, `?` or a bare call in the caller's own code.

## Not in scope

- Inferring a function value's type from its body (`fn { 2 * }`): step 3, which this enables.
  Implemented; see `design/inferred-effects.md`.
- Inlining combinators when their function value is written in place: step 4.
- More than one stack variable in a list, or one anywhere but the bottom.
