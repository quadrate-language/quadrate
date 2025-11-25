#include "temp_dir_guard.h"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>

std::string createTempDir(bool useCwd) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	// Determine base directory: use cwd if --save-temps, otherwise use system temp
	std::filesystem::path baseDir;
	if (useCwd) {
		baseDir = std::filesystem::current_path();
	} else {
		baseDir = std::filesystem::temp_directory_path();
	}

	// Try up to 10 times to create a unique directory
	for (int attempt = 0; attempt < 10; attempt++) {
		std::stringstream ss;
		ss << "qd_";
		for (int i = 0; i < 8; i++) {
			ss << std::hex << dis(gen);
		}

		std::filesystem::path tmpDir = baseDir / ss.str();

		// Try to create the directory atomically (no TOCTOU race)
		std::error_code ec;
		if (std::filesystem::create_directory(tmpDir, ec)) {
			return tmpDir.string();
		}

		// If directory already exists, try again with a different name
		if (ec.value() == EEXIST) {
			continue;
		}

		// For other errors, fail immediately
		std::cerr << "quadc: failed to create temporary directory: " << ec.message() << std::endl;
		exit(1);
	}

	// Failed to create directory after multiple attempts
	std::cerr << "quadc: failed to create temporary directory after 10 attempts" << std::endl;
	exit(1);
}
