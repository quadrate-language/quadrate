package main

import (
	"fmt"
	"strings"

	"git.sr.ht/~klahr/quadrate/lexer"
)

func main() {
	s := lexer.NewScanner([]rune("use fmt use os fn foo(a:int b:float  --c:str) {} fn main() {push -45;push 0xa5\npush 2.5\nadd\npush 4e04;if eq { push 1\n}for 0 1 10 { push 1}}"))
	if tokens, err := s.Lex(); err != nil {
		panic(err)
	} else {
		sb := strings.Builder{}

		use := false
		var previousT lexer.Token
		for _, t := range tokens {
			fmt.Printf("%s: %s\n", t.Type, t.Value)
		}
		ind := 0
		for _, t := range tokens {
			if use && t.Type != lexer.Identifier && t.Type != lexer.Use {
				sb.WriteString("\n")
				use = false
			}
			switch t.Type {
			case lexer.EOF:
				t.Value = "EOF"
			case lexer.EOL:
				if previousT.Type == lexer.Identifier || previousT.Type == lexer.IntLiteral || previousT.Type == lexer.FloatLiteral || previousT.Type == lexer.HexNumber || previousT.Type == lexer.StringLiteral {
					sb.WriteString("\n")
				}
			case lexer.Function:
				sb.WriteString("fn ")
			case lexer.For:
				indent(&sb, ind, previousT)
				sb.WriteString("for ")
			case lexer.Semicolon:
				sb.WriteString(" ;")
			case lexer.Dollar:
				sb.WriteString(" $")
			case lexer.Ampersand:
				sb.WriteString(" &")
			case lexer.If:
				indent(&sb, ind, previousT)
				sb.WriteString("if ")
			case lexer.Identifier:
				if previousT.Type != lexer.If {
					if previousT.Type == lexer.LParen || previousT.Type == lexer.DoubleDash {
						sb.WriteString(" ")
					}
					indent(&sb, ind, previousT)
				}
				sb.WriteString(fmt.Sprintf("%s", t.Value))
				if previousT.Type == lexer.Use {
					sb.WriteString("\n")
				}
			case lexer.LParen:
				sb.WriteString("(")
			case lexer.RParen:
				sb.WriteString(")")
			case lexer.LBrace:
				sb.WriteString(" {\n")
				ind++
			case lexer.Int:
				sb.WriteString("int ")
			case lexer.Float:
				sb.WriteString("float ")
			case lexer.String:
				sb.WriteString("str ")
			case lexer.Colon:
				sb.WriteString(":")
			case lexer.DoubleDash:
				sb.WriteString("--")
			case lexer.RBrace:
				if previousT.Type != lexer.EOL && previousT.Type != lexer.RBrace {
					sb.WriteString("\n")
				}
				ind--
				indent(&sb, ind, previousT)
				sb.WriteString("}\n")
			case lexer.HexNumber:
				sb.WriteString(fmt.Sprintf("%s", t.Value))
			case lexer.IntLiteral, lexer.FloatLiteral:
				sb.WriteString(fmt.Sprintf("%s", t.Value))
			case lexer.StringLiteral:
				sb.WriteString(fmt.Sprintf("\"%s\"", t.Value))
			case lexer.Use:
				use = true
				sb.WriteString("use ")
			case lexer.Comment:
			case lexer.Unknown:
			}

			previousT = t
			//println(t.Type, t.Value)
		}

		fmt.Printf("%s", sb.String())
	}
}

func indent(sb *strings.Builder, i int, previousT lexer.Token) {
	if previousT.Type == lexer.Semicolon {
		sb.WriteString(" ")
	} else {
		for range i {
			sb.WriteString("\t")
		}
	}
}
