; Keywords
[
  "pub"
  "fn"
  "struct"
  "const"
  "use"
  "import"
  "as"
] @keyword

; Control flow keywords
[
  "if"
  "else"
  "for"
  "while"
  "switch"
  "defer"
  "ctx"
  "loop"
] @keyword.control


; Break, continue, and return statements
(break_statement) @keyword.control
(continue_statement) @keyword.control
(return_statement) @keyword.control

; Primitive types (more specific, must come first)
((type (identifier) @type.builtin)
  (#any-of? @type.builtin "i64" "f64" "ptr" "str"))

; User-defined types
(type
  (identifier) @type)

; Struct construction type
(struct_construction
  type: (identifier) @type)
(struct_construction
  type: (namespaced_identifier
    namespace: (identifier) @module
    name: (identifier) @type))

; Function definitions
(function_definition
  name: (identifier) @function)

; Test definitions
(test_definition
  keyword: (test_keyword) @keyword
  name: (string) @function)

; Constant definitions
(constant_definition
  name: (identifier) @constant)

; Compile-time env() function
(env_call
  "env" @function.builtin)

; Struct definitions
(struct_definition
  name: (identifier) @type)

; Type parameters in generic definitions
(type_parameters
  (type_parameter) @type.parameter)

; Generic instructions (make<T>)
(generic_instruction) @function.builtin

; Struct field types - primitives
((struct_field
  type: (type (identifier) @type.builtin))
  (#any-of? @type.builtin "i64" "f64" "ptr" "str"))

; Struct field types - user-defined
(struct_field
  type: (type (identifier) @type))

; Struct fields
(struct_field
  name: (identifier) @variable.member)

; Field access
(field_access
  field: (identifier) @variable.member)

; Field set
(field_set
  field: (identifier) @variable.member)

; Field initializers in struct construction
(field_init
  name: (identifier) @variable.member)

; Use statements (module imports)
(use_statement
  module: (identifier) @module)

; Import statements
(import_statement
  library: (string) @string.special
  namespace: (string) @module)

; Import function declarations
(import_function
  name: (identifier) @function)

; Namespaced identifiers (module::function or module::constant calls)
; These have higher priority and should be matched before the catch-all identifier rule
(namespaced_identifier
  namespace: (identifier) @module
  name: (identifier) @function.call)

; Built-in stack operations
(builtin_operation) @function.builtin
[
  "dup" "swap" "drop" "over" "rot" "nip" "tuck" "pick" "roll"
  "dup2" "swap2" "over2" "drop2" "dupd" "swapd" "overd" "nipd" "depth" "clear"
] @function.builtin

; Built-in arithmetic operations
[
  "add" "sub" "mul" "div" "inc" "dec" "abs" "sqrt" "sq" "pow" "mod"
  "neg" "inv" "fac" "cb" "cbrt"
] @function.builtin

; Built-in math functions
[
  "sin" "cos" "tan" "asin" "acos" "atan" "ln" "log10"
  "ceil" "floor" "round" "min" "max"
] @function.builtin

; Built-in comparison operations
[
  "eq" "neq" "lt" "gt" "lte" "gte" "within"
] @function.builtin

; Built-in logic operations
[
  "and" "or" "not" "xor"
] @function.builtin

; Built-in bitwise operations
[
  "lshift" "rshift"
] @function.builtin

; Built-in type casting operations
[
  "cast"
] @function.builtin

; Built-in I/O operations
[
  "print" "prints" "printv" "printsv" "call" "nl" "read"
] @function.builtin

; Built-in threading operations
[
  "spawn" "detach" "wait"
] @function.builtin

; Built-in error handling
[
  "error"
] @function.builtin

; Parameters in stack signatures
(parameter
  name: (identifier) @variable.parameter)

; Literals
(number) @number
(string) @string

; Special variables
(loop_variable) @variable.builtin

; For loop iterator variable
(for_loop
  variable: (identifier) @variable.parameter)

; Local variable declarations
(local_declaration
  name: (identifier) @variable)

; Pointer operations
(pointer_operation) @operator

; Operator symbols
(operator_symbol) @operator

; Operators
[
  "--"
  ":"
  "::"
  "="
  "->"
] @operator

; Delimiters
[
  "("
  ")"
  "{"
  "}"
] @punctuation.bracket

; Comments
(comment) @comment @spell

; Identifiers (catch-all for variables and unqualified function calls)
(identifier) @variable
