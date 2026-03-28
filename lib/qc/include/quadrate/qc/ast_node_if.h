#ifndef QD_QC_AST_NODE_IF_H
#define QD_QC_AST_NODE_IF_H

#include "ast_node_base.h"
#include <memory>

namespace Qd {
	class AstNodeIfStatement : public AstNodeBase<IAstNode::Type::IF_STATEMENT> {
	public:
		AstNodeIfStatement() {
		}

		~AstNodeIfStatement() = default;

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
				return mThenBody.get();
			}
			if (mElseBody && index == currentIndex++) {
				return mElseBody.get();
			}
			return nullptr;
		}

		void setThenBody(IAstNode* thenBody) {
			mThenBody.reset(thenBody);
		}

		void setElseBody(IAstNode* elseBody) {
			mElseBody.reset(elseBody);
		}

		IAstNode* thenBody() const {
			return mThenBody.get();
		}

		IAstNode* elseBody() const {
			return mElseBody.get();
		}

	private:
		std::unique_ptr<IAstNode> mThenBody;
		std::unique_ptr<IAstNode> mElseBody;
	};
}

#endif
