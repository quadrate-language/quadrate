package lexer

import (
	"encoding/json"
	"fmt"
	"testing"
)

const sampleSource = `fn main() {
	push 4
}`

func TestLex(t *testing.T) {
	source := []rune(sampleSource)
	scanner := NewScanner(source)

	expectedTokens := []Token{
		{Type: Function, Value: "fn", Line: 1, Character: 1, Length: 2, Offset: 0},
		{Type: Identifier, Value: "main", Line: 1, Character: 4, Length: 4, Offset: 3},
		{Type: LParen, Value: "(", Line: 1, Character: 8, Length: 1, Offset: 7},
		{Type: RParen, Value: ")", Line: 1, Character: 9, Length: 1, Offset: 8},
		{Type: LBrace, Value: "{", Line: 1, Character: 11, Length: 1, Offset: 10},
		{Type: EOL, Value: "EOL", Line: 1, Character: 12, Length: 1, Offset: 11},
		{Type: Identifier, Value: "push", Line: 2, Character: 5, Length: 4, Offset: 13},
		{Type: Number, Value: "4", Line: 2, Character: 10, Length: 1, Offset: 18},
		{Type: EOL, Value: "EOL", Line: 2, Character: 11, Length: 1, Offset: 19},
		{Type: RBrace, Value: "}", Line: 3, Character: 1, Length: 1, Offset: 20},
		{Type: EOF, Value: "EOF", Line: 3, Character: 2, Length: 0, Offset: 20},
	}
	if len(expectedTokens) != 11 {
		t.Fatalf("Expected 9 tokens, got %d", len(expectedTokens))
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
			token.Line != expected.Line || token.Character != expected.Character ||
			token.Length != expected.Length || token.Offset != expected.Offset {
			t.Errorf("Token mismatch at index %d: got %+v, want %+v", i, token, expected)
		}
	}
}
