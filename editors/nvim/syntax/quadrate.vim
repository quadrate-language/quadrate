" Vim syntax file
" Language: Quadrate
" Maintainer: Quadrate Project
" Latest Revision: 2025

if exists("b:current_syntax")
  finish
endif

" Keywords
syn keyword quadrateKeyword fn const use
syn keyword quadrateConditional if else switch case default
syn keyword quadrateRepeat for break continue
syn keyword quadrateStatement defer

" Types
syn keyword quadrateType float

" Built-in stack operations
syn keyword quadrateStack dup swap drop over rot nip tuck pick roll
syn keyword quadrateStack dup2 swap2 over2 drop2 depth clear

" Arithmetic operations
syn keyword quadrateArithmetic add sub mul div inc dec abs sqrt sq pow mod
syn keyword quadrateArithmetic neg inv fac cb cbrt

" Math functions
syn keyword quadrateMath sin cos tan asin acos atan ln log10
syn keyword quadrateMath ceil floor round min max

" Comparison operations
syn keyword quadrateComparison eq neq lt gt lte gte within

" Logical operations
syn keyword quadrateLogic and or not

" I/O operations
syn keyword quadrateIO print prints printv printsv call

" Comments
syn keyword quadrateTodo TODO FIXME XXX NOTE contained
syn match quadrateComment "//.*$" contains=quadrateTodo
syn region quadrateComment start="/\*" end="\*/" contains=quadrateTodo

" Numbers
syn match quadrateNumber "\<\d\+\>"
syn match quadrateFloat "\<\d\+\.\d*\>"
syn match quadrateFloat "\<\d*\.\d\+\>"

" Strings
syn region quadrateString start=+"+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+ end=+$+

" Special variables
syn match quadrateSpecial "\$"

" Operators
syn match quadrateOperator "--"
syn match quadrateOperator ":"

" Function definitions
syn match quadrateFunction "\<fn\s\+\w\+"he=s+3

" Constants
syn match quadrateConstant "\<const\s\+\w\+"he=s+6

" Link to standard highlighting groups
hi def link quadrateKeyword       Keyword
hi def link quadrateConditional   Conditional
hi def link quadrateRepeat        Repeat
hi def link quadrateStatement     Statement
hi def link quadrateType          Type
hi def link quadrateStack         Function
hi def link quadrateArithmetic    Function
hi def link quadrateMath          Function
hi def link quadrateComparison    Function
hi def link quadrateLogic         Function
hi def link quadrateIO            Function
hi def link quadrateComment       Comment
hi def link quadrateTodo          Todo
hi def link quadrateNumber        Number
hi def link quadrateFloat         Float
hi def link quadrateString        String
hi def link quadrateSpecial       Special
hi def link quadrateOperator      Operator
hi def link quadrateFunction      Function
hi def link quadrateConstant      Constant

let b:current_syntax = "quadrate"
