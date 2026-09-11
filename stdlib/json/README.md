# json

JSON parsing module for Quadrate.

## Installation

`json` ships with the Quadrate toolchain, so there is nothing to install:

```quadrate
use json
```

## Usage

```qd
use json

fn main() {
    '{"name": "Alice", "age": 30}' -> data
    data "name" json::get_string -> name
    data "age" json::get_int -> age
    name print nl
    age print nl
}
```

## Functions

- `get_string(json:str key:str -- value:str found:bool)` - Get string value by key
- `get_int(json:str key:str -- value:int found:bool)` - Get integer value by key
- `get_bool(json:str key:str -- value:bool found:bool)` - Get boolean value by key
- `has_key(json:str key:str -- found:bool)` - Check if key exists
- `get_type(json:str key:str -- type:str)` - Get type of value ("string", "number", "bool", "array", "object", "null")
- `array_len(json:str -- len:int)` - Get length of JSON array
- `array_get(json:str index:int -- value:str)` - Get array element by index

## License

Apache 2.0

## Contributing

Contributions welcome! Please open an issue or pull request on [GitHub](https://github.com/quadrate-language/quadrate).
