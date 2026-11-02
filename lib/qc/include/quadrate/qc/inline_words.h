#ifndef QD_QC_INLINE_WORDS_H
#define QD_QC_INLINE_WORDS_H

#include "ast_node.h"

namespace Qd {
	// Replaces each call to an `inline fn` declared in the same tree with a copy of its body,
	// as a Forth word is the words it is made of. Only a word whose body can stand in its
	// caller unchanged is spliced: a stack `fn`, neither fallible nor generic nor a method, whose
	// body names nothing of the function it is in -- no local but the hidden ones lowering
	// makes, no `return` or `defer`, no loop exit but from a loop of its own -- and is built of
	// statements and words alone. A word is not spliced into itself, and not where the caller
	// declares a local of a name the body uses.
	//
	// Code generation decides on the AST whether a function has a native version, which a call
	// to one with more than one result rules out; spliced, a factored word costs nothing.
	// Runs after validation and lowerCombinators, before code generation.
	void inlineWords(IAstNode* root);
}

#endif
