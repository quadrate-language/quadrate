; Indent
[
  (block)
  (if_expression)
  (for_loop)
  (switch_expression)
  (case_clause)
  (default_clause)
  (defer_block)
  (function_definition)
] @indent.begin

; Dedent
[
  "}"
] @indent.dedent

; Ignore (don't affect indentation)
[
  (comment)
] @indent.ignore
