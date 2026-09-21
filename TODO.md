# TODO

What is open. Finished work is not kept here: `CHANGELOG.md` has the prose and git has the
history. The `R<n>` identifiers come from full reviews of the language surface -- R1-R47 from
2026-09-17, R48-R85 from 2026-09-21 -- and are stable, so they can be referred to while working
through them.

## Language design

- [ ] **Sum types / tagged unions.** `enum` gives bare ints and `struct` gives records, but there
      is no "one of these". That absence is why `Ok`/`Err` are conflated with `true`/`false` and
      why `null` is `0` — four spellings each of 0 and 1 — and a `Result<T, E>`-shaped variant
      type would let much of that be deleted rather than maintained.
      **Both arguments filed for it have since been withdrawn, each by an implementation that
      turned out not to need the feature.** The first was `stdlib/json`, which had no `parse` at
      all: it has one as of 2026-09-19. A JSON value is a tagged union in the abstract, but a
      `struct Value { kind:i64 … }` with a payload field per kind is what every C and Go parser
      writes, it costs 80 bytes a node, and the `kind` tag is checked in exactly the places a
      `match` would have been. The `(value, found)` pair *was* the real cost, and it was paid off
      without the feature: the accessors are fallible (`as_int!`, `get!`) and report `ErrType`,
      `ErrKey`, `ErrIndex`, so nothing returns a bare flag beside its value.
      The second was the error channel — R24, closed 2026-09-20, see the `error` entry in
      `CHANGELOG.md` — which `stdlib/error` answered the same way and in forty lines of Quadrate:
      the control flow already exists and is enforced, and what was missing was a payload type,
      which a refcounted struct already is.
      So what is left is the spelling of 0 and 1, not a blocked use case.
      Deliberately **not** adding: interfaces/traits (generics see 5 uses, not under strain),
      slices/iterators (`len`/`nth`/`append`/`set` over `ptr` arrays is the right level),
      `comptime`/const-generics. Labeled `break` will bite eventually with nested `for`, but not yet.
      **Open question**: twice the concrete case dissolved on contact with a real implementation,
      and both times what replaced it was a struct with a tag field. What would a third candidate
      have to look like to survive that — and if none presents itself, is this an item or a
      preference?

- [ ] **A count beside an array, in the `strings` FFI.** `sort::*` takes `[]T` and nothing
      else — the array carries its length. `strings::sort`, `sort_desc`, `join` and `column`
      still take `arr:[]str count:i64`, and `split`, `lines`, `words` and `split_n` still
      *return* a count beside the array they built. The two spellings now sit side by side for
      the same job: `strs sort::strings` against `strs count strings::sort`.
      These are FFI, so the count comes off the stack in C rather than out of the signature;
      dropping it means reading `qd_array_length` in `stdlib/strings/src/strings.c` and
      revisiting what the four producers return. Left out of the sort conversion on purpose —
      it is a separate change to a separate module, and C rather than Quadrate.
      Found 2026-09-21 on finishing the sort conversion.

- [ ] **R54 -- `while` is unusable as specified and unused in practice.** Across `examples/`,
      `stdlib/` and `cmd/`: `loop` 217 uses, `for` 119, **`while` 0**. The only 19 uses in the
      tree are in `tests/`. The cause is the condition-delimiting rule: the parser takes the
      maximal backward run of expression-shaped nodes (`lib/qc/src/ast_parse.h:849`,
      `lib/qc/src/ast_statements.cc:336`) and the validator then requires *that whole run* to
      net exactly `+1` (`lib/qc/src/semantic_validator_typecheck.cc:2385`). So no value may be
      pending on the stack where a `while` is written:

          0 -> i
          42                          // any pending value at all
          i 3 < while { i 1 + -> i }  // rejected: condition "leaves 2 values"

      A loop that cannot appear in the middle of stack code is not usable in a stack language,
      and the corpus shows what people write instead: `loop { cond if { break } ... }`.
      An explicit `while { cond } { body }` would cost one brace pair and delete the lookbehind
      rule, the whole-run restriction and R78 together.

- [ ] **R55 -- the stack model is not carrying its weight.** Counted over `examples/`,
      `stdlib/` and `cmd/`: 4503 `->` bindings against roughly 270 uses of every stack-shuffling
      word combined (`drop` 122, `swap` 39, `dup` 37, `depth` 23, `over` 18, `rot` 13, `nip` 11,
      `clear` 6, `dup2` 5, `pick` 2). Real code uses the stack as a calling convention and named
      locals for everything else. That is worth deciding about deliberately, because the stack
      is what forces the `while` design (R54), the branch depth-matching rules of §6.1.1, and
      the modularity hole of R52. Either the stack words get good enough that locals become the
      exception, or the costs of pretending get paid down.

- [ ] **R56 -- an error is not a value, only a syntactic position.** A bare fallible call must
      be *immediately* followed by `if`, `switch`, `!` or `?`:

          0 f -> status    // error: must be immediately followed by 'if' or 'switch'

      so a status cannot be bound, stored, returned, or handed to a helper that checks it, and
      there is no way to write a function that wraps a fallible call and reports on it. `err`
      compounds this: it clears on read, so the failure arm -- which per §10.3 is not given the
      status either -- gets exactly one chance to capture a message it was never handed, and a
      second `err` yields `0` and the empty string.

- [ ] **R57 -- the value a fallible call leaves depends on the token that follows it.** Before
      `if` the status is a boolean; before `switch` it carries the error code (§10.3). A call
      whose stack effect is determined by lookahead of its consumer is hard to explain, hard to
      keep consistent between the compiler and the interpreter, and is the reason `Ok` has to be
      magic (R58) rather than a value.

- [ ] **R58 -- four spellings of 0 and four of 1, and `Ok` means two different things.**
      This is the concrete surface of the sum-type item above; filing it separately because the
      `Ok` part is actionable without sum types.

          Ok true ==     // 1
          Err null ==    // 1
          false Err ==   // 1

          1 switch { Ok { ... } }               // matches: Ok is the literal 1
          0 f switch { Ok { ... } 1 { ... } }   // panic code 1 hits the `1` arm, not Ok

      `Ok` is the literal `1` everywhere except as a case label after a fallible call, where it
      means "succeeded" -- same token, two meanings, nothing in the syntax marking which. And
      `null == false == Err == 0` makes `p null ==` and `p false ==` indistinguishable, so a
      null check and a boolean test are the same program.

- [ ] **R59 -- whitespace is significant, and §2.4 says it is not.** §2.4: *"Whitespace
      (spaces, tabs, newlines) separates tokens but is otherwise insignificant."* Meanwhile:

          [3]i64     // array of three zeros
          [3] i64    // array [3], then: Undefined identifier 'i64'
          10 1 -     // 9
          10-1       // pushes 10 and -1 -- silently, and a C reader sees 9

      §3.2.2 mandates the adjacency rule for `[n]T` and separately requires two adjacent array
      literals to be space-separated, so the rule is load-bearing. The §2.4 sentence is what
      should go, and the `10-1` case deserves a diagnostic.

- [ ] **R60 -- `<` carries four jobs and explicit generic instantiation misparses into one of
      them.** Comparison, generic parameter lists, `cast<T>`/`sizeof<T>`, and the `<<`/`>>`
      field operators. The bill is paid twice: the shifts had to become the words `shl`/`shr`
      because `<<`/`>>` were spent on field access, and a call cannot name its type arguments:

          fn id<T>(x:T -- y:T) { x }
          5 id<i64>
          // Undefined identifier 'i64'; did you mean 'id'?
          // Type error in 'lt': Stack underflow (requires 2 values)

      It parses as `id` `<` `i64` `>`. Struct construction supports `Box<i64> { ... }` and
      function calls do not, and the failure mode names `lt`, which appears nowhere in the
      source line.

- [ ] **R61 -- `--` is both the signature separator and decrement.** `fn f(a:i64--b:i64)`
      compiles, `x--` decrements, and the two readings are distinguished only by whether the
      parser is inside a signature. Together with R59 this makes `-` the most context-dependent
      character in the grammar.

- [ ] **R62 -- `*T` means "nullable T", not "pointer to T", and is documented as the latter.**

          struct N { v:i64 next:N  }   // N { next = null } -> expects N, got ptr
          struct N { v:i64 next:*N }   // accepted

      Structs are already references (§3.2.1), so `*` adds nothing pointer-shaped; what it
      actually does is admit `null`. §5.6 calls it "Pointer type" and the difference from a
      bare `N` is never stated. Either rename it to what it does or drop it and make nullability
      explicit some other way.

- [ ] **R63 -- most floats cannot be written down.**

          0.5   // ok
          .5    // error: Unexpected character '.'
          5.    // accepted, though §2.3.4's grammar requires digits on both sides
          1e3   // error: exponent notation is not supported; write the value out

      There is no spelling for `1e-300` at all, and `1e300` needs 300 digits. No underscores,
      no hex floats. Accepting a trailing dot while rejecting a leading one is backwards from
      the usual convention, and `5.` contradicts the spec's own `float := digit+ '.' digit+`.

- [ ] **R64 -- string interpolation takes a bare identifier and nothing else.** The spec's own
      §2.3.4 example does not compile:

          $"result={x + y}"   // Undefined identifier 'x + y'
          $"result={x y +}"   // Undefined identifier 'x y +'
          $"{{literal}}"      // Undefined identifier '{ and '

      No field access, no calls, no arithmetic -- and no escape for a literal `{` in a `$"..."`
      string, so a brace cannot be printed from one at all. Either the holes take expressions
      (which Appendix A already claims) or §2.3.4 stops calling it "expression interpolation",
      but the escape gap needs closing either way.

- [ ] **R65 -- four mutation conventions, one per container operation.** `>>field` returns the
      receiver; `append` returns the array; `set` returns nothing; `sort::ints` mutates in place
      and returns nothing; `Vec::push!` returns a newly allocated struct (R48). Every one needs a
      different call-site shape, and none of the differences carries meaning.

- [ ] **R66 -- `->` both declares and assigns, and an unused local is silent.**

          0 -> counter
          5 -> countr     // typo: silently a new local, no warning
          counter print nl

      compiles clean. An unused *parameter* is a warning (`Unused parameter 'y'`), so the two
      halves of the same idea are policed differently -- and §7.5 says an unused named parameter
      is an error, which it is not. A rebind that does not match an existing local is the single
      most likely typo in the language and nothing reports it.

## Correctness and safety

Found 2026-09-21. Each of these was reproduced against `quad 0.5.0`; the programs are short
enough to be inlined here.

- [ ] **R48 -- `ct::Vec` shares its buffer between stale copies, so a `release` frees memory
      another live `Vec` still points at.** `push` allocates a *new* `Vec<T>` struct on every
      call (`stdlib/ct/qd/ct/vec.qd:65`, last line of the body) and returns it, sharing `data`
      with the value it was called on. The old value stays valid, keeps its old `len`, and
      addresses the same buffer.

          ct::Vec<i64> { data = null len = 0 cap = 0 } -> v
          v 10 push! -> v
          v 20 push! -> v2        // v and v2 now share one buffer
          v2 release
          v 0 get! print nl       // prints 23139457034, not 10
          v release               // frees it a second time; exit status 0, no diagnostic

      Three faults in one line of design: a stale-but-valid alias after every push, a heap
      struct allocated per element, and a use-after-free reachable without touching `mem`. The
      fix is to write through the receiver the way `>>field` does (§8.4) and return the same
      struct, which also removes the per-push allocation. `pop`, `set` and `reset` have the
      same shape and the same problem.

- [ ] **R49 -- an out-of-bounds `nth` pushes nothing and lets the program continue.** It is
      neither a trap, nor a catchable error, nor a defined value; the stack is silently one
      shorter and the program dies later somewhere unrelated.

          [1 2 3] -> a
          a 9 nth drop
          "still running" print nl

      gives `nth: index 9 out of bounds (length 3)` on stderr, then
      `Fatal error in drop: Stack underflow` -- an error naming `drop`, which is not the bug.
      `set` with a bad index prints and carries on with no corruption, which is a third
      behaviour again. Whatever is chosen, the same choice has to hold for both.

- [ ] **R50 -- three unrelated failure models for arithmetic and indexing.** `10 0 /` is a
      compile error, `10 z /` (`0 -> z`) is a fatal uncatchable abort, `a 9 nth` is the silent
      truncation of R49, and `panic` is the catchable one. A program cannot guard integer
      division at all: the divisor check has to be written by hand before every `/`, because
      the failure is not routed through the error channel that `if`/`switch`/`!`/`?` read.
      Float division by zero is meanwhile defined as `inf` (§12.1), so the same operator has
      a fatal case and a total case depending on operand type.

- [ ] **R51 -- a receiver method can shadow a builtin instruction, including `drop`.**

          struct A { v:i64 }
          fn (a:A) drop( -- ) { "not dropped" print nl }
          fn main() { A { v = 1 } -> a  a drop  "end" print nl }

      prints `not dropped` and leaks the reference. The same works for `dup`, `print` and
      `len`. Core stack words dispatching on the type of the top of stack means a library can
      silently break memory management for its callers by defining a method with an unlucky
      name. Method names are also in a single global namespace -- `v length` needs no module
      qualifier -- so there is nothing scoping the collision either.

- [ ] **R52 -- `clear` and `depth` reach across call frames and the checker does not model
      them.** There is one global stack (§4.1), so a callee sees and can wipe its caller's
      values:

          fn clr( -- ) { clear }
          fn main() { 1 2 3 clr depth print nl }
          // rejected: main "declares 0 output(s) but body leaves 3 value(s)"

          fn d( -- n:i64) { depth }
          fn main() { 1 2 3 d print nl drop drop drop }   // prints 3

      `clear` is a sanctioned way to violate every declared stack effect in the program, and
      the static model quietly disagrees with the runtime about what happened. Either both
      words become frame-local, or the checker has to treat a call to a function containing
      them as unpredictable.

- [ ] **R53 -- a type alias launders a sized integer type past the check that rejects it.**
      §3.1.1 requires `u8` and friends to be rejected anywhere but a struct field or a `mem`
      accessor, and the direct spelling is:

          fn f(b:u8 -- r:i64)      // error: 'u8' is a memory-layout type and means nothing
                                   //        on a parameter
          type Byte = u8
          fn f(b:Byte -- r:i64)    // accepted

      The rejection happens before alias resolution. It needs to happen after.

## Standard library

- [ ] **R67 -- two array types with incompatible APIs.** Built-in `[]T` gives
      `append`/`nth`/`len`, automatic lifetime, non-fallible, silent out-of-bounds (R49).
      `ct::Vec<T>` gives `push!`/`get!`/`length`, manual `release`, fallible, panicking
      out-of-bounds. Neither is a superset. A reader has to know which one they hold before
      they can read any line that touches it.

- [ ] **R68 -- "length" is spelled three ways.** Built-in `len` (arrays only -- `"hi" len` is
      a type error), `strings::len`, and `Vec::length`/`Map::length`. Related to the array-count
      item above, which is the same seam from the other side.

- [ ] **R69 -- `sort` is monomorphic in a language that has generics.** `ints`, `ints_desc`,
      `floats`, `floats_desc`, `strings`, `strings_desc`, `is_sorted`, `is_sorted_floats`, and
      `by`/`is_sorted_by`/`lower_bound_by` all fixed to `[]i64` -- while `hof` next door is
      fully `<T, U>` (`map`, `filter`, `fold`, `any`, `all`, `find`). Also `sort::strings`
      reads as the `strings` module and `sort::min`/`max` shadow the reading of
      `math::min`/`max`.

- [ ] **R70 -- generic methods use their type parameter before declaring it.**

          pub fn (v:Vec<T>) push<T>(elem:T -- v2:Vec<T>)!

      The receiver mentions `T` to the left of the `<T>` that introduces it. Nothing else in
      the grammar reads right-to-left like this.

- [ ] **R71 -- `ct::Vec` has no constructor and requires manual release.** Callers write
      `Vec<i64> { data = null len = 0 cap = 0 }` by hand and must remember `release`, in a
      language whose §11 promises automatic reference counting. A `Vec<T> new` plus a
      destructor would remove both, and would remove the double-free of R48 as a class.

## Specification

The spec is stamped **0.2.0**; the toolchain is **0.5.0**. Most of what follows is that gap.

- [ ] **R72 -- version drift.** `docs/docs/specification.md:3` says 0.2.0 and the Document
      History stops there. Either the spec tracks releases or it states which release it
      describes.

- [ ] **R73 -- Appendix A still spells constructs the compiler has removed.** All three are
      rejected with "has been removed" messages, so the grammar is behind the diagnostics:
      `make "<" type ">"` in `instruction`; `error_lit = "error" "{" "code" "=" ... "}"`;
      `field_set = ">>" identifier [ "!" ]`. The `error` literal is also absent from the keyword
      list while appearing in the grammar.

- [ ] **R74 -- `while` is in neither the keyword list nor the grammar.** §2.3.1 does not list
      it and Appendix A has no `while_stmt` and does not mention it in `statement`. `return` is
      in the keyword list with no production either, and `statement` omits local binding too.

- [ ] **R75 -- `var` has no prose section anywhere.** §5 runs 5.1 through 5.9 with no entry for
      it; the only descriptions are the keyword list, one incidental mention at line 255, and
      `var_decl` at line 2172. Module-level mutable state in a language with threads needs
      initialisation order, visibility and thread-safety written down.

- [ ] **R76 -- §13 misstates which stdlib functions are fallible.** §13.7 documents `spawn!`,
      `join!`, `detach!`, `mutex_new!`, `lock!`, `unlock!`, `chan_new!`, `send!`, `recv!`,
      `wg_new!` and `wait!`; `stdlib/thread` has **zero** fallible functions. §13.6 lists time's
      functions as total and omits `format!` and `parse!`, which are the two that are fallible.
      Worth generating this section rather than maintaining it.

- [ ] **R77 -- §2.3.4's interpolation examples do not compile.** See R64. Appendix A's
      `interp_string = '$"' { character | "{" expression "}" } '"'` is wrong the same way.

- [ ] **R78 -- §6.3.1 describes a `while` rule nobody implemented.** The spec says the condition
      is *"the shortest run of words immediately preceding `while` whose net stack effect is
      `( -- flag )`"*, under which `5 i 3 < while` is legal with condition `i 3 <`. The
      implementation rejects it (R54). Fixing R54 should replace this paragraph rather than
      correct it.

- [ ] **R79 -- logical and bitwise are classified inconsistently in two places.** §14.3 lists
      `qd_and`, `qd_or`, `qd_xor`, `qd_not`, `qd_shl`, `qd_shr` under **"Bitwise"**, but §12.3
      makes `and`/`or`/`not` logical and puts bitwise in `bits`. Appendix B.3, titled "Bitwise
      Operators", then contains three rows described as "logical AND/OR/NOT".

- [ ] **R80 -- small grammar gaps.** `scoped_id = identifier "::" identifier` cannot express
      the `module::EnumName::Variant` form §5.4.2 requires. `integer := digit+` has no sign,
      but §5.4.1 requires `Variant5 = -1` and negative literals are real tokens
      (`10 -1 print` prints `-1`). §12.6 gives `sizeof` the stack effect `(T -- n)` as though
      a type were pushed; it is written `sizeof<P>`.

- [ ] **R81 -- `nth` is documented twice under different headings.** §4.2 lists it under stack
      "Rearrangement" and §12.5 under "Array Operations". It is an array operation.

- [ ] **R82 -- compiler internals are in the user-facing type table.** §3.5 lists `UNKNOWN`,
      `TAINTED` and `TYPEVAR` beside `any`. An implementer does not need them and a user cannot
      write them.

- [ ] **R83 -- changelog prose sits inside normative text.** §6.1.1 carries a paragraph
      beginning *"This paragraph used to say the opposite..."* and recounting that *"fourteen
      guards in the `ct` module pushed their error message onto the stack as data"*; §11.2.2
      has a similar passage about three fixed defects. Both belong in `CHANGELOG.md`; a
      specification should state the rule, not its history.

- [ ] **R84 -- §6.3.1 "Conditional Loops" is nested under §6.3 "Infinite Loops".** `while`
      should be a sibling of `loop` and `for`, not a subsection of one of them.

- [ ] **R85 -- §7.5 says an unused named parameter is an error; it is a warning.** See R66 for
      the matching silence on locals.

## Deferred

- [ ] Package registry — searchable index instead of raw Git URLs.
- [ ] (Stretch) Inline asm — `asm("cli; hlt")` style. Today everything privileged lives in a `.S`
      file called via FFI, which works fine; this is quality-of-life for short sequences (port I/O,
      halt) so kernel code can stay in `.qd`.
