package quadrate

import (
	"os"
	"strings"
)

type TranslationUnit struct {
	filepath string
	tokens   []Token
	program  *Program
}

func NewTranslationUnit(filepath string) *TranslationUnit {
	return &TranslationUnit{
		filepath: filepath,
	}
}

func (tu *TranslationUnit) Lex() error {
	if data, err := os.ReadFile(tu.filepath); err != nil {
		return err
	} else {
		l := NewLexer(tu.filepath, data, "")
		tu.tokens = l.Lex().Tokens
	}
	return nil
}

func (tu *TranslationUnit) Parse() *SyntaxError {
	p := NewParser(tu.filepath, &tu.tokens)
	if pgm, err := p.Parse(); err != nil {
		return err
	} else {
		tu.program = pgm
	}
	return nil
}

func (tu *TranslationUnit) GetFilename() string {
	return strings.ReplaceAll(tu.filepath, "/", "_")
}

func (tu *TranslationUnit) GetTokens() []Token {
	return tu.tokens
}

func (tu *TranslationUnit) GetProgram() *Program {
	return tu.program
}
