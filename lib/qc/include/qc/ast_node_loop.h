#ifndef QD_QC_AST_NODE_LOOP_H
#define QD_QC_AST_NODE_LOOP_H

#include "ast_node.h"

namespace Qd {
	class AstNodeLoopStatement : public IAstNode {
	public:
		AstNodeLoopStatement() : mParent(nullptr), mBody(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeLoopStatement() {
			if (mBody) {
				delete mBody;
			}
		}

		IAstNode::Type type() const override {
			return Type::LOOP_STATEMENT;
		}

		size_t childCount() const override {
			return mBody ? 1 : 0;
		}

		IAstNode* child(size_t index) const override {
			if (index == 0 && mBody) {
				return mBody;
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

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

	private:
		IAstNode* mParent;
		IAstNode* mBody;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
