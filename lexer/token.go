package lexer

import "git.sr.ht/~klahr/quadrate/diagnostic"

type TokenType string

const (
	Ampersand            = "AMPERSAND"
	Break                = "BREAK"
	Colon                = "COLON"
	Comment              = "COMMENT"
	Const                = "CONST"
	Continue             = "CONTINUE"
	Defer                = "DEFER"
	Dollar               = "DOLLAR"
	DoubleColon          = "DOUBLE_COLON"
	DoubleDash           = "DOUBLE_DASH"
	Else                 = "ELSE"
	ElseIf               = "ELSEIF"
	End                  = "END"
	EOF                  = "EOF"
	EOL                  = "EOL"
	Float                = "FLOAT"
	FloatLiteral         = "FLOAT_LITERAL"
	For                  = "FOR"
	Function             = "FUNCTION"
	HexNumber            = "HEX_NUMBER"
	Identifier           = "IDENTIFIER"
	If                   = "IF"
	Illegal              = "ILLEGAL"
	InlineC              = "INLINE_C"
	Int                  = "INT"
	IntLiteral           = "INT_LITERAL"
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
	RBrace               = "RBRACE"
	Reduce               = "REDUCE"
	Return               = "RETURN"
	RParen               = "RPAREN"
	Semicolon            = "SEMICOLON"
	String               = "STRING"
	StringLiteral        = "STRING_LITERAL"
	Unknown              = "UNKNOWN"
	Use                  = "USE"
)

type Token struct {
	diagnostic.SourceSpan
	Type  TokenType `json:"type"`
	Value string    `json:"value"`
}
