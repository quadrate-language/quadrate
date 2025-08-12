package parser

import "git.sr.ht/~klahr/quadrate/ast"

type DefaultDecl struct {
}

func (d *DefaultDecl) Items() []ast.Node {
	return nil
}

func (d *DefaultDecl) Append(item ast.Node) {
}

type ModuleDecl struct {
	Children []ast.Node `json:"items"`
}

func (m *ModuleDecl) Items() []ast.Node {
	return m.Children
}

func (m *ModuleDecl) Append(item ast.Node) {
	m.Children = append(m.Children, item)
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
	DefaultDecl

	Type AstNodeType `json:"type"`
	Path string      `json:"path"`
}
