#ifndef QD_CGEN_WRITER_H
#define QD_CGEN_WRITER_H

#include <sstream>

namespace Qd {
	class IAstNode;

	class Writer {
	public:
		void write(IAstNode* root, const char* packageName, const char* filename) const;
		void writeMain(const char* filename) const;

	private:
		void writeHeader(std::stringstream& out) const;
		void traverse(IAstNode* node, const char* packageName, std::stringstream& out, int indent = 0) const;
		void writeFooter(std::stringstream& out) const;
	};
}

#endif
