#ifndef QD_QC_AST_PRINTER_H
#define QD_QC_AST_PRINTER_H

namespace Qd {
	class IAstNode;

	class AstPrinter {
	public:
		static void print(const IAstNode* node);
	};
}

#endif
