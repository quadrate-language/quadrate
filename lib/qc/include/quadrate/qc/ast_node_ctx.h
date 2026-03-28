#ifndef QD_QC_AST_NODE_CTX_H
#define QD_QC_AST_NODE_CTX_H

#include "ast_node_base.h"
#include <memory>
#include <vector>

namespace Qd {
	/**
	 * @brief AST node for ctx blocks
	 *
	 * The ctx block executes statements in an isolated context.
	 * The parent context (including stack) is deep copied, the block executes
	 * in the child context, and exactly one value is returned to the parent.
	 *
	 * Syntax: ctx { statements }
	 * Stack effect: ( S -- S r ) where r is the single value returned
	 */
	class AstNodeCtx : public AstNodeBase<IAstNode::Type::CTX_STATEMENT> {
	public:
		~AstNodeCtx() override = default;

		void addChild(IAstNode* child) {
			mChildren.emplace_back(child);
		}

		size_t childCount() const override {
			return mChildren.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mChildren.size()) {
				return mChildren[index].get();
			}
			return nullptr;
		}

	private:
		std::vector<std::unique_ptr<IAstNode>> mChildren;
	};
}

#endif
