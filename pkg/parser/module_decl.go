package parser

import "git.sr.ht/~klahr/quadrate/pkg/ast"

type ModuleDecl struct {
	Children []ast.Node `json:"items"`
}

func (m *ModuleDecl) Items() []ast.Node {
	return m.Children
}

func (m *ModuleDecl) Append(item ast.Node) {
	m.Children = append(m.Children, item)
}
