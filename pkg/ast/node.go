package ast

type Node interface {
	Items() []Node
	Append(item Node)
}
