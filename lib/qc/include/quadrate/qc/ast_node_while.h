#ifndef QD_QC_AST_NODE_WHILE_H
#define QD_QC_AST_NODE_WHILE_H

#include "ast_node_base.h"
#include <memory>

namespace Qd {
	class AstNodeWhileStatement : public AstNodeBase<IAstNode::Type::WHILE_STATEMENT> {
	public:
		AstNodeWhileStatement() {
		}

		~AstNodeWhileStatement() = default;

		size_t childCount() const override {
			return mBody ? 1 : 0;
		}

		IAstNode* child(size_t index) const override {
			if (index == 0 && mBody) {
				return mBody.get();
			}
			return nullptr;
		}

		void setBody(IAstNode* body) {
			mBody.reset(body);
		}

		IAstNode* body() const {
			return mBody.get();
		}

	private:
		std::unique_ptr<IAstNode> mBody;
	};
}

#endif
