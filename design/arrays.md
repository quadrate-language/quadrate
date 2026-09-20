# Array creation: remove `make`, keep only `[]`

Status: **implemented**, 2026-09-20. See `CHANGELOG.md` for the shipped description; this
document is the reasoning behind it and is kept for that.

Three things changed between the proposal and the implementation, each noted where it applies
below:

- The fill form `[v; n]` was dropped in favour of zero-initialisation, `[n]T`, which removed
  the separator question and the "evaluated once or once per slot" question with it.
- `[n]T` for a struct builds n distinct instances rather than n nulls.
- `[]T` for a struct is allowed whether or not the struct has defaults: the size is left out,
  so nothing is constructed and no zero is needed. Requiring one there would have left a type
  with no sensible default no way to get an array of it at all.

Every way to bring an array into existence becomes a `[…]` literal. The five `make` builtins
— `make<T>`, `makei`, `makef`, `makes`, `makep` — are removed, and nothing replaces them at
the instruction level.

## Why

Array *types* are already written `[]T` — in parameters, in returns, in struct fields, and in
the element type the validator tracks for `nth`. Array *creation* is written `make<T>`, which
names the element type instead, so the same array is spelled two different ways depending on
which side of the signature it is on. There is no reason for the second spelling.

It is also barely load-bearing. Across the whole tree:

| | count |
|---|---|
| `make<T>` in `stdlib/` (38 modules) | 1 |
| `make<T>` in `examples/` | 0 |
| `makei`/`makef`/`makes`/`makep` in `stdlib/` | 1 |
| `makei`/`makef`/`makes`/`makep` in `examples/` | 2 |

The stdlib's own array builders do not use it. `hof::map` and `hof::filter`
(`stdlib/hof/qd/hof/hof.qd:173`, `:186`) both build their result with `[] -> out` and
`append`. The single `make<T>` in the stdlib, `MAX_STATES make<State>`
(`stdlib/regex/qd/regex/regex.qd:71`), preallocates 256 null slots that `add_state` then
fills strictly in order — `re <<states n s set` with `n` equal to `re <<nstates`, incremented
after each write. It is an append loop wearing a preallocation.

Two things `make` is currently the only answer for, which the new form has to cover:

- **A presized array.** `set` requires `idx < len`, so without a presized array there is no
  way to build one by index at all.
- **A typed empty array.** `[]` infers as `[]any`, which no typed parameter accepts —
  `[] f` against `fn f(xs:[]i64 …)` is a compile error — so `0 make<i64>` is today the only
  way to write an empty `[]i64`.

Three smaller things the removal takes with it:

- `make` with no type parameter passes the validator, which defaults its tracked type to
  `[]i64` (`lib/qc/src/semantic_validator_instructions.cc:985`), but codegen only handles
  `make` *with* a type parameter, so it emits a call to a `qd_make` that does not exist.
  `3 make -> a` compiles clean and dies at link: `undefined reference to 'qd_make'`.
  `quadlint` says nothing. The comment at `lib/qc/src/ast_parse.h:338` records this same
  failure being fixed once for struct-field position; the cause is still there.
- `make<[]i64>` is rejected by the parser — `parseInstructionTypeParam` accepts only an
  identifier — even though codegen already dispatches on a `[]`-prefixed type parameter
  (`lib/llvmgen/src/generator_nodes_instructions.cc:694`). Nested typed arrays are
  unreachable today; `tests/qd/arrays/nested_basic.qd` has to use `makep` and `cast<ptr>` on
  every access.
- The interpreter tier has no `make` at all. Its builtin table
  (`lib/interp/src/interp.cc:122`) has the four legacy spellings and not the generic one, so
  `make<i64>` does not run there. Array literals do.

## The two literal forms

```qd
[1 2 3]      // elements
[10]i64      // ten zeros
```

And their degenerate cases, both of which already have meaning today or fall straight out:

```qd
[]           // no elements — element type decided on first append
[]i64        // no size given, so zero of them — an empty []i64
```

### The rule

Inside `[…]`, whether the brackets hold **elements** or a **size** is decided by what follows
the `]`: a type name immediately after the `]`, with no whitespace, makes the brackets a size.
Anything else makes them elements.

| written | means |
|---|---|
| `[]` | elements, none — `[]any` |
| `[1 2 3]` | elements |
| `[10]` | elements — a one-element array holding `10` |
| `[]i64` | size omitted, so 0 — an empty `[]i64` |
| `[10]i64` | size 10 — ten zeros |
| `[src len]i64` | size is an expression |

So there is one form, `[size]T`, in which the size may be omitted; `[]T` is not a separate
case. The size is any expression leaving exactly one `i64`, the rule `while` conditions
already follow.

**Tokenizing.** The adjacency requirement is what keeps this unambiguous, and the parser
already makes exactly this distinction elsewhere: `parseInstructionTypeParam` uses
`peekNextChar`, which does not skip whitespace, to tell `make<T>` from `len <`. No existing
source changes meaning, since nothing today can have a type name hard against a `]`.

The one hazard is `[10]i64` against `[10] i64`. The second is an array literal followed by a
bare `i64`, which is not a valid term, so it is a compile error rather than a silent
difference in meaning — but the diagnostic should name the adjacency rule rather than say
"undefined identifier".

### Zero values

`[n]T` fills with the zero value of `T`. These are not new: they are what the corresponding
`make` already produces, so the runtime does not change.

| `T` | zero | runtime today |
|---|---|---|
| `i64`, `i32`, `i16`, `i8`, `u64`, `u32`, `u16`, `u8` | `0` | `qd_makei` |
| `f64`, `f32` | `0.0` | `qd_makef` |
| `str` | `""` — a real empty string object, not null; `strings::len` on it returns 0 | `qd_makes` |
| `ptr` | null | `qd_makep` |
| a type parameter of the enclosing generic | none — the array adopts a type on first append | `qd_makea` |
| a struct type | `n` distinct instances, each `T {}` — allowed when every field of `T` has a default; see below | `qd_makep` + `n` constructions |

**`T` is an element type, and `[n]T` is an expression, not a type.** This is where the Go
resemblance stops: in Go `[10]int` is a fixed-size array *type* distinct from the slice type
`[]int`. Here arrays are dynamic and there is only one array type, so `[10]i64` and `[]i64`
both produce a value of type `[]i64`, differing only in starting length. `fn f(xs:[10]i64 …)`
is not a type and has to be rejected with that sentence, not with a parse error.

**Lowering** is what `make<T>` does today, with the size expression in place of the stack
argument — `qd_makei`, `qd_makef`, `qd_makes`, `qd_makep` by element type, and `qd_makea`
when `T` is a type parameter, whose concrete type an erased generic cannot know at run time.
That dispatch already exists (`lib/llvmgen/src/generator_nodes_instructions.cc:687`) and
moves from the instruction to the literal unchanged.

### Struct element types

`[2]Point` holds two *instances*, not two nulls. Anything else reintroduces the problem the
zero value exists to solve: `make<Point>` gives null slots today, and reading a field out of
one segfaults, with a diagnostic that names the wrong cause —

```qd
3 make<Point> -> p
p 0 nth <<x print nl
// Fatal error: Segmentation fault (stack overflow from unbounded recursion?)
```

The language already has both the syntax and the rule for this, and neither has to be
invented. Struct fields take defaults, and a construction may omit every field that has one:

```qd
struct Point { x:f64 = 0.0  y:f64 = 0.0 }
Point {} -> p        // legal today; a fresh, distinct instance each time
```

So the definition is: **`[n]T` for a struct `T` is `n` × `T {}`**, and it is allowed exactly
where `T {}` is allowed. When a field has no default, the existing diagnostic already says
the right thing — `Missing field 'y' in struct construction 'Point'` — and it points at the
struct, which is where the decision belongs. Whether a type has a sensible zero is the
author's call, not the array's.

That rule also settles the recursive case for free. `struct Node { value:i64  next:Node }`
cannot give `next` a default, because a struct-valued default does not parse (see below), so
`[n]Node` is rejected rather than recursing forever trying to build the zero of a linked
list.

Two consequences worth stating plainly:

- **It is not free.** `[1000]Point` is 1001 allocations where `make<Point>` was one. That is
  the cost of the elements being real, and the alternative is the segfault above. Where the
  slots are going to be overwritten anyway, `[]Point` and `append` is both cheaper and more
  honest — which is what `regex` should do (see Migration).
- **Cleanup already works.** `qd_array_release` walks a `QD_ARRAY_TYPE_PTR` array and calls
  `qd_array_release` or `qd_struct_release` per element (`lib/rt/src/array.c:148`), so an
  array owning `n` structs frees them. Nothing new is needed in the runtime.

**Blocking gap.** Field defaults today accept scalar literals only. Both of these are parse
errors:

```qd
struct T { xs:[]i64 = [] }        // error: Expected a default value after '='
struct T { i:Inner = Inner {} }   // error: Unmatched '}' at top level
```

The first has to be fixed for this to be useful, since a struct with an array field could
otherwise never be default-constructed and so could never be an element type. It is small and
right on its own merits. The second should stay unsupported: allowing it is what would make
`Node { next = Node {} }` recurse, and leaving it out is what keeps `[n]Node` rejected.

### Nested arrays

`[][]i64` does not parse today — not as a literal, and not as a type either:

```qd
fn f(g:[][]i64 -- n:i64) { g len }   // error: Expected an element type after '[]'
type Grid = [][]i64                  // error: Expected type after '=' in type alias
struct S { g:[][]i64 }               // error
```

So an array of arrays exists only untyped: `makep`, then `cast<ptr>` before every access, as
`tests/qd/arrays/nested_basic.qd` has to do. The literal alone does not fix this, because the
type is unwriteable on the other side of the signature.

The cause is that `[]T` is parsed by six hand-rolled copies of the same six lines — scan `[`,
expect `]`, expect one identifier — at `ast_declarations.cc:157`, `:278`, `:540`, `:655`
(the named and the unnamed parameter form, each duplicated between the anonymous-function and
the named-function parser), `ast_types.cc:188` (struct fields) and `ast_types.cc:914` (type
aliases). None of them recurses, none of them accepts a qualified `mod::Name` either, and
each reports its own wording.

The fix is one shared type parser that recurses, replacing all six — the same consolidation
`parseInstructionTypeParam` already got, for the same reason. That is a prerequisite for
`[][]i64` but not for the rest of this proposal, and it is worth doing on its own merits
whether or not `make` goes.

**Not done here.** `[][]i64` is reserved rather than supported: the literal reports "a nested
array element type is not supported yet". Without that it read as two adjacent literals, `[]`
and `[]i64`, which left a value on the stack and reported the enclosing function's stack
effect instead of the real problem.

## Migration

| today | becomes |
|---|---|
| `10 make<i64>` | `[10]i64` |
| `10 make<f64>` | `[10]f64` |
| `10 make<str>` | `[10]str` |
| `0 make<i64>` | `[]i64` |
| `10 makei` | `[10]i64` |
| `10 makef` | `[10]f64` |
| `10 makes` | `[10]str` |
| `10 makep` | `[10]ptr` |
| `n make<Point>` | `[n]Point` (instances, not nulls), or `[]Point` + `append` |
| `make<T>` for a type parameter `T` | `[]T` |
| `1 makep` + `cast<ptr>` per access | `[][]i64`, once the type parsers recurse |

Every rewrite of a scalar element type is mechanical and changes no runtime behaviour,
because `[n]T` lowers to the call the old spelling lowered to.

A struct element type is the exception, and deliberately so: `n make<Point>` gives null slots
and `[n]Point` gives instances. The one case in the tree is `MAX_STATES make<State>` in
`regex`, and the mechanical rewrite is the wrong one for it — `[MAX_STATES]State` would
allocate 256 `State` structs that `add_state` immediately overwrites. It should become
`[]State` plus `append`, which is what the code already means, and which also means `State`
does not need defaults declared.

All five names go into `REMOVED_INSTRUCTIONS` (`lib/qc/include/quadrate/qc/instructions.h:71`),
which already produces the right shape of diagnostic — old stack effect, exact rewrite — and
already has an `advice` field per entry for cases the default sentence does not fit. Each
entry needs its own, since the rewrite differs by element type.

`make` with no type parameter currently reaches the linker rather than the user; after this it
reaches `findRemovedInstruction` and reports.

## Surfaces

- `lib/qc/src/ast_expressions.cc:579` — the array-literal parser. The size form lands here.
- `lib/qc/src/ast_types.cc:773` (and its nested-literal branch at `:819`) — the second
  array-literal parser, for a literal inside a struct construction. It needs the same form,
  and the duplication is a reason to factor the literal parser out the way
  `parseInstructionTypeParam` was factored out.
- `lib/qc/src/ast_declarations.cc:157`, `:278`, `:540`, `:655`, `lib/qc/src/ast_types.cc:188`,
  `:914` — the six copies of the `[]T` *type* parser. Only needed for nested arrays, but
  that is where `[][]i64` is blocked; see "Nested arrays". These are also where `[10]i64` in
  type position has to be rejected with its own sentence.
- `lib/qc/src/semantic_validator_typecheck.cc:1694` — element-type inference for literals;
  gains the `[size]T` case, where the type is declared rather than inferred from elements.
  For a struct element type it also has to check default-constructibility and report the
  existing "Missing field" diagnostic against the literal.
- The struct-construction path — `[n]T` for a struct emits `n` × `T {}`, so it reuses the
  lowering `T {}` already has rather than adding one.
- The struct field-default parser — array-typed defaults (`xs:[]i64 = []`) currently fail
  with "Expected a default value after '='"; see open question 2.
- `lib/qc/src/semantic_validator_instructions.cc:961` — the `make` family's validation, deleted.
- `lib/qc/include/quadrate/qc/instructions.h` — five names move from `BUILTIN_INSTRUCTIONS`
  to `REMOVED_INSTRUCTIONS`.
- `lib/qc/src/source_formatter.cc` — `quadfmt` has to not insert a space after the `]`, which
  would change meaning.
- `lib/llvmgen/src/generator_control.cc:1169`, `generator_nodes_instructions.cc:683` — the
  `make` lowering is deleted and its element-type dispatch moves to the literal.
- `lib/interp/src/interp.cc:122` — four legacy entries deleted; `evalArrayLiteral` gains the
  size form. This is the tier where `make<T>` never worked, so it gains capability.
- `cmd/quadmcp/resources.qd:317`, `cmd/quadmcp/tools.qd:722` — the instruction table and the
  loop/array template the MCP server hands out.
- `tools/playground/templates/index.html:744` — syntax highlighting keyword list.
- 10 documentation pages: `reference.md`, `specification.md` (3.2.2 and the 1876 table),
  `reference/arrays.md`, `reference/generics.md`, `reference/misc.md`, `reference/threading.md`,
  `learn/5-data-structures/arrays.md`, `learn/5-data-structures/structs.md`,
  `learn/7-advanced/generics.md`, `learn/7-advanced/function-pointers.md`.
- `tests/qd/arrays/` — 24 files use `make<T>`, 22 use a legacy spelling. The `make_generic_*`
  set becomes the `[size]T` set; `typed_make.qd` and the `make*_basic.qd` files are renamed to
  match their new subject. New tests needed for: `[n]T` with an expression size, the
  adjacency rule in both directions (`[10]i64` versus `[10] i64`, `[]i64` versus `[] -> x`),
  `[]T` with a type parameter, `[][]i64`, `[10]i64` rejected in type position, and each
  removed name reporting its rewrite.
- `stdlib/regex/qd/regex/regex.qd:71` — the one stdlib migration.
- `stdlib/hof/qd/hof/hof.qd:173`, `:186` — not required, but `[]U` and `[]T` are strictly
  better typed than the `[]` they use now.

## Open questions, as resolved

1. **Does bare `[]` survive?** Yes, kept. `[]T` gives the typed empty array that `[]any` could
   not, but `[]` still reads well where the element type genuinely is not known yet, and the
   stdlib's generic builders use it. It stays `[]any`, with the same deferred error it always
   had: `[] -> c  c 1 append -> c  c "x" append` compiles and dies at run time, because the
   runtime adopts `int` from the first append. Migrating `hof` to `[]T`/`[]U` would be an
   improvement on its own terms and was not done here.

2. **How far do field defaults need to go?** Array-typed defaults (`xs:[]i64 = []i64`) were
   added, because without them a struct with an array field could never be default-constructed
   and so could never be an element type. Struct-typed defaults (`i:Inner = Inner {}`) were
   left unsupported, which is what keeps `[n]Node` on a self-referential type from recursing
   while trying to build the zero of a linked list. That is load-bearing by accident rather
   than by design, and deserves a second look if struct-valued defaults are ever wanted.

Settled, recorded so it is not re-litigated:

- **A size taken from the stack** (`10 []i64`) was considered and rejected. The same term
  would have a different stack effect depending on whether something was underneath it, which
  no reader and no static stack model should have to resolve. The size goes inside the
  brackets.
- **Null slots for a struct element type** (what `make<Point>` does today) were considered
  and rejected. `[2]Point` holds two instances. Null slots are the reason field access on an
  unfilled slot segfaults, and carrying that forward into new syntax would be adopting a bug
  as a semantic.
- **A fill form with an explicit value** (`[0; 10]`, Rust-style) was considered and rejected
  in favour of zero-initialisation. It needed a new separator, and it raised a question with
  no good answer — whether the value is evaluated once or once per slot — that
  zero-initialisation does not raise at all.

## Not in scope

Two things found while reviewing this, both real, both independent of it, both filed
separately in TODO.md:

- `[]T` satisfies a declared `ptr` parameter silently, so an array can be passed to the 41
  stdlib functions that take a raw `arr:ptr count:i64` buffer. `[3 1 2] -> a  a a len
  sort::ints` compiles clean and corrupts memory.
- The two array worlds: `[]T` (`qd_array_t`, refcounted, bounds-checked, 7 stdlib functions)
  and raw `ptr` buffers (`mem::alloc` plus byte offsets, 41 stdlib functions).
