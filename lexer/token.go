package lexer

type TokenType string

const (
	EOF        = "EOF"
	EOL        = "EOL"
	Function   = "FUNCTION"
	Identifier = "IDENTIFIER"
	LParen     = "LPAREN"
	RParen     = "RPAREN"
	LBrace     = "LBRACE"
	RBrace     = "RBRACE"
	HexNumber  = "HEX_NUMBER"
	Number     = "NUMBER"
	String     = "STRING"
	Operator   = "OPERATOR"
	Keyword    = "KEYWORD"
	Comment    = "COMMENT"
	Whitespace = "WHITESPACE"
	Unknown    = "UNKNOWN"
)

type Token struct {
	Type      TokenType `json:"type"`
	Value     string    `json:"value"`
	Line      int       `json:"line"`
	Character int       `json:"character"`
	Length    int       `json:"length"`
	Offset    int       `json:"offset"`
}
