// Cross-platform filesystem operations using C++17 std::filesystem
// Provides C-compatible interface for use from os.c

#include "os_fs.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

extern "C" {

// Check if a file or directory exists
// Returns 1 if exists, 0 if not
int os_fs_exists(const char* path) {
	try {
		return fs::exists(path) ? 1 : 0;
	} catch (...) {
		return 0;
	}
}

// Check if path is a directory
// Returns 1 if directory, 0 if not
int os_fs_is_dir(const char* path) {
	try {
		return fs::is_directory(path) ? 1 : 0;
	} catch (...) {
		return 0;
	}
}

// Check if path is a regular file
// Returns 1 if regular file, 0 if not
int os_fs_is_file(const char* path) {
	try {
		return fs::is_regular_file(path) ? 1 : 0;
	} catch (...) {
		return 0;
	}
}

// Create a directory
// Returns 0 on success, error code on failure
int os_fs_mkdir(const char* path) {
	try {
		std::error_code ec;
		if (fs::create_directory(path, ec)) {
			return 0; // Success
		}
		return ec.value() ? ec.value() : 1; // Return error code or generic error
	} catch (...) {
		return 1; // Generic error
	}
}

// Create a directory and all parent directories (mkdir -p)
// Returns 0 on success, error code on failure
int os_fs_mkdir_p(const char* path) {
	try {
		std::error_code ec;
		fs::create_directories(path, ec);
		if (ec) {
			return ec.value();
		}
		return 0; // Success
	} catch (...) {
		return 1; // Generic error
	}
}

// Remove a directory and all its contents recursively (rm -rf)
// Returns 0 on success, error code on failure
int os_fs_rmdir_r(const char* path) {
	try {
		std::error_code ec;
		fs::remove_all(path, ec);
		if (ec) {
			return ec.value();
		}
		return 0; // Success
	} catch (...) {
		return 1; // Generic error
	}
}

// List directory contents
// Returns NULL-terminated array of strings on success, NULL on failure
// Caller must free the array and all strings using os_fs_free_list()
char** os_fs_list_dir(const char* path, size_t* count) {
	*count = 0;

	try {
		std::error_code ec;

		// Check if directory exists
		if (!fs::is_directory(path, ec)) {
			return nullptr;
		}

		// First pass: count entries
		size_t entry_count = 0;
		for ([[maybe_unused]] const auto& entry : fs::directory_iterator(path, ec)) {
			if (ec) {
				return nullptr;
			}
			entry_count++;
		}

		// Allocate array (plus one for NULL terminator)
		char** entries = static_cast<char**>(malloc((entry_count + 1) * sizeof(char*)));
		if (!entries) {
			return nullptr;
		}

		// Second pass: copy filenames
		size_t i = 0;
		for (const auto& entry : fs::directory_iterator(path, ec)) {
			if (ec) {
				// Clean up on error
				for (size_t j = 0; j < i; j++) {
					free(entries[j]);
				}
				free(entries);
				return nullptr;
			}

			std::string filename = entry.path().filename().string();
			entries[i] = strdup(filename.c_str());
			if (!entries[i]) {
				// Clean up on error
				for (size_t j = 0; j < i; j++) {
					free(entries[j]);
				}
				free(entries);
				return nullptr;
			}
			i++;
		}

		entries[entry_count] = nullptr; // NULL terminator
		*count = entry_count;
		return entries;

	} catch (...) {
		return nullptr;
	}
}

// Free the list returned by os_fs_list_dir()
void os_fs_free_list(char** list) {
	if (!list) {
		return;
	}

	for (size_t i = 0; list[i] != nullptr; i++) {
		free(list[i]);
	}
	free(list);
}

// Helper for recursive walk
static int walk_recursive(const fs::path& dir_path, os_fs_walk_callback callback, void* user_data, int depth) {
	std::error_code ec;

	for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
		if (ec) {
			return ec.value();
		}

		std::string path_str = entry.path().string();
		int is_dir = entry.is_directory(ec) ? 1 : 0;

		// Call the callback
		int result = callback(path_str.c_str(), is_dir, depth, user_data);
		if (result != 0) {
			return result; // Stop walking
		}

		// Recurse into directories
		if (is_dir) {
			int recurse_result = walk_recursive(entry.path(), callback, user_data, depth + 1);
			if (recurse_result != 0) {
				return recurse_result;
			}
		}
	}

	return 0;
}

// Walk a directory tree recursively
// Returns 0 on success, error code on failure
int os_fs_walk(const char* path, os_fs_walk_callback callback, void* user_data) {
	try {
		std::error_code ec;

		if (!fs::is_directory(path, ec)) {
			return ec.value() ? ec.value() : ENOTDIR;
		}

		return walk_recursive(path, callback, user_data, 0);
	} catch (...) {
		return 1;
	}
}

// Helper to check if a string matches a glob pattern
static bool glob_match(const std::string& pattern, const std::string& str) {
	size_t pi = 0, si = 0;
	size_t star_pi = std::string::npos, star_si = 0;

	while (si < str.size()) {
		if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == str[si])) {
			pi++;
			si++;
		} else if (pi < pattern.size() && pattern[pi] == '*') {
			star_pi = pi++;
			star_si = si;
		} else if (star_pi != std::string::npos) {
			pi = star_pi + 1;
			si = ++star_si;
		} else {
			return false;
		}
	}

	while (pi < pattern.size() && pattern[pi] == '*') {
		pi++;
	}

	return pi == pattern.size();
}

// Glob pattern matching
// Returns NULL-terminated array of matching paths, NULL on failure
// Caller must free using os_fs_free_list()
char** os_fs_glob(const char* pattern, size_t* count) {
	*count = 0;

	try {
		std::vector<std::string> matches;
		fs::path pat_path(pattern);

		// Find the base directory (everything before the first glob character)
		fs::path base_dir = ".";
		std::string glob_pattern = pattern;

		// Check if pattern has directory components
		size_t first_glob = glob_pattern.find_first_of("*?[");
		if (first_glob != std::string::npos) {
			size_t last_sep = glob_pattern.rfind('/', first_glob);
			if (last_sep != std::string::npos) {
				base_dir = glob_pattern.substr(0, last_sep);
				glob_pattern = glob_pattern.substr(last_sep + 1);
			}
		} else {
			// No glob chars - just check if file exists
			if (fs::exists(pattern)) {
				matches.push_back(pattern);
			}
		}

		// Handle ** for recursive matching
		bool recursive = glob_pattern.find("**") != std::string::npos;

		std::error_code ec;
		if (fs::is_directory(base_dir, ec)) {
			if (recursive) {
				for (const auto& entry : fs::recursive_directory_iterator(base_dir, ec)) {
					if (ec) break;
					std::string filename = entry.path().filename().string();
					std::string rel_path = entry.path().string();
					// Simple pattern match on filename for now
					std::string simple_pattern = glob_pattern;
					size_t star_star = simple_pattern.find("**");
					if (star_star != std::string::npos) {
						simple_pattern = simple_pattern.substr(star_star + 2);
						if (!simple_pattern.empty() && simple_pattern[0] == '/') {
							simple_pattern = simple_pattern.substr(1);
						}
					}
					if (simple_pattern.empty() || glob_match(simple_pattern, filename)) {
						matches.push_back(rel_path);
					}
				}
			} else {
				for (const auto& entry : fs::directory_iterator(base_dir, ec)) {
					if (ec) break;
					std::string filename = entry.path().filename().string();
					if (glob_match(glob_pattern, filename)) {
						matches.push_back(entry.path().string());
					}
				}
			}
		}

		if (matches.empty()) {
			return nullptr;
		}

		// Allocate result array
		char** result = static_cast<char**>(malloc((matches.size() + 1) * sizeof(char*)));
		if (!result) {
			return nullptr;
		}

		for (size_t i = 0; i < matches.size(); i++) {
			result[i] = strdup(matches[i].c_str());
			if (!result[i]) {
				for (size_t j = 0; j < i; j++) {
					free(result[j]);
				}
				free(result);
				return nullptr;
			}
		}
		result[matches.size()] = nullptr;
		*count = matches.size();
		return result;

	} catch (...) {
		return nullptr;
	}
}

} // extern "C"
