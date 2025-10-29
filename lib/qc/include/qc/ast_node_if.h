#ifndef QD_QC_AST_NODE_IF_H
#define QD_QC_AST_NODE_IF_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeIfStatement : public IAstNode {
	public:
		AstNodeIfStatement() : mParent(nullptr), mThenBody(nullptr), mElseBody(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeIfStatement() {
			if (mThenBody) {
				delete mThenBody;
			}
			if (mElseBody) {
				delete mElseBody;
			}
		}

		IAstNode::Type type() const override {
			return Type::IF_STATEMENT;
		}

		size_t childCount() const override {
			size_t count = 0;
			if (mThenBody) {
				count++;
			}
			if (mElseBody) {
				count++;
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t currentIndex = 0;
			if (mThenBody && index == currentIndex++) {
				return mThenBody;
			}
			if (mElseBody && index == currentIndex++) {
				return mElseBody;
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

		void setThenBody(IAstNode* thenBody) {
			mThenBody = thenBody;
		}

		void setElseBody(IAstNode* elseBody) {
			mElseBody = elseBody;
		}

		IAstNode* thenBody() const {
			return mThenBody;
		}

		IAstNode* elseBody() const {
			return mElseBody;
		}

	private:
		IAstNode* mParent;
		IAstNode* mThenBody;
		IAstNode* mElseBody;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
