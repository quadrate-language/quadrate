package parser

type AstNodeType string

const (
	AstNodeTypeBody                = "Body"
	AstNodeTypeFunctionDeclaration = "FunctionDeclaration"
	AstNodeTypeUseDeclaration      = "UseDeclaration"
	AstNodeTypeReturnStatement     = "ReturnStatement"
)
