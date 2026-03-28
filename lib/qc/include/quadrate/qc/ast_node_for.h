#ifndef QD_QC_AST_NODE_FOR_H
#define QD_QC_AST_NODE_FOR_H

#include "ast_node_base.h"
#include <memory>
#include <string>

namespace Qd {
	class AstNodeForStatement : public AstNodeBase<IAstNode::Type::FOR_STATEMENT> {
	public:
		AstNodeForStatement() : mIteratorName("it") {
		}

		~AstNodeForStatement() = default;

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

		void setIteratorName(const std::string& name) {
			mIteratorName = name;
		}

		const std::string& iteratorName() const {
			return mIteratorName;
		}

	private:
		std::unique_ptr<IAstNode> mBody;
		std::string mIteratorName;
	};
}

#endif
