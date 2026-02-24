#ifndef QD_QC_AST_NODE_ARRAY_INDEX_H
#define QD_QC_AST_NODE_ARRAY_INDEX_H

#include "ast_node.h"

namespace Qd {

	/**
	 * @brief AST node representing array element access
	 *
	 * Example: p@skills 0 nth
	 * Stack: ( array index -- value )
	 */
	class AstNodeArrayIndex : public IAstNode {
	public:
		AstNodeArrayIndex() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::ARRAY_INDEX;
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

	private:
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

} // namespace Qd

#endif
