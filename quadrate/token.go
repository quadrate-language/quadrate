package quadrate

type TokenType string

const (
	Comma              = "comma"
	CurlyBracketLeft   = "curly_bracket_left"
	CurlyBracketRight  = "curly_bracket_right"
	DoubleColon        = "double_colon"
	EOF                = "EOF"
	FnSignature        = "fn_signature"
	Identifier         = "identifier"
	Illegal            = "illegal"
	InlineC            = "inline_c"
	Module             = "module"
	NewLine            = "new_line"
	NumericConstant    = "numeric_constant"
	ParenthesisLeft    = "parenthesis_left"
	ParenthesisRight   = "parenthesis_right"
	Scope              = "scope"
	SquareBracketLeft  = "square_bracket_left"
	SquareBracketRight = "square_bracket_right"
	Use                = "use"
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
