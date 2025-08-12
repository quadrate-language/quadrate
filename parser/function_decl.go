package parser

import "git.sr.ht/~klahr/quadrate/ast"

type FunctionDecl struct {
	Type    AstNodeType         `json:"type"`
	Name    string              `json:"name"`
	Inputs  []FunctionParameter `json:"inputs"`
	Outputs []FunctionParameter `json:"outputs"`
	Body    []ast.Node          `json:"body"`
}

func (f *FunctionDecl) Items() []ast.Node {
	return f.Body
}

func (f *FunctionDecl) Append(item ast.Node) {
	f.Body = append(f.Body, item)
}
