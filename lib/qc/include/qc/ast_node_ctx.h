#ifndef QD_QC_AST_NODE_CTX_H
#define QD_QC_AST_NODE_CTX_H

#include "ast_node.h"
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
	class AstNodeCtx : public IAstNode {
	public:
		AstNodeCtx() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeCtx() override {
			for (auto* child : mChildren) {
				delete child;
			}
		}

		IAstNode::Type type() const override {
			return Type::CTX_STATEMENT;
		}

		void addChild(IAstNode* child) {
			mChildren.push_back(child);
		}

		size_t childCount() const override {
			return mChildren.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mChildren.size()) {
				return mChildren[index];
			}
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

		size_t line() const override {
			return mLine;
		}

		size_t column() const override {
			return mColumn;
		}

		void setPosition(size_t line, size_t column) override {
			mLine = line;
			mColumn = column;
		}

	private:
		IAstNode* mParent;
		std::vector<IAstNode*> mChildren;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
