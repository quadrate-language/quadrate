package quadrate2

import "errors"

type Lexer struct {
	filename string
	source   []rune
	cursor   int
	line     int
	column   int
}

type LexResult struct {
	Filename string  `json:"filename"`
	Tokens   []Token `json:"tokens"`
}

func NewLexer(filename string, source []rune) *Lexer {
	return &Lexer{
		filename: filename,
		source:   source,
		cursor:   0,
		line:     1,
		column:   1,
	}
}

func (l *Lexer) Lex() (LexResult, error) {
	result := LexResult{
		Filename: l.filename,
		Tokens:   []Token{},
	}

	if l.source == nil || len(l.source) == 0 {
		return result, errors.New("source is empty or nil")
	}

	l.cursor = 0
	c := l.cursorChar()
	var err error

	for {
		l.skipWhitespaces()

		switch c {
		case '\n':
			result.Tokens = append(result.Tokens, l.readEOL())
		}

		if c, err = l.read(); err != nil {
			// End of source reached
			result.Tokens = append(result.Tokens, Token{
				Type:      EOF2,
				Value:     "",
				Line:      l.line,
				Character: l.column,
				Length:    0,
				Offset:    l.cursor,
			})
			break
		}
	}

	return result, nil
}

func (l *Lexer) skipWhitespaces() error {
	c := l.cursorChar()
	var err error
	for {
		if c != ' ' && c != '\t' {
			break
		}
		if c, err = l.read(); err != nil {
			return err
		}
	}
	return nil
}

func (l *Lexer) cursorChar() rune {
	if l.cursor < 0 || l.cursor >= len(l.source) {
		panic("cursor out of bounds")
	}
	return l.source[l.cursor]
}

func (l *Lexer) readEOL() Token {
	return Token{
		Type:      EOL,
		Value:     "",
		Line:      l.line,
		Character: l.column,
		Length:    1,
		Offset:    l.cursor,
	}
}

func (l *Lexer) read() (rune, error) {
	l.cursor++
	l.column++
	if l.cursor >= len(l.source) {
		l.cursor = len(l.source) - 1
		return 0, errors.New("end of source reached")
	}
	return l.cursorChar(), nil
}
