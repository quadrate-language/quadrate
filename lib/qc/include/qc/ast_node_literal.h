#ifndef QD_QC_AST_NODE_LITERAL_H
#define QD_QC_AST_NODE_LITERAL_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeLiteral : public IAstNode {
	public:
		enum class LiteralType {
			Integer,
			Float,
			String
		};

		AstNodeLiteral(const std::string& value, LiteralType literalType)
			: mValue(value), mLiteralType(literalType), mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::Literal;
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
	};
}

#endif
