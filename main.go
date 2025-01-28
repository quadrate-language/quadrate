package main

import (
	"flag"
	"fmt"
	"log"
	"os"

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
		push 1
		push 1.5
		push 13.55
	}`
	l := quadrate.NewLexer([]byte(s))
	tokens := l.Lex()
	for _, t := range tokens {
		switch t.Type {
		case quadrate.NEW_LINE:
			fmt.Println()
		case quadrate.IDENTIFIER:
			fmt.Printf("identifier '%s' [%d:%d]\n", t.Literal, t.Line, t.Column)
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
		tus = append(tus, tu)
	}
}
