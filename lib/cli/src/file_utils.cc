// SPDX-License-Identifier: GPL-3.0-or-later
// Common file utilities for Quadrate CLI tools

#include "quadrate/cli/file_utils.h"
#include <jansson.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/module_resolver.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
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
			throw std::runtime_error(ec.message());
		}
		if (fs::is_directory(status)) {
			throw std::runtime_error("Is a directory");
		}
		if (!fs::is_regular_file(status)) {
			throw std::runtime_error("Not a regular file");
		}

		std::ifstream file(filename);
		if (!file.good()) {
			throw std::runtime_error(std::strerror(errno));
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

	std::unordered_set<std::string> qdFileImports(const std::string& filePath) {
		std::unordered_set<std::string> names;
		std::ifstream file(filePath);
		if (!file) {
			return names;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		const std::string source = buffer.str();
		Qd::Ast ast;
		std::function<void(Qd::IAstNode*)> collect = [&](Qd::IAstNode* node) {
			if (!node) {
				return;
			}
			if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
				const std::string& used = static_cast<Qd::AstNodeUse*>(node)->module();
				if (used.size() > 3 && used.compare(used.size() - 3, 3, ".qd") == 0) {
					names.insert(fs::path(used).filename().string());
				}
			}
			for (size_t i = 0; i < node->childCount(); i++) {
				collect(node->child(i));
			}
		};
		collect(ast.generate(source.c_str(), false, filePath.c_str()));
		return names;
	}

	std::vector<std::string> getSiblingQdFiles(const std::string& filePath) {
		std::vector<std::string> siblings;

		try {
			std::filesystem::path path(filePath);
			std::filesystem::path dir = path.parent_path();
			if (dir.empty()) {
				dir = ".";
			}

			// Get canonical path of the input file for comparison
			std::string canonicalInput = std::filesystem::canonical(filePath).string();

			// Helper: check if a file has a main() function (quick text search)
			auto hasMainFunction = [](const std::string& file) -> bool {
				std::ifstream ifs(file);
				if (!ifs) {
					return false;
				}
				std::string line;
				while (std::getline(ifs, line)) {
					// Look for "fn main(" at the start of a line (possibly with leading whitespace)
					size_t pos = line.find("fn main(");
					if (pos != std::string::npos) {
						// Check if it's at the start of line (ignoring whitespace)
						bool isStart = true;
						for (size_t i = 0; i < pos; i++) {
							if (line[i] != ' ' && line[i] != '\t') {
								isStart = false;
								break;
							}
						}
						if (isStart) {
							return true;
						}
					}
				}
				return false;
			};

			// Collect all .qd files in the directory except the input file
			for (const auto& entry : std::filesystem::directory_iterator(dir)) {
				if (!entry.is_regular_file()) {
					continue;
				}

				std::string filename = entry.path().filename().string();

				// Must end in .qd
				if (filename.size() < 3 || filename.substr(filename.size() - 3) != ".qd") {
					continue;
				}

				// Exclude test files
				if (filename.size() > 8 && filename.substr(filename.size() - 8) == "_test.qd") {
					continue;
				}

				// Exclude the input file itself
				std::string canonicalEntry = std::filesystem::canonical(entry.path()).string();
				if (canonicalEntry == canonicalInput) {
					continue;
				}

				// Exclude files that have their own main() function
				// These are standalone programs, not helper modules
				if (hasMainFunction(canonicalEntry)) {
					continue;
				}

				siblings.push_back(canonicalEntry);
			}

			// A file the program `use`s, or that `use`s it, comes in through that `use`, as in a
			// module's own directory
			siblings.push_back(canonicalInput);
			siblings = Qd::moduleSiblings(canonicalInput, siblings, qdFileImports);
			std::sort(siblings.begin(), siblings.end());
		} catch (...) {
			// Ignore filesystem errors
		}

		return siblings;
	}

	std::vector<std::string> manifestIncludePaths(const std::string& directory) {
		std::vector<std::string> includePaths;
		json_error_t error;
		json_t* root = json_load_file((directory + "/qd.json").c_str(), 0, &error);
		if (!root) {
			return includePaths;
		}
		json_t* dependencies = json_object_get(root, "dependencies");
		const char* depName;
		json_t* depValue;
		json_object_foreach(dependencies, depName, depValue) {
			// "name": "url-or-path", or "name": { "url": "url-or-path", ... }
			json_t* url = json_is_object(depValue) ? json_object_get(depValue, "url") : depValue;
			const std::string written = json_is_string(url) ? json_string_value(url) : "";
			if (written.empty()) {
				continue;
			}
			std::error_code ec;
			const bool isPath = written[0] == '/' || written[0] == '.' || written.rfind("~/", 0) == 0;
			if (isPath) {
				std::filesystem::path path(Qd::expandTilde(written));
				if (path.is_relative()) {
					path = std::filesystem::path(directory) / path;
				}
				path = std::filesystem::weakly_canonical(path, ec);
				if (!ec && std::filesystem::exists(path, ec)) {
					includePaths.push_back(path.string());
				}
				continue;
			}
			// A git dependency, found where quadpm installed it
			const std::string link = Qd::packagesDirectory() + "/_namespaces/" + depName;
			if (std::filesystem::exists(link, ec)) {
				const std::filesystem::path target = std::filesystem::canonical(link, ec);
				includePaths.push_back(ec ? link : target.string());
			}
		}
		json_decref(root);
		return includePaths;
	}

} // namespace qdcli
