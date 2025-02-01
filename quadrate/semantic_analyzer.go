package quadrate

type SemanticAnalyzer struct {
	lexResult *LexResult
}

func NewSemanticAnalyzer(lexResult *LexResult) *SemanticAnalyzer {
	return &SemanticAnalyzer{
		lexResult: lexResult,
	}
}

func (sa *SemanticAnalyzer) Analyze(pgm *Program) *SyntaxError {
	return nil
}
