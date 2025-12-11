#ifndef QD_QC_AST_NODE_CONTINUE_H
#define QD_QC_AST_NODE_CONTINUE_H

#include "ast_node_leaf.h"

namespace Qd {
	class AstNodeContinue : public AstNodeLeaf<IAstNode::Type::CONTINUE_STATEMENT> {};
}

#endif
