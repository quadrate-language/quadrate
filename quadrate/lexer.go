package quadrate

type Lexer struct {
	ch       byte
	column   int
	line     int
	position int
	source   string
}

func NewLexer(source []byte) *Lexer {
	l := &Lexer{
		source: string(source),
	}
	return l
}

func (l *Lexer) Lex() []Token {
	var tokens []Token
	l.column = 0
	l.line = 1
	l.position = -1
	l.readChar()

	for {
		l.skipWhitespace()

		switch l.ch {
		case '\n':
			t := NewToken(NewLine, "RET", l.line, l.column)
			tokens = append(tokens, t)
			l.line++
			l.column = -1
		case '(':
			t := NewToken(ParanthesisLeft, "(", l.line, l.column)
			tokens = append(tokens, t)
		case ')':
			t := NewToken(ParanthesisRight, ")", l.line, l.column)
			tokens = append(tokens, t)
		case '{':
			t := NewToken(CurlyBracketLeft, "{", l.line, l.column)
			tokens = append(tokens, t)
		case '}':
			t := NewToken(CurlyBracketRight, "}", l.line, l.column)
			tokens = append(tokens, t)
		case '[':
			t := NewToken(SquareBracketLeft, "[", l.line, l.column)
			tokens = append(tokens, t)
		case ']':
			t := NewToken(SquareBracketRight, "]", l.line, l.column)
			tokens = append(tokens, t)
		case 0:
			t := NewToken(EOF, "EOF", 0, 0)
			tokens = append(tokens, t)
			return tokens
		default:
			if isLetter(l.ch) {
				line := l.line
				column := l.column
				literal := l.readIdentifier()
				t := NewToken(l.lookupIdentifier(literal), literal, line, column)
				tokens = append(tokens, t)
				continue
			} else if isDigit(l.ch) {
				line := l.line
				column := l.column
				literal := l.readNumber()
				t := NewToken(NumericConstant, literal, line, column)
				tokens = append(tokens, t)
				continue
			} else {
				t := NewToken(Illegal, string(l.ch), l.line, l.column)
				tokens = append(tokens, t)
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
