#ifndef QD_QC_AST_NODE_BLOCK_H
#define QD_QC_AST_NODE_BLOCK_H

#include <memory>
#include <quadrate/qc/ast_node.h>
#include <vector>

namespace Qd {
	class AstNodeBlock : public IAstNode {
	public:
		// Destructor is default — unique_ptr handles cleanup
		~AstNodeBlock() = default;

		IAstNode::Type type() const override {
			return Type::BLOCK;
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

		// Takes ownership of node via unique_ptr
		void addChild(IAstNode* node) {
			mChildren.emplace_back(node);
		}

	private:
		IAstNode* mParent = nullptr;
		std::vector<std::unique_ptr<IAstNode>> mChildren;
		size_t mLine = 0;
		size_t mColumn = 0;
	};
}

#endif
