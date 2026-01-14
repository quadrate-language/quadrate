#ifndef QD_QC_AST_NODE_LEAF_H
#define QD_QC_AST_NODE_LEAF_H

#include "ast_node_base.h"

namespace Qd {
	/**
	 * @brief Template base class for leaf AST nodes (nodes with no children)
	 *
	 * This template provides common implementation for simple statement nodes
	 * like break, continue, and return that have no children.
	 *
	 * @tparam NodeType The IAstNode::Type value for this node type
	 */
	template <IAstNode::Type NodeType>
	class AstNodeLeaf : public AstNodeBase<NodeType> {
	public:
		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
			return nullptr;
		}
	};
}

#endif
