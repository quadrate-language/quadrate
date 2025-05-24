package lexer

import (
	"errors"
	"fmt"
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

	if l.source == nil || len(l.source) == 0 {
		return tokens, errors.New("source is empty or nil")
	}

	l.cursor = 0
	c := l.cursorChar()
	var err error

itr:
	for {
		fmt.Printf("Current cursor position: %d, character: '%c'\n", l.cursor, c)

		switch c {
		case ' ':
			break
		case '\t':
			l.column += 3
		case '\n':
			tokens = append(tokens, l.readEOL())
			l.column = 0
			l.line++
		case '(':
			tokens = append(tokens, l.readPunctuation(LParen, 1))
		case ')':
			tokens = append(tokens, l.readPunctuation(RParen, 1))
		case '{':
			tokens = append(tokens, l.readPunctuation(LBrace, 1))
		case '}':
			tokens = append(tokens, l.readPunctuation(RBrace, 1))
		default:
			if l.isDigit(c) || (c == '-' && l.isDigit(l.peekChar())) {
				if t, err := l.readNumber(); err != nil {
					return nil, err
				} else {
					tokens = append(tokens, t)
				}
			} else if l.isLetter(c) {
				if t, err := l.readIdentifier(); err != nil {
					return nil, err
				} else {
					tokens = append(tokens, t)
				}
			}
		}

		if c, err = l.read(); err != nil {
			tokens = append(tokens, l.readEOF())
			break itr
		}
	}

	return tokens, nil
}

func (l *Scanner) isLetter(c rune) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
}

func (l *Scanner) isDigit(c rune) bool {
	return c >= '0' && c <= '9'
}

func (l *Scanner) cursorChar() rune {
	if l.cursor < 0 || l.cursor >= len(l.source) {
		panic("cursor out of bounds")
	}
	return l.source[l.cursor]
}

func (l *Scanner) peekChar() rune {
	if l.cursor+1 < len(l.source) {
		return l.source[l.cursor+1]
	}
	return 0
}

func (l *Scanner) readPunctuation(tokenType TokenType, length int) Token {
	return Token{
		Type:   tokenType,
		Value:  string(l.cursorChar()),
		Line:   l.line,
		Column: l.column,
		Length: length,
		Offset: l.cursor,
	}
}

func (l *Scanner) readIdentifier() (Token, error) {
	value := ""
	line := l.line
	column := l.column

	for l.cursor < len(l.source) && (l.isLetter(l.cursorChar()) || l.isDigit(l.cursorChar())) {
		value += string(l.cursorChar())
		if _, err := l.read(); err != nil {
			return Token{}, err
		}
	}

	l.cursor--
	l.column--

	return Token{
		Type:   l.lookupType(value),
		Value:  value,
		Line:   line,
		Column: column,
		Length: len(value),
		Offset: l.cursor + 1 - len(value),
	}, nil
}

func (l *Scanner) lookupType(literal string) TokenType {
	switch literal {
	case "fn":
		return Function
	default:
		return Identifier
	}
}

func (l *Scanner) readNumber() (Token, error) {
	value := ""
	line := l.line
	column := l.column

	if l.cursorChar() == '0' && l.peekChar() == 'x' {
		value += "0x"
		for range 2 {
			if _, err := l.read(); err != nil {
				return Token{}, err
			}
		}
		for l.cursor < len(l.source) && (l.cursorChar() >= '0' && l.cursorChar() <= '9' || l.cursorChar() >= 'a' && l.cursorChar() <= 'f' || l.cursorChar() >= 'A' && l.cursorChar() <= 'F') {
			value += string(l.cursorChar())
			if _, err := l.read(); err != nil {
				return Token{}, err
			}
		}
		l.cursor--
		l.column--
		return Token{
			Type:   HexNumber,
			Value:  value,
			Line:   line,
			Column: column,
			Length: len(value),
			Offset: l.cursor - len(value),
		}, nil
	}

	if l.cursorChar() == '-' {
		value += "-"
		if _, err := l.read(); err != nil {
			return Token{}, err
		}
		if l.cursor >= len(l.source) || !l.isDigit(l.cursorChar()) {
			return Token{}, errors.New("invalid number format")
		}
	}

	if l.cursorChar() == '.' {
		return Token{}, errors.New("invalid number format: unexpected decimal point at the start")
	}

	if !l.isDigit(l.cursorChar()) {
		return Token{}, errors.New("invalid number format: expected digit at the start")
	}

	value += string(l.cursorChar())
	if _, err := l.read(); err != nil {
		return Token{}, err
	}

	if l.cursor < len(l.source) && l.cursorChar() == '.' || l.cursorChar() == 'e' || l.cursorChar() == 'E' {
		value += string(l.cursorChar())
		if _, err := l.read(); err != nil {
			return Token{}, err
		}
		if l.cursor >= len(l.source) || !l.isDigit(l.cursorChar()) {
			return Token{}, errors.New("invalid number format: expected digit after decimal point")
		}
	}
	l.cursor--
	l.column--
	return Token{
		Type:   Number,
		Value:  value,
		Line:   line,
		Column: column,
		Length: len(value),
		Offset: l.cursor + 1 - len(value),
	}, nil
}

func (l *Scanner) readEOL() Token {
	return Token{
		Type:   EOL,
		Value:  "EOL",
		Line:   l.line,
		Column: l.column,
		Length: 1,
		Offset: l.cursor,
	}
}

func (l *Scanner) readEOF() Token {
	return Token{
		Type:   EOF,
		Value:  "EOF",
		Line:   l.line,
		Column: l.column,
		Length: 0,
		Offset: l.cursor,
	}
}

func (l *Scanner) read() (rune, error) {
	l.cursor++
	l.column++
	if l.cursor >= len(l.source) {
		l.cursor = len(l.source) - 1
		return 0, errors.New("end of source reached")
	}
	return l.cursorChar(), nil
}
