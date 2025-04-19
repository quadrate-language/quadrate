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
	Scope    string
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
			if symbol.Name == s.Name && symbol.Scope == s.Scope {
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
		"__panic_stack_underflow",
		"__panic_stack_overflow",
		"abs",
		"acos",
		"add",
		"asin",
		"atan",
		"avg",
		"break",
		"call",
		"cb",
		"cbrt",
		"ceil",
		"clear",
		"continue",
		"cos",
		"dec",
		"depth",
		"div",
		"dup",
		"dup2",
		"end",
		"error",
		"eval",
		"fac",
		"floor",
		"for",
		"inc",
		"inv",
		"jmp",
		"je",
		"jge",
		"jgez",
		"jg",
		"jgz",
		"jle",
		"jlez",
		"jlz",
		"jl",
		"jne",
		"jnz",
		"jz",
		"ln",
		"local",
		"log10",
		"loop",
		"mark",
		"max",
		"min",
		"mod",
		"mul",
		"neg",
		"nip",
		"over",
		"pick",
		"print",
		"pop",
		"pow",
		"push",
		"read",
		"revert",
		"roll",
		"rot",
		"rot2",
		"round",
		"scale",
		"sin",
		"sq",
		"sqrt",
		"sub",
		"sum",
		"swap",
		"swap2",
		"tan",
		"test",
		"tuck",
		"within",
		"write",
	}
	return slices.Contains(keywords, name)
}

func (sa *SemanticAnalyzer) isLocal(i int, tokens []Token) bool {
	for j := i - 1; j >= 0; j-- {
		if tokens[j].Type == Local {
			return true
		}
		if tokens[j].Type == NewLine {
			break
		}
	}
	return false
}

func (sa *SemanticAnalyzer) getSymbols(tus *[]TranslationUnit) []Symbol {
	symbols := []Symbol{}
	currentScope := ""
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
					currentScope = symbols[len(symbols)-1].Name
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
				} else if sa.isLocal(i, tu.tokens) {
					symbols = append(symbols, Symbol{
						Name:     t.Literal,
						Filename: tu.filepath,
						Line:     t.Line,
						Column:   t.Column,
						Scope:    currentScope,
					})
					if sa.dumpTokens {
						println(fmt.Sprintf("V: %s %s:%d:%d", t.Literal, tu.filepath, t.Line, t.Column+1))
					}
				} else if tu.tokens[i+1].Type == Colon {
					symbols = append(symbols, Symbol{
						Name:     t.Literal,
						Filename: tu.filepath,
						Line:     t.Line,
						Column:   t.Column,
						Scope:    currentScope,
					})
					if sa.dumpTokens {
						println(fmt.Sprintf("L: %s %s:%d:%d", t.Literal, tu.filepath, t.Line, t.Column+1))
					}
				}
			}
		}
	}
	return symbols
}
