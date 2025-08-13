package parser

type AstNodeType string

const (
	AstNodeTypeBody                = "Body"
	AstNodeTypeFunctionCall        = "FunctionCall"
	AstNodeTypeFunctionDeclaration = "FunctionDeclaration"
	AstNodeTypePush                = "Push"
	AstNodeTypeReturnStatement     = "ReturnStatement"
	AstNodeTypeUseDeclaration      = "UseDeclaration"
)
