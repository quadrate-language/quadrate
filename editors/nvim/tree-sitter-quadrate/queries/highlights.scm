; Keywords
[
  "fn"
  "const"
  "use"
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

; Built-in stack operations
(builtin_operation) @function.builtin
[
  "dup" "swap" "drop" "over" "rot" "nip" "tuck" "pick" "roll"
  "dup2" "swap2" "over2" "drop2" "depth" "clear"
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
  "and" "or" "not"
] @function.builtin

; Built-in I/O operations
[
  "print" "prints" "printv" "printsv" "call"
] @function.builtin

; Parameters in stack signatures
(parameter
  name: (identifier) @variable.parameter)

; Literals
(number) @number
(string) @string

; Special variables
(loop_variable) @variable.builtin

; Operators
[
  "--"
  ":"
  "="
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
