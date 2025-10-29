#ifndef QD_QC_AST_NODE_BREAK_H
#define QD_QC_AST_NODE_BREAK_H

#include "ast_node.h"

namespace Qd {
	class AstNodeBreak : public IAstNode {
	public:
		AstNodeBreak() : mParent(nullptr), mLine(0), mColumn(0) {
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
		size_t mLine;
		size_t mColumn;
	};
}

#endif
