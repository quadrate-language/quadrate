// Cross-platform filesystem operations using C++17 std::filesystem
// Provides C-compatible interface for use from os.c

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

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

} // extern "C"
