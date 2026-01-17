#ifndef QUADPM_GIT_REF_H
#define QUADPM_GIT_REF_H

#include <string>

// Parse Git reference (tag, branch, or commit)
// Format: url[@ref]
// Examples:
//   https://git.sr.ht/~user/zlib@1.2.0
//   https://github.com/user/http@main
//   https://github.com/user/json  (uses repo's default branch)
struct GitRef {
	std::string url;
	std::string ref;
	std::string moduleName;
	std::string hostPath; // e.g., "git.sr.ht/~user/zlib" for directory structure
};

// Extract module name from Git URL
// Examples:
//   https://git.sr.ht/~user/zlib -> zlib
//   https://github.com/user/http-lib -> http-lib
//   git@github.com:user/json.git -> json
std::string extractModuleName(const std::string& gitUrl);

// Extract host/path from Git URL for directory structure
// Examples:
//   https://git.sr.ht/~user/zlib -> git.sr.ht/~user/zlib
//   https://github.com/user/http-lib -> github.com/user/http-lib
//   git@github.com:user/json.git -> github.com/user/json
std::string extractHostPath(const std::string& gitUrl);

// Parse a git URL into its components
GitRef parseGitUrl(const std::string& input);

#endif
