package quadrate

type TokenType string

const (
	CUR_BRACKET_LEFT   = "CUR_BRACKET_LEFT"
	CUR_BRACKET_RIGHT  = "CUR_BRACKET_RIGHT"
	EOF                = "EOF"
	FUNCTION           = "FUNCTION"
	IDENTIFIER         = "IDENTIFIER"
	ILLEGAL            = "ILLEGAL"
	NEW_LINE           = "NEW_LINE"
	NUMERICAL_CONSTANT = "NUMERICAL_CONSTANT"
	PARAN_LEFT         = "PARAN_LEFT"
	PARAN_RIGHT        = "PARAN_RIGHT"
	SQR_BRACKET_LEFT   = "SQR_BRACKET_LEFT"
	SQR_BRACKET_RIGHT  = "SQR_BRACKET_RIGHT"
	USE                = "USE"
)

type Token struct {
	Type    TokenType `json:"type"`
	Literal string    `json:"literal"`
	Line    int       `json:"line"`
	Column  int       `json:"column"`
}

func NewToken(t TokenType, literal string, line, column int) Token {
	return Token{
		Type:    t,
		Literal: literal,
		Line:    line,
		Column:  column,
	}
}
