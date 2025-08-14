package parser

type FunctionCall struct {
	DefaultDecl

	Type   AstNodeType `json:"type"`
	Name   string      `json:"name"`
	Module string      `json:"module"`
}
