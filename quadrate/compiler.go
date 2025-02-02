package quadrate

import (
	"log"
)

type ModuleToCompile struct {
	Module   string
	Compiled bool
}

type Compiler struct {
	translationUnits []TranslationUnit
	modulesToCompile []ModuleToCompile
}

func NewCompiler() *Compiler {
	return &Compiler{}
}

func (c *Compiler) Compile(files []string) (*[]TranslationUnit, *SyntaxError) {
	for _, file := range files {
		c.modulesToCompile = append(c.modulesToCompile, ModuleToCompile{
			Module:   file,
			Compiled: false,
		})
	}

	allCompiled := false
	for !allCompiled {
		allCompiled = true
		for i, module := range c.modulesToCompile {
			if module.Compiled {
				continue
			} else {
				allCompiled = false
				tu, err := c.compile(module.Module)
				if err != nil {
					return nil, err
				}
				c.translationUnits = append(c.translationUnits, *tu)
				c.modulesToCompile[i].Compiled = true
				for _, submodule := range tu.module.Submodules {
					alreadyAdded := false
					for _, compiledModule := range c.modulesToCompile {
						if compiledModule.Module == submodule {
							alreadyAdded = true
							break
						}
					}
					if !alreadyAdded {
						c.modulesToCompile = append(c.modulesToCompile, ModuleToCompile{
							Module:   submodule,
							Compiled: false,
						})
					}
				}
				break
			}
		}
	}
	return &c.translationUnits, nil
}

func (c *Compiler) compile(file string) (*TranslationUnit, *SyntaxError) {
	tu := NewTranslationUnit(file)
	if err := tu.Lex(); err != nil {
		log.Fatalf("quadrate: error: %s\n", err.Message)
		return nil, err
	}
	if err := tu.Parse(); err != nil {
		return nil, err
	}
	// TODO: Generate intermediate representation
	return tu, nil
}
