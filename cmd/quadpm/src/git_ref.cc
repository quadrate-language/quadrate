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

std::string extractHostPath(const std::string& gitUrl) {
	std::string url = gitUrl;

	// Remove .git suffix if present
	if (url.size() > 4 && url.substr(url.size() - 4) == ".git") {
		url = url.substr(0, url.size() - 4);
	}

	// Check if this is a local path (starts with /, ./, ../, or ~/)
	if (!url.empty() && (url[0] == '/' || url[0] == '.' || url[0] == '~')) {
		// For local paths, use "local" as pseudo-host and keep the path structure
		// e.g., /tmp/foo/bar -> local/tmp/foo/bar
		// e.g., ../mylib -> local/mylib (just the last component for relative)
		if (url[0] == '/' || url[0] == '~') {
			// Absolute path - use full path under "local/"
			return "local" + url;
		} else {
			// Relative path - just use the module name
			size_t lastSlash = url.find_last_of('/');
			if (lastSlash != std::string::npos) {
				return "local/" + url.substr(lastSlash + 1);
			}
			return "local/" + url;
		}
	}

	// Handle https:// and http:// URLs
	// e.g., https://git.sr.ht/~user/repo -> git.sr.ht/~user/repo
	if (url.substr(0, 8) == "https://") {
		return url.substr(8);
	}
	if (url.substr(0, 7) == "http://") {
		return url.substr(7);
	}

	// Handle git@host:user/repo format
	// e.g., git@github.com:user/repo -> github.com/user/repo
	if (url.substr(0, 4) == "git@") {
		std::string rest = url.substr(4);
		size_t colonPos = rest.find(':');
		if (colonPos != std::string::npos) {
			std::string host = rest.substr(0, colonPos);
			std::string path = rest.substr(colonPos + 1);
			return host + "/" + path;
		}
	}

	// Handle ssh://git@host/user/repo format
	if (url.substr(0, 6) == "ssh://") {
		std::string rest = url.substr(6);
		// Skip git@ if present
		if (rest.substr(0, 4) == "git@") {
			rest = rest.substr(4);
		}
		return rest;
	}

	// Fallback: return as-is
	return url;
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
		result.ref = ""; // Empty = use repo's default branch
	}

	result.moduleName = extractModuleName(result.url);
	result.hostPath = extractHostPath(result.url);
	return result;
}
