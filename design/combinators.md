# Combinators as builtins

Status: **implemented**, 2026-11-01. See `CHANGELOG.md` for the shipped description; this
document is the reasoning behind it.

`dip`, `keep`, `bi`, `tri`, `both`, `times`, `each`, `map`, `filter` and `fold` are builtin
words. A `fn { }` written right before one is checked on the real stack and compiled inline.

```qd
10 20 fn { 1 + } dip                         // 11 20
0 [1 2 3] fn { + } each                      // 6
arr fn { 2 mod 0 == } filter fn { dup * } map 0 fn { + } fold
```

This is step 4 of making Quadrate more concatenative, after option A's fallible calls, stack
variables (`design/stack-variables.md`) and inferred effects (`design/inferred-effects.md`).

## Why

Step 2 made the combinators general and step 3 made their function values short. What was left
was cost, in three places:

- **Spelling.** `use hof` and `hof::` on every use. Factor has these in its core vocabulary,
  because they are how its code is written; a prefix says they are an extra.
- **Types.** A `fn { }` checked on its own has `_` for every input, so `arr fn { 2 * } map` did
  not know it was mapping integers. Checked where it runs, it does.
- **Run time.** Each use built a function value -- a closure, when it captured -- and called a
  generic function that called it back. A `for` loop with a local did neither, so the local was
  also the faster way to write it.

## Words

| Combinator | Effect | Factor |
|------------|--------|--------|
| `dip` | `( ..a x q -- ..b x )` | `dip` |
| `keep` | `( ..a x q -- ..b x )` | `keep` |
| `bi` | `( ..a x p q -- ..c )` | `bi` |
| `tri` | `( ..a x p q r -- ..d )` | `tri` |
| `both` | `( ..a x y q -- ..c )` | `bi@` |
| `times` | `( ..a n q -- ..a )` | `times` |
| `each` | `( ..a arr q -- ..a )` | `each` |
| `map` | `( arr q -- arr2 )` | `map` |
| `filter` | `( arr q -- arr2 )` | `filter` |
| `fold` | `( arr init q -- result )` | `reduce` |

`both` and `fold` keep `hof`'s names: `@` is not an identifier character, and `fold` is what
the rest of the language family calls `reduce`. `apply` is gone, since it was `call`.

## Typing: do what it does

A combinator is not given a signature. It is checked by doing to the modelled stack what it does
at run time: `dip` pops the function value and `x`, runs the function value, and pushes `x`
back. Running a function value means one of two things:

- **Written in place.** A `fn { }` immediately before the combinator has its body walked right
  there, on the real stack, with the types of the real values, under the rules of its own body:
  not fallible, no loop outside it to leave, nothing to return from.
- **Anything else** -- a local, a parameter, `&name`, a lambda with a signature -- is applied by
  its type exactly as `call` applies it, stack variables and all.

The result is the most precise check available without a signature system for every builtin,
and needs none: `dip` and `keep` work at every arity because they never claim one.

Each word adds its own rule. The function value of `times` and `each` runs again on what it
leaves, so it must leave the stack as found, in depth and types. That of `map` and `filter` is
walked on a stack holding only the element and must leave exactly one value; that of `fold` on
the accumulator and the element, and must leave the accumulator's type. No combinator may take a
fallible function value: there is nowhere for the failure to go.

## Lowering

After validation, a pass rewrites each combinator into what every backend already runs:

```
x q dip         -> $x  Q  $x
x q keep        -> $x  $x Q  $x
x p q bi        -> $x  $x P  $x Q                      (tri likewise)
x y q both      -> $q -> $y -> $x  $x $q call  $y $q call
n q times       -> $n  0 $n 1 for $i { Q }
arr q each      -> $a  0 $a len 1 for $i { $a $i nth Q }
arr q map       -> $a  [] -> $o  0 $a len 1 for $i { $o $a $i nth Q append -> $o }  $o
arr q filter    -> $a  [] -> $o  0 $a len 1 for $i { $a $i nth -> $e  $e Q if { $o $e append -> $o } }  $o
arr acc q fold  -> $c -> $a  $c  0 $a len 1 for $i { $a $i nth Q }
```

`Q` is the body of a function value written in place, spliced in, when nothing in it would mean
something else in the enclosing function: no local of its own, no `return` or `defer`, no `for`
iterator, no loop exit outside a loop of its own. Otherwise it is bound and called. `both` never
splices, since it would need the body twice. The `$` names cannot be written in source, so they
collide with nothing, and each site numbers its own.

Lowering rather than generating code for each word means LLVM code generation, the
compile-time-stack paths, defers and refcounting all see ordinary locals and loops, which they
already get right, and the interpreter gets the combinators for the price of calling the pass. In
qdos's shell that means `0 [1 2 3] fn { + } each` works although that tier has no anonymous
functions: the one written in place is spliced away before it runs.

A combinator's name is skipped where a local of that name is in scope, and where the validator
resolved it to a method.

## Names

Ten common words became reserved. A function in a program can no longer take any builtin's
name: it never could in effect, since a call by that name ran the builtin, and the tests and the
manual had several that did nothing. A module's function still may (`mem::free`), being called
qualified, and so may a method. qdos's calculator word `times`, multiplication, became
`multiply`, and its triangle solver `tri` became `triangle`.

## What does not change

- **The out-of-line path.** A function value from anywhere is still run by `call`, with the same
  checks as before.
- **`hof`** keeps what is not built in: `when`, `unless`, `bi_star`, `fold_right`, `any`, `all`,
  `find`.

## Not in scope

- Inlining a function value bound to a local and used once.
- A combinator generic over fallibility.
- `2bi`, `2dip`, `cleave` and Factor's other variants, until code wants them.
