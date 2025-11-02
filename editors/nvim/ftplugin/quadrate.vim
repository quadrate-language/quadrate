" Vim filetype plugin
" Language: Quadrate
" Maintainer: Quadrate Project

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

" Comment settings
setlocal commentstring=//\ %s
setlocal comments=://,s1:/*,mb:*,ex:*/

" Indentation
setlocal tabstop=4
setlocal shiftwidth=4
setlocal noexpandtab
setlocal autoindent
setlocal smartindent

" Format options
setlocal formatoptions-=t
setlocal formatoptions+=croql

" Matching pairs
setlocal matchpairs+=<:>

" Enable folding on braces
setlocal foldmethod=syntax
setlocal foldlevel=99

" Undo ftplugin settings
let b:undo_ftplugin = "setlocal commentstring< comments< tabstop< shiftwidth< expandtab< autoindent< smartindent< formatoptions< matchpairs< foldmethod< foldlevel<"
