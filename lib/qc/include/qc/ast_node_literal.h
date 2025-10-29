#ifndef QD_QC_AST_NODE_LITERAL_H
#define QD_QC_AST_NODE_LITERAL_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeLiteral : public IAstNode {
	public:
		enum class LiteralType {
			INTEGER,
			FLOAT,
			STRING
		};

		AstNodeLiteral(const std::string& value, LiteralType literalType)
			: mValue(value), mLiteralType(literalType), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::LITERAL;
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

		const std::string& value() const {
			return mValue;
		}

		LiteralType literalType() const {
			return mLiteralType;
		}

	private:
		std::string mValue;
		LiteralType mLiteralType;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
