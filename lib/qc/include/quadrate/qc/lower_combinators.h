#ifndef QD_QC_LOWER_COMBINATORS_H
#define QD_QC_LOWER_COMBINATORS_H

#include "ast_node.h"

namespace Qd {
	// Rewrites every combinator in the tree -- dip, keep, bi, tri, both, times, each, map,
	// filter, fold -- into what a backend already runs: locals, `for`, `call`. A function value
	// written in place before one is inlined when its body can stand in the enclosing function
	// unchanged; otherwise it is bound and called. Runs after validation, which is where a
	// combinator's types are checked, and before code generation or interpretation. Running it
	// twice changes nothing.
	void lowerCombinators(IAstNode* root);
}

#endif
