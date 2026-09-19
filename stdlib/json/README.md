# json

JSON parsing and building for Quadrate.

## Installation

`json` ships with the Quadrate toolchain, so there is nothing to install:

```quadrate
use json
```

## Two ways in

`parse` reads a document once into a `Value` tree. Reading a field out of the
result is a walk of the tree, so this is what you want whenever more than one
value is read out of one document.

The `get_*` family scans the text for a single key and allocates nothing. It is
cheaper for exactly that — one lookup, one document — and it is what an HTTP
handler that only wants `"id"` should keep using.

## Parsing into a tree

```qd
use json

fn main() {
    "{\"name\": \"Alice\", \"tags\": [\"a\", \"b\"]}" json::parse! -> doc

    doc "name" json::get! json::as_str! print nl     // Alice
    doc "tags" json::get! json::len print nl         // 2
    doc "tags" json::get! 0 json::at! json::as_str! print nl  // a

    doc 2 json::pretty print nl
}
```

Failure carries the byte offset: `json: expected a colon at offset 17`. The
codes are `ErrSyntax`, `ErrType`, `ErrIndex`, `ErrKey` and `ErrDepth`.

Children are a linked list, so `first`/`next` walks them in linear time while
`at` walks from the head on every call:

```qd
doc json::first! -> c
loop {
    c json::name print ": " print c json::stringify print nl
    c json::has_next 0 == if { break }
    c json::next! -> c
}
```

## Building a tree

```qd
json::new_object -> o
o "n" 42 json::new_int json::put! -> o
o "xs" json::new_array -> xs
xs "a" json::new_string json::push! -> xs
xs json::put! -> o
o json::stringify print nl        // {"n":42,"xs":["a"]}
```

## Functions

### Tree

- `parse(text:str -- v:Value)!` — parse a document
- `(v:Value) stringify( -- s:str)` / `(v:Value) pretty(width:i64 -- s:str)` — serialize
- `(v:Value) is_null/is_bool/is_number/is_string/is_array/is_object( -- b:i64)`
- `(v:Value) as_int/as_float/as_str/as_bool( -- x)!` — read a scalar
- `(v:Value) len( -- n:i64)`, `(v:Value) at(index:i64 -- child:Value)!`
- `(v:Value) get(key:str -- child:Value)!`, `(v:Value) has(key:str -- exists:i64)`
- `(v:Value) first( -- child:Value)!`, `(v:Value) next( -- sibling:Value)!`,
  `(v:Value) has_next( -- b:i64)`, `(v:Value) name( -- k:str)`
- `new_null/new_bool/new_int/new_float/new_string/new_array/new_object`
- `(v:Value) push(child:Value -- v2:Value)!`, `(v:Value) put(key:str child:Value -- v2:Value)!`

### Scanner

- `get_string(json:str key:str -- value:str found:i64)` — get string value by key
- `get_int(json:str key:str -- value:i64 found:i64)` — get integer value by key
- `get_float(json:str key:str -- value:f64 found:i64)` — get float value by key
- `get_bool(json:str key:str -- value:i64 found:i64)` — get boolean value by key
- `get_object(json:str key:str -- value:str found:i64)` / `get_array(...)` — get raw nested text
- `has_key(json:str key:str -- exists:i64)` — check whether a key is present
- `type_at(json:str pos:i64 -- result:i64)` — the kind of the value at a position
- `array_len(json:str -- length:i64)`, `array_get_string/int/float/object/array(json:str index:i64 -- value found:i64)`

### Building text directly

- `encode(s:str -- encoded:str)`, `kv`, `kv_int`, `kv_bool`, `kv_raw`, `obj`,
  `arr`, `empty_obj`, `empty_arr`, `join`

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or pull request on [GitHub](https://github.com/quadrate-language/quadrate).
