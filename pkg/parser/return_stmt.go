package parser

type ReturnStmt struct {
	DefaultDecl

	Type AstNodeType `json:"type"`
}
