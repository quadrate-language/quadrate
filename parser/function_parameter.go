package parser

import "git.sr.ht/~klahr/quadrate/lexer"

type FunctionParameter struct {
	Name string          `json:"name"`
	Type lexer.TokenType `json:"type"`
}
