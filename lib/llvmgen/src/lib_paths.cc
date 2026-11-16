#include <quadrate/llvmgen/lib_paths.h>

#include <cstdlib>
#include <filesystem>
#include <system_error>

extern "C" {
#include "src/platform/exe_path_platform.h"
}

namespace fs = std::filesystem;

namespace Qd {

	std::string findStaticLib(const std::string& libDir, const std::string& name) {
		const fs::path dir(libDir);
		const std::string file = "lib" + name + ".a";
		const std::string staticFile = "lib" + name + "_static.a";
		const fs::path candidates[] = {
				dir / "quadrate" / file,					// installed / dist layout
				dir / name / staticFile,					// build tree
				dir / name / file,							// build tree
				dir.parent_path() / "stdlib" / name / file, // build tree stdlib
				dir / file,									// legacy flat layout
				dir / staticFile,							// legacy flat layout
		};
		std::error_code ec;
		for (const auto& candidate : candidates) {
			if (fs::exists(candidate, ec)) {
				return candidate.string();
			}
		}
		return "";
	}

	static bool hasRuntime(const fs::path& dir) {
		std::error_code ec;
		return !findStaticLib(dir.string(), "rt").empty() || fs::exists(dir / "libqdrt.so", ec);
	}

	std::string findLibraryDir(const std::string& buildRoot) {
		std::error_code ec;

		if (const char* env = std::getenv("QUADRATE_LIBDIR")) {
			fs::path path = fs::absolute(env, ec);
			if (!ec && fs::exists(path, ec)) {
				return path.string();
			}
		}

		if (!buildRoot.empty() && hasRuntime(fs::path(buildRoot) / "lib")) {
			return (fs::path(buildRoot) / "lib").string();
		}

		fs::path distLib = fs::absolute("dist/lib", ec);
		if (!ec && hasRuntime(distLib)) {
			return distLib.string();
		}

		char exePathBuf[4096];
		int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
		if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
			fs::path exePath = fs::canonical(exePathBuf, ec);
			if (!ec) {
				fs::path installedLib = exePath.parent_path() / ".." / "lib";
				if (hasRuntime(installedLib)) {
					return installedLib.string();
				}
			}
		}

		if (const char* home = std::getenv("HOME")) {
			fs::path localLib = fs::path(home) / ".local" / "lib";
			if (hasRuntime(localLib)) {
				return localLib.string();
			}
		}

#ifdef __HAIKU__
		return "/boot/system/lib";
#else
		if (!hasRuntime("/usr/lib") && hasRuntime("/usr/local/lib")) {
			return "/usr/local/lib";
		}
		return "/usr/lib";
#endif
	}

} // namespace Qd
