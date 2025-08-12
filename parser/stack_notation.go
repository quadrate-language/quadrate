package parser

import "git.sr.ht/~klahr/quadrate/lexer"

type Parameter struct {
	Name string          `json:"name"`
	Type lexer.TokenType `json:"type"`
}

type StackNotation struct {
	Inputs  []Parameter `json:"inputs"`
	Outputs []Parameter `json:"outputs"`
}
