#ifndef QD_QC_AST_NODE_WHILE_H
#define QD_QC_AST_NODE_WHILE_H

#include "ast_node_base.h"
#include <memory>

namespace Qd {
	// `cond while { body }` -- loops while `cond` holds, re-evaluating it each iteration.
	//
	// The condition is not written twice. The parser moves the run of expression nodes that
	// precede the keyword into mCondition, so a source-level `x 0 > while { ... }` becomes a node
	// whose condition block holds `x 0 >` and whose body block holds the rest. Those nodes are
	// generated once per iteration, which is what makes restating them at the end of the body
	// unnecessary -- the shape the earlier `while` required, and the reason it saved nothing over
	// `loop { cond if { break } ... }`.
	class AstNodeWhileStatement : public AstNodeBase<IAstNode::Type::WHILE_STATEMENT> {
	public:
		AstNodeWhileStatement() {
		}

		~AstNodeWhileStatement() = default;

		size_t childCount() const override {
			size_t count = 0;
			if (mCondition) {
				count++;
			}
			if (mBody) {
				count++;
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t next = 0;
			if (mCondition) {
				if (index == next) {
					return mCondition.get();
				}
				next++;
			}
			if (mBody && index == next) {
				return mBody.get();
			}
			return nullptr;
		}

		void setCondition(IAstNode* condition) {
			mCondition.reset(condition);
		}

		IAstNode* condition() const {
			return mCondition.get();
		}

		// Index into the condition block at which the condition proper begins. The parser hands over
		// the whole run of expression nodes before the keyword, which can pick up a statement that
		// merely happens to be expression-shaped -- `"Processing..." print nl` nets zero and so is
		// invisible to a net-effect check, but re-running it every iteration prints it every
		// iteration. The validator, which knows each node's effect, trims the run to the shortest
		// suffix that produces the flag and records where it starts; anything before it is
		// generated once, before the loop.
		void setConditionStart(size_t index) {
			mConditionStart = index;
		}

		size_t conditionStart() const {
			return mConditionStart;
		}

		void setBody(IAstNode* body) {
			mBody.reset(body);
		}

		IAstNode* body() const {
			return mBody.get();
		}

	private:
		std::unique_ptr<IAstNode> mCondition;
		size_t mConditionStart = 0;
		std::unique_ptr<IAstNode> mBody;
	};
}

#endif
