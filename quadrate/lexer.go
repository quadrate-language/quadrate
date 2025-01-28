package quadrate

type Lexer struct {
	ch       byte
	column   int
	line     int
	position int
	source   string
}

func NewLexer(source string) *Lexer {
	l := &Lexer{
		source: source,
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
			t := NewToken(NEW_LINE, "", l.line, l.column)
			tokens = append(tokens, t)
			l.line++
			l.column = -1
		case '(':
			t := NewToken(PARAN_LEFT, "(", l.line, l.column)
			tokens = append(tokens, t)
		case ')':
			t := NewToken(PARAN_RIGHT, ")", l.line, l.column)
			tokens = append(tokens, t)
		case '{':
			t := NewToken(BRACKET_LEFT, "{", l.line, l.column)
			tokens = append(tokens, t)
		case '}':
			t := NewToken(BRACKET_RIGHT, "}", l.line, l.column)
			tokens = append(tokens, t)
		case 0:
			t := NewToken(EOF, "", 0, 0)
			tokens = append(tokens, t)
			return tokens
		default:
			if isLetter(l.ch) {
				line := l.line
				column := l.column
				literal := l.readIdentifier()
				tokenType := l.lookupIdentifier(literal)
				t := NewToken(tokenType, literal, line, column)
				tokens = append(tokens, t)
				continue
			} else if isDigit(l.ch) {
				//				literal := l.readNumber()
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

	if l.ch == '-' {
		l.readChar()
		if !isDigit(l.ch) && l.ch != '.' {
			return ""
		}
	}

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

	ch := l.peek()
	for isLetter(ch) || isDigit(ch) || ch == '_' {
		//ch = l.peek()
		l.readChar()
		ch = l.ch
	}
	return l.source[start:l.position]
}

func (l *Lexer) lookupIdentifier(ident string) TokenType {
	switch ident {
	case "use":
		return USE
	case "fn":
		return FUNCTION
	default:
		return IDENTIFIER
	}
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
