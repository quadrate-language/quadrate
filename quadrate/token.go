package quadrate

type TokenType string

const (
	BRACKET_LEFT       = "BRACKET_LEFT"
	BRACKET_RIGHT      = "BRACKET_RIGHT"
	EOF                = "EOF"
	FUNCTION           = "FUNCTION"
	IDENTIFIER         = "IDENTIFIER"
	ILLEGAL            = "ILLEGAL"
	NEW_LINE           = "NEW_LINE"
	NUMERICAL_CONSTANT = "NUMERICAL_CONSTANT"
	PARAN_LEFT         = "PARAN_LEFT"
	PARAN_RIGHT        = "PARAN_RIGHT"
	USE                = "USE"
)

type Token struct {
	Type    TokenType
	Literal string
	Line    int
	Column  int
}

func NewToken(t TokenType, literal string, line, column int) Token {
	return Token{
		Type:    t,
		Literal: literal,
		Line:    line,
		Column:  column,
	}
}
