package lexer

import (
	"fmt"
	"strings"
	"text/scanner"

	"git.sr.ht/~klahr/quadrate/diagnostic"
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

func (l *Scanner) Lex() ([]Token, *diagnostic.Issue) {
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
		case scanner.Int:
			tokens = append(tokens, l.readToken(IntLiteral, s))
		case scanner.Float:
			tokens = append(tokens, l.readToken(FloatLiteral, s))
		case scanner.String:
			tokens = append(tokens, l.readToken(StringLiteral, s))
		case '\n':
			tokens = append(tokens, l.readToken(EOL, s))
		case ':':
			tokens = append(tokens, l.readToken(Colon, s))
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
			switch t {
			case scanner.Int:
				tokens = append(tokens, Token{
					Type:  IntLiteral,
					Value: "-" + s.TokenText(),
					SourceSpan: diagnostic.SourceSpan{
						Line:   s.Line,
						Column: s.Column,
						Length: len(s.TokenText()) + 1,
						Offset: s.Offset,
					},
				})
			case scanner.Float:
				tokens = append(tokens, Token{
					Type:  FloatLiteral,
					Value: "-" + s.TokenText(),
					SourceSpan: diagnostic.SourceSpan{
						Line:   s.Line,
						Column: s.Column,
						Length: len(s.TokenText()) + 1,
						Offset: s.Offset,
					},
				})
			case '-':
				tokens = append(tokens, l.readToken(DoubleDash, s))
			default:
				return nil, &diagnostic.Issue{
					Severity: diagnostic.SeverityError,
					Message:  fmt.Sprintf("Unexpected token '-' at line %d, column %d", s.Line, s.Column),
					Category: diagnostic.CategoryLexer,
				}
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
	case DoubleDash:
		value = "--"
	default:
		value = s.TokenText()
	}

	return Token{
		Type:  tokenType,
		Value: value,
		SourceSpan: diagnostic.SourceSpan{
			Line:   s.Line,
			Column: s.Column,
			Length: len(s.TokenText()),
			Offset: s.Offset,
		},
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
	case "float":
		return Float
	case "fn":
		return Function
	case "for":
		return For
	case "if":
		return If
	case "int":
		return Int
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
	case "str":
		return String
	case "use":
		return Use
	default:
		return Identifier
	}
}
