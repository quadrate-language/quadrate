#ifndef QD_CGEN_SOURCE_FILE_H
#define QD_CGEN_SOURCE_FILE_H

#include <string>

namespace Qd {
	struct SourceFile {
		std::string filename;
		std::string package;
		std::string content;
	};
}

#endif
