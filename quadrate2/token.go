package quadrate2

type TokenType string

const (
	EOF2 = "EOF"
	EOL  = "EOL"
)

type Token struct {
	Type      TokenType `json:"type"`
	Value     string    `json:"value"`
	Line      int       `json:"line"`
	Character int       `json:"character"`
	Length    int       `json:"length"`
	Offset    int       `json:"offset"`
}
