package quadrate

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/acarl005/stripansi"
)

type ModuleToCompile struct {
	Compiled bool
	Module   string
	Name     string
}

type Compiler struct {
	modulesToCompile []ModuleToCompile
	translationUnits []TranslationUnit
	dumpTokens       bool
	output           string
}

func NewCompiler(dumpTokens bool, output string) *Compiler {
	return &Compiler{
		dumpTokens: dumpTokens,
		output:     output,
	}
}

func (c *Compiler) Compile(files []string) (*[]TranslationUnit, *SyntaxError) {
	for _, file := range files {
		c.modulesToCompile = append(c.modulesToCompile, ModuleToCompile{
			Compiled: false,
			Module:   file,
			Name:     "",
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
				tu, err := c.compile(module.Module, module.Name)
				if err != nil {
					return nil, err
				}
				c.translationUnits = append(c.translationUnits, *tu)
				c.modulesToCompile[i].Compiled = true
				for _, submodule := range tu.module.Submodules {
					alreadyAdded := false
					for _, compiledModule := range c.modulesToCompile {
						if compiledModule.Module == submodule.Module {
							alreadyAdded = true
							break
						}
					}
					if !alreadyAdded {
						c.modulesToCompile = append(c.modulesToCompile, ModuleToCompile{
							Module:   submodule.Module,
							Compiled: false,
							Name:     submodule.Name,
						})
					}
				}
				break
			}
		}
	}
	return &c.translationUnits, nil
}

func (c *Compiler) compile(file, name string) (*TranslationUnit, *SyntaxError) {
	tu := NewTranslationUnit(file, name)
	if err := tu.Lex(c.dumpTokens); err != nil {
		fmt.Printf("\033[1mquadc: \033[31merror:\033[0m %s\n", err.Message)
		return nil, err
	}
	if err := tu.Parse(); err != nil {
		return nil, err
	}

	return tu, nil
}

func (c *Compiler) CompileAndLink() {
	folderPath := ".qd_gen"

	cFiles, err := filepath.Glob(filepath.Join(folderPath, "*.c"))
	if err != nil {
		panic(err)
	}

	if len(cFiles) == 0 {
		fmt.Println(stripansi.Strip("\033[1mquadc: \033[31mfatal error:\033[0m no intermediate files\n"))
		return
	}

	var args []string
	args = append(args, "-o", c.output, "-I", folderPath)
	args = append(args, cFiles...)
	args = append(args, "-lm")

	cmd := exec.Command("gcc", args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err = cmd.Run()
	if err != nil {
		fmt.Println(stripansi.Strip(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m %s\n", err.Error())))
		return
	}
}
