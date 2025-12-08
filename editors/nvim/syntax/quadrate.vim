" Vim syntax file
" Language: Quadrate
" Maintainer: Quadrate Project
" Latest Revision: 2025

if exists("b:current_syntax")
  finish
endif

" Keywords
syn keyword quadrateKeyword fn const use pub struct import as
syn keyword quadrateConditional if else
syn keyword quadrateRepeat loop for break continue
syn keyword quadrateStatement defer

" Import block region (to highlight pub inside import blocks)
syn region quadrateImportBlock start="\<import\s\+\w\+\s*{" end="}" contains=quadrateKeyword,quadrateComment transparent

" Types
syn keyword quadrateType i64 f64 str ptr any

" Built-in stack operations
syn keyword quadrateStack dup dup2 dupd swap swap2 swapd drop drop2
syn keyword quadrateStack over over2 overd rot nip nipd tuck
syn keyword quadrateStack pick roll depth clear len nth free

" Arithmetic operations
syn keyword quadrateArithmetic add sub mul div mod neg inc dec

" Math functions (from math library)
syn keyword quadrateMath abs sqrt sq cb cbrt pow inv fac
syn keyword quadrateMath sin cos tan asin acos atan ln log10
syn keyword quadrateMath ceil floor round min max

" Comparison operations
syn keyword quadrateComparison eq neq lt gt lte gte within

" Array operations
syn keyword quadrateArray append makef makei makep makes set

" Type casting
syn keyword quadrateCast cast

" I/O operations
syn keyword quadrateIO print prints printv printsv nl read call

" Threading
syn keyword quadrateThread spawn wait detach

" Error handling
syn keyword quadrateError error

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

" Arrow operator for variable binding
syn match quadrateArrow "->"

" Operators
syn match quadrateOperator "--"
syn match quadrateOperator ":"
syn match quadrateOperator "!"
syn match quadrateOperator "\$"
syn match quadrateOperator "&"

" Module path separator
syn match quadrateNamespace "::"

" Function definitions
syn match quadrateFunction "\<fn\s\+\w\+"he=s+3

" Struct definitions
syn match quadrateStructDef "\<struct\s\+\w\+"he=s+7

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
hi def link quadrateArray         Function
hi def link quadrateCast          Function
hi def link quadrateIO            Function
hi def link quadrateThread        Function
hi def link quadrateError         Exception
hi def link quadrateComment       Comment
hi def link quadrateTodo          Todo
hi def link quadrateNumber        Number
hi def link quadrateFloat         Float
hi def link quadrateString        String
hi def link quadrateArrow         Operator
hi def link quadrateOperator      Operator
hi def link quadrateNamespace     Delimiter
hi def link quadrateFunction      Function
hi def link quadrateStructDef     Structure
hi def link quadrateConstant      Constant

let b:current_syntax = "quadrate"
