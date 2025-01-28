package main

import (
	"fmt"

	"git.sr.ht/~klahr/quadrate/quadrate"
)

func main() {
	s := `use fmt
	fn main() { // a comment
		push 1.2
	}`
	l := quadrate.NewLexer(s)
	tokens := l.Lex()
	// if (-dump-tokens) {
	for _, t := range tokens {
		switch t.Type {
		case quadrate.NEW_LINE:
			fmt.Println()
		case quadrate.IDENTIFIER:
			fmt.Printf("identifier '%s' [%d:%d]\n", t.Literal, t.Line, t.Column)
		case quadrate.EOF:
			fmt.Println("< EOF >")
		default:
			fmt.Printf("< '%s' >\n", t.Literal)
		}
	}
}
