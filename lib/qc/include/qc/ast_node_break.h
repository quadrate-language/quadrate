#ifndef QD_QC_AST_NODE_BREAK_H
#define QD_QC_AST_NODE_BREAK_H

#include "ast_node_leaf.h"

namespace Qd {
	class AstNodeBreak : public AstNodeLeaf<IAstNode::Type::BREAK_STATEMENT> {};
}

#endif
