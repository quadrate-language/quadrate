package diagnostic

type SourceSpan struct {
	Column int `json:"column"`
	Length int `json:"length"`
	Line   int `json:"line"`
	Offset int `json:"offset"`
}
