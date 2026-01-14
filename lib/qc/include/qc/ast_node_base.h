#ifndef QD_QC_AST_NODE_BASE_H
#define QD_QC_AST_NODE_BASE_H

#include "ast_node.h"

namespace Qd {
	/**
	 * @brief Template base class for AST nodes with common parent/position handling
	 *
	 * This template provides common implementation for parent pointer and position
	 * tracking. Derived classes must still implement childCount() and child().
	 *
	 * @tparam NodeType The IAstNode::Type value for this node type
	 */
	template <IAstNode::Type NodeType>
	class AstNodeBase : public IAstNode {
	public:
		AstNodeBase() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return NodeType;
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

	protected:
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif // QD_QC_AST_NODE_BASE_H
