package parser

import (
	"encoding/json"
	"fmt"
	"testing"

	"git.sr.ht/~klahr/quadrate/lexer"
)

func TestParse(t *testing.T) {
	source := []rune(`use fmt
	fn hello(a:int b:float --c:str) {
		return
	}
	fn dj() {
	}
	fn main() {
		1 2
		3.1 4.5 -4.3 "hello"
		"world"
		pop
		pop
		fmt::print
}`)

	scanner := lexer.NewScanner(source)
	if tokens, err := scanner.Lex(); err != nil {
		t.Fatalf("Lexing failed: %v", err)
	} else {
		for _, t := range tokens {
			fmt.Printf("%s: %s\n", t.Type, t.Value)
		}
		parser := NewASTBuilder()
		if module, issues := parser.Build(tokens); issues != nil {
			t.Fatalf("Parsing failed: %v", issues.Error())
		} else {
			if js, err := json.MarshalIndent(module, "", "  "); err == nil {
				fmt.Println(string(js))
			}
			t.FailNow()
		}
	}
}
