#ifndef QD_QC_AST_NODE_BREAK_H
#define QD_QC_AST_NODE_BREAK_H

#include "ast_node.h"

namespace Qd {
	class AstNodeBreak : public IAstNode {
	public:
		AstNodeBreak() : mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::BreakStatement;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

	private:
		IAstNode* mParent;
	};
}

#endif
