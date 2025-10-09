#ifndef QD_QC_AST_NODE_DEFER_H
#define QD_QC_AST_NODE_DEFER_H

#include "ast_node.h"
#include <vector>

namespace Qd {
	class AstNodeDefer : public IAstNode {
	public:
		AstNodeDefer() : mParent(nullptr) {
		}

		~AstNodeDefer() override {
			for (auto* child : mChildren) {
				delete child;
			}
		}

		IAstNode::Type type() const override {
			return Type::DeferStatement;
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

	private:
		IAstNode* mParent;
		std::vector<IAstNode*> mChildren;
	};
}

#endif
