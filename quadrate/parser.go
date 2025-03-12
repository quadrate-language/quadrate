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

type Node any

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

type ConstValue struct {
	Name  string
	Value string
}

type Body struct {
	Statements []Node
}

type ReturnStatement struct {
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
				Column:   t.Column,
				Filename: p.filename,
			}
		case NumericConstant:
			message := fmt.Sprintf("unexpected numeric constant ‘%s‘", t.Literal)
			return nil, &SyntaxError{
				Message:  message,
				Line:     t.Line,
				Column:   t.Column,
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
		case Const:
			if c, err := p.parseConstValue(); err != nil {
				return nil, err
			} else {
				pgm.Statements = append(pgm.Statements, c)
			}
			p.current++
		case EOF:
			p.current++
		case BeginScopeComment:
			if err := p.parseScopeComment(); err != nil {
				return nil, err
			}
			p.current++
		default:
			return nil, &SyntaxError{
				Message:  fmt.Sprintf("unexpected token ‘%s‘", t.Literal),
				Line:     t.Line,
				Column:   t.Column + 1,
				Filename: p.filename,
			}
		}
	}
	return &pgm, nil
}

func (p *Parser) parseConstValue() (Node, *SyntaxError) {
	p.current++
	t := (*p.tokens)[p.current]
	c := ConstValue{
		Name: t.Literal,
	}
	p.current++
	t = (*p.tokens)[p.current]
	if t.Type != NumericConstant {
		return nil, &SyntaxError{
			Message:  fmt.Sprintf("expected numeric constant but got ‘%s‘", t.Literal),
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}
	c.Value = t.Literal
	return c, nil
}

func (p *Parser) parseBody() (Node, *SyntaxError) {
	t := (*p.tokens)[p.current]
	var stmts []Node
	var deferStmts []Node

body_loop:
	for p.current < len(*p.tokens) {
		p.current++
		t = (*p.tokens)[p.current]
		switch t.Type {
		case CurlyBracketLeft:
			return nil, &SyntaxError{
				Message:  "unexpected ‘{‘",
				Line:     t.Line,
				Column:   t.Column,
				Filename: p.filename,
			}
		case CurlyBracketRight:
			p.current++
			break body_loop
		case NewLine:
			continue
		case InlineC:
			stmts = append(stmts, InlineCCode{
				Code: t.Literal,
			})
		case BeginScopeComment:
			if err := p.parseScopeComment(); err != nil {
				return nil, err
			}
		case Const:
			return nil, &SyntaxError{
				Message:  "unexpected ‘const‘",
				Line:     t.Line,
				Column:   t.Column,
				Filename: p.filename,
			}
		case Defer:
			if n, err := p.parseDefer(); err != nil {
				return nil, err
			} else {
				deferStmts = append(deferStmts, n)
			}
		case Return:
			for _, stmt := range deferStmts {
				stmts = append(stmts, stmt)
			}
			stmts = append(stmts, ReturnStatement{})
		default:
			if fnCall, err := p.parseFunctionCall(); err != nil {
				return nil, err
			} else {
				stmts = append(stmts, fnCall)
			}
		}
	}
	for _, stmt := range deferStmts {
		stmts = append(stmts, stmt)
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
		} else if t.Type == NumericConstant {
			functionCall.Args = append(functionCall.Args, t.Literal)
		} else if t.Type == Identifier {
			if p.peek() == DoubleColon {
				m := t.Literal
				p.current++
				p.current++
				t := (*p.tokens)[p.current]
				functionCall.Args = append(functionCall.Args, fmt.Sprintf("__qd_%s_%s", m, t.Literal))
			} else {
				functionCall.Args = append(functionCall.Args, fmt.Sprintf("__qd_%s", t.Literal))
			}
		} else if t.Type == DoubleColon {
			p.current++
			t = (*p.tokens)[p.current]
			if t.Type == Identifier {
				functionCall.Name += "_" + t.Literal
			}
		} else if t.Type == String {
			functionCall.Args = append(functionCall.Args, t.Literal)
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

func (p *Parser) peek() TokenType {
	if p.current >= len(*p.tokens)-1 {
		return EOF
	}
	return (*p.tokens)[p.current+1].Type
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

func (p *Parser) parseDefer() (Node, *SyntaxError) {
	p.current++
	if n, err := p.parseFunctionCall(); err != nil {
		return nil, err
	} else {
		return n, nil
	}
}

func (p *Parser) parseScopeComment() *SyntaxError {
	begin := p.current
	p.current++
	for p.current < len(*p.tokens) {
		t := (*p.tokens)[p.current]
		if t.Type == EndScopeComment {
			return nil
		}
		p.current++
	}
	return &SyntaxError{
		Message:  "unterminated comment",
		Line:     (*p.tokens)[begin].Line,
		Column:   (*p.tokens)[begin].Column,
		Filename: p.filename,
	}
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
