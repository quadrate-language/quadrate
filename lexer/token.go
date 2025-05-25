package lexer

type TokenType string

const (
	Comment    = "COMMENT"
	EOF        = "EOF"
	EOL        = "EOL"
	Function   = "FUNCTION"
	HexNumber  = "HEX_NUMBER"
	Identifier = "IDENTIFIER"
	LBrace     = "LBRACE"
	LParen     = "LPAREN"
	Number     = "NUMBER"
	RBrace     = "RBRACE"
	RParen     = "RPAREN"
	String     = "STRING"
	Unknown    = "UNKNOWN"
	Use        = "USE"
)

type Token struct {
	Column int       `json:"column"`
	Length int       `json:"length"`
	Line   int       `json:"line"`
	Offset int       `json:"offset"`
	Type   TokenType `json:"type"`
	Value  string    `json:"value"`
}
