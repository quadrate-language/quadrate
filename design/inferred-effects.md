# Inferred effects: `fn { … }`

Status: **implemented**, 2026-11-01. See `CHANGELOG.md` for the shipped description; this
document is the reasoning behind it.

A function value can be written without a signature. Its body works on the stack, and its stack
effect is worked out from the body:

```qd
5 fn { 2 * } call                 // 10
10 20 fn { 1 + } hof::dip         // 11 20
[1 2 3] fn { dup * } hof::map     // [1 4 9]
```

This is step 3 of making Quadrate more concatenative, after option A's fallible calls and stack
variables (`design/stack-variables.md`).

## Why

A lambda today spells out what its body already says. `fn (x:i64 -- r:i64) { x 2 * }` is
eighteen characters of signature around five of body, and the signature binds a name the body then
has to use. `stack fn (i64 -- i64) { 2 * }` drops the name but keeps the rest. For the short
function values combinators take -- the common case, and the one that decides whether `dip`,
`keep` and `bi` get used at all -- the signature is the larger part of the code.

Factor writes `[ 2 * ]` and infers the effect. `[ ]` is an array literal here, so the spelling is
`fn { }`: the keyword already says "function value", and the braces are a body's.

## Meaning

`fn { body }` is an anonymous function with no signature:

- **The body works on the stack**, as a `stack fn`'s does. There are no parameters to bind.
- **Inputs** are the fewest values the body has to find on the stack for none of its words to
  underflow.
- **Outputs** are what the body leaves, with those inputs consumed.
- **Types.** An output's type is what the body makes it: `fn { 2.0 * }` leaves an `f64`,
  `fn { "x" }` a `str`, and a struct literal its struct. An input's type is not inferred, nor is the
  type of an output that is an input passed through or arithmetic on one. These are `_`, which
  unifies with any type without binding a type parameter.

The inferred effect is the value's type, `fn(_ -- _)` for `fn { 2 * }`, and from there on it is
an ordinary function type: `call` applies it, a parameter of function type unifies with it, and
step 2's stack variables bind from it. Since it is the *minimal* effect, the implicit row of a
plain function type supplies the rest of the stack, and `fn { + }` used by `dip` binds
`..a = ..r _ _`.

## Inference

The validator already simulates a body on a modelled stack and reports underflow. Inference uses
that rather than a second engine:

1. Walk the body on k values of unknown type, k = 0, 1, 2, …, with diagnostics held back, until
   no word underflows. Each walk starts from the same state; lambdas inside are checked again.
2. Walk it once more on that k, reporting errors as usual. What is left is the outputs.

The walk is monotone in k -- more values below can only remove an underflow -- so the first k
that works is the least, and the effect is minimal. Bodies are short, and a cap of 16 inputs ends
a body that underflows whatever is beneath it (one that reaches below a stack variable); its
underflow is then reported.

An operand of unknown type gave arithmetic nothing to push before: `checkBuiltinArithmetic`
rejected it as non-numeric and dropped the result, which nothing noticed because errors naming an
unknown type are suppressed. It now pushes a value of unknown type, or an `f64` if the other
operand is one.

If the body calls a function value whose effect is unknown, or anything else makes its stack
unpredictable, its effect is unknown, and its type is the empty one that a bare `ptr` has: a
`call` on it switches the checks off, as for any such value.

## Why not infer input types

Types could be inferred by constraint: `2 *` needs a number, `strings::len` a `str`. That needs
every builtin and every call to say what it requires of an unknown operand, not only what it
accepts of a known one: a second type system beside the first, for a case where the type is
usually fixed at the use site anyway, by `map`'s `T` or the value already on the stack. Step 4
compiles combinators inline when their function value is written in place, and there the body is
checked on the real stack, with real types. So the counts are inferred, and types are left to the
site. A reader who needs to see the types writes a signature.

## What does not change

- **Captures** work as they do for any lambda.
- **Fallibility.** A `fn { }` is not fallible: `panic` and `?` in its body are errors that say to
  give it a signature. A fallible function value's failure exit needs its output types, and
  `fn { }!` would be a spelling for a rare case.
- **Code generation.** A `fn { }` is a `stack fn` with no parameters to bind, and is compiled as
  one.
- **`stack fn { }`** is an error: there is no signature for `stack` to modify.

## Surfaces

- Parser: `fn {` where a lambda may start.
- AST: `hasSignature()`, and the inferred type for the tools.
- Validator: the trial walks; `_` in type strings and in unification; the arithmetic fix.
- Formatter: `fn{2*}` becomes `fn { 2 * }`; lambdas inside a lambda are normalised on the same
  pass.
- LSP: an inlay hint after the `fn` giving the inferred effect, `(_ -- _)`.
- Tree-sitter: `anonymous_function` without a signature.
- Spec §7.5.1 and Appendix A; the anonymous-functions chapter.

## Not in scope

- Inferring input types (above).
- A signature-less named function, `fn name { }`. Step 5 decides what a plain `fn` means, and a
  named function is read by its signature first.
