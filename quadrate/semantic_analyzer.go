package quadrate

import "fmt"

type SemanticAnalyzer struct {
}

func NewSemanticAnalyzer() *SemanticAnalyzer {
	return &SemanticAnalyzer{}
}

func (sa *SemanticAnalyzer) Analyze(tus *[]TranslationUnit) *SemanticError {
	entrypoints := 0
	for _, tu := range *tus {
		if sa.fnIsDefined(&tu, "main") {
			entrypoints++
		}
	}
	if entrypoints == 0 {
		return &SemanticError{
			Message: "undefined reference to `main`",
		}
	}

	for _, tu := range *tus {
		for _, t := range tu.tokens {
			if t.Type == Identifier {
				if err := sa.checkRedefinedIdentifier(FnSignature, tus, t.Literal); err != nil {
					return err
				}
				if err := sa.checkRedefinedIdentifier(Const, tus, t.Literal); err != nil {
					return err
				}
			}
		}
	}

	return nil
}

func (sa *SemanticAnalyzer) fnIsDefined(tu *TranslationUnit, name string) bool {
	for _, t := range tu.tokens {
		if t.Type == Identifier {
			if t.Literal == name {
				return true
			}
		}
	}
	return false
}

func (sa *SemanticAnalyzer) checkRedefinedIdentifier(tokenType TokenType, tus *[]TranslationUnit, name string) *SemanticError {
	definitions := 0
	for _, tu := range *tus {
		for i, t := range tu.tokens {
			if (t.Type == Identifier) && t.Literal == name {
				if i > 0 && tu.tokens[i-1].Type == tokenType {
					definitions++
				}
			}
		}
	}
	if definitions > 1 {
		return &SemanticError{
			Message: fmt.Sprintf("redefinition of %s ‘"+name+"‘", tokenType),
		}
	}
	return nil
}
