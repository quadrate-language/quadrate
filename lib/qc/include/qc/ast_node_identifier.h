#ifndef QD_QC_AST_NODE_IDENTIFIER_H
#define QD_QC_AST_NODE_IDENTIFIER_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeIdentifier : public IAstNode {
	public:
		AstNodeIdentifier(const std::string& name) : mName(name), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::Identifier;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
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

		const std::string& name() const {
			return mName;
		}

	private:
		std::string mName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
