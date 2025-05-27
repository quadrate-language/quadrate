package lexer

type TokenType string

const (
	Ampersand            = "AMPERSAND"
	Break                = "BREAK"
	Comment              = "COMMENT"
	Const                = "CONST"
	Continue             = "CONTINUE"
	Defer                = "DEFER"
	Dollar               = "DOLLAR"
	Else                 = "ELSE"
	ElseIf               = "ELSEIF"
	End                  = "END"
	EOF                  = "EOF"
	EOL                  = "EOL"
	For                  = "FOR"
	Function             = "FUNCTION"
	HexNumber            = "HEX_NUMBER"
	Identifier           = "IDENTIFIER"
	If                   = "IF"
	InlineC              = "INLINE_C"
	Jump                 = "JMP"
	JumpEqual            = "JE"
	JumpGreater          = "JG"
	JumpGreaterEqual     = "JGE"
	JumpGreaterEqualZero = "JGEZ"
	JumpGreaterZero      = "JGZ"
	JumpLesser           = "JL"
	JumpLesserEqual      = "JLE"
	JumpLesserEqualZero  = "JLEZ"
	JumpLesserZero       = "JLZ"
	JumpNotEqual         = "JNE"
	JumpNotZero          = "JNZ"
	JumpZero             = "JZ"
	LBrace               = "LBRACE"
	Local                = "LOCAL"
	Loop                 = "LOOP"
	LParen               = "LPAREN"
	Number               = "NUMBER"
	RBrace               = "RBRACE"
	Reduce               = "REDUCE"
	Return               = "RETURN"
	RParen               = "RPAREN"
	Semicolon            = "SEMICOLON"
	String               = "STRING"
	Unknown              = "UNKNOWN"
	Use                  = "USE"
)

type Token struct {
	Column int       `json:"column"`
	Length int       `json:"length"`
	Line   int       `json:"line"`
	Offset int       `json:"offset"`
	Type   TokenType `json:"type"`
	Value  string    `json:"value"`
}
