package diagnostic

type Issues []*Issue

func (i Issues) Error() string {
	if len(i) == 0 {
		return ""
	}
	var result string
	for _, issue := range i {
		result += issue.Message + "\n"
	}
	return result
}

type Issue struct {
	SourceSpan
	Message  string
	Notes    []string
	Category int
	Severity int
}
