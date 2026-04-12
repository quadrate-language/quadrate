#ifndef QUADC_BUILD_CACHE_H
#define QUADC_BUILD_CACHE_H

#include <string>
#include <vector>

// Build cache for incremental compilation.
// Caches final executables keyed by a hash of all source files and compiler options.
// Cache location: ~/.cache/quadrate/builds/
class BuildCache {
public:
	// Add a source file path to the hash inputs.
	// The file's contents and modification time are included in the hash.
	void addSourceFile(const std::string& filePath);

	// Add a compiler option string to the hash inputs (e.g. "-O2", target triple).
	void addOption(const std::string& option);

	// Compute the final cache key from all added inputs.
	// Returns a hex string suitable for use as a filename.
	std::string computeKey();

	// Check if a cached executable exists for the current key.
	// If found, copies it to outputPath and returns true.
	bool restore(const std::string& outputPath);

	// Store an executable in the cache for the current key.
	void store(const std::string& executablePath);

	// Get the cache directory path.
	static std::string cacheDir();

private:
	std::vector<std::string> mFileHashes;
	std::vector<std::string> mOptions;
	std::string mKey; // computed lazily
};

#endif
