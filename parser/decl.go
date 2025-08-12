package parser

import "git.sr.ht/~klahr/quadrate/ast"

type ModuleDecl struct {
	items []ast.Node
}

func (m *ModuleDecl) Items() []ast.Node {
	return m.items
}

func (m *ModuleDecl) Append(item ast.Node) {
	m.items = append(m.items, item)
}

type FunctionDecl struct {
	Type          AstNodeType   `json:"type"`
	Name          string        `json:"name"`
	StackNotation StackNotation `json:"stack_notation"`
	Body          []ast.Node    `json:"body"`
}

func (f *FunctionDecl) Items() []ast.Node {
	return f.Body
}

func (f *FunctionDecl) Append(item ast.Node) {
	f.Body = append(f.Body, item)
}

type UseDecl struct {
	Type AstNodeType `json:"type"`
	Path string      `json:"path"`
}

func (u *UseDecl) Items() []ast.Node {
	return nil
}

func (u *UseDecl) Append(item ast.Node) {
}
