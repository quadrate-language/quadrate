#ifndef QD_QC_AST_NODE_RETURN_H
#define QD_QC_AST_NODE_RETURN_H

#include "ast_node_leaf.h"

namespace Qd {
	class AstNodeReturn : public AstNodeLeaf<IAstNode::Type::RETURN_STATEMENT> {};
}

#endif
