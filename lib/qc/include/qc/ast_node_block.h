#ifndef QD_QC_AST_NODE_BLOCK_H
#define QD_QC_AST_NODE_BLOCK_H

#include "ast_node.h"
#include <vector>

namespace Qd {
	class AstNodeBlock : public IAstNode {
	public:
		~AstNodeBlock() {
			for (auto* child : mChildren) {
				delete child;
			}
		}

		IAstNode::Type type() const override {
			return Type::BLOCK;
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

		void addChild(IAstNode* node) {
			mChildren.push_back(node);
		}

	private:
		IAstNode* mParent = nullptr;
		std::vector<IAstNode*> mChildren;
		size_t mLine = 0;
		size_t mColumn = 0;
	};
}

#endif
