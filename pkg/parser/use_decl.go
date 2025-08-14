package parser

type UseDecl struct {
	DefaultDecl

	Type AstNodeType `json:"type"`
	Path string      `json:"path"`
}
