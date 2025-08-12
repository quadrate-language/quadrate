package lexer

import (
	"encoding/json"
	"fmt"
	"testing"
)

func TestLexMain(t *testing.T) {
	source := []rune(`fn main() {
	push -8
	push "Hello, world!"
}`)
	scanner := NewScanner(source)

	expectedTokens := []Token{
		{Type: Function, Value: "fn", Line: 1, Column: 1, Length: 2},
		{Type: Identifier, Value: "main", Line: 1, Column: 4, Length: 4},
		{Type: LParen, Value: "(", Line: 1, Column: 8, Length: 1},
		{Type: RParen, Value: ")", Line: 1, Column: 9, Length: 1},
		{Type: LBrace, Value: "{", Line: 1, Column: 11, Length: 1},
		{Type: EOL, Value: "", Line: 1, Column: 12, Length: 1},
		{Type: Identifier, Value: "push", Line: 2, Column: 2, Length: 4},
		{Type: IntLiteral, Value: "-8", Line: 2, Column: 8, Length: 2},
		{Type: EOL, Value: "", Line: 2, Column: 9, Length: 1},
		{Type: Identifier, Value: "push", Line: 3, Column: 2, Length: 4},
		{Type: StringLiteral, Value: "\"Hello, world!\"", Line: 3, Column: 7, Length: 15},
		{Type: EOL, Value: "", Line: 3, Column: 22, Length: 1},
		{Type: RBrace, Value: "}", Line: 4, Column: 1, Length: 1},
		{Type: EOF, Value: "", Line: 4, Column: 2, Length: 0},
	}
	if len(expectedTokens) != 14 {
		t.Fatalf("Expected 14 tokens, got %d", len(expectedTokens))
	}

	tokens, err := scanner.Lex()
	if err != nil {
		t.Fatalf("Lexing failed: %v", err)
	}

	if js, err := json.MarshalIndent(tokens, "", "  "); err == nil {
		fmt.Println(string(js))
	}

	if len(tokens) == 0 {
		t.Error("Expected tokens, but got none")
	}

	for i, token := range tokens {
		expected := expectedTokens[i]
		if token.Type != expected.Type || token.Value != expected.Value ||
			token.Line != expected.Line || token.Column != expected.Column ||
			token.Length != expected.Length {
			t.Errorf("Token mismatch at index %d: got %+v, want %+v", i, token, expected)
		}
	}
}

func TestLexFn(t *testing.T) {
	source := []rune(`fn foo( a:int b:str -- c:float )`)
	scanner := NewScanner(source)

	expectedTokens := []Token{
		{Type: Function, Value: "fn"},
		{Type: Identifier, Value: "foo"},
		{Type: LParen, Value: "("},
		{Type: Identifier, Value: "a"},
		{Type: Colon, Value: ":"},
		{Type: Int, Value: "int"},
		{Type: Identifier, Value: "b"},
		{Type: Colon, Value: ":"},
		{Type: String, Value: "str"},
		{Type: DoubleDash, Value: "--"},
		{Type: Identifier, Value: "c"},
		{Type: Colon, Value: ":"},
		{Type: Float, Value: "float"},
		{Type: RParen, Value: ")"},
		{Type: EOF, Value: ""},
	}

	if len(expectedTokens) != 15 {
		t.Fatalf("Expected 15 tokens, got %d", len(expectedTokens))
	}

	tokens, err := scanner.Lex()
	if err != nil {
		t.Fatalf("Lexing failed: %v", err)
	}

	if js, err := json.MarshalIndent(tokens, "", "  "); err == nil {
		fmt.Println(string(js))
	}

	if len(tokens) == 0 {
		t.Error("Expected tokens, but got none")
	}

	for i, token := range tokens {
		expected := expectedTokens[i]
		if token.Type != expected.Type || token.Value != expected.Value {
			t.Errorf("Token mismatch at index %d: got %+v, want %+v", i, token, expected)
		}
	}
}
