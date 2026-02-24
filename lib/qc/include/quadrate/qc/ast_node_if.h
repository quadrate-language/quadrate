#ifndef QD_QC_AST_NODE_IF_H
#define QD_QC_AST_NODE_IF_H

#include "ast_node_base.h"

namespace Qd {
	class AstNodeIfStatement : public AstNodeBase<IAstNode::Type::IF_STATEMENT> {
	public:
		AstNodeIfStatement() : mThenBody(nullptr), mElseBody(nullptr) {
		}

		~AstNodeIfStatement() {
			delete mThenBody;
			delete mElseBody;
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
		IAstNode* mThenBody;
		IAstNode* mElseBody;
	};
}

#endif
