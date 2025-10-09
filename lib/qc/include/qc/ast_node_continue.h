#ifndef QD_QC_AST_NODE_CONTINUE_H
#define QD_QC_AST_NODE_CONTINUE_H

#include "ast_node.h"

namespace Qd {
	class AstNodeContinue : public IAstNode {
	public:
		AstNodeContinue() : mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::ContinueStatement;
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
