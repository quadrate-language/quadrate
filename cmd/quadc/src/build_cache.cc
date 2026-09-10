#include "build_cache.h"

#include "version.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>

#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

// Simple FNV-1a hash (64-bit) — fast, good distribution, no external deps
static uint64_t fnv1a(const void* data, size_t len, uint64_t hash = 0xcbf29ce484222325ULL) {
	const auto* bytes = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < len; i++) {
		hash ^= bytes[i];
		hash *= 0x100000001b3ULL;
	}
	return hash;
}

static std::string hashToHex(uint64_t h1, uint64_t h2) {
	std::ostringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(16) << h1 << std::setw(16) << h2;
	return ss.str();
}

void BuildCache::addSourceFile(const std::string& filePath) {
	try {
		std::ifstream f(filePath, std::ios::binary | std::ios::ate);
		if (!f.is_open()) {
			// File can't be read — include path as fallback so the hash still changes
			mFileHashes.push_back("missing:" + filePath);
			return;
		}
		auto size = f.tellg();
		f.seekg(0);
		std::string contents(static_cast<size_t>(size), '\0');
		f.read(&contents[0], size);

		// Hash the file contents (not mtime — content-based like Go)
		uint64_t h = fnv1a(contents.data(), contents.size());
		std::ostringstream ss;
		ss << std::hex << std::setfill('0') << std::setw(16) << h;
		mFileHashes.push_back(ss.str());
	} catch (...) {
		mFileHashes.push_back("error:" + filePath);
	}
}

void BuildCache::addOption(const std::string& option) {
	mOptions.push_back(option);
}

void BuildCache::addCompilerIdentity() {
	// Version and commit cover released builds, where the binary is reproducible.
	mOptions.push_back(std::string("qdc-version:") + QUADRATE_VERSION);
	mOptions.push_back(std::string("qdc-commit:") + QUADRATE_GIT_COMMIT);

	// During development the version string does not move between rebuilds, so also mix in
	// the running binary's size and mtime. Best-effort: if the path can't be resolved the
	// version fields above still apply.
	std::error_code ec;
	fs::path self = fs::read_symlink("/proc/self/exe", ec);
	if (ec || self.empty()) {
		return;
	}

	auto size = fs::file_size(self, ec);
	if (!ec) {
		mOptions.push_back("qdc-size:" + std::to_string(size));
	}

	auto mtime = fs::last_write_time(self, ec);
	if (!ec) {
		mOptions.push_back("qdc-mtime:" + std::to_string(mtime.time_since_epoch().count()));
	}
}

namespace {

	// Resolve the library directory the same way lib/llvmgen does at link time
	// (generator.cc, "Determine library directory"). Duplicated rather than shared
	// because that logic is inline in a large function; if the precedence there
	// changes, this must follow.
	fs::path resolveLibDir() {
		std::error_code ec;

		if (const char* env = std::getenv("QUADRATE_LIBDIR")) {
			fs::path p(env);
			if (p.is_relative()) {
				p = fs::absolute(p, ec);
			}
			if (!ec && fs::exists(p, ec)) {
				return p;
			}
		}

		fs::path distLib = fs::absolute("./dist/lib", ec);
		if (!ec && fs::exists(distLib, ec)) {
			return distLib;
		}

		fs::path self = fs::read_symlink("/proc/self/exe", ec);
		if (!ec && !self.empty()) {
			fs::path installed = self.parent_path() / ".." / "lib";
			if (fs::exists(installed, ec)) {
				return installed;
			}
		}

		if (const char* home = std::getenv("HOME")) {
			fs::path localLib = fs::path(home) / ".local" / "lib";
			if (fs::exists(localLib, ec)) {
				return localLib;
			}
		}

		if (fs::exists("/usr/lib", ec)) {
			return fs::path("/usr/lib");
		}
		return {};
	}

	// Mix in one archive's identity. Contents are not hashed: a .a can be tens of
	// megabytes and this runs on every build, so size+mtime is the same trade the
	// compiler-identity check above already makes.
	void addArchive(std::vector<std::string>& out, const fs::path& file) {
		std::error_code ec;
		auto size = fs::file_size(file, ec);
		if (ec) {
			return;
		}
		auto mtime = fs::last_write_time(file, ec);
		if (ec) {
			return;
		}
		out.push_back("lib:" + file.filename().string() + ":" + std::to_string(size) + ":" +
					  std::to_string(mtime.time_since_epoch().count()));
	}

} // namespace

void BuildCache::addStdlibIdentity() {
	// The compiler identity above does not cover the runtime and stdlib archives a
	// build links against, so rebuilding libstrings.a and re-running the tests
	// served executables compiled against the previous one -- reporting failures
	// citing an error message that no longer existed in the source, and equally
	// able to report a pass that is no longer true.
	fs::path libDir = resolveLibDir();
	if (libDir.empty()) {
		return;
	}

	std::vector<std::string> archives;
	std::error_code ec;
	for (const fs::path& dir : {libDir, libDir / "quadrate"}) {
		if (!fs::is_directory(dir, ec)) {
			continue;
		}
		for (const auto& entry : fs::directory_iterator(dir, ec)) {
			if (ec) {
				break;
			}
			if (entry.is_regular_file(ec) && entry.path().extension() == ".a") {
				addArchive(archives, entry.path());
			}
		}
	}

	// directory_iterator order is unspecified; sort so the key is stable.
	std::sort(archives.begin(), archives.end());
	for (const auto& a : archives) {
		mOptions.push_back(a);
	}
}

std::string BuildCache::computeKey() {
	if (!mKey.empty()) {
		return mKey;
	}

	// Combine all file hashes and options into a single hash
	uint64_t h1 = 0xcbf29ce484222325ULL;
	uint64_t h2 = 0x6c62272e07bb0142ULL;

	for (const auto& fh : mFileHashes) {
		h1 = fnv1a(fh.data(), fh.size(), h1);
		h2 = fnv1a(fh.data(), fh.size(), h2);
	}
	// Separator between files and options
	uint8_t sep = 0xff;
	h1 = fnv1a(&sep, 1, h1);
	h2 = fnv1a(&sep, 1, h2);

	for (const auto& opt : mOptions) {
		h1 = fnv1a(opt.data(), opt.size(), h1);
		h2 = fnv1a(opt.data(), opt.size(), h2);
	}

	mKey = hashToHex(h1, h2);
	return mKey;
}

std::string BuildCache::cacheDir() {
	std::string dir;

	// XDG_CACHE_HOME takes precedence
	if (const char* xdg = std::getenv("XDG_CACHE_HOME")) {
		dir = std::string(xdg) + "/quadrate/builds";
	} else if (const char* home = std::getenv("HOME")) {
		dir = std::string(home) + "/.cache/quadrate/builds";
	} else {
		return ""; // Can't determine cache dir
	}

	return dir;
}

bool BuildCache::restore(const std::string& outputPath) {
	std::string dir = cacheDir();
	if (dir.empty()) {
		return false;
	}

	std::string key = computeKey();
	std::string cachedPath = dir + "/" + key;

	if (!fs::exists(cachedPath)) {
		return false;
	}

	try {
		// Create parent directories for output if needed
		fs::path outDir = fs::path(outputPath).parent_path();
		if (!outDir.empty()) {
			fs::create_directories(outDir);
		}
		fs::copy_file(cachedPath, outputPath, fs::copy_options::overwrite_existing);
		// Ensure executable permissions
		fs::permissions(outputPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
				fs::perm_options::add);
		return true;
	} catch (...) {
		return false;
	}
}

void BuildCache::store(const std::string& executablePath) {
	std::string dir = cacheDir();
	if (dir.empty()) {
		return;
	}

	try {
		fs::create_directories(dir);

		std::string key = computeKey();
		std::string cachedPath = dir + "/" + key;

		fs::copy_file(executablePath, cachedPath, fs::copy_options::overwrite_existing);
	} catch (...) {
		// Cache store failure is not fatal
	}
}
