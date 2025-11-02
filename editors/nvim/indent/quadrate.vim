" Vim indent file
" Language: Quadrate
" Maintainer: Quadrate Project

if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal indentexpr=GetQuadrateIndent()
setlocal indentkeys=0{,0},0),0],!^F,o,O,e

" Only define the function once
if exists("*GetQuadrateIndent")
  finish
endif

function! GetQuadrateIndent()
  let lnum = prevnonblank(v:lnum - 1)

  if lnum == 0
    return 0
  endif

  let ind = indent(lnum)
  let prevline = getline(lnum)
  let curline = getline(v:lnum)

  " Increase indent after opening brace
  if prevline =~ '{\s*$'
    let ind += &shiftwidth
  endif

  " Decrease indent on closing brace
  if curline =~ '^\s*}'
    let ind -= &shiftwidth
  endif

  " Increase indent after if/else without brace
  if prevline =~ '^\s*\(if\|else\)\s*$'
    let ind += &shiftwidth
  endif

  return ind
endfunction
