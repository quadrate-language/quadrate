#ifndef QD_QC_AST_H
#define QD_QC_AST_H

#include <u8t/scanner.h>

namespace Qd {
	class IAstNode;

	class Ast {
	public:
		IAstNode* generate(const char* src) const;
	};
}

#endif
