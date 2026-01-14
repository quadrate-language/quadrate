#ifndef QD_QC_AST_NODE_LOOP_H
#define QD_QC_AST_NODE_LOOP_H

#include "ast_node_base.h"

namespace Qd {
	class AstNodeLoopStatement : public AstNodeBase<IAstNode::Type::LOOP_STATEMENT> {
	public:
		AstNodeLoopStatement() : mBody(nullptr) {
		}

		~AstNodeLoopStatement() {
			delete mBody;
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

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

	private:
		IAstNode* mBody;
	};
}

#endif
