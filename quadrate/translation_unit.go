package quadrate

import "os"

type TranslationUnit struct {
	filepath string
	tokens   []Token
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
		l := NewLexer(data)
		tu.tokens = l.Lex()
	}
	return nil
}
