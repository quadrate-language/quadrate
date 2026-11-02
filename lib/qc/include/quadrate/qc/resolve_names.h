#ifndef QD_QC_RESOLVE_NAMES_H
#define QD_QC_RESOLVE_NAMES_H

#include "ast_node.h"

#include <string>
#include <utility>
#include <vector>

namespace Qd {
	// Names a module's own declarations by their qualified names everywhere in the module:
	// `bump` becomes `m::bump`, `Scale` `m::Scale`, `-> count` `-> m::count`, `Point { }`
	// `m::Point { }`, `Mode::Fast` `m::Mode::Fast`, and the struct the validator found under a
	// field read or a method call `m::Point`. A local of the function keeps its name.
	//
	// After this a name means the same wherever its node goes, so nothing downstream asks which
	// module it is in: a struct default built in another module, a global's initializer run in
	// main, a word spliced into a caller elsewhere. Before it, each of those looked names up
	// against "the current module", and each got it wrong in its own way -- a module's call of its
	// own `bump` reached the program's `bump` when the program had one.
	//
	// For a module that is not merged into the program; a merged file shares the program's
	// names. Runs after validation and lowerCombinators, before inlining and code generation.
	// Each file is named with its module; a module of several files is resolved as one.
	void resolveNames(const std::vector<std::pair<std::string, IAstNode*>>& files);
}

#endif
