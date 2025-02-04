package quadrate

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
)

type ModuleToCompile struct {
	Compiled bool
	Module   string
	Name     string
}

type Compiler struct {
	modulesToCompile []ModuleToCompile
	translationUnits []TranslationUnit
}

func NewCompiler() *Compiler {
	return &Compiler{}
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
	c.compileAndLink()
	return &c.translationUnits, nil
}

func (c *Compiler) compile(file, name string) (*TranslationUnit, *SyntaxError) {
	tu := NewTranslationUnit(file, name)
	if err := tu.Lex(); err != nil {
		log.Fatalf("quadrate: error: %s\n", err.Message)
		return nil, err
	}
	if err := tu.Parse(); err != nil {
		return nil, err
	}

	return tu, nil
}

func (c *Compiler) compileAndLink() {
	folderPath := ".qd_gen"

	cFiles, err := filepath.Glob(filepath.Join(folderPath, "*.c"))
	if err != nil {
		fmt.Println("Error finding C files:", err)
		return
	}

	if len(cFiles) == 0 {
		fmt.Println("No C files found")
		return
	}

	outputFile := "program.out"

	var args []string
	args = append(args, "-o", outputFile, "-I", folderPath)
	args = append(args, cFiles...)

	cmd := exec.Command("gcc", args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	fmt.Println("Running command:", cmd.String())

	err = cmd.Run()
	if err != nil {
		fmt.Println("Compilation failed:", err)
		return
	}

	fmt.Println("Compilation successful! Executable:", outputFile)
}
