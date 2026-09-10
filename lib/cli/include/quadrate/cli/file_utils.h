// SPDX-License-Identifier: GPL-3.0-or-later
// Common file utilities for Quadrate CLI tools

#ifndef QDCLI_FILE_UTILS_H
#define QDCLI_FILE_UTILS_H

#include <string>
#include <vector>

namespace qdcli {

	// Collect all .qd files from a path (file or directory)
	// If path is a directory, recursively finds all .qd files
	// If path is a file, returns a vector with just that file
	std::vector<std::string> collectFiles(const std::string& path);

	// Report any path that does not exist, and return false if one did.
	// collectFiles() passes an unknown path straight through to the caller, which
	// leaves each tool to notice on its own -- quadfmt and quaduses happen to fail
	// when the read fails, while quadlint silently linted nothing and exited 0.
	// Call this before collecting so a mistyped path is an error everywhere.
	bool checkPathsExist(const std::vector<std::string>& paths, const char* toolName);

	// Read entire file contents into a string
	// Throws std::runtime_error if file cannot be opened
	std::string readFile(const std::string& filename);

	// Write string contents to a file
	// Throws std::runtime_error if file cannot be written
	void writeFile(const std::string& filename, const std::string& content);

	// Validate UTF-8 encoding of a string
	// Returns false if string contains invalid UTF-8 or null bytes
	bool isValidUtf8(const std::string& source);

} // namespace qdcli

#endif // QDCLI_FILE_UTILS_H
