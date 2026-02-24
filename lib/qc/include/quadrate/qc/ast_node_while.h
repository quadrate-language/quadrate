#ifndef QD_QC_AST_NODE_WHILE_H
#define QD_QC_AST_NODE_WHILE_H

#include "ast_node_base.h"

namespace Qd {
	class AstNodeWhileStatement : public AstNodeBase<IAstNode::Type::WHILE_STATEMENT> {
	public:
		AstNodeWhileStatement() : mBody(nullptr) {
		}

		~AstNodeWhileStatement() {
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
