package parser

import (
	"git.sr.ht/~klahr/quadrate/ast"
	"git.sr.ht/~klahr/quadrate/diagnostic"
	"git.sr.ht/~klahr/quadrate/lexer"
)

type ASTBuilder struct {
	current int
	tokens  []lexer.Token
}

func NewASTBuilder() *ASTBuilder {
	return &ASTBuilder{}
}

func (b *ASTBuilder) Build(tokens []lexer.Token) (*ast.Tree, diagnostic.Issues) {
	b.current = 0
	b.tokens = tokens

	issues := diagnostic.Issues{}
	p := &ast.Tree{
		Root: &ModuleDecl{},
	}

	node := p.Root

	for b.current < len(tokens) {
		t := tokens[b.current]

		switch t.Type {
		case lexer.Function:
			if parseFn, issue := b.parseFunctionDecl(); issue != nil {
				issues = append(issues, issue)
			} else {
				node.Append(parseFn)

				if t, issue := b.read(); issue != nil {
					issues = append(issues, issue)
				} else if t.Type != lexer.LBrace {
					issues = append(issues, &diagnostic.Issue{
						Message:    "Expected '{' after function declaration",
						Category:   diagnostic.CategoryParser,
						Severity:   diagnostic.SeverityError,
						SourceSpan: t.SourceSpan,
					})
				} else {
				}
			}
		case lexer.Use:
			if parseUse, issue := b.parseUseDecl(); issue != nil {
				issues = append(issues, issue)
			} else {
				node.Append(parseUse)
			}
		case lexer.Identifier:
			issues = append(issues, &diagnostic.Issue{
				Message:    "Unexpected identifier outside of function declaration",
				Category:   diagnostic.CategoryParser,
				Severity:   diagnostic.SeverityError,
				Notes:      []string{"Identifiers should be part of a function declaration or use statement."},
				SourceSpan: t.SourceSpan,
			})
			b.read() // Skip the identifier
		default:
			b.read()
		}
	}

	if len(issues) > 0 {
		return nil, issues
	}
	return p, nil
}

func (b *ASTBuilder) parseFunctionDecl() (*FunctionDecl, *diagnostic.Issue) {
	if t, issue := b.read(); issue != nil {
		return nil, issue
	} else if t.Type != lexer.Identifier {
		return nil, &diagnostic.Issue{
			Message:    "Expected function name after 'fn'",
			Category:   diagnostic.CategoryParser,
			Severity:   diagnostic.SeverityError,
			SourceSpan: t.SourceSpan,
		}
	} else {
		fn := &FunctionDecl{
			Type: AstNodeTypeFunctionDeclaration,
			Name: t.Value,
			StackNotation: StackNotation{
				Inputs:  []Parameter{},
				Outputs: []Parameter{},
			},
			Body: []ast.Node{},
		}
		if t, issue := b.read(); issue != nil {
			return nil, issue
		} else if t.Type != lexer.LParen {
			return nil, &diagnostic.Issue{
				Message:    "Expected '(' after function name",
				Category:   diagnostic.CategoryParser,
				Severity:   diagnostic.SeverityError,
				SourceSpan: t.SourceSpan,
			}
		} else {
			ddFound := false
			for {
				if t, issue := b.read(); issue != nil {
					return nil, issue
				} else if t.Type == lexer.RParen {
					break
				} else if t.Type == lexer.Identifier {
					if t2, issue := b.read(); issue != nil {
						return nil, issue
					} else if t2.Type == lexer.Colon {
						if t3, issue := b.read(); issue != nil {
							return nil, issue
						} else if t3.Type == lexer.Int || t3.Type == lexer.Float || t3.Type == lexer.String {
							if ddFound {
								fn.StackNotation.Outputs = append(fn.StackNotation.Outputs, Parameter{
									Name: t.Value,
									Type: t3.Type,
								})
							} else {
								fn.StackNotation.Inputs = append(fn.StackNotation.Inputs, Parameter{
									Name: t.Value,
									Type: t3.Type,
								})
							}
						} else {
							return nil, &diagnostic.Issue{
								Message:    "Expected type after ':'",
								Category:   diagnostic.CategoryParser,
								Severity:   diagnostic.SeverityError,
								Notes:      []string{"Expected 'int', 'float', or 'str' after ':'"},
								SourceSpan: t3.SourceSpan,
							}
						}
					} else {
						return nil, &diagnostic.Issue{
							Message:    "Expected ':' after parameter name",
							Category:   diagnostic.CategoryParser,
							Severity:   diagnostic.SeverityError,
							SourceSpan: t2.SourceSpan,
						}
					}
				} else if t.Type == lexer.DoubleDash {
					ddFound = true
				}
			}
		}
		return fn, nil
	}
}

func (b *ASTBuilder) parseUseDecl() (*UseDecl, *diagnostic.Issue) {
	if t, issue := b.read(); issue != nil {
		return nil, issue
	} else {
		b.read()
		return &UseDecl{
			Type: AstNodeTypeUseDeclaration,
			Path: t.Value,
		}, nil
	}
}

func (b *ASTBuilder) read() (*lexer.Token, *diagnostic.Issue) {
	b.current++
	if b.current >= len(b.tokens) {
		return nil, &diagnostic.Issue{
			Message:  "Unexpected end of input",
			Category: diagnostic.CategoryParser,
			Severity: diagnostic.SeverityError,
		}
	}
	return &b.tokens[b.current], nil
}
