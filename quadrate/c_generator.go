package quadrate

import (
	_ "embed"
	"fmt"
	"os"
	"path"
	"path/filepath"
	"strconv"
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
	os.Mkdir(cg.outputDir, 0755)

	if err := os.WriteFile(path.Join(cg.outputDir, "qd_base.h"), []byte(baseHeader), 0644); err != nil {
		panic(err)
	}
	if err := os.WriteFile(path.Join(cg.outputDir, "qd_base.c"), []byte(baseSource), 0644); err != nil {
		panic(err)
	}

	return &cg
}

func (cg *CGenerator) Generate(tu *TranslationUnit) *SyntaxError {
	var sb strings.Builder

	cg.writeHeader(tu, &sb)
	filename := path.Join(cg.outputDir, cg.generateFilename(tu.filepath, tu.name))
	os.WriteFile(fmt.Sprintf("%s.h", filename), []byte(sb.String()), 0644)

	sb.Reset()
	cg.writeSource(tu, &sb)
	os.WriteFile(fmt.Sprintf("%s.c", filename), []byte(sb.String()), 0644)

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
			sb.WriteString(fmt.Sprintf("void %s_%s_%s(int n, ...);\n", cg.prefix, tu.name, n.Name))
			/*			if len(n.Parameters) == 0 {
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
			*/
		case ConstValue:
			s := fmt.Sprintf("extern const __qd_real_t %s_%s_%s;\n", cg.prefix, tu.name, n.Name)
			s = strings.ReplaceAll(s, "__qd__", "__qd_")
			sb.WriteString(s)
		default:
			continue
		}
	}

	sb.WriteString("\n#endif\n")
}

func isFloat(s string) bool {
	_, err := strconv.ParseFloat(s, 64)
	return err == nil
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
			s := fmt.Sprintf("void %s_%s_%s(int n, ...)", cg.prefix, tu.name, n.Name)
			s = strings.ReplaceAll(s, "__qd__", "__qd_")
			sb.WriteString(s)
			/*			if len(n.Parameters) == 0 {
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
			*/
		case Body:
			sb.WriteString(" {\n")
			for _, stmt := range n.Statements {
				switch n := stmt.(type) {
				case FunctionCall:
					sb.WriteString(fmt.Sprintf("\t%s_%s(", cg.prefix, n.Name))
					if len(n.Args) == 0 {
						sb.WriteString("0")
					} else {
						for i, arg := range n.Args {
							if i > 0 {
								sb.WriteString(", ")
							} else {
								if isFloat(arg) {
									sb.WriteString(fmt.Sprintf("%d, (__qd_real_t)%s", len(n.Args), arg))
								} else {
									sb.WriteString(fmt.Sprintf("%d, %s", len(n.Args), arg))
								}
								continue
							}
							sb.WriteString("(__qd_real_t)" + arg)
						}
					}
					sb.WriteString(");\n")
				case InlineCCode:
					sb.WriteString(n.Code + "\n")
				case ReturnStatement:
					sb.WriteString("\treturn;\n")
				case Label:
					sb.WriteString(n.Name + ":;\n")
				case Jmp:
					sb.WriteString("\tgoto " + n.Label + ";\n")
				case Je:
					sb.WriteString("\tif (__qd_stack_ptr <= 1) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-1] == __qd_stack[__qd_stack_ptr-2]) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jg:
					sb.WriteString("\tif (__qd_stack_ptr <= 1) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-2] > __qd_stack[__qd_stack_ptr-1]) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jge:
					sb.WriteString("\tif (__qd_stack_ptr <= 1) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-2] >= __qd_stack[__qd_stack_ptr-1]) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jl:
					sb.WriteString("\tif (__qd_stack_ptr <= 1) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-2] < __qd_stack[__qd_stack_ptr-1]) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jle:
					sb.WriteString("\tif (__qd_stack_ptr <= 1) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-2] <= __qd_stack[__qd_stack_ptr-1]) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jne:
					sb.WriteString("\tif (__qd_stack_ptr <= 1) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-1] != __qd_stack[__qd_stack_ptr-2]) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jnz:
					sb.WriteString("\tif (__qd_stack_ptr == 0) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-1] != 0.0) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				case Jz:
					sb.WriteString("\tif (__qd_stack_ptr == 0) {\n\t\t__qd_panic_stack_underflow();\n\t}\n")
					sb.WriteString("\tif (__qd_stack[__qd_stack_ptr-1] == 0.0) {\n\t\tgoto " + n.Label + ";\n\t}\n")
				}
			}
			sb.WriteString("}\n")
		case InlineCCode:
			sb.WriteString(n.Code + "\n")
		case ConstValue:
			s := fmt.Sprintf("const __qd_real_t %s_%s_%s = %s;\n", cg.prefix, tu.name, n.Name, n.Value)
			s = strings.ReplaceAll(s, "__qd__", "__qd_")
			sb.WriteString(s)
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
