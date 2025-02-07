package quadrate

import (
	"fmt"
	"path/filepath"
)

type Parser struct {
	tokens   *[]Token
	current  int
	filename string
}

type Node interface{}

type ProgramModule struct {
	Name       string
	Statements []Node
	Submodules []ImportDirective
}

type ImportDirective struct {
	Module string
	Name   string
}

type Parameter struct {
	Type string
	Name string
}

type FunctionDeclaration struct {
	ReturnType string
	Name       string
	Parameters []Parameter
}

type FunctionCall struct {
	Name     string
	PushArgs []string
	Args     []string
}

type InlineCCode struct {
	Code string
}

type Body struct {
	Statements []Node
}

func NewParser(filename string, tokens *[]Token) *Parser {
	return &Parser{
		filename: filename,
		tokens:   tokens,
	}
}

func (p *Parser) Parse() (*ProgramModule, *SyntaxError) {
	var pgm ProgramModule

	for p.current < len(*p.tokens) {
		t := (*p.tokens)[p.current]

		switch t.Type {
		case Identifier:
			message := fmt.Sprintf("unexpected identifier ‘%s‘", t.Literal)
			return nil, &SyntaxError{
				Message:  message,
				Line:     t.Line,
				Column:   t.Column + 1,
				Filename: p.filename,
			}
		case NumericConstant:
			message := fmt.Sprintf("unexpected numeric constant ‘%s‘", t.Literal)
			return nil, &SyntaxError{
				Message:  message,
				Line:     t.Line,
				Column:   t.Column + 1,
				Filename: p.filename,
			}
		case NewLine:
			p.current++
		case Use:
			if n, err := p.parseUse(); err != nil {
				return nil, err
			} else {
				pgm.Submodules = append(pgm.Submodules, n.(ImportDirective))
				pgm.Statements = append(pgm.Statements, n)
			}
		case FnSignature:
			if n, err := p.parseFnSignature(); err != nil {
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
					if n, err := p.parseBody(); err != nil {
						return nil, err
					} else {
						pgm.Statements = append(pgm.Statements, n)
					}
				}
			}
		case InlineC:
			pgm.Statements = append(pgm.Statements, InlineCCode{
				Code: t.Literal,
			})
			p.current++
		case EOF:
			p.current++
		default:
			return nil, &SyntaxError{
				Message:  fmt.Sprintf("unexpected token ‘%s‘", t.Literal),
				Line:     t.Line,
				Column:   t.Column + 1,
				Filename: p.filename,
			}
			//p.current++
		}
	}
	return &pgm, nil
}

func (p *Parser) parseStatement() (Node, *SyntaxError) {
	t := (*p.tokens)[p.current]

	switch t.Type {
	case Use:
		return p.parseUse()
	case FnSignature:
		return p.parseFnSignature()
	}
	return nil, nil
}

func (p *Parser) parseBody() (Node, *SyntaxError) {
	t := (*p.tokens)[p.current]
	var stmts []Node

body_loop:
	for p.current < len(*p.tokens) {
		p.current++
		t = (*p.tokens)[p.current]
		switch t.Type {
		case CurlyBracketRight:
			p.current++
			break body_loop
		case NewLine:
			continue
		case InlineC:
			stmts = append(stmts, InlineCCode{
				Code: t.Literal,
			})
		default:
			if fnCall, err := p.parseFunctionCall(); err != nil {
				return nil, err
			} else {
				stmts = append(stmts, fnCall)
			}
		}
	}

	return Body{
		Statements: stmts,
	}, nil
}

func (p *Parser) parseFunctionCall() (Node, *SyntaxError) {
	functionCall := FunctionCall{
		Name: (*p.tokens)[p.current].Literal,
	}
	if p.current >= len(*p.tokens)-1 {
		return nil, &SyntaxError{
			Message:  "expected declaration or statement at end of input",
			Line:     (*p.tokens)[p.current].Line,
			Column:   (*p.tokens)[p.current].Column,
			Filename: p.filename,
		}
	}
	p.current++
	for p.current < len(*p.tokens)-1 {
		t := (*p.tokens)[p.current]
		if t.Type == NewLine {
			break
		} else if t.Type == Identifier || t.Type == NumericConstant {
			functionCall.Args = append(functionCall.Args, t.Literal)
		} else if t.Type == SquareBracketLeft {
			t = (*p.tokens)[p.current]
			for p.current < len(*p.tokens) {
				p.current++
				t = (*p.tokens)[p.current]
				if t.Type == SquareBracketRight {
					break
				} else if t.Type == NumericConstant {
					functionCall.PushArgs = append(functionCall.PushArgs, t.Literal)
				} else {
					return nil, &SyntaxError{
						Message:  fmt.Sprintf("expected numeric constant but got ‘%s‘", t.Literal),
						Line:     t.Line,
						Column:   t.Column,
						Filename: p.filename,
					}
				}
			}
		} else if t.Type == DoubleColon {
			p.current++
			t = (*p.tokens)[p.current]
			if t.Type == Identifier {
				functionCall.Name += "_" + t.Literal
			}
		} else {
			return nil, &SyntaxError{
				Message:  fmt.Sprintf("expected identifier but got ‘%s‘", t.Literal),
				Line:     t.Line,
				Column:   t.Column,
				Filename: p.filename,
			}
		}
		p.current++
	}
	return functionCall, nil
}

func (p *Parser) parseUse() (Node, *SyntaxError) {
	p.current++
	t := (*p.tokens)[p.current]
	if t.Type != Module {
		return nil, &SyntaxError{
			Message:  "expected identifier after ‘use‘",
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}
	name := filepath.Base(filepath.Dir(t.Literal))
	p.current++
	return ImportDirective{
		Module: t.Literal,
		Name:   name,
	}, nil
}

func (p *Parser) parseFnSignature() (Node, *SyntaxError) {
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

	t = (*p.tokens)[p.current]
	if t.Type != ParenthesisLeft {
		return nil, &SyntaxError{
			Message:  fmt.Sprintf("expected ‘(‘ but got ‘%s‘", t.Literal),
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}

	for p.current < len(*p.tokens) {
		p.current++
		t = (*p.tokens)[p.current]
		if t.Type == ParenthesisRight {
			p.current++
			break
		} else if t.Type == Identifier {
			fd.Parameters = append(fd.Parameters, Parameter{
				Type: "real",
				Name: t.Literal,
			})
		} else if t.Type == Comma {
		} else {
			return nil, &SyntaxError{
				Message:  fmt.Sprintf("expected identifier but got ‘%s‘", t.Literal),
				Line:     t.Line,
				Column:   t.Column,
				Filename: p.filename,
			}
		}
	}
	return fd, nil
}
