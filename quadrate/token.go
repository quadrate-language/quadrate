package quadrate

type TokenType string

const (
	BeginScopeComment    = "begin_scope_comment"
	Break                = "break"
	Colon                = "colon"
	Comma                = "comma"
	Const                = "const"
	Continue             = "continue"
	CurlyBracketLeft     = "curly_bracket_left"
	CurlyBracketRight    = "curly_bracket_right"
	DashDash             = "dash_dash"
	Defer                = "defer"
	DoubleColon          = "double_colon"
	EOF                  = "EOF"
	End                  = "end"
	EndScopeComment      = "end_scope_comment"
	FnSignature          = "fn_signature"
	For                  = "for"
	Identifier           = "identifier"
	Illegal              = "illegal"
	InlineC              = "inline_c"
	Iterator             = "iterator"
	Jump                 = "jmp"
	JumpEqual            = "je"
	JumpGreater          = "jg"
	JumpGreaterEqual     = "jge"
	JumpGreaterEqualZero = "jgez"
	JumpGreaterZero      = "jgz"
	JumpLesser           = "jl"
	JumpLesserEqual      = "jle"
	JumpLesserEqualZero  = "jlez"
	JumpLesserZero       = "jlz"
	JumpNotEqual         = "jne"
	JumpNotZero          = "jnz"
	JumpZero             = "jz"
	Local                = "local"
	Loop                 = "loop"
	Module               = "module"
	NewLine              = "new_line"
	NumericConstant      = "numeric_constant"
	ParenthesisLeft      = "parenthesis_left"
	ParenthesisRight     = "parenthesis_right"
	Pointer              = "pointer"
	Reduce               = "reduce"
	Return               = "return"
	Scope                = "scope"
	SemiColon            = "semicolon"
	String               = "string"
	Use                  = "use"
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
