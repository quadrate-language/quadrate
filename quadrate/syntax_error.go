package quadrate

type SyntaxError struct {
	Message  string
	Line     int
	Column   int
	Filename string
}

type SemanticError SyntaxError
