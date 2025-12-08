/**
 * Tree-sitter grammar for Quadrate programming language
 */

// Helper for comma-separated lists with at least one element
function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

module.exports = grammar({
  name: 'quadrate',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => repeat($._statement),

    _statement: $ => choice(
      $.function_definition,
      $.test_definition,
      $.struct_definition,
      $.constant_definition,
      $.use_statement,
      $.import_statement,
      $._expression,
    ),

    // Function definition: fn name( x:float y:float -- z:float ) { ... }
    // or: pub fn name( x:float y:float -- z:float ) { ... }
    // or: fn name<T>( x:T -- y:T ) { ... } (generic)
    function_definition: $ => seq(
      optional('pub'),
      'fn',
      field('name', $.identifier),
      field('type_parameters', optional($.type_parameters)),
      field('signature', optional($.stack_signature)),
      field('body', $.block),
    ),

    // Type parameters: <T> or <T, U> or <A, B, C>
    type_parameters: $ => seq(
      '<',
      field('params', commaSep1($.type_parameter)),
      '>',
    ),

    type_parameter: $ => $.identifier,

    // Test definition: test "name" { ... }
    test_definition: $ => seq(
      field('keyword', alias('test', $.test_keyword)),
      field('name', $.string),
      field('body', $.block),
    ),

    test_keyword: $ => 'test',

    // Struct definition: struct Name { x:f64 y:f64 }
    // or: pub struct Name { x:f64 y:f64 }
    // or: struct Name<T> { value:T } (generic)
    struct_definition: $ => seq(
      optional('pub'),
      'struct',
      field('name', $.identifier),
      field('type_parameters', optional($.type_parameters)),
      '{',
      repeat($.struct_field),
      '}',
    ),

    struct_field: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
    ),

    // Stack signature: ( x:float y:float -- z:float ) or just ()
    stack_signature: $ => seq(
      '(',
      optional(seq(
        optional($.parameter_list),
        '--',
        optional($.parameter_list),
      )),
      ')',
    ),

    parameter_list: $ => repeat1($.parameter),

    parameter: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
    ),

    type: $ => seq($.identifier),

    // Constant: const name = value or pub const name = value
    // or: const name = env("VAR", "default")
    constant_definition: $ => seq(
      optional('pub'),
      'const',
      field('name', $.identifier),
      '=',
      field('value', choice($.env_call, $._expression)),
    ),

    // Compile-time env() function: env("VAR") or env("VAR", "default")
    env_call: $ => seq(
      'env',
      '(',
      field('var_name', $.string),
      optional(seq(',', field('default', $.string))),
      ')',
    ),

    // Use statement: use module
    use_statement: $ => seq(
      'use',
      field('module', $.identifier),
    ),

    // Import statement: import "library.so" as "namespace" { fn name(...) }
    import_statement: $ => seq(
      'import',
      field('library', $.string),
      'as',
      field('namespace', $.string),
      '{',
      repeat($.import_function),
      '}',
    ),

    // Function declaration within import: fn name(inputs -- outputs)
    // or: pub fn name(inputs -- outputs)
    import_function: $ => seq(
      optional('pub'),
      'fn',
      field('name', $.identifier),
      field('signature', optional($.stack_signature)),
    ),

    // Block: { ... }
    block: $ => seq(
      '{',
      repeat($._statement),
      '}',
    ),

    _expression: $ => choice(
      $.number,
      $.string,
      $.generic_instruction,
      $.builtin_operation,
      $.operator_symbol,
      $.struct_construction,
      $.namespaced_identifier,
      $.field_access,
      $.field_set,
      $.identifier,
      $.local_declaration,
      $.if_expression,
      $.for_loop,
      $.while_loop,
      $.loop_statement,
      $.switch_expression,
      $.defer_block,
      $.ctx_block,
      $.break_statement,
      $.continue_statement,
      $.return_statement,
      $.loop_variable,
      $.pointer_operation,
    ),

    // Struct construction: StructName { field1 = value1 field2 = value2 ... }
    // or: namespace::StructName { field1 = value1 ... }
    struct_construction: $ => prec(2, seq(
      field('type', choice($.namespaced_identifier, $.identifier)),
      '{',
      repeat($.field_init),
      '}',
    )),

    // Field initialization: fieldname = value
    // Each field has exactly one expression value for simplicity
    // Complex expressions like "1.0 2.0 +" should still parse, just not as separate values
    field_init: $ => seq(
      field('name', $.identifier),
      '=',
      field('value', $._field_value),
    ),

    // Values allowed in field initializers
    _field_value: $ => choice(
      $.number,
      $.string,
      $.builtin_operation,
      $.operator_symbol,
      $.struct_construction,
      $.namespaced_identifier,
      $.identifier,
      $.loop_variable,
    ),

    // If expression: if { ... } else { ... }
    if_expression: $ => seq(
      'if',
      field('then', $.block),
      optional(seq('else', field('else', $.block))),
    ),

    // For loop: start end step for name { ... }
    for_loop: $ => seq(
      'for',
      field('variable', $.identifier),
      field('body', $.block),
    ),

    // While loop: condition while { ... next-condition }
    while_loop: $ => seq(
      'while',
      field('body', $.block),
    ),

    // Infinite loop: loop { ... }
    loop_statement: $ => seq(
      'loop',
      field('body', $.block),
    ),

    // Switch expression: value switch { N { } _ { } }
    switch_expression: $ => seq(
      'switch',
      '{',
      repeat($.case_clause),
      optional($.default_clause),
      '}',
    ),

    // Case clause: value { body } (no 'case' keyword)
    // Higher precedence to resolve conflict with standalone number expressions
    case_clause: $ => prec(1, seq(
      field('value', choice($.number, $.namespaced_identifier, $.identifier)),
      field('body', $.block),
    )),

    // Default clause: _ { body } (no 'default' keyword)
    default_clause: $ => seq(
      '_',
      field('body', $.block),
    ),

    // Defer block: defer { ... }
    defer_block: $ => seq(
      'defer',
      field('body', $.block),
    ),

    // Ctx block: ctx { ... }
    ctx_block: $ => seq(
      'ctx',
      field('body', $.block),
    ),

    // Break, continue, and return
    break_statement: $ => 'break',
    continue_statement: $ => 'continue',
    return_statement: $ => 'return',

    // Loop variable: it
    loop_variable: $ => 'it',

    // Pointer operations: @p, @i, @f, !p, !i, !f
    pointer_operation: $ => choice(
      '@p', '@i', '@f',
      '!p', '!i', '!f',
    ),

    // Field access: @fieldname (gets field value from struct on stack)
    field_access: $ => seq(
      '@',
      field('field', $.identifier),
    ),

    // Field set: .fieldname (sets field value in struct on stack)
    field_set: $ => seq(
      '.',
      field('field', $.identifier),
    ),

    // Operator symbols (single-char and multi-char operators)
    operator_symbol: $ => prec(1, choice(
      '+', '-', '*', '/', '%',
      '==', '!=', '<', '>', '<=', '>=',
      '!',
    )),

    // Generic instruction: make<T>, cast<T>, etc.
    generic_instruction: $ => prec(2, seq(
      field('name', choice('make', 'makei', 'makef', 'makes', 'cast')),
      '<',
      field('type_arg', $.identifier),
      '>',
    )),

    // Built-in operations
    builtin_operation: $ => prec(1, choice(
      // Stack operations
      'dup', 'swap', 'drop', 'over', 'rot', 'nip', 'tuck', 'pick', 'roll',
      'dup2', 'swap2', 'over2', 'drop2', 'dupd', 'swapd', 'overd', 'nipd', 'depth', 'clear',
      // Arithmetic
      'add', 'sub', 'mul', 'div', 'inc', 'dec', 'abs', 'sqrt', 'sq', 'pow', 'mod',
      'neg', 'inv', 'fac', 'cb', 'cbrt',
      // Math functions
      'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'ln', 'log10',
      'ceil', 'floor', 'round', 'min', 'max',
      // Comparison
      'eq', 'neq', 'lt', 'gt', 'lte', 'gte', 'within',
      // Logic
      'and', 'or', 'not', 'xor',
      // Bitwise
      'lshift', 'rshift',
      // Type casting
      'cast',
      // I/O
      'print', 'prints', 'printv', 'printsv', 'call', 'nl', 'read',
      // Threading
      'detach', 'spawn', 'wait',
      // Error handling
      'error',
      // Memory management
      'free',
    )),

    // Literals
    number: $ => token(choice(
      /-\d+\.\d+/,   // Negative float
      /\d+\.\d+/,    // Positive float
      /-\d+/,        // Negative integer
      /\d+/,         // Positive integer
    )),

    string: $ => seq(
      '"',
      repeat(choice(
        token.immediate(/[^"\\]/),
        seq('\\', /./),
      )),
      '"',
    ),

    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    // Namespaced identifier: namespace::function
    namespaced_identifier: $ => seq(
      field('namespace', $.identifier),
      '::',
      field('name', $.identifier),
    ),

    // Local variable declaration: -> variable_name
    // Note: The compiler supports multiple assignment (-> a b c), but tree-sitter
    // only highlights the first identifier since it can't distinguish newlines.
    local_declaration: $ => seq(
      '->',
      field('name', $.identifier),
    ),

    // Comments
    comment: $ => token(choice(
      seq('//', /.*/),
      seq(
        '/*',
        repeat(choice(
          /[^*]/,
          seq('*', /[^/]/)
        )),
        '*/'
      ),
    )),
  },
});
