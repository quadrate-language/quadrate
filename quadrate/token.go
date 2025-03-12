package quadrate

type TokenType string

const (
	BeginScopeComment = "begin_scope_comment"
	Comma             = "comma"
	Const             = "const"
	CurlyBracketLeft  = "curly_bracket_left"
	CurlyBracketRight = "curly_bracket_right"
	Defer             = "defer"
	DoubleColon       = "double_colon"
	EOF               = "EOF"
	EndScopeComment   = "end_scope_comment"
	FnSignature       = "fn_signature"
	Identifier        = "identifier"
	Illegal           = "illegal"
	InlineC           = "inline_c"
	Module            = "module"
	NewLine           = "new_line"
	NumericConstant   = "numeric_constant"
	ParenthesisLeft   = "parenthesis_left"
	ParenthesisRight  = "parenthesis_right"
	Return            = "return"
	Scope             = "scope"
	String            = "string"
	Use               = "use"
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
