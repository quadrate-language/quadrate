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
