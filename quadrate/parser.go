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

type LocalValue struct {
	Name string
}

type Body struct {
	Statements []Node
}

type EndStatement struct {
}

type ReturnStatement struct {
}

type ContinueStatement struct {
}

type BreakStatement struct {
}

type IteratorStatement struct {
}

type Label struct {
	Name string
}

type Jmp struct {
	Label string
}

type ReduceStmt struct {
	Identifier string
}

type ForLoop struct {
	Start string
	Step  string
	End   string
}

type LoopLoop struct {
}

type Je Jmp
type Jge Jmp
type Jg Jmp
type Jle Jmp
type Jl Jmp
type Jne Jmp
type Jnz Jmp
type Jz Jmp
type Jgez Jmp
type Jlez Jmp
type Jlz Jmp
type Jgz Jmp

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

	loopDepth := 0

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
		case For:
			loopDepth++
			if n, err := p.parseForLoop(); err != nil {
				return nil, err
			} else {
				stmts = append(stmts, n)
			}
		case Loop:
			loopDepth++
			stmts = append(stmts, LoopLoop{})
		case NewLine:
			continue
		case Iterator:
			if loopDepth <= 0 {
				return nil, &SyntaxError{
					Message:  "unexpected ‘$‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
			stmts = append(stmts, IteratorStatement{})
		case Break:
			if loopDepth <= 0 {
				return nil, &SyntaxError{
					Message:  "unexpected ‘break‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
			stmts = append(stmts, BreakStatement{})
		case Continue:
			if loopDepth <= 0 {
				return nil, &SyntaxError{
					Message:  "unexpected ‘continue‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
			stmts = append(stmts, ContinueStatement{})
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
		case Reduce:
			if p.peek() == Identifier {
				p.current++
				literal := (*p.tokens)[p.current].Literal
				switch literal {
				case "add":
				case "sub":
				case "mul":
				case "div":
				default:
					return nil, &SyntaxError{
						Message:  fmt.Sprintf("unknown reduction ‘%s‘", literal),
						Line:     t.Line,
						Column:   t.Column,
						Filename: p.filename,
					}
				}
				stmts = append(stmts, ReduceStmt{
					Identifier: literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘add, div, mul or sub‘ after ‘reduce‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case Return:
			for _, stmt := range deferStmts {
				stmts = append(stmts, stmt)
			}
			stmts = append(stmts, ReturnStatement{})
		case End:
			if loopDepth <= 0 {
				return nil, &SyntaxError{
					Message:  "unexpected ‘end‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
			loopDepth--
			stmts = append(stmts, EndStatement{})
		case Jump:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jmp{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jmp‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpEqual:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Je{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘je‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpGreater:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jg{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jg‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpGreaterEqual:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jge{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jge‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpLesser:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jl{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jl‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpLesserEqual:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jle{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jle‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpNotEqual:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jne{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jne‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpNotZero:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jnz{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jnz‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpZero:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jz{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jz‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpLesserEqualZero:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jlez{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jlez‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpGreaterEqualZero:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jgez{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jgez‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpGreaterZero:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jgz{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jgz‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case JumpLesserZero:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, Jlz{
					Label: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected ‘label‘ after ‘jlz‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		case Local:
			if p.peek() == Identifier {
				p.current++
				stmts = append(stmts, LocalValue{
					Name: (*p.tokens)[p.current].Literal,
				})
			} else {
				return nil, &SyntaxError{
					Message:  "expected identifier after ‘local‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
		default:
			if (*p.tokens)[p.current+1].Type == Colon {
				stmts = append(stmts, Label{
					Name: t.Literal,
				})
				p.current++
			} else {
				if fnCall, err := p.parseFunctionCall(loopDepth); err != nil {
					return nil, err
				} else {
					stmts = append(stmts, fnCall)
				}
			}
		}
	}
	for _, stmt := range deferStmts {
		stmts = append(stmts, stmt)
	}

	if loopDepth != 0 {
		return nil, &SyntaxError{
			Message:  "missing ‘end‘",
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}

	return Body{
		Statements: stmts,
	}, nil
}

func (p *Parser) parseFunctionCall(loopDepth int) (Node, *SyntaxError) {
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
		} else if t.Type == Pointer {
			p.current++
			t = (*p.tokens)[p.current]
			if t.Type == Identifier {
				functionCall.Args = append(functionCall.Args, "__qd_ptr_to_real(__qd_"+t.Literal+")")
			}
		} else if t.Type == Iterator {
			if loopDepth <= 0 {
				return nil, &SyntaxError{
					Message:  "unexpected ‘$‘",
					Line:     t.Line,
					Column:   t.Column,
					Filename: p.filename,
				}
			}
			functionCall.Args = append(functionCall.Args, "__qd_itr")
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
			Message:  "path not found after ‘use‘",
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
	if n, err := p.parseFunctionCall(0); err != nil {
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

func (p *Parser) parseForLoop() (Node, *SyntaxError) {
	p.current++
	t := (*p.tokens)[p.current]
	if t.Type != NumericConstant {
		return nil, &SyntaxError{
			Message:  fmt.Sprintf("expected numeric constant but got ‘%s‘", t.Literal),
			Line:     t.Line,
			Column:   t.Column,
			Filename: p.filename,
		}
	}
	start := t.Literal
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
	step := t.Literal
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
	end := t.Literal

	fl := ForLoop{
		Start: start,
		Step:  step,
		End:   end,
	}

	return fl, nil
}
