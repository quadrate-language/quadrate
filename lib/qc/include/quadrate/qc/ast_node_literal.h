#ifndef QD_QC_AST_NODE_LITERAL_H
#define QD_QC_AST_NODE_LITERAL_H

#include "ast_node_base.h"
#include <string>

namespace Qd {
	class AstNodeLiteral : public AstNodeBase<IAstNode::Type::LITERAL> {
	public:
		enum class LiteralType {
			INTEGER,
			FLOAT,
			STRING,
			NULL_PTR
		};

		AstNodeLiteral(const std::string& value, LiteralType literalType) : mValue(value), mLiteralType(literalType) {
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
			return nullptr;
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
	};
}

#endif
