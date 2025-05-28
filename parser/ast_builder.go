package parser

import (
	"git.sr.ht/~klahr/quadrate/ast"
	"git.sr.ht/~klahr/quadrate/lexer"
)

type ASTBuilder struct {
}

func NewASTBuilder() *ASTBuilder {
	return &ASTBuilder{}
}

func (b *ASTBuilder) Build(tokens []lexer.Token) (*ast.Tree, error) {
	return nil, nil
}
