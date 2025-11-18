; Keywords
[
  "fn"
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
  "switch"
  "case"
  "default"
  "defer"
  "ctx"
  "loop"
] @keyword.control

; Break and continue statements
(break_statement) @keyword.control
(continue_statement) @keyword.control

; Types
(type) @type.builtin

; Function definitions
(function_definition
  name: (identifier) @function)

; Constant definitions
(constant_definition
  name: (identifier) @constant)

; Use statements
(use_statement
  module: (identifier) @namespace)

; Import statements
(import_statement
  library: (string) @string.special
  namespace: (string) @namespace)

; Import function declarations
(import_function
  name: (identifier) @function)

; Namespaced identifiers (function calls with namespace)
(namespaced_identifier
  namespace: (identifier) @namespace
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
  "casti" "castf" "casts"
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

; Identifiers (catch-all for function calls)
(identifier) @variable
