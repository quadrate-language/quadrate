/**
 * Tree-sitter grammar for Quadrate programming language
 */

module.exports = grammar({
  name: 'quadrate',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  rules: {
    source_file: $ => repeat($._statement),

    _statement: $ => choice(
      $.function_definition,
      $.constant_definition,
      $.use_statement,
      $._expression,
    ),

    // Function definition: fn name( x:float y:float -- z:float ) { ... }
    function_definition: $ => seq(
      'fn',
      field('name', $.identifier),
      field('signature', optional($.stack_signature)),
      field('body', $.block),
    ),

    // Stack signature: ( x:float y:float -- z:float )
    stack_signature: $ => seq(
      '(',
      optional($.parameter_list),
      '--',
      optional($.parameter_list),
      ')',
    ),

    parameter_list: $ => repeat1($.parameter),

    parameter: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $.type),
    ),

    type: $ => 'float',

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

    // Block: { ... }
    block: $ => seq(
      '{',
      repeat($._statement),
      '}',
    ),

    _expression: $ => choice(
      $.number,
      $.string,
      $.identifier,
      $.if_expression,
      $.for_loop,
      $.switch_expression,
      $.defer_block,
      $.builtin_operation,
      $.break_statement,
      $.continue_statement,
      $.loop_variable,
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

    // Break and continue
    break_statement: $ => 'break',
    continue_statement: $ => 'continue',

    // Loop variable: $
    loop_variable: $ => '$',

    // Built-in operations
    builtin_operation: $ => choice(
      // Stack operations
      'dup', 'swap', 'drop', 'over', 'rot', 'nip', 'tuck', 'pick', 'roll',
      'dup2', 'swap2', 'over2', 'drop2', 'depth', 'clear',
      // Arithmetic
      'add', 'sub', 'mul', 'div', 'inc', 'dec', 'abs', 'sqrt', 'sq', 'pow', 'mod',
      'neg', 'inv', 'fac', 'cb', 'cbrt',
      // Math functions
      'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'ln', 'log10',
      'ceil', 'floor', 'round', 'min', 'max',
      // Comparison
      'eq', 'neq', 'lt', 'gt', 'lte', 'gte', 'within',
      // Logic
      'and', 'or', 'not',
      // I/O
      'print', 'prints', 'printv', 'printsv', 'call',
    ),

    // Literals
    number: $ => token(choice(
      /-?\d+\.\d+/,  // Float (with optional negative sign)
      /-?\d+/,       // Integer (with optional negative sign)
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

    // Comments
    comment: $ => choice(
      token(seq('//', /.*/)),
      token(seq(
        '/*',
        /[^*]*\*+([^/*][^*]*\*+)*/,
        '/',
      )),
    ),
  },
});
