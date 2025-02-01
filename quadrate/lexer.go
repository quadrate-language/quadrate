package quadrate

import (
	"fmt"
	"os"
	"path/filepath"
)

type Lexer struct {
	ch       byte
	column   int
	filename string
	line     int
	position int
	source   string
	Modules  []string
}

type LexResult struct {
	Tokens   []Token
	Filename string
}

func NewLexer(filename string, source []byte) *Lexer {
	l := &Lexer{
		filename: filename,
		source:   string(source),
	}
	if module, err := filepath.Abs(filename); err == nil {
		l.Modules = append(l.Modules, module)
	} else {
		panic(err)
	}
	return l
}

func (l *Lexer) Lex() LexResult {
	r := LexResult{
		Filename: l.filename,
	}
	l.column = 0
	l.line = 1
	l.position = -1
	l.readChar()

	for {
		l.skipWhitespace()

		switch l.ch {
		case '\n':
			t := NewToken(NewLine, "RET", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
			l.line++
			l.column = -1
		case '(':
			t := NewToken(ParanthesisLeft, "(", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case ')':
			t := NewToken(ParanthesisRight, ")", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case '{':
			t := NewToken(CurlyBracketLeft, "{", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case '}':
			t := NewToken(CurlyBracketRight, "}", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case '[':
			t := NewToken(SquareBracketLeft, "[", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case ']':
			t := NewToken(SquareBracketRight, "]", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case 0:
			t := NewToken(EOF, "EOF", 0, 0)
			r.Tokens = append(r.Tokens, t)
			return r
		default:
			if isLetter(l.ch) {
				line := l.line
				column := l.column
				literal := l.readIdentifier()
				t := NewToken(l.lookupIdentifier(literal), literal, line, column)
				if len(r.Tokens) > 0 && t.Type == Identifier && r.Tokens[len(r.Tokens)-1].Type == Use {
					if _, err := os.Stat(literal + ".qd"); err == nil {
						fmt.Println("found module:", literal+".qd")
					} else if _, err := os.Stat(literal + "/module.qd"); err == nil {
						fmt.Println("found module:", literal+"/module.qd")
					} else {
						panic("module not found")
					}
					r.Tokens = r.Tokens[:len(r.Tokens)-1]
				} else {
					r.Tokens = append(r.Tokens, t)
					continue
				}
			} else if isDigit(l.ch) || (l.ch == '-' && isDigit(l.peek())) {
				line := l.line
				column := l.column
				literal := l.readNumber()
				t := NewToken(NumericConstant, literal, line, column)
				r.Tokens = append(r.Tokens, t)
				continue
			} else if l.ch == ':' && l.peek() == ':' {
				t := NewToken(DoubleColon, "::", l.line, l.column)
				r.Tokens = append(r.Tokens, t)
				l.readChar()
			} else {
				t := NewToken(Illegal, string(l.ch), l.line, l.column)
				r.Tokens = append(r.Tokens, t)
			}
		}
		l.readChar()
	}
}

func (l *Lexer) skipComment() {
	if l.ch == '/' && l.peek() == '/' {
		l.readChar()
		l.readChar()
		for l.ch != '\n' && l.ch != 0 {
			l.readChar()
		}
	}
}

func (l *Lexer) lookupIdentifier(i string) TokenType {
	switch i {
	case "use":
		return Use
	case "fn":
		return Function
	case "__c":
		return InlineC
	}
	return Identifier
}

func (l *Lexer) skipWhitespace() {
	for l.ch == ' ' || l.ch == '\t' || l.ch == '\r' || (l.ch == '/' && l.peek() == '/') {
		if l.ch == '/' && l.peek() == '/' {
			l.skipComment()
		} else {
			l.readChar()
		}
	}
}

func (l *Lexer) readNumber() string {
	start := l.position

	if l.position > 0 && l.source[l.position-1] == '-' {
		if !isDigit(l.ch) && l.ch != '.' {
			return ""
		}
		start--
	}

	l.readChar()
	for isDigit(l.ch) {
		l.readChar()
	}

	if l.ch == '.' {
		l.readChar()
		for isDigit(l.ch) {
			l.readChar()
		}
	}
	return l.source[start:l.position]
}

func (l *Lexer) readIdentifier() string {
	start := l.position

	l.readChar()
	for isLetter(l.ch) || isDigit(l.ch) || l.ch == '_' {
		l.readChar()
	}
	return l.source[start:l.position]
}

func (l *Lexer) peek() byte {
	if l.position >= len(l.source)-1 {
		return 0
	}
	return l.source[l.position+1]
}

func (l *Lexer) readChar() {
	l.ch = l.peek()
	l.position++
	l.column++
}

func isLetter(ch byte) bool {
	return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_'
}

func isDigit(ch byte) bool {
	return '0' <= ch && ch <= '9'
}
