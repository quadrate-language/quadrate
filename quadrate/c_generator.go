package quadrate

import (
	"fmt"
	"os"
	"strings"
	"unicode"
)

// //go:embed data/qd_base.h
//var baseHeader string

// //go:embed data/qd_base.c
//var baseSource string

type CGenerator struct {
	outputDir string
	prefix    string
}

func NewCGenerator(outputDir string) *CGenerator {
	cg := CGenerator{
		outputDir: outputDir,
		prefix:    "__qd_",
	}
	os.Mkdir(cg.outputDir+"/.qd_gen", 0755)
	return &cg
}

func (cg *CGenerator) Generate(tu *TranslationUnit) *SyntaxError {
	var sb strings.Builder

	cg.writeHeader(tu, &sb)
	cg.writeSource(tu, &sb)

	fmt.Println(sb.String())

	return nil
}

func (cg *CGenerator) writeHeader(tu *TranslationUnit, sb *strings.Builder) {
	sb.WriteString("writeHeader\n")
	sb.WriteString(cg.generateIncludeGuard(tu.filepath))
	sb.WriteString("#include \"qd_base.h\"\n\n")

	for _, stmt := range tu.module.Statements {
		switch n := stmt.(type) {
		case ImportDirective:
			sb.WriteString("#include \"" + n.Module + ".h\"\n")
		case FunctionDeclaration:
			if n.Name == "main" {
				sb.WriteString("int main(int argc, char** argv);\n")
				continue
			}
			sb.WriteString("void " + cg.prefix + n.Name + "(")
			for i, p := range n.Parameters {
				if i > 0 {
					sb.WriteString(", ")
				}
				sb.WriteString(p.Type + " " + p.Name)
			}
			sb.WriteString(");\n")
		case FunctionCall:
			sb.WriteString("void " + cg.prefix + n.Name + "(")
			for i, arg := range n.Args {
				if i > 0 {
					sb.WriteString(", ")
				}
				sb.WriteString(arg)
			}
			sb.WriteString(");\n")
		case InlineCCode:
			sb.WriteString(n.Code + "\n")
		}
	}

	sb.WriteString("\n#endif\n")
}

func (cg *CGenerator) writeSource(tu *TranslationUnit, sb *strings.Builder) {
	sb.WriteString("writeSource\n")
	sb.WriteString("#include \"" + tu.filepath + ".h\"\n\n")

	for _, stmt := range tu.module.Statements {
		switch n := stmt.(type) {
		case ImportDirective:
			sb.WriteString("#include \"" + n.Module + ".h\"\n")
		case FunctionDeclaration:
			if n.Name == "main" {
				sb.WriteString("int main(int argc, char** argv)")
				continue
			}
			sb.WriteString("void " + cg.prefix + n.Name + "(")
			for i, p := range n.Parameters {
				if i > 0 {
					sb.WriteString(", ")
				}
				sb.WriteString(p.Type + " " + p.Name)
			}
			sb.WriteString(")")
		case Body:
			sb.WriteString(" {\n")
			for _, stmt := range n.Statements {
				switch n := stmt.(type) {
				case FunctionCall:
					sb.WriteString("\t" + cg.prefix + n.Name + "(")
					for i, arg := range n.Args {
						if i > 0 {
							sb.WriteString(", ")
						}
						sb.WriteString(arg)
					}
					sb.WriteString(");\n")
				case InlineCCode:
					sb.WriteString(n.Code + "\n")
				}
			}
			sb.WriteString("}\n")
		case InlineCCode:
			sb.WriteString(n.Code + "\n")
		}
	}
}

func (cg *CGenerator) generateIncludeGuard(filename string) string {
	var result []rune
	for _, r := range filename {
		if unicode.IsLetter(r) || unicode.IsDigit(r) {
			result = append(result, unicode.ToUpper(r))
		} else {
			result = append(result, '_')
		}
	}

	includeGuard := string(result) + "_H"
	return "#ifndef QD" + includeGuard + "\n#define QD" + includeGuard + "\n\n"
}
