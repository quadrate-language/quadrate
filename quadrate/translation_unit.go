package quadrate

import (
	"fmt"
	"os"
)

type TranslationUnit struct {
	filepath string
	module   *ProgramModule
	name     string
	tokens   []Token
}

func NewTranslationUnit(filepath string, name string) *TranslationUnit {
	return &TranslationUnit{
		filepath: filepath,
		name:     name,
	}
}

func (tu *TranslationUnit) Lex(dumpTokens bool) *SyntaxError {
	if data, err := os.ReadFile(tu.filepath); err != nil {
		return &SyntaxError{
			Message: err.Error(),
		}
	} else {
		l := NewLexer(tu.filepath, data, tu.name)
		tu.tokens = l.Lex().Tokens
		if dumpTokens {
			tu.Print()
		}
	}
	return nil
}

func (tu *TranslationUnit) Parse() *SyntaxError {
	p := NewParser(tu.filepath, &tu.tokens)
	if pgm, err := p.Parse(); err != nil {
		return err
	} else {
		tu.module = pgm
	}
	return nil
}

func (tu *TranslationUnit) Print() {
	fmt.Printf("Translation Unit: %s\n", tu.filepath)
	for _, t := range tu.tokens {
		switch t.Type {
		case NewLine:
			fmt.Println()
		case Identifier:
			fmt.Printf("identifier '%s' [%d:%d]\n", t.Literal, t.Line, t.Column)
		case InlineC:
			fmt.Printf("< inline c > [%d:%d]\n%s\n", t.Line, t.Column, t.Literal)
		case Module:
			fmt.Printf("< module '%s' > [%d:%d]\n", t.Literal, t.Line, t.Column)
		case BeginScopeComment:
			fmt.Printf("< begin scope comment > [%d:%d]\n", t.Line, t.Column)
		case EndScopeComment:
			fmt.Printf("< end scope comment > [%d:%d]\n", t.Line, t.Column)
		case EOF:
			fmt.Println("< EOF >")
		default:
			fmt.Printf("< '%s' >\n", t.Literal)
		}
	}
}
