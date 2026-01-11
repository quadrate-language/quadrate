// SPDX-License-Identifier: GPL-3.0-or-later
// Common file utilities for Quadrate CLI tools

#include "qdcli/file_utils.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace qdcli {

std::vector<std::string> collectFiles(const std::string& path) {
	std::vector<std::string> files;

	if (fs::is_directory(path)) {
		for (const auto& entry : fs::recursive_directory_iterator(path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".qd") {
				files.push_back(entry.path().string());
			}
		}
		std::sort(files.begin(), files.end());
	} else {
		files.push_back(path);
	}

	return files;
}

std::string readFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("No such file or directory");
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void writeFile(const std::string& filename, const std::string& content) {
	std::ofstream file(filename);
	if (!file.good()) {
		throw std::runtime_error("Cannot write to file");
	}
	file << content;
}

bool isValidUtf8(const std::string& source) {
	size_t i = 0;
	while (i < source.length()) {
		auto c = static_cast<unsigned char>(source[i]);

		// Check for null bytes (binary file indicator)
		if (c == 0) {
			return false;
		}

		// ASCII (0xxxxxxx)
		if ((c & 0x80) == 0) {
			i++;
			continue;
		}

		// Determine number of continuation bytes
		size_t cont_bytes = 0;
		if ((c & 0xE0) == 0xC0) {
			cont_bytes = 1; // 110xxxxx
		} else if ((c & 0xF0) == 0xE0) {
			cont_bytes = 2; // 1110xxxx
		} else if ((c & 0xF8) == 0xF0) {
			cont_bytes = 3; // 11110xxx
		} else {
			return false; // Invalid UTF-8 start byte
		}

		// Check we have enough bytes
		if (i + cont_bytes >= source.length()) {
			return false;
		}

		// Validate continuation bytes (10xxxxxx)
		for (size_t j = 1; j <= cont_bytes; j++) {
			auto next = static_cast<unsigned char>(source[i + j]);
			if ((next & 0xC0) != 0x80) {
				return false;
			}
		}

		i += cont_bytes + 1;
	}
	return true;
}

} // namespace qdcli
