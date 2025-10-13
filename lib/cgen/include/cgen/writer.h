#ifndef QD_CGEN_WRITER_H
#define QD_CGEN_WRITER_H

#include <sstream>

namespace Qd {
	class IAstNode;

	class Writer {
	public:
		void write(IAstNode* root, const char* filename) const;

	private:
		void traverse(IAstNode* node, std::stringstream& out) const;
	};
}

#endif
