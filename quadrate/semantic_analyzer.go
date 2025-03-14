package quadrate

import (
	"fmt"
	"slices"
)

type SemanticAnalyzer struct {
	dumpTokens bool
}

type Symbol struct {
	Name     string
	Filename string
	Line     int
	Column   int
}

func NewSemanticAnalyzer(dumpTokens bool) *SemanticAnalyzer {
	return &SemanticAnalyzer{
		dumpTokens: dumpTokens,
	}
}

func (sa *SemanticAnalyzer) Analyze(tus *[]TranslationUnit) *SemanticError {
	symbols := sa.getSymbols(tus)

	if err := sa.checkRedefinitions(symbols); err != nil {
		return err
	}

	if err := sa.checkDefined(symbols, "main"); err != nil {
		return err
	}

	for _, tu := range *tus {
		for i, t := range tu.tokens {
			if t.Type == Identifier {
				var module string
				if i > 1 && tu.tokens[i-1].Type == DoubleColon {
					module = tu.tokens[i-2].Literal
				} else if i > 0 && tu.tokens[i-1].Type == Const || tu.tokens[i-1].Type == FnSignature {
					continue
				}
				var name string
				if module != "" {
					name = fmt.Sprintf("%s::%s", module, t.Literal)
				} else {
					name = t.Literal
				}
				if sa.isPrimitiveInstruction(name) {
					continue
				}
				if sa.isModule(tus, name) {
					continue
				}
				if err := sa.checkDefined(symbols, name); err != nil {
					err.Filename = tu.filepath
					err.Line = t.Line
					err.Column = t.Column
					return err
				}
			}
		}
	}

	return nil
}

func (sa *SemanticAnalyzer) checkRedefinitions(symbols []Symbol) *SemanticError {
	for _, symbol := range symbols {
		count := 0
		for _, s := range symbols {
			if symbol.Name == s.Name {
				count++
				if count > 1 {
					return &SemanticError{
						Message:  fmt.Sprintf("redefinition of ‘%s‘", s.Name),
						Filename: s.Filename,
						Line:     s.Line,
						Column:   s.Column,
					}
				}
			}
		}
	}
	return nil
}

func (sa *SemanticAnalyzer) checkDefined(symbols []Symbol, name string) *SemanticError {
	defined := false
	for _, symbol := range symbols {
		if symbol.Name == name {
			defined = true
			break
		}
	}
	if !defined {
		return &SemanticError{
			Message: fmt.Sprintf("undefined reference to ‘%s‘", name),
		}
	}
	return nil
}

func (sa *SemanticAnalyzer) isModule(tus *[]TranslationUnit, name string) bool {
	for _, tu := range *tus {
		if tu.name == name {
			return true
		}
	}
	return false
}

func (sa *SemanticAnalyzer) isPrimitiveInstruction(name string) bool {
	keywords := []string{
		"abs",
		"acos",
		"add",
		"asin",
		"atan",
		"cbrt",
		"ceil",
		"clear",
		"cos",
		"dec",
		"depth",
		"div",
		"dup",
		"error",
		"eval",
		"floor",
		"inc",
		"inv",
		"jmp",
		"je",
		"jge",
		"jg",
		"jle",
		"jl",
		"jne",
		"jz",
		"ln",
		"log10",
		"mod",
		"mul",
		"neg",
		"over",
		"print",
		"pop",
		"pow",
		"push",
		"read",
		"rot",
		"rot2",
		"scale",
		"sin",
		"sq",
		"sqrt",
		"sub",
		"swap",
		"tan",
		"write",
	}
	return slices.Contains(keywords, name)
}

func (sa *SemanticAnalyzer) getSymbols(tus *[]TranslationUnit) []Symbol {
	symbols := []Symbol{}
	for _, tu := range *tus {
		for i, t := range tu.tokens {
			if i > 0 && t.Type == Identifier {
				var prefix string
				if tu.name != "" {
					prefix = fmt.Sprintf("%s::", tu.name)
				}
				if tu.tokens[i-1].Type == FnSignature {
					symbols = append(symbols, Symbol{
						Name:     fmt.Sprintf("%s%s", prefix, t.Literal),
						Filename: tu.filepath,
						Line:     t.Line,
						Column:   t.Column,
					})
					if sa.dumpTokens {
						println(fmt.Sprintf("F: %s%s %s:%d:%d", prefix, t.Literal, tu.filepath, t.Line, t.Column+1))
					}
				} else if tu.tokens[i-1].Type == Const {
					symbols = append(symbols, Symbol{
						Name:     fmt.Sprintf("%s%s", prefix, t.Literal),
						Filename: tu.filepath,
						Line:     t.Line,
						Column:   t.Column,
					})
					if sa.dumpTokens {
						println(fmt.Sprintf("C: %s%s %s:%d:%d", prefix, t.Literal, tu.filepath, t.Line, t.Column+1))
					}
				} else if tu.tokens[i+1].Type == Colon {
					symbols = append(symbols, Symbol{
						Name:     fmt.Sprintf("%s%s", prefix, t.Literal),
						Filename: tu.filepath,
						Line:     t.Line,
						Column:   t.Column,
					})
					if sa.dumpTokens {
						println(fmt.Sprintf("L: %s%s %s:%d:%d", prefix, t.Literal, tu.filepath, t.Line, t.Column+1))
					}
				}
			}
		}
	}
	return symbols
}
