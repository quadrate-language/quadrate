package quadrate

import "fmt"

type Parser struct {
	tokens   *[]Token
	current  int
	filename string
}

type Node interface{}

type Program struct {
	Statements []Node
}

type ImportDirective struct {
	Module string
}

type FunctionDeclaration struct {
	ReturnType string
	Name       string
}

type FunctionCall struct {
	Name string
	Args []string
}

type Scope struct {
	Statements []Node
}

func NewParser(filename string, tokens *[]Token) *Parser {
	return &Parser{
		filename: filename,
		tokens:   tokens,
	}
}

func (p *Parser) Parse() (*Program, *SyntaxError) {
	var pgm Program

	for p.current < len(*p.tokens) {
		t := (*p.tokens)[p.current]

		switch t.Type {
		case NewLine:
			p.current++
		case Use:
			if n, err := p.parseUse(); err != nil {
				return nil, err
			} else {
				pgm.Statements = append(pgm.Statements, n)
			}
		case Function:
			if n, err := p.parseFunction(); err != nil {
				return nil, err
			} else {
				pgm.Statements = append(pgm.Statements, n)
				t = (*p.tokens)[p.current]
				if t.Type != CurlyBracketLeft {
					var message string
					switch t.Type {
					case NumericConstant:
						message = "expected ‘{‘ before numeric constant"
					default:
						message = fmt.Sprintf("expected ‘{‘ before ‘%s‘", t.Literal)
					}
					return nil, &SyntaxError{
						Message:  message,
						Line:     t.Line,
						Column:   t.Column,
						Filename: p.filename,
					}
				} else {
					p.current++
					pgm.Statements = append(pgm.Statements, Scope{})
				}
			}
		default:
			p.current++
		}
	}
	return &pgm, nil
}

func (p *Parser) parseStatement() (Node, *SyntaxError) {
	t := (*p.tokens)[p.current]

	switch t.Type {
	case Use:
		return p.parseUse()
	case Function:
		return p.parseFunction()
	}
	return nil, nil
}

func (p *Parser) parseScope() (Node, error) {
	return nil, nil
}

func (p *Parser) parseUse() (Node, *SyntaxError) {
	p.current++
	t := (*p.tokens)[p.current]
	if t.Type != Identifier {
		return nil, &SyntaxError{
			Message:  "expected identifier after ‘use‘",
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}
	p.current++
	return ImportDirective{
		Module: t.Literal,
	}, nil
}

func (p *Parser) parseFunction() (Node, *SyntaxError) {
	p.current++
	t := (*p.tokens)[p.current]
	if t.Type != Identifier {
		return nil, &SyntaxError{
			Message:  "expected identifier after ‘fn‘",
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}

	fd := FunctionDeclaration{
		Name: t.Literal,
	}

	if fd.Name == "main" {
		fd.ReturnType = "int"
	} else {
		fd.ReturnType = "void"
	}

	p.current++

	return fd, nil
}
