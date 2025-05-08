package quadrate

type Args struct {
	DumpTokens        bool
	NoColors          bool
	Output            string
	Run               bool
	SaveTemps         bool
	Sources           []string
	Version           bool
	StackDepth        int
	MarkStackDepth    int
	CLinkLibraries    StringSlice
	CLinkLibraryPaths StringSlice
}
