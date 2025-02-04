package quadrate

import (
	_ "embed"
	"fmt"
	"os"
	"path"
	"path/filepath"
	"strings"
	"unicode"
)

//go:embed data/qd_base.h
var baseHeader string

//go:embed data/qd_base.c
var baseSource string

type CGenerator struct {
	outputDir string
	prefix    string
}

func NewCGenerator(outputDir string) *CGenerator {
	cg := CGenerator{
		outputDir: outputDir,
		prefix:    "__qd",
	}
	os.RemoveAll(cg.outputDir)
	os.Mkdir(cg.outputDir, 777)

	os.WriteFile(path.Join(cg.outputDir, "qd_base.h"), []byte(baseHeader), 777)
	os.WriteFile(path.Join(cg.outputDir, "qd_base.c"), []byte(baseSource), 777)

	return &cg
}

func (cg *CGenerator) Generate(tu *TranslationUnit) *SyntaxError {
	var sb strings.Builder

	cg.writeHeader(tu, &sb)
	filename := path.Join(cg.outputDir, cg.generateFilename(tu.filepath, tu.name))
	os.WriteFile(fmt.Sprintf("%s.h", filename), []byte(sb.String()), 777)

	sb.Reset()
	cg.writeSource(tu, &sb)
	os.WriteFile(fmt.Sprintf("%s.c", filename), []byte(sb.String()), 777)

	fmt.Println(sb.String())

	return nil
}

func (cg *CGenerator) writeHeader(tu *TranslationUnit, sb *strings.Builder) {
	sb.WriteString(cg.generateIncludeGuard(tu.filepath))
	sb.WriteString("#include \"qd_base.h\"\n\n")

	for _, stmt := range tu.module.Statements {
		switch n := stmt.(type) {
		case ImportDirective:
			sb.WriteString("#include \"" + cg.generateFilename(n.Module, n.Name) + ".h\"\n")
		case FunctionDeclaration:
			if n.Name == "main" {
				sb.WriteString("int main(int argc, char** argv);\n")
				continue
			}
			sb.WriteString(fmt.Sprintf("void %s_%s_%s(", cg.prefix, tu.name, n.Name))
			if len(n.Parameters) == 0 {
				sb.WriteString("void")
			} else {
				for i, p := range n.Parameters {
					if i > 0 {
						sb.WriteString(", ")
					}
					sb.WriteString(p.Type + " " + p.Name)
				}
			}
			sb.WriteString(");\n")
		default:
			continue
		}
	}

	sb.WriteString("\n#endif\n")
}

func (cg *CGenerator) writeSource(tu *TranslationUnit, sb *strings.Builder) {
	sb.WriteString("#include \"" + cg.generateFilename(tu.filepath, tu.name) + ".h\"\n")

	for _, stmt := range tu.module.Statements {
		switch n := stmt.(type) {
		case ImportDirective:
			//sb.WriteString("#include \"" + n.Module + ".h\"\n")
		case FunctionDeclaration:
			if n.Name == "main" {
				sb.WriteString("int main(int argc, char** argv)")
				continue
			}
			sb.WriteString(fmt.Sprintf("void %s_%s_%s(", cg.prefix, tu.name, n.Name))
			if len(n.Parameters) == 0 {
				sb.WriteString("void")
			} else {
				for i, p := range n.Parameters {
					if i > 0 {
						sb.WriteString(", ")
					}
					sb.WriteString(p.Type + " " + p.Name)
				}
			}
			sb.WriteString(")")
		case Body:
			sb.WriteString(" {\n")
			for _, stmt := range n.Statements {
				switch n := stmt.(type) {
				case FunctionCall:
					sb.WriteString(fmt.Sprintf("\t%s_%s(", cg.prefix, n.Name))
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

func (cg *CGenerator) generateFilename(filename, name string) string {
	return fmt.Sprintf("%s_%s", name, filepath.Base(filename))
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
