#ifndef QD_CGEN_SOURCE_FILE_H
#define QD_CGEN_SOURCE_FILE_H

#include <string>
#include <unordered_set>

namespace Qd {
	struct SourceFile {
		std::string filename;
		std::string package;
		std::string content;
		std::unordered_set<std::string> importedModules;
		std::unordered_set<std::string> importedLibraries; // e.g., "libstdqd.so"
		std::string sourceDirectory;
	};
}

#endif
