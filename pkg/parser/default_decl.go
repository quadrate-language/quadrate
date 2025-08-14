package parser

import "git.sr.ht/~klahr/quadrate/pkg/ast"

type DefaultDecl struct {
}

func (d *DefaultDecl) Items() []ast.Node {
	return nil
}

func (d *DefaultDecl) Append(item ast.Node) {
}
