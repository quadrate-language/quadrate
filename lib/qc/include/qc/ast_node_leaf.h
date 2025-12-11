#ifndef QD_QC_AST_NODE_LEAF_H
#define QD_QC_AST_NODE_LEAF_H

#include "ast_node.h"

namespace Qd {
	/**
	 * @brief Template base class for leaf AST nodes (nodes with no children)
	 *
	 * This template provides common implementation for simple statement nodes
	 * like break, continue, and return that have no children and only store
	 * position information.
	 *
	 * @tparam NodeType The IAstNode::Type value for this node type
	 */
	template<IAstNode::Type NodeType>
	class AstNodeLeaf : public IAstNode {
	public:
		AstNodeLeaf() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return NodeType;
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
}

#endif
