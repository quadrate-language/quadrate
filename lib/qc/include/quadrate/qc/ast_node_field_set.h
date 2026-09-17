#ifndef QD_QC_AST_NODE_FIELD_SET_H
#define QD_QC_AST_NODE_FIELD_SET_H

#include "ast_node.h"
#include <string>

namespace Qd {
	/**
	 * @brief AST node representing field mutation (set)
	 *
	 * Example: foo 42 >>x
	 * Pops 42 and sets field 'x' of the struct beneath it; the struct stays on the stack.
	 */
	class AstNodeFieldSet : public IAstNode {
	public:
		AstNodeFieldSet(const std::string& fieldName) : mFieldName(fieldName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::FIELD_SET;
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

		const std::string& fieldName() const {
			return mFieldName;
		}

	private:
		std::string mFieldName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
