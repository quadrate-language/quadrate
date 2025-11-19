/**
 * Tree-sitter grammar for Quadrate programming language
 */

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
      $.constant_definition,
      $.use_statement,
      $.import_statement,
      $._expression,
    ),

    // Function definition: fn name( x:float y:float -- z:float ) { ... }
    // or: pub fn name( x:float y:float -- z:float ) { ... }
    function_definition: $ => seq(
      optional('pub'),
      'fn',
      field('name', $.identifier),
      field('signature', optional($.stack_signature)),
      field('body', $.block),
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

    // Constant: const name = value
    constant_definition: $ => seq(
      'const',
      field('name', $.identifier),
      '=',
      field('value', $._expression),
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
    import_function: $ => seq(
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
      $.builtin_operation,
      $.operator_symbol,
      $.namespaced_identifier,
      $.identifier,
      $.local_declaration,
      $.if_expression,
      $.for_loop,
      $.loop_statement,
      $.switch_expression,
      $.defer_block,
      $.ctx_block,
      $.break_statement,
      $.continue_statement,
      $.loop_variable,
      $.pointer_operation,
    ),

    // If expression: if { ... } else { ... }
    if_expression: $ => seq(
      'if',
      field('then', $.block),
      optional(seq('else', field('else', $.block))),
    ),

    // For loop: start end step for { ... }
    for_loop: $ => seq(
      'for',
      field('variable', optional($.identifier)),
      field('body', $.block),
    ),

    // Infinite loop: loop { ... }
    loop_statement: $ => seq(
      'loop',
      field('body', $.block),
    ),

    // Switch expression: value switch { case N { } default { } }
    switch_expression: $ => seq(
      'switch',
      '{',
      repeat($.case_clause),
      optional($.default_clause),
      '}',
    ),

    case_clause: $ => seq(
      'case',
      field('value', $._expression),
      field('body', $.block),
    ),

    default_clause: $ => seq(
      'default',
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

    // Break and continue
    break_statement: $ => 'break',
    continue_statement: $ => 'continue',

    // Loop variable: $
    loop_variable: $ => '$',

    // Pointer operations: @p, @i, @f, !p, !i, !f
    pointer_operation: $ => choice(
      '@p', '@i', '@f',
      '!p', '!i', '!f',
    ),

    // Operator symbols (single-char and multi-char operators)
    operator_symbol: $ => prec(1, choice(
      '.', '+', '-', '*', '/', '%',
      '==', '!=', '<', '>', '<=', '>=',
      '!',
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
      'casti', 'castf', 'casts',
      // I/O
      'print', 'prints', 'printv', 'printsv', 'call', 'nl', 'read',
      // Threading
      'detach', 'spawn', 'wait',
      // Error handling
      'error',
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
