#ifndef QD_QC_AST_NODE_FOR_H
#define QD_QC_AST_NODE_FOR_H

#include "ast_node_base.h"
#include <string>

namespace Qd {
	class AstNodeForStatement : public AstNodeBase<IAstNode::Type::FOR_STATEMENT> {
	public:
		AstNodeForStatement() : mBody(nullptr), mIteratorName("it") {
		}

		~AstNodeForStatement() {
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

		void setIteratorName(const std::string& name) {
			mIteratorName = name;
		}

		const std::string& iteratorName() const {
			return mIteratorName;
		}

	private:
		IAstNode* mBody;
		std::string mIteratorName;
	};
}

#endif
