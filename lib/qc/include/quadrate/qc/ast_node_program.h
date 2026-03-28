#ifndef QD_QC_AST_NODE_PROGRAM_H
#define QD_QC_AST_NODE_PROGRAM_H

#include "ast_node.h"
#include <memory>
#include <vector>

namespace Qd {
	class AstProgram : public IAstNode {
	public:
		~AstProgram() = default;

		IAstNode::Type type() const override {
			return Type::PROGRAM;
		}

		virtual size_t childCount() const override {
			return mChildren.size();
		}

		virtual IAstNode* child(size_t index) const override {
			if (index < mChildren.size()) {
				return mChildren[index].get();
			}
			return nullptr;
		}

		virtual IAstNode* parent() const override {
			return nullptr;
		}

		virtual void setParent(IAstNode*) override {
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
			mChildren.emplace_back(node);
		}

	private:
		std::vector<std::unique_ptr<IAstNode>> mChildren;
		size_t mLine = 0;
		size_t mColumn = 0;
	};
}

#endif
