#include "git_ref.h"

std::string extractModuleName(const std::string& gitUrl) {
	// Find last '/' or ':'
	size_t lastSlash = gitUrl.find_last_of("/:");
	if (lastSlash == std::string::npos) {
		return gitUrl;
	}

	std::string name = gitUrl.substr(lastSlash + 1);

	// Remove .git suffix if present
	if (name.size() > 4 && name.substr(name.size() - 4) == ".git") {
		name = name.substr(0, name.size() - 4);
	}

	return name;
}

GitRef parseGitUrl(const std::string& input) {
	GitRef result;

	// Check if there's an @ symbol for version/ref
	size_t atPos = input.find_last_of('@');

	// Make sure @ is not part of git@github.com
	if (atPos != std::string::npos && atPos > 0 && input[atPos - 1] != ':') {
		result.url = input.substr(0, atPos);
		result.ref = input.substr(atPos + 1);
	} else {
		result.url = input;
		result.ref = "main"; // Default to main branch
	}

	result.moduleName = extractModuleName(result.url);
	return result;
}
