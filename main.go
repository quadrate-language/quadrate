package main

import (
	"flag"
	"fmt"
	"log"
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
	/*
		// debug
		s := `use fmt
		fn foo(a, b) {
			push [4 5] 1 2 3
		}

		fn main() { // a comment
			push -0.1
			push -0.5
			push -13.55
			pop [1] 1
			fmt::print [-1 2.0 -3.01 4.003]
			__c {
				int x = 4;
				if (x == 4) {
					return;
				}
			}
		}`
		l := quadrate.NewLexer("main.qd", []byte(s), "")
		lexResult := l.Lex()
		for _, t := range lexResult.Tokens {
			switch t.Type {
			case quadrate.NewLine:
				fmt.Println()
			case quadrate.Identifier:
				fmt.Printf("identifier '%s' [%d:%d]\n", t.Literal, t.Line, t.Column)
			case quadrate.InlineC:
				fmt.Printf("< inline c > [%d:%d]\n%s\n", t.Line, t.Column, t.Literal)
			case quadrate.Module:
				fmt.Printf("< module '%s' > [%d:%d]\n", t.Literal, t.Line, t.Column)
			case quadrate.EOF:
				fmt.Println("< EOF >")
			default:
				fmt.Printf("< '%s' >\n", t.Literal)
			}
		}

		p := quadrate.NewParser(lexResult.Filename, &lexResult.Tokens)
		if module, err := p.Parse(); err != nil {
			panic(err.Message)
		} else {
			for _, stmt := range module.Statements {
				fmt.Println(stmt)
			}
			for _, m := range module.Submodules {
				fmt.Println(m)
			}
		}
	*/

	args.Sources = append(args.Sources, "data/main.qd")

	if len(args.Sources) == 0 {
		log.Fatal("no input files")
		os.Exit(1)
	}

	var absFilepaths []string
	for _, file := range args.Sources {
		if _, err := os.Stat(file); os.IsNotExist(err) {
			log.Fatalf("quadrate: error: no such file or directory: %s\n", file)
			os.Exit(1)
		} else {
			if absPath, err := filepath.Abs(file); err != nil {
				log.Fatalf("quadrate: error: %s\n", err.Error())
				os.Exit(1)
			} else {
				absFilepaths = append(absFilepaths, absPath)
			}
		}
	}

	compiler := quadrate.NewCompiler()
	if tus, err := compiler.Compile(absFilepaths); err != nil {
		if b, e := os.ReadFile(err.Filename); e != nil {
			log.Fatalf("quadrate: error: %s\n", e.Error())
		} else {
			lines := strings.Split(string(b), "\n")
			fmt.Printf("\033[1m%s:%d:%d: \033[31merror:\033[0m %s\n", err.Filename, err.Line, err.Column, err.Message)
			fmt.Printf("%d | %s\n", err.Line, lines[err.Line-1])
			fmt.Printf("%s | %s\033[1;31m^\033[0m\n", strings.Repeat(" ", len(fmt.Sprintf("%d", err.Line))), strings.Repeat(" ", err.Column-1))
		}
		os.Exit(1)
	} else {
		for _, tu := range *tus {
			tu.Print()
		}
		generator := quadrate.NewCGenerator("./.qd_gen")
		for _, tu := range *tus {
			if err := generator.Generate(&tu); err != nil {
				log.Fatalf("quadrate: error: %s\n", err.Message)
				os.Exit(1)
			}
		}
		os.Exit(0)
	}
}
