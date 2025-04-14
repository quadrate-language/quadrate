package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"git.sr.ht/~klahr/quadrate/quadrate"
	"github.com/acarl005/stripansi"
)

var Version string

func printMessage(message string, noColors bool) {
	if noColors {
		fmt.Println(stripansi.Strip(message))
	} else {
		fmt.Println(message)
	}
}

func main() {
	var args quadrate.Args
	flag.StringVar(&args.Output, "o", "a.out", "Output filename")
	flag.BoolVar(&args.Run, "run", false, "Run the compiled program")
	flag.BoolVar(&args.SaveTemps, "save-temps", false, "Save temporary files")
	flag.BoolVar(&args.DumpTokens, "dump-tokens", false, "Print tokens")
	flag.BoolVar(&args.Version, "version", false, "Print version information")
	flag.BoolVar(&args.NoColors, "no-colors", false, "Disable colored output")
	flag.Parse()

	args.Sources = flag.Args()

	if args.Version {
		fmt.Printf("quadc %s\n", Version)
		os.Exit(0)
	}

	if args.Run {
		if f, err := os.CreateTemp("", "qd-*.out"); err != nil {
			printMessage(fmt.Sprintf("\033[1mquadc: \033[31mfatal error:\033[0m %s", err.Error()), args.NoColors)
			os.Exit(1)
		} else {
			args.Output = f.Name()
			defer os.Remove(args.Output)
			f.Close()
		}
	}

	if len(args.Sources) == 0 {
		printMessage(fmt.Sprintf("\033[1mquadc: \033[31mfatal error:\033[0m no input files"), args.NoColors)
		os.Exit(1)
	}

	var absFilepaths []string
	for _, file := range args.Sources {
		if _, err := os.Stat(file); os.IsNotExist(err) {
			printMessage(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m no such file or directory: %s", file), args.NoColors)
			os.Exit(1)
		} else {
			if absPath, err := filepath.Abs(file); err != nil {
				printMessage(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m %s", err.Error()), args.NoColors)
				os.Exit(1)
			} else {
				absFilepaths = append(absFilepaths, absPath)
			}
		}
	}

	compiler := quadrate.NewCompiler(args.DumpTokens, args.Output)
	if tus, err := compiler.Compile(absFilepaths); err != nil {
		if b, e := os.ReadFile(err.Filename); e != nil {
			printMessage(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m %s", e.Error()), args.NoColors)
		} else {
			lines := strings.Split(string(b), "\n")
			printMessage(fmt.Sprintf("\033[1m%s:%d:%d: \033[31merror:\033[0m %s", err.Filename, err.Line, err.Column+1, err.Message), args.NoColors)
			if err.Line >= 1 {
				printMessage(fmt.Sprintf("%d | %s", err.Line, strings.Replace(lines[err.Line-1], "\t", " ", -1)), args.NoColors)
				printMessage(fmt.Sprintf("%s | %s\033[1;31m^\033[0m", strings.Repeat(" ", len(fmt.Sprintf("%d", err.Line))), strings.Repeat(" ", err.Column)), args.NoColors)
			}
		}
		os.Exit(1)
	} else {
		sa := quadrate.NewSemanticAnalyzer(args.DumpTokens)
		if err := sa.Analyze(tus); err != nil {
			if b, e := os.ReadFile(err.Filename); e != nil {
				printMessage(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m %s", e.Error()), args.NoColors)
			} else {
				lines := strings.Split(string(b), "\n")
				printMessage(fmt.Sprintf("\033[1m%s:%d:%d: \033[31merror:\033[0m %s", err.Filename, err.Line, err.Column+1, err.Message), args.NoColors)
				if err.Line >= 1 {
					printMessage(fmt.Sprintf("%d | %s", err.Line, strings.Replace(lines[err.Line-1], "\t", " ", -1)), args.NoColors)
					printMessage(fmt.Sprintf("%s | %s\033[1;31m^\033[0m", strings.Repeat(" ", len(fmt.Sprintf("%d", err.Line))), strings.Repeat(" ", err.Column)), args.NoColors)
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
				printMessage(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m %s", err.Message), args.NoColors)
				if !args.SaveTemps {
					os.RemoveAll("./.qd_gen")
				}
				os.Exit(1)
			}
		}

		compiler.CompileAndLink()

		if args.Run {
			cmd := exec.Command(args.Output)
			cmd.Stdout = os.Stdout
			cmd.Stderr = os.Stderr
			if err := cmd.Run(); err != nil {
				printMessage(fmt.Sprintf("\033[1mquadc: \033[31merror:\033[0m %s", err.Error()), args.NoColors)
				if exitErr, ok := err.(*exec.ExitError); ok {
					if status, ok := exitErr.ProcessState.Sys().(interface{ ExitStatus() int }); ok {
						os.Exit(status.ExitStatus())
					} else {
						os.Exit(1)
					}
				}
			}
		}

		if !args.SaveTemps {
			os.RemoveAll("./.qd_gen")
		}
		os.Exit(0)
	}
}
