// SPDX-License-Identifier: GPL-3.0-or-later
// Common file utilities for Quadrate CLI tools

#ifndef QDCLI_FILE_UTILS_H
#define QDCLI_FILE_UTILS_H

#include <string>
#include <unordered_set>
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

	// The file names of the .qd files `filePath` imports with `use "name.qd"`
	std::unordered_set<std::string> qdFileImports(const std::string& filePath);

	// The .qd files a program's directory adds to it: every one beside filePath that is
	// neither filePath itself, a *_test.qd, a program with its own main(), nor a file linked
	// to filePath by a `use`, which brings it in already. Canonical paths, sorted. quadc
	// merges these into the program and quadlsp checks an open file against them, so both
	// see the same program.
	std::vector<std::string> getSiblingQdFiles(const std::string& filePath);

	// The module search paths the qd.json in `directory` adds: each local dependency's
	// directory, and each git dependency quadpm has installed. Empty without a qd.json.
	std::vector<std::string> manifestIncludePaths(const std::string& directory);

} // namespace qdcli

#endif // QDCLI_FILE_UTILS_H
