# Sum types: `union`

Status: **draft**, 2026-10-31. Nothing here is implemented.

A `union` is a type whose value is exactly one of several named variants, each carrying its own
fields. A variant is built by a constructor word that takes its fields from the stack, and taken
apart by `switch`, whose arm for that variant starts with the fields on the stack.

```qd
use math

union Shape {
	Circle(r:f64)
	Rect(w:f64 h:f64)
}

def area(s:Shape -- a:f64) {
	s switch {
		Shape::Circle { dup * math::Pi * }
		Shape::Rect { * }
	}
}

fn main() {
	2.0 Shape::Circle area print nl      // 12.566...
	3.0 4.0 Shape::Rect area print nl    // 12
}
```

## Why

`enum` gives named integers and `struct` gives records, but nothing says "one of these". What
code writes instead is a struct with a tag field and one payload field per kind, which is how
`stdlib/json` represents a value: `struct Value { kind:i64 … }`, with the `kind` checked by hand
everywhere a variant would have been matched. That works, and the TODO entry records it working
twice. What it cannot do is:

- **Stop a payload being read under the wrong tag.** A JSON number's `text` field is an empty
  string, not an error. A failed call's placeholder result (option A of the fallible-call fix)
  is a zero, not an absence.
- **Check that every kind is handled.** Adding a kind to a tag-field struct finds no forgotten
  `switch`. An enum-typed `switch` already warns on a missing variant; a union can do the same
  with the payloads included.
- **Say "maybe a value" without a sentinel.** Today that is `null`, `-1`, `""` or a
  `(value, found)` pair, chosen per function.

Each of these is survivable alone. Together they are what sum types are for.

## Declaring

```qd
[pub] union Name [<T, ...>] {
	Variant(field:type field:type)
	Variant(field:type)
	Variant
}
```

- A variant's fields are written as a signature's inputs are: `name:type`, in order, bottom of
  the stack first. A variant with no fields is written bare.
- Field names are documentation and are what `print` shows; they are not in scope anywhere.
- A union MUST declare at least two variants. A union whose variants all lack fields is an
  `enum`, and SHOULD be diagnosed as one.
- A union MAY refer to itself, directly or through another type: values are reference counted
  on the heap, as structs are, so a recursive union has a finite size.

```qd
union Option<T> {
	Some(value:T)
	None
}

union Expr {
	Num(value:f64)
	Add(left:Expr right:Expr)
	Mul(left:Expr right:Expr)
	Neg(operand:Expr)
}
```

## Constructing

Each variant is a word, scoped like an enum variant, whose stack effect is its fields in and the
union out:

| Word | Effect |
|---|---|
| `Shape::Circle` | `( r:f64 -- s:Shape )` |
| `Shape::Rect` | `( w:f64 h:f64 -- s:Shape )` |
| `Option::None` | `( -- o:Option<T> )` |

So a variant is built the way any word is called, from what is on the stack, and composes like
one:

```qd
1.0 Expr::Num 2.0 Expr::Num Expr::Add -> e     // 1 + 2
[1.0 2.0 3.0] -> xs
xs 0 nth Option::Some                           // Option<f64>
```

The type parameters of a generic union are inferred from the fields, as a generic function's are
from its arguments. A field-less variant of a generic union takes its type from where it is used
-- a return type, a binding's later use, or a parameter -- and is an error where nothing decides
it.

A struct-literal form, `Shape::Rect { w = 3.0 h = 4.0 }`, is deliberately not proposed: the word
form already covers it, and two spellings of one construction is what R58 spends its time
removing.

## Matching

`switch` on a union value takes it apart. An arm is labelled with a variant, and starts with that
variant's fields pushed in declaration order:

```qd
e switch {
	Expr::Num { }                          // ( value )
	Expr::Add { -> r -> l  l eval r eval + }
	Expr::Mul { -> r -> l  l eval r eval * }
	Expr::Neg { eval neg }
}
```

The rules are those of §6.5.1 with one change: arms no longer all start from the same stack. Each
starts from the stack before the `switch`, minus the subject, plus its variant's fields. Every
arm that reaches the end MUST leave the same depth, as now, and diverging arms are exempt.

- **Exhaustive.** A `switch` on a union MUST name every variant or have a `_` arm. Today a
  non-exhaustive `switch` on an enum is a warning; on a union it is an error, since an unmatched
  union value has nothing sensible to fall through with.
- **`_`** starts from the stack before the `switch`, minus the subject. It receives no fields,
  since it does not know which variant it got.
- **Labels** MUST be variants of the subject's union. Mixing in an integer or a string label is
  an error.

A variant's fields are pushed, not bound: the arm names them with `->` if it wants names, and a
`stack`-style arm that just works on them needs nothing. This is what keeps the construct
concatenative. An arm is a block with a known effect, and nothing is bound behind its back.

## Generic unions and the standard library

`Option<T>` and `Result<T>` belong in the standard library rather than the language:

```qd
pub union Option<T> {
	Some(value:T)
	None
}

pub union Result<T> {
	Ok(value:T)
	Fail(code:i64 message:str)
}
```

Their helpers are ordinary functions written with `switch`:

```qd
pub def (o:Option<T>) value_or<T>(fallback:T -- v:T) {
	o switch {
		Option::Some { }
		Option::None { fallback }
	}
}

pub fn (o:Option<T>) is_some<T>( -- yes:bool) {
	o switch {
		Option::Some { drop true }
		Option::None { false }
	}
}
```

Once quotations exist, the combinators that make this style pleasant are ordinary words too, with
fixed effects:

```qd
// [ some-quot ] [ none-quot ] either
xs 3 find [ print nl ] [ "absent" print nl ] either
```

## Examples

**A lookup that may find nothing**, where today the function returns a `(value, found)` pair:

```qd
def find(xs:[]i64 wanted:i64 -- at:Option<i64>) {
	0 xs len 1 for i {
		xs i nth wanted == if { i Option::Some return }
	}
	Option::None
}

fn main() {
	[4 8 15 16] 15 find switch {
		Option::Some { "at " print print nl }
		Option::None { "absent" print nl }
	}
}
```

**An evaluator over a recursive type:**

```qd
def eval(e:Expr -- v:f64) {
	e switch {
		Expr::Num { }
		Expr::Add { -> r -> l  l eval r eval + }
		Expr::Mul { -> r -> l  l eval r eval * }
		Expr::Neg { eval neg }
	}
}

fn main() {
	// -(2 * (3 + 4))
	2.0 Expr::Num  3.0 Expr::Num 4.0 Expr::Num Expr::Add  Expr::Mul  Expr::Neg
	eval print nl                                  // -14
}
```

**A token stream**, the shape every lexer has:

```qd
pub union Token {
	Number(value:f64)
	Ident(name:str)
	Op(symbol:str)
	End
}

def describe(t:Token -- s:str) {
	t switch {
		Token::Number { cast<str> "number " swap strings::concat }
		Token::Ident { "name " swap strings::concat }
		Token::Op { "operator " swap strings::concat }
		Token::End { "end of input" }
	}
}
```

**JSON, as `stdlib/json` could have it**, replacing the `kind` field and the payload fields only
some kinds use:

```qd
pub union Value {
	Null
	Bool(value:bool)
	Number(value:f64)
	Text(value:str)
	Array(items:[]Value)
	Object(keys:[]str values:[]Value)
}
```

**A fallible result kept as one value**, the shape option A's `( result status )` pair takes if
fallible calls later return a `Result`:

```qd
text strconv::parse_int switch {      // if parse_int returned Result<i64>
	Result::Ok { -> port  port listen }
	Result::Fail { -> msg -> code  msg print nl }
}
```

## Representation

A union value is one stack slot holding a pointer, as a struct is: a reference-counted heap
object with a tag and the variant's fields, laid out as a struct with those fields would be. A
variant without fields MAY be a shared, never-freed instance, so `Option::None` allocates nothing.
`null` is not a union value and MUST be rejected where one is expected, which is the point of
`Option`.

`print` renders a value as its variant and fields, `Shape::Rect(3 4)`, recursing into nested
unions as it does into arrays.

## Relation to what exists

- **`enum`** is unchanged: named integers, interchangeable with `i64`. A union is not an integer
  and does not convert to one.
- **Fallible calls.** Option A (a bare call leaves its results and a status, tested by `ok` and
  `fail`) does not depend on this and should land first. A later step can have a bare call leave
  a `Result` instead. The `switch` arms keep their meaning (`Ok`, specific codes, `_`), so code
  written for option A moves over mechanically, and the placeholder results and their `drop`s go.
- **`null`** stays, for pointers and FFI. New Quadrate APIs that mean "maybe" return an `Option`.
- **R58**, the four spellings each of 0 and 1: `Ok`/`Err` as constants become unnecessary once
  `Result` exists, and can go with it.

## Surfaces

- Parser: the `union` declaration; variant names as scoped words.
- Validator: variant constructor effects; generic inference for unions; per-arm starting stacks
  in `switch`; exhaustiveness; rejecting `null` and mixed labels.
- Code generation, both paths: allocation and tag; dispatch on the tag in `switch`; pushing the
  fields; release of the fields when the value's count reaches zero.
- Interpreter: the same, so the tiers agree.
- `print`, `dump`, the LSP (hover, completion of variant names, arm snippets), the formatter,
  the linter, the tree-sitter grammar and queries, and the spec: a new §3.2.3 and changes to
  §6.5.

## Open questions

- **Keyword.** `union` reads as C's untagged union to some. The alternatives are a new word such
  as `variant` or `data`, or extending `enum` so that an enum with any payload is a sum type, as
  Rust and Swift do. Extending `enum` gives one keyword two meanings depending on its contents,
  one an integer and one not, which is the kind of double meaning the R-items keep finding
  costly. This draft prefers a separate word.
- **Testing without matching.** `s Shape::Circle is` → flag, for code that only asks which
  variant it has. It can be written with `switch` today, so it waits to see whether real code
  wants it.
- **Equality.** Whether `==` compares unions structurally, by identity, or not at all. Structs
  set the precedent, so it should follow whatever they do.
- **Field access by name.** `s <<r` on a known variant. It needs the variant to be known
  statically, which only holds inside an arm, where the fields are already on the stack. Probably
  unnecessary.

## Not in scope

- Anonymous or structural sum types (`i64 | str`).
- Pattern matching beyond one level: nested patterns, guards, bindings in labels.
- Changing what fallible calls return. That follows separately, after option A.
