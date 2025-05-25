package main

import (
	"fmt"

	"git.sr.ht/~klahr/quadrate/lexer"
)

func main() {
	s := lexer.NewScanner([]rune("use fmt use os fn main() {push 4\npush 2\nadd}"))
	if tokens, err := s.Lex(); err != nil {
		panic(err)
	} else {
		use := false
		var previousT lexer.Token
		for _, t := range tokens {
			fmt.Printf("%s: %s\n", t.Type, t.Value)
		}
		indent := 0
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
			case lexer.Identifier:
				if indent > 0 {
					for range indent {
						fmt.Printf("\t")
					}
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
				indent++
			case lexer.RBrace:
				fmt.Printf("\n}")
				indent--
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
