#ifndef QD_QC_AST_H
#define QD_QC_AST_H

#include <u8t/scanner.h>

namespace Qd {
	class IAstNode;

	class Ast {
	public:
		~Ast();

		IAstNode* generate(const char* src);

	private:
		IAstNode* mRoot = nullptr;
	};
}

#endif
