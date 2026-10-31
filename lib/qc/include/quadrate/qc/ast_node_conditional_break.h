#ifndef QD_QC_AST_NODE_CONDITIONAL_BREAK_H
#define QD_QC_AST_NODE_CONDITIONAL_BREAK_H

#include "ast_node_leaf.h"

namespace Qd {
	// `while` and `until` inside a loop body: pop a flag and leave the innermost loop on it.
	// `while` leaves when the flag is false, `until` when it is true, so
	//
	//     loop { i n < while  ... }       loop { ... done until }
	//
	// are a loop tested at the top and one tested at the bottom. Each is a word with the fixed
	// effect ( flag -- ): the condition is whatever came before it, as for every other word, and
	// nothing has to be found by looking backwards.
	class AstNodeConditionalBreak : public AstNodeLeaf<IAstNode::Type::CONDITIONAL_BREAK> {
	public:
		explicit AstNodeConditionalBreak(bool breakWhen) : mBreakWhen(breakWhen) {
		}

		// The flag value that leaves the loop: true for `until`, false for `while`
		bool breakWhen() const {
			return mBreakWhen;
		}

		const char* keyword() const {
			return mBreakWhen ? "until" : "while";
		}

	private:
		bool mBreakWhen;
	};
}

#endif
