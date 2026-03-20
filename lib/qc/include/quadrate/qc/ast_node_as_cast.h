#ifndef QD_QC_AST_NODE_AS_CAST_H
#define QD_QC_AST_NODE_AS_CAST_H

#include "ast_node.h"
#include <string>

namespace Qd {
	/**
	 * @brief AST node representing an 'as' type narrowing cast
	 *
	 * Example: c as Ctx @body
	 * Narrows a ptr value to the specified struct type for field access.
	 * This is a compile-time only operation with no runtime cost.
	 */
	class AstNodeAsCast : public IAstNode {
	public:
		explicit AstNodeAsCast(const std::string& typeName)
			: mTypeName(typeName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::AS_CAST;
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

		const std::string& typeName() const {
			return mTypeName;
		}

	private:
		std::string mTypeName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
