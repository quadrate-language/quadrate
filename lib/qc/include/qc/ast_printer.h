#ifndef QD_QC_AST_PRINTER_H
#define QD_QC_AST_PRINTER_H

#include "ast_node.h"

namespace Qd {
	class AstPrinter {
	public:
		static void print(const IAstNode* node);
		static void printTree(const IAstNode* node, const char* prefix = "", bool isLast = true);
	};
}

#endif
