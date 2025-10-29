#ifndef QD_QC_AST_NODE_FOR_H
#define QD_QC_AST_NODE_FOR_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeForStatement : public IAstNode {
	public:
		AstNodeForStatement(const std::string& loopVar)
			: mLoopVar(loopVar), mParent(nullptr), mBody(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeForStatement() {
			if (mBody) {
				delete mBody;
			}
		}

		IAstNode::Type type() const override {
			return Type::FOR_STATEMENT;
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
		size_t mLine;
		size_t mColumn;
	};
}

#endif
