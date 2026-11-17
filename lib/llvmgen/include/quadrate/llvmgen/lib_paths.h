#ifndef QD_LLVMGEN_LIB_PATHS_H
#define QD_LLVMGEN_LIB_PATHS_H

#include <string>

namespace Qd {

	// Finds the directory holding the Quadrate runtime and standard libraries. Tries, in order:
	// QUADRATE_LIBDIR, <buildRoot>/lib (when buildRoot is not empty), ./dist/lib, ../lib next to
	// the executable, ~/.local/lib, then the system locations. Every candidate except
	// QUADRATE_LIBDIR must contain the runtime.
	std::string findLibraryDir(const std::string& buildRoot = "");

	// Finds lib<name>.a (or lib<name>_static.a) under libDir in the installed (quadrate/),
	// build-tree (<name>/) and flat layouts. Returns an empty string when not found.
	std::string findStaticLib(const std::string& libDir, const std::string& name);

} // namespace Qd

#endif
