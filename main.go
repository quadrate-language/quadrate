package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"git.sr.ht/~klahr/quadrate/quadrate"
)

func main() {
	var args quadrate.Args
	flag.StringVar(&args.Output, "o", "a.out", "Output filename")
	flag.BoolVar(&args.Run, "run", false, "Run the compiled program")
	flag.BoolVar(&args.SaveTemps, "save-temps", false, "Save temporary files")
	flag.BoolVar(&args.DumpTokens, "dump-tokens", false, "Print tokens")
	flag.Parse()

	args.Sources = flag.Args()

	if len(args.Sources) == 0 {
		fmt.Printf("\033[1mquadc: \033[31mfatal error:\033[0m no input files\n")
		os.Exit(1)
	}

	var absFilepaths []string
	for _, file := range args.Sources {
		if _, err := os.Stat(file); os.IsNotExist(err) {
			fmt.Printf("\033[1mquadc: \033[31merror:\033[0m no such file or directory: %s\n", file)
			os.Exit(1)
		} else {
			if absPath, err := filepath.Abs(file); err != nil {
				fmt.Printf("\033[1mquadc: \033[31merror:\033[0m %s\n", err.Error())
				os.Exit(1)
			} else {
				absFilepaths = append(absFilepaths, absPath)
			}
		}
	}

	compiler := quadrate.NewCompiler(args.DumpTokens, args.Output)
	if tus, err := compiler.Compile(absFilepaths); err != nil {
		if b, e := os.ReadFile(err.Filename); e != nil {
			fmt.Printf("\033[1mquadc: \033[31merror:\033[0m %s\n", e.Error())
		} else {
			lines := strings.Split(string(b), "\n")
			fmt.Printf("\033[1m%s:%d:%d: \033[31merror:\033[0m %s\n", err.Filename, err.Line, err.Column+1, err.Message)
			if err.Line >= 1 {
				fmt.Printf("%d | %s\n", err.Line, lines[err.Line-1])
				fmt.Printf("%s | %s\033[1;31m^\033[0m\n", strings.Repeat(" ", len(fmt.Sprintf("%d", err.Line))), strings.Repeat(" ", err.Column))
			}
		}
		os.Exit(1)
	} else {
		sa := quadrate.NewSemanticAnalyzer()
		if err := sa.Analyze(tus); err != nil {
			if b, e := os.ReadFile(err.Filename); e != nil {
				fmt.Printf("\033[1mquadc: \033[31merror:\033[0m %s\n", e.Error())
			} else {
				lines := strings.Split(string(b), "\n")
				fmt.Printf("\033[1m%s:%d:%d: \033[31merror:\033[0m %s\n", err.Filename, err.Line, err.Column+1, err.Message)
				if err.Line >= 1 {
					fmt.Printf("%d | %s\n", err.Line, lines[err.Line-1])
					fmt.Printf("%s | %s\033[1;31m^\033[0m\n", strings.Repeat(" ", len(fmt.Sprintf("%d", err.Line))), strings.Repeat(" ", err.Column))
				}
			}
			if !args.SaveTemps {
				os.RemoveAll("./.qd_gen")
			}
			os.Exit(1)
		}
		generator := quadrate.NewCGenerator("./.qd_gen")
		for _, tu := range *tus {
			if err := generator.Generate(&tu); err != nil {
				fmt.Printf("\033[1mquadc: \033[31merror:\033[0m %s\n", err.Message)
				if !args.SaveTemps {
					os.RemoveAll("./.qd_gen")
				}
				os.Exit(1)
			}
		}

		compiler.CompileAndLink()

		if !args.SaveTemps {
			os.RemoveAll("./.qd_gen")
		}
		os.Exit(0)
	}
}
