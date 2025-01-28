package main

import (
	"fmt"

	"git.sr.ht/~klahr/quadrate/quadrate"
)

func main() {
	s := `use fmt
	fn main() { // a comment
	}`
	l := quadrate.NewLexer(s)
	tokens := l.Lex()
	for _, t := range tokens {
		fmt.Printf("%s: '%s':%d:%d\n", t.Type, t.Literal, t.Line, t.Column)
	}
}
