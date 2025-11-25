#ifndef QUADC_TEMP_DIR_GUARD_H
#define QUADC_TEMP_DIR_GUARD_H

#include <filesystem>
#include <string>

// RAII guard for automatic temp directory cleanup
class TempDirGuard {
public:
	explicit TempDirGuard(const std::string& path) : mPath(path), mShouldDelete(true) {
	}

	~TempDirGuard() {
		if (mShouldDelete && !mPath.empty()) {
			std::filesystem::remove_all(mPath);
		}
	}

	void release() {
		mShouldDelete = false;
	}

	// Prevent copying
	TempDirGuard(const TempDirGuard&) = delete;
	TempDirGuard& operator=(const TempDirGuard&) = delete;

private:
	std::string mPath;
	bool mShouldDelete;
};

// Create a temporary directory
// If useCwd is true, creates in current directory, otherwise in system temp
std::string createTempDir(bool useCwd);

#endif
