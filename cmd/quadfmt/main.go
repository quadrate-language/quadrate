package main

import (
	"fmt"

	"git.sr.ht/~klahr/quadrate/lexer"
)

func main() {
	s := lexer.NewScanner([]rune("use fmt use os fn main() {push 45;push 0xa5\npush 2\nadd\npush 4e04;if eq { push 1\n}for 0 1 10 { push 1}}"))
	if tokens, err := s.Lex(); err != nil {
		panic(err)
	} else {
		use := false
		var previousT lexer.Token
		for _, t := range tokens {
			fmt.Printf("%s: %s\n", t.Type, t.Value)
		}
		ind := 0
		for _, t := range tokens {
			if use && t.Type != lexer.Identifier && t.Type != lexer.Use {
				fmt.Printf("\n")
				use = false
			}
			switch t.Type {
			case lexer.EOF:
				t.Value = "EOF"
			case lexer.EOL:
				if previousT.Type == lexer.Identifier || previousT.Type == lexer.Number || previousT.Type == lexer.HexNumber || previousT.Type == lexer.String {
					fmt.Printf("\n")
				}
			case lexer.Function:
				fmt.Printf("fn ")
			case lexer.Semicolon:
				fmt.Printf(" ;")
			case lexer.If:
				indent(ind, previousT)
				fmt.Printf("if ")
			case lexer.Identifier:
				if previousT.Type != lexer.If {
					indent(ind, previousT)
				}
				fmt.Printf("%s", t.Value)
				if previousT.Type == lexer.Use {
					fmt.Printf("\n")
				}
			case lexer.LParen:
				fmt.Printf("(")
			case lexer.RParen:
				fmt.Printf(")")
			case lexer.LBrace:
				fmt.Printf(" {\n")
				ind++
			case lexer.RBrace:
				if previousT.Type != lexer.EOL && previousT.Type != lexer.RBrace {
					fmt.Printf("\n")
				}
				ind--
				indent(ind, previousT)
				fmt.Printf("}\n")
			case lexer.HexNumber:
				fmt.Printf(" %s", t.Value)
			case lexer.Number:
				fmt.Printf(" %s", t.Value)
			case lexer.String:
				fmt.Printf("\"%s\"", t.Value)
			case lexer.Use:
				use = true
				fmt.Printf("use ")
			case lexer.Comment:
			case lexer.Unknown:
			}

			previousT = t
			//println(t.Type, t.Value)
		}
	}
}

func indent(i int, previousT lexer.Token) {
	if previousT.Type == lexer.Semicolon {
		fmt.Printf(" ")
	} else {
		for range i {
			fmt.Printf("\t")
		}
	}
}
