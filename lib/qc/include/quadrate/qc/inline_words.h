#ifndef QD_QC_INLINE_WORDS_H
#define QD_QC_INLINE_WORDS_H

#include "ast_node.h"

#include <string>
#include <vector>

namespace Qd {
	// Replaces each call to an `inline fn` declared in the same tree with a copy of its body,
	// as a Forth word is the words it is made of. Only a word whose body can stand in its
	// caller unchanged is spliced: a stack `fn`, neither fallible nor generic nor a method, whose
	// body names nothing of the function it is in -- no local but the hidden ones lowering
	// makes, no `return` or `defer`, no loop exit but from a loop of its own -- and is built of
	// statements and words alone. A word is not spliced into itself, and not where the caller
	// declares a local of a name the body uses.
	//
	// Spliced, a factored word costs nothing, not even the call its native version would be.
	// Across files: a word of a file merged from the program's directory is spliced as one of
	// the program's own, sharing its names, and a module's word wherever it is called as
	// `module::name`: resolveNames has named everything in it qualified, so it means the same
	// in any caller.
	//
	// Runs after validation and lowerCombinators, before code generation.
	struct InlineModule {
		std::string name;
		IAstNode* root;
		bool merged;
	};

	void inlineWords(IAstNode* root, const std::vector<InlineModule>& modules = {});
}

#endif
