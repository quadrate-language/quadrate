#include "git_ref.h"
#include <filesystem>

std::string extractModuleName(const std::string& gitUrl) {
	std::string url = gitUrl;
	while (url.size() > 1 && url.back() == '/') {
		url.pop_back();
	}

	// Find last '/' or ':'
	size_t lastSlash = url.find_last_of("/:");
	if (lastSlash == std::string::npos) {
		return url;
	}

	std::string name = url.substr(lastSlash + 1);

	// Remove .git suffix if present
	if (name.size() > 4 && name.substr(name.size() - 4) == ".git") {
		name = name.substr(0, name.size() - 4);
	}

	return name;
}

namespace {
	std::string localHostPath(const std::string& absolutePath) {
		return "local" + std::filesystem::path(absolutePath).lexically_normal().generic_string();
	}

	std::string stripUserinfo(const std::string& authorityAndPath) {
		size_t slash = authorityAndPath.find('/');
		std::string authority = authorityAndPath.substr(0, slash);
		size_t at = authority.rfind('@');
		if (at == std::string::npos) {
			return authorityAndPath;
		}
		return authorityAndPath.substr(at + 1);
	}

	bool isScpLike(const std::string& url) {
		size_t colon = url.find(':');
		size_t slash = url.find('/');
		return colon != std::string::npos && (slash == std::string::npos || colon < slash);
	}
} // namespace

std::string extractHostPath(const std::string& gitUrl) {
	std::string url = gitUrl;

	while (url.size() > 1 && url.back() == '/') {
		url.pop_back();
	}

	// Remove .git suffix if present
	if (url.size() > 4 && url.substr(url.size() - 4) == ".git") {
		url = url.substr(0, url.size() - 4);
	}

	// Check if this is a local path (starts with /, ./, ../, or ~/)
	if (!url.empty() && (url[0] == '/' || url[0] == '.' || url[0] == '~')) {
		// For local paths, use "local" as pseudo-host and keep the path structure
		// e.g., /tmp/foo/bar -> local/tmp/foo/bar
		// e.g., ../mylib -> local/mylib (just the last component for relative)
		if (url[0] == '/') {
			return localHostPath(url);
		}
		if (url[0] == '~') {
			return "local" + url;
		}
		size_t lastSlash = url.find_last_of('/');
		if (lastSlash != std::string::npos) {
			return "local/" + url.substr(lastSlash + 1);
		}
		return "local/" + url;
	}

	// file:///abs/path -> local/abs/path, like a plain absolute path
	if (url.substr(0, 7) == "file://") {
		std::string path = url.substr(7);
		if (!path.empty() && path[0] == '/') {
			return localHostPath(path);
		}
		return "local/" + path;
	}

	// Any scheme://[user@]host/path -> host/path
	// e.g., https://github.com/user/repo -> github.com/user/repo
	// e.g., ssh://git@host/user/repo -> host/user/repo
	size_t schemeEnd = url.find("://");
	if (schemeEnd != std::string::npos) {
		return stripUserinfo(url.substr(schemeEnd + 3));
	}

	// scp-like [user@]host:path
	// e.g., git@github.com:user/repo -> github.com/user/repo
	if (isScpLike(url)) {
		size_t colonPos = url.find(':');
		std::string host = url.substr(0, colonPos);
		size_t at = host.rfind('@');
		if (at != std::string::npos) {
			host = host.substr(at + 1);
		}
		std::string path = url.substr(colonPos + 1);
		while (!path.empty() && path[0] == '/') {
			path.erase(0, 1);
		}
		return host + "/" + path;
	}

	// Fallback: return as-is
	return url;
}

GitRef parseGitUrl(const std::string& input) {
	GitRef result;

	size_t pathStart = 0;
	size_t schemeEnd = input.find("://");
	if (schemeEnd != std::string::npos) {
		size_t slash = input.find('/', schemeEnd + 3);
		pathStart = (slash == std::string::npos) ? input.size() : slash;
	} else if (isScpLike(input)) {
		pathStart = input.find(':');
	}

	size_t atPos = input.find_last_of('@');
	if (atPos != std::string::npos && atPos > pathStart) {
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

bool isSafeGitArgument(const std::string& value) {
	if (value.empty()) {
		return true; // empty refs are handled by callers (default branch)
	}
	// A leading '-' makes git treat the value as an option.
	if (value[0] == '-') {
		return false;
	}
	// Newlines would let a value smuggle extra lines into git's stdin-driven
	// flows; reject control characters outright.
	for (char c : value) {
		if (c == '\n' || c == '\r' || c == '\0') {
			return false;
		}
	}
	return true;
}
