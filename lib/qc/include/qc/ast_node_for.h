#ifndef QD_QC_AST_NODE_FOR_H
#define QD_QC_AST_NODE_FOR_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeForStatement : public IAstNode {
	public:
		AstNodeForStatement(const std::string& loopVar) : mLoopVar(loopVar), mParent(nullptr), mBody(nullptr) {
		}

		~AstNodeForStatement() {
			if (mBody) {
				delete mBody;
			}
		}

		IAstNode::Type type() const override {
			return Type::ForStatement;
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

		const std::string& loopVar() const {
			return mLoopVar;
		}

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

	private:
		std::string mLoopVar;
		IAstNode* mParent;
		IAstNode* mBody;
	};
}

#endif
