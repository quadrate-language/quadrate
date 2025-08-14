package parser

import "git.sr.ht/~klahr/quadrate/pkg/ast"

type Body struct {
	Type     AstNodeType `json:"type"`
	Children []ast.Node  `json:"items"`
}

func (b *Body) Items() []ast.Node {
	return b.Children
}

func (b *Body) Append(item ast.Node) {
	b.Children = append(b.Children, item)
}
