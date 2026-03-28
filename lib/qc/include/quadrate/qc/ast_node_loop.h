#ifndef QD_QC_AST_NODE_LOOP_H
#define QD_QC_AST_NODE_LOOP_H

#include "ast_node_base.h"
#include <memory>

namespace Qd {
	class AstNodeLoopStatement : public AstNodeBase<IAstNode::Type::LOOP_STATEMENT> {
	public:
		AstNodeLoopStatement() {
		}

		~AstNodeLoopStatement() = default;

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
