package parser

import "git.sr.ht/~klahr/quadrate/lexer"

type Push struct {
	DefaultDecl

	Type      AstNodeType     `json:"type"`
	Value     string          `json:"value"`
	ValueType lexer.TokenType `json:"value_type"`
}
