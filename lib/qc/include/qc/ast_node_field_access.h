#ifndef QD_QC_AST_NODE_FIELD_ACCESS_H
#define QD_QC_AST_NODE_FIELD_ACCESS_H

#include "ast_node.h"
#include <string>

namespace Qd {
	/**
	 * @brief AST node representing field access
	 *
	 * Example: v @x
	 * Accesses field 'x' from struct in local variable 'v'
	 */
	class AstNodeFieldAccess : public IAstNode {
	public:
		AstNodeFieldAccess(const std::string& varName, const std::string& fieldName)
			: mVarName(varName), mFieldName(fieldName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::FIELD_ACCESS;
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

		const std::string& varName() const {
			return mVarName;
		}

		const std::string& fieldName() const {
			return mFieldName;
		}

	private:
		std::string mVarName;
		std::string mFieldName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
