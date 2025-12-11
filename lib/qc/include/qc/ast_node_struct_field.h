#ifndef QD_QC_AST_NODE_STRUCT_FIELD_H
#define QD_QC_AST_NODE_STRUCT_FIELD_H

#include "ast_node.h"
#include <string>

namespace Qd {
	/**
	 * @brief AST node representing a struct field
	 */
	class AstNodeStructField : public IAstNode {
	public:
		AstNodeStructField(const std::string& name, const std::string& typeName)
			: mName(name), mTypeName(typeName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::STRUCT_FIELD;
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

		const std::string& typeName() const {
			return mTypeName;
		}

	private:
		std::string mName;
		std::string mTypeName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
