package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
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

	// debug
	s := `use fmt
	fn main() { // a comment
		push -0.1
		push -0.5
		push -13.55
		fmt::print [-1 2.0 -3.01 4.003]
		__c {
			int x = 4;
		}
	}`
	l := quadrate.NewLexer("main.qd", []byte(s))
	lexResult := l.Lex()
	for _, t := range lexResult.Tokens {
		switch t.Type {
		case quadrate.NewLine:
			fmt.Println()
		case quadrate.Identifier:
			fmt.Printf("identifier '%s' [%d:%d]\n", t.Literal, t.Line, t.Column)
		case quadrate.InlineC:
			fmt.Println("< inline c >")
		case quadrate.EOF:
			fmt.Println("< EOF >")
		default:
			fmt.Printf("< '%s' >\n", t.Literal)
		}
	}

	args.Sources = append(args.Sources, "data/alpha.qd")
	args.Sources = append(args.Sources, "data/bravo.qd")
	// debug

	if len(args.Sources) == 0 {
		log.Fatal("no input files")
		os.Exit(1)
	}

	tus := make([]*quadrate.TranslationUnit, 0, len(args.Sources))
	for _, src := range args.Sources {
		tu := quadrate.NewTranslationUnit(src)
		if err := tu.Lex(); err != nil {
			log.Fatalf("quadrate: error: %s\n", err.Error())
			os.Exit(1)
		}
		if err := tu.Parse(); err != nil {
			if b, e := os.ReadFile(err.Filename); e != nil {
				log.Fatalf("quadrate: error: %s\n", e.Error())
			} else {
				lines := strings.Split(string(b), "\n")
				fmt.Printf("\033[1m%s:%d:%d: \033[31merror:\033[0m %s\n", err.Filename, err.Line, err.Column, err.Message)
				fmt.Printf("%d | %s\n", err.Line, lines[err.Line-1])
				fmt.Printf("%s | %s\033[1;31m^\033[0m\n", strings.Repeat(" ", len(fmt.Sprintf("%d", err.Line))), strings.Repeat(" ", err.Column-1))
			}
			os.Exit(1)
		}
		fmt.Print(tu.GetProgram())
		tus = append(tus, tu)
	}

	os.Mkdir(".qd_output", 0755)

	for _, tu := range tus {
		if b, err := json.Marshal(tu.GetTokens()); err != nil {
			panic(err)
		} else {
			os.WriteFile(".qd_output/"+tu.GetFilename()+".json", b, 0644)
		}
	}

	if !args.SaveTemps {
		os.RemoveAll(".qd_output")
	}
}
