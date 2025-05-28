package lexer

import (
	"errors"
	"fmt"
	"strings"
	"text/scanner"
)

type Scanner struct {
	source []rune
	cursor int
	line   int
	column int
}

type Result struct {
	Tokens []Token `json:"tokens"`
}

func NewScanner(source []rune) *Scanner {
	return &Scanner{
		source: source,
		cursor: 0,
		line:   1,
		column: 1,
	}
}

func (l *Scanner) Lex() ([]Token, error) {
	tokens := make([]Token, 0)
	var s scanner.Scanner

	s.Init(strings.NewReader(string(l.source)))
	s.Whitespace = 1<<' ' | 1<<'\t'
done:
	for t := s.Scan(); ; t = s.Scan() {
		switch t {
		case scanner.EOF:
			tokens = append(tokens, l.readToken(EOF, s))
			break done
		case scanner.Ident:
			tokens = append(tokens, l.readToken(l.lookupType(s.TokenText()), s))
		case scanner.Int, scanner.Float:
			tokens = append(tokens, l.readToken(Number, s))
		case scanner.String:
			tokens = append(tokens, l.readToken(String, s))
		case '\n':
			tokens = append(tokens, l.readToken(EOL, s))
		case ';':
			tokens = append(tokens, l.readToken(Semicolon, s))
		case '(':
			tokens = append(tokens, l.readToken(LParen, s))
		case ')':
			tokens = append(tokens, l.readToken(RParen, s))
		case '{':
			tokens = append(tokens, l.readToken(LBrace, s))
		case '}':
			tokens = append(tokens, l.readToken(RBrace, s))
		case '&':
			tokens = append(tokens, l.readToken(Ampersand, s))
		case '$':
			tokens = append(tokens, l.readToken(Dollar, s))
		case '-':
			t = s.Scan()
			if t == scanner.Int || t == scanner.Float {
				tokens = append(tokens, Token{
					Type:   Number,
					Value:  "-" + s.TokenText(),
					Line:   s.Line,
					Column: s.Column,
					Length: len(s.TokenText()) + 1,
					Offset: s.Offset,
				})
			} else {
				return nil, errors.New(fmt.Sprintf("Unexpected token '-' at line %d, column %d", s.Line, s.Column))
			}
		default:
			tokens = append(tokens, l.readToken(Illegal, s))
		}
	}

	return tokens, nil
}

func (l *Scanner) readToken(tokenType TokenType, s scanner.Scanner) Token {
	var value string
	switch tokenType {
	case EOF, EOL:
		value = ""
	default:
		value = s.TokenText()
	}

	return Token{
		Type:   tokenType,
		Value:  value,
		Line:   s.Line,
		Column: s.Column,
		Length: len(s.TokenText()),
		Offset: s.Offset,
	}
}

func (l *Scanner) lookupType(literal string) TokenType {
	switch literal {
	case "break":
		return Break
	case "continue":
		return Continue
	case "__c":
		return InlineC
	case "const":
		return Const
	case "defer":
		return Defer
	case "end":
		return End
	case "fn":
		return Function
	case "for":
		return For
	case "if":
		return If
	case "else":
		return Else
	case "elseif":
		return ElseIf
	case "return":
		return Return
	case "jmp":
		return Jump
	case "je":
		return JumpEqual
	case "jge":
		return JumpGreaterEqual
	case "jg":
		return JumpGreater
	case "jgz":
		return JumpGreaterZero
	case "jgez":
		return JumpGreaterEqualZero
	case "jle":
		return JumpLesserEqual
	case "jlz":
		return JumpLesserZero
	case "jlez":
		return JumpLesserEqualZero
	case "jl":
		return JumpLesser
	case "jne":
		return JumpNotEqual
	case "jnz":
		return JumpNotZero
	case "jz":
		return JumpZero
	case "local":
		return Local
	case "loop":
		return Loop
	case "reduce":
		return Reduce
	case "use":
		return Use
	default:
		return Identifier
	}
}
