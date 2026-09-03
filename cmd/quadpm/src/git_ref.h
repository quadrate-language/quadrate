#ifndef QUADPM_GIT_REF_H
#define QUADPM_GIT_REF_H

#include <string>

// Parse Git reference (tag, branch, or commit)
// Format: url[@ref]
// Examples:
//   https://github.com/user/zlib@1.2.0
//   https://github.com/user/http@master
//   https://github.com/user/json  (uses repo's default branch)
struct GitRef {
	std::string url;
	std::string ref;
	std::string moduleName;
	std::string hostPath; // e.g., "github.com/user/zlib" for directory structure
};

// Extract module name from Git URL
// Examples:
//   https://github.com/user/zlib -> zlib
//   https://github.com/user/http-lib -> http-lib
//   git@github.com:user/json.git -> json
std::string extractModuleName(const std::string& gitUrl);

// Extract host/path from Git URL for directory structure
// Examples:
//   https://github.com/user/zlib -> github.com/user/zlib
//   https://github.com/user/http-lib -> github.com/user/http-lib
//   git@github.com:user/json.git -> github.com/user/json
std::string extractHostPath(const std::string& gitUrl);

// Parse a git URL into its components
GitRef parseGitUrl(const std::string& input);

// Reject a URL or ref that could be misinterpreted by git as an option.
// A value beginning with '-' (e.g. "--upload-pack=...") would be treated as a
// flag rather than a positional argument, so such values are unsafe to pass to
// git even via an argv vector. Returns true when the value is safe to use.
bool isSafeGitArgument(const std::string& value);

#endif
