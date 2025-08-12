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
}`)

	scanner := lexer.NewScanner(source)
	if tokens, err := scanner.Lex(); err != nil {
		t.Fatalf("Lexing failed: %v", err)
	} else {
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
