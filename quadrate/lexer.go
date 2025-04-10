package quadrate

import (
	"os"
	"path/filepath"
	"strings"
)

type Lexer struct {
	ch       byte
	column   int
	filename string
	line     int
	name     string
	position int
	source   string
	Modules  []string
}

type LexResult struct {
	Tokens   []Token
	Filename string
}

func NewLexer(filename string, source []byte, name string) *Lexer {
	l := &Lexer{
		filename: filename,
		name:     name,
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
			t := NewToken(ParenthesisLeft, "(", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case ')':
			t := NewToken(ParenthesisRight, ")", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case '{':
			t := NewToken(CurlyBracketLeft, "{", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case '}':
			t := NewToken(CurlyBracketRight, "}", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case ',':
			t := NewToken(Comma, ",", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case '&':
			t := NewToken(Pointer, "&", l.line, l.column)
			r.Tokens = append(r.Tokens, t)
		case 0:
			t := NewToken(EOF, "EOF", 0, 0)
			r.Tokens = append(r.Tokens, t)
			return r
		case '"':
			t := NewToken(String, l.readString(), l.line, l.column)

			if len(r.Tokens) > 0 && r.Tokens[len(r.Tokens)-1].Type == Use {
				literal := strings.ReplaceAll(t.Literal, "\"", "")
				var path string
				if strings.HasPrefix(literal, "/") {
					path = ""
				} else {
					path = filepath.Dir(l.filename) + "/"
				}
				if _, err := os.Stat(path + literal + ".qd"); err == nil {
					l.Modules = append(l.Modules, path+literal+".qd")
					t := NewToken(Module, path+literal+".qd", l.line, l.column)
					r.Tokens = append(r.Tokens, t)
				} else if _, err := os.Stat(path + literal + "/module.qd"); err == nil {
					l.Modules = append(l.Modules, path+literal+"/module.qd")
					t := NewToken(Module, path+literal+"/module.qd", l.line, l.column)
					r.Tokens = append(r.Tokens, t)
				} else {
					path := os.Getenv("QUADRATE_ROOT")
					if path == "" {
						path = filepath.Join(os.Getenv("HOME"), "quadrate")
					}
					for p := range strings.SplitSeq(path, string(os.PathListSeparator)) {
						modPath := filepath.Join(p, literal, "module.qd")
						if _, err := os.Stat(modPath); err == nil {
							l.Modules = append(l.Modules, modPath)
							t := NewToken(Module, modPath, l.line, l.column)
							r.Tokens = append(r.Tokens, t)
							break
						}
					}
				}
			} else {
				r.Tokens = append(r.Tokens, t)
			}
		default:
			if isLetter(l.ch) {
				line := l.line
				column := l.column
				literal := l.readIdentifier()
				tokenType := l.lookupIdentifier(literal)
				if tokenType == InlineC {
					c := l.readCBlock()
					t := NewToken(InlineC, c, line, column)
					r.Tokens = append(r.Tokens, t)
					l.readChar()
					continue
				}
				if tokenType == Jump ||
					tokenType == JumpEqual ||
					tokenType == JumpGreater ||
					tokenType == JumpGreaterEqual ||
					tokenType == JumpLesser ||
					tokenType == JumpLesserEqual ||
					tokenType == JumpNotEqual ||
					tokenType == JumpNotZero ||
					tokenType == JumpZero {
					t := NewToken(tokenType, literal, line, column)
					r.Tokens = append(r.Tokens, t)
					continue
				}

				t := NewToken(tokenType, literal, line, column)
				if len(r.Tokens) > 0 && t.Type == Identifier && r.Tokens[len(r.Tokens)-1].Type == Use {
					path := filepath.Dir(l.filename)
					if _, err := os.Stat(path + "/" + literal + ".qd"); err == nil {
						l.Modules = append(l.Modules, path+"/"+literal+".qd")
						t := NewToken(Module, path+"/"+literal+".qd", line, column)
						r.Tokens = append(r.Tokens, t)
					} else if _, err := os.Stat(path + "/" + literal + "/module.qd"); err == nil {
						l.Modules = append(l.Modules, path+"/"+literal+"/module.qd")
						t := NewToken(Module, path+"/"+literal+"/module.qd", line, column)
						r.Tokens = append(r.Tokens, t)
					} else {
						path := os.Getenv("QUADRATE_ROOT")
						if path == "" {
							path = filepath.Join(os.Getenv("HOME"), "quadrate")
						}
						for p := range strings.SplitSeq(path, string(os.PathListSeparator)) {
							modPath := filepath.Join(p, literal, "module.qd")
							if _, err := os.Stat(modPath); err == nil {
								l.Modules = append(l.Modules, modPath)
								t := NewToken(Module, modPath, line, column)
								r.Tokens = append(r.Tokens, t)
								break
							}
						}
					}
					continue
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
			} else if l.ch == ':' {
				if l.peek() == ':' {
					t := NewToken(DoubleColon, "::", l.line, l.column)
					r.Tokens = append(r.Tokens, t)
					l.readChar()
				} else {
					t := NewToken(Colon, ":", l.line, l.column)
					r.Tokens = append(r.Tokens, t)
				}
			} else if l.ch == '/' && l.peek() == '*' {
				t := NewToken(BeginScopeComment, "/*", l.line, l.column)
				r.Tokens = append(r.Tokens, t)
				if l.skipBlockComment() {
					t := NewToken(EndScopeComment, "*/", l.line, l.column-2)
					r.Tokens = append(r.Tokens, t)
				}
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

func (l *Lexer) skipBlockComment() bool {
	if l.ch == '/' && l.peek() == '*' {
		l.readChar()
		l.readChar()
		for l.ch != 0 {
			if l.ch == '*' && l.peek() == '/' {
				l.readChar()
				l.readChar()
				if l.ch == '\n' {
					l.line++
				}
				return true
			} else if l.ch == '\n' {
				l.line++
				l.readChar()
			} else {
				l.readChar()
			}
		}
	}
	return false
}

func (l *Lexer) lookupIdentifier(i string) TokenType {
	switch i {
	case "use":
		return Use
	case "fn":
		return FnSignature
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
	case "for":
		return For
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
	case "reduce":
		return Reduce
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

	if l.ch == 'e' || l.ch == 'E' {
		l.readChar()

		if l.ch == '+' || l.ch == '-' {
			l.readChar()
		}

		if !isDigit(l.ch) {
			return l.source[start:l.position] // Return what we got so far
		}

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

func (l *Lexer) readString() string {
	l.readChar()
	start := l.position
	for l.ch != '"' && l.ch != 0 {
		l.readChar()
	}
	return "\"" + l.source[start:l.position] + "\""
}

func (l *Lexer) readCBlock() string {
	var c strings.Builder
	brackets := 0
	for l.ch != 0 {
		if l.ch == '{' {
			brackets++
			if brackets == 1 {
				l.readChar()
				continue
			}
		} else if l.ch == '}' {
			brackets--
			if brackets == 0 {
				break
			}
		}
		c.WriteByte(l.ch)
		l.readChar()
	}
	return c.String()
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
