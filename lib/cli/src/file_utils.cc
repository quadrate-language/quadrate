// SPDX-License-Identifier: GPL-3.0-or-later
// Common file utilities for Quadrate CLI tools

#include "quadrate/cli/file_utils.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace qdcli {

	std::vector<std::string> collectFiles(const std::string& path) {
		std::vector<std::string> files;

		// Every filesystem call here takes the error_code overload. The throwing
		// ones abort the process instead of reporting: is_directory() throws on a
		// path longer than NAME_MAX, and the plain recursive iterator throws the
		// first time it meets a directory it may not read, so `quadfmt /` died with
		// an uncaught filesystem_error rather than formatting what it could reach.
		std::error_code ec;
		if (!fs::is_directory(path, ec) || ec) {
			files.push_back(path);
			return files;
		}

		fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
		if (ec) {
			files.push_back(path);
			return files;
		}

		const fs::recursive_directory_iterator end;
		while (it != end) {
			const fs::directory_entry& entry = *it;
			std::error_code entryEc;
			if (entry.is_regular_file(entryEc) && !entryEc && entry.path().extension() == ".qd") {
				files.push_back(entry.path().string());
			}
			// increment(ec) keeps walking past an unreadable subtree; operator++
			// would throw out of the loop and take the process with it.
			it.increment(ec);
			if (ec) {
				break;
			}
		}

		std::sort(files.begin(), files.end());
		return files;
	}

	bool checkPathsExist(const std::vector<std::string>& paths, const char* toolName) {
		bool ok = true;
		for (const auto& path : paths) {
			std::error_code ec;
			if (!fs::exists(path, ec)) {
				std::cerr << toolName << ": " << path << ": No such file or directory\n";
				ok = false;
			}
		}
		return ok;
	}

	std::string readFile(const std::string& filename) {
		// Only ever read something that has an end. A character device such as
		// /dev/zero opens and streams happily forever, so `quadfmt /dev/zero` hung
		// until it was killed; a directory opens too on Linux and then fails the
		// read with no useful message.
		std::error_code ec;
		const fs::file_status status = fs::status(filename, ec);
		if (ec) {
			throw std::runtime_error("No such file or directory");
		}
		if (fs::is_directory(status)) {
			throw std::runtime_error("Is a directory");
		}
		if (!fs::is_regular_file(status)) {
			throw std::runtime_error("Not a regular file");
		}

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
