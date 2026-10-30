#include <quadrate/qd/qd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <quadrate/llvmgen/generator.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/module_resolver.h>
#include <quadrate/qc/numeric_literal.h>
#include <quadrate/qc/semantic_validator.h>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Platform abstractions
extern "C" {
#include "src/platform/dynlib_platform.h"
#include "src/platform/exe_path_platform.h"
}

namespace fs = std::filesystem;

// Helper to find the library directory (either build or installed location)
static std::string findLibraryDir() {
	// Try 1: QUADRATE_LIBDIR environment variable (for development/testing)
	const char* libDir = getenv("QUADRATE_LIBDIR");
	if (libDir && fs::exists(libDir)) {
		return libDir;
	}

	// Try 2: Build directory (MESON_BUILD_ROOT/lib, debug builds only)
	if (MESON_BUILD_ROOT[0] != '\0') {
		fs::path buildLibPath = fs::path(MESON_BUILD_ROOT) / "lib";
		if (fs::exists(buildLibPath / "rt" / "librt_static.a") || fs::exists(buildLibPath / "rt" / "librt.a")) {
			return buildLibPath.string();
		}
	}

	// Try 3: Library directory relative to executable (installed binaries)
	{
		char exePathBuf[4096];
		int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
		if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
			try {
				fs::path exePath = fs::canonical(exePathBuf);
				fs::path exeDir = exePath.parent_path();
				fs::path installedLib = exeDir / ".." / "lib";
				if ((fs::exists(installedLib / "quadrate" / "librt.a") || fs::exists(installedLib / "libqdrt.so")) &&
						fs::exists(installedLib / "quadrate" / "libqd.a")) {
					return installedLib.string();
				}
			} catch (const std::filesystem::filesystem_error&) {
				// Ignore errors resolving path
			}
		}
	}

#ifdef __HAIKU__
	// Try 4: Haiku system library location
	if (fs::exists("/boot/system/lib/quadrate/libqd.a")) {
		return "/boot/system/lib";
	}
#else
	// Try 4: System installed location (/usr/lib)
	if (fs::exists("/usr/lib/quadrate/libqd.a")) {
		return "/usr/lib";
	}

	// Try 5: /usr/local/lib
	if (fs::exists("/usr/local/lib/quadrate/libqd.a")) {
		return "/usr/local/lib";
	}
#endif

	// Fallback
#ifdef __HAIKU__
	return "/boot/system/lib";
#else
	return "/usr/lib";
#endif
}

// Helper to find a static library file
static std::string findStaticLib(const std::string& libDir, const std::string& libName) {
	// In build directory: lib/<name>/lib<name>_static.a or lib/<name>/lib<name>.a
	fs::path nestedStatic = fs::path(libDir) / libName / ("lib" + libName + "_static.a");
	if (fs::exists(nestedStatic)) {
		return nestedStatic.string();
	}
	fs::path nestedLib = fs::path(libDir) / libName / ("lib" + libName + ".a");
	if (fs::exists(nestedLib)) {
		return nestedLib.string();
	}
	fs::path buildStdlib = fs::path(libDir).parent_path() / "stdlib" / libName / ("lib" + libName + ".a");
	if (fs::exists(buildStdlib)) {
		return buildStdlib.string();
	}
	// In installed location: quadrate/lib<name>.a
	fs::path quadrateLib = fs::path(libDir) / "quadrate" / ("lib" + libName + ".a");
	if (fs::exists(quadrateLib)) {
		return quadrateLib.string();
	}
	// Legacy flat location: lib<name>.a
	fs::path flatLib = fs::path(libDir) / ("lib" + libName + ".a");

	if (fs::exists(flatLib)) {
		return flatLib.string();
	}
	return "";
}

static const char* findCC() {
	const char* cc = getenv("CC");
	return (cc && cc[0]) ? cc : "cc";
}

static const char* findCXX() {
	const char* cxx = getenv("CXX");
	return (cxx && cxx[0]) ? cxx : "c++";
}

// Quote a path for inclusion in a popen()/system() command line. Module names
// and temp/library paths are interpolated into the compile/link commands below;
// quoting prevents a path containing shell metacharacters from injecting
// commands into the build host.
static std::string shellQuote(const std::string& s) {
	std::string out = "'";
	for (char c : s) {
		if (c == '\'') {
			out += "'\\''";
		} else {
			out += c;
		}
	}
	out += "'";
	return out;
}

// Module implementation
struct qd_module {
	std::string name;
	std::vector<std::string> scripts;

	struct NativeFunction {
		qd_native_fn fn;
		void* userdata;
		std::string signature; // Optional type signature, e.g. "( -- v:f64)"
	};

	std::unordered_map<std::string, NativeFunction> native_functions;
	std::unordered_map<std::string, std::string> symbol_map; // function_name -> full_symbol_name
	dynlib_handle_t dl_handle;								 // dynamic library handle
	fs::path temp_dir;
	fs::path so_path;
	bool compiled;
	size_t warning_min_line; // Minimum line for warnings (0 = no suppression)
	qd_context* owner_ctx;	 // Context that owns this module (for qd_call_native)

	qd_module(const std::string& n)
		: name(n), dl_handle(nullptr), compiled(false), warning_min_line(0), owner_ctx(nullptr) {
	}

	~qd_module() {
		// Clean up dynamic library
		if (dl_handle) {
			dynlib_platform_close(dl_handle);
		}
		// Clean up temp directory
		if (!temp_dir.empty() && fs::exists(temp_dir)) {
			std::error_code ec;
			fs::remove_all(temp_dir, ec);
		}
	}
};

// Global module registry (stored per-context would be better, but API doesn't support it).
// g_modules_mutex guards structural changes to the map (insert/erase), which would
// otherwise race when modules are created or released from multiple threads.
static std::unordered_map<qd_context*, std::unordered_map<std::string, qd_module*>> g_context_modules;
static std::mutex g_modules_mutex;

qd_module* qd_get_module(qd_context* ctx, const char* name) {
	if (!ctx || !name) {
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(g_modules_mutex);

	// Get or create module registry for this context
	auto& modules = g_context_modules[ctx];

	// Check if module already exists
	auto it = modules.find(name);
	if (it != modules.end()) {
		return it->second;
	}

	// Create new module
	qd_module* mod = new qd_module(name);
	mod->owner_ctx = ctx;
	modules[name] = mod;
	return mod;
}

void qd_release_modules(qd_context* ctx) {
	if (!ctx) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_modules_mutex);
	auto ctx_it = g_context_modules.find(ctx);
	if (ctx_it == g_context_modules.end()) {
		return;
	}
	for (auto& mod_pair : ctx_it->second) {
		delete mod_pair.second;
	}
	g_context_modules.erase(ctx_it);
}

void qd_add_script(qd_module* mod, const char* script) {
	if (!mod || !script) {
		return;
	}
	mod->scripts.push_back(script);
}

int qd_load_file(qd_module* mod, const char* path) {
	if (!mod || !path) {
		return -1;
	}

	std::ifstream file(path, std::ios::in | std::ios::ate);
	if (!file.is_open()) {
		fprintf(stderr, "qd_load_file: Could not open '%s'\n", path);
		return -1;
	}

	auto pos = file.tellg();
	if (pos < 0) {
		return -1;
	}
	size_t size = static_cast<size_t>(pos);
	file.seekg(0);

	std::string contents(size, '\0');
	file.read(&contents[0], static_cast<std::streamsize>(size));

	mod->scripts.push_back(std::move(contents));
	return 0;
}

void qd_register_function(qd_module* mod, const char* name, const char* signature, qd_native_fn fn, void* userdata) {
	if (!mod || !name || !fn || !signature) {
		return;
	}
	mod->native_functions[name] = {fn, userdata, signature};

	// Also record it on the context, where lib/interp can find it: an embedder
	// registering once should reach both execution tiers.
	const std::string scoped = mod->name + "::" + name;
	qd_native_register(mod->owner_ctx, scoped.c_str(), signature, fn, userdata);
}

void qd_set_warning_min_line(qd_module* mod, size_t line) {
	if (!mod) {
		return;
	}
	mod->warning_min_line = line;
}

// Run a command line, collecting what it prints. Its exit status, or -1 if it did not run.
static int runCommand(const std::string& command, std::string& output) {
	FILE* pipe = popen(command.c_str(), "r");
	if (!pipe) {
		return -1;
	}
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), pipe)) {
		output += buffer;
	}
	return pclose(pipe);
}

static bool writeTextFile(const fs::path& path, const std::string& contents) {
	std::ofstream file(path, std::ios::binary);
	file << contents;
	file.close();
	return !file.fail();
}

// Every `use` under `node`, the module it names
static void forEachUse(Qd::IAstNode* node, const std::function<void(const std::string&)>& visit) {
	if (!node) {
		return;
	}
	if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
		visit(static_cast<Qd::AstNodeUse*>(node)->module());
	}
	for (size_t i = 0; i < node->childCount(); i++) {
		forEachUse(node->child(i), visit);
	}
}

static bool isQdFile(const std::string& name) {
	return name.size() > 3 && name.compare(name.size() - 3, 3, ".qd") == 0;
}

// The modules of `ctx` by name, as they stand now
static std::unordered_map<std::string, qd_module*> contextModules(qd_context* ctx) {
	std::lock_guard<std::mutex> lock(g_modules_mutex);
	auto it = g_context_modules.find(ctx);
	return it == g_context_modules.end() ? std::unordered_map<std::string, qd_module*>{} : it->second;
}

// The other modules of the context with typed native functions, written out as .qd
// files of FFI declarations, so that the validator and the generator see their
// signatures. libqd<name>.a is what gives their functions the usr_<ns>_<func> names
// the stubs below define.
static void writeTypedModules(qd_module* mod) {
	for (const auto& [name, other] : contextModules(mod->owner_ctx)) {
		if (other == mod) {
			continue;
		}
		std::string declarations;
		for (const auto& [fname, nf] : other->native_functions) {
			if (!nf.signature.empty()) {
				declarations += "\tpub fn " + fname + nf.signature + "\n";
			}
		}
		if (declarations.empty()) {
			continue;
		}
		const fs::path dir = mod->temp_dir / name;
		fs::create_directories(dir);
		writeTextFile(
				dir / (name + ".qd"), "import \"libqd" + name + ".a\" as \"" + name + "\" {\n" + declarations + "}\n");
	}
}

// Loads what a script uses, through the search quadc runs: the temp directory first,
// where writeTypedModules put the typed native modules, then the module roots. A module
// is every one of its files, each validated alongside the others.
class ModuleLoader {
public:
	explicit ModuleLoader(const fs::path& tempDir) : mTempDir(tempDir.string()) {
		mSearch.sourceDir = mTempDir;
	}

	void load(Qd::IAstNode* root) {
		collect(root, "", mTempDir);
	}

	// Namespace and root of every file loaded, a file before the files it uses
	std::vector<std::pair<std::string, Qd::IAstNode*>> modules;

private:
	Qd::IAstNode* parse(const std::string& path) {
		auto cached = mParsed.find(path);
		if (cached != mParsed.end()) {
			return cached->second;
		}
		Qd::IAstNode* root = nullptr;
		std::ifstream file(path);
		if (file) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			const std::string source = buffer.str();
			auto ast = std::make_unique<Qd::Ast>();
			root = ast->generate(source.c_str(), false, path.c_str());
			if (root && ast->hasErrors()) {
				root = nullptr;
			}
			mOwners.push_back(std::move(ast));
		}
		mParsed[path] = root;
		return root;
	}

	// The file names a file imports with `use helper.qd`
	std::unordered_set<std::string> qdImportsOf(const std::string& path) {
		std::unordered_set<std::string> names;
		forEachUse(parse(path), [&](const std::string& used) {
			if (isQdFile(used)) {
				names.insert(fs::path(used).filename().string());
			}
		});
		return names;
	}

	// One file of namespace `ns`, once, then whatever it uses in turn
	void loadFile(const std::string& path, const std::string& ns, const std::vector<std::string>& moduleFiles) {
		std::error_code ec;
		const fs::path canonical = fs::weakly_canonical(path, ec);
		if (!mLoadedFiles.emplace(ns, ec ? path : canonical.string()).second) {
			return;
		}
		Qd::IAstNode* root = parse(path);
		if (!root) {
			return;
		}
		Qd::SemanticValidator validator;
		validator.setIncludePaths({mTempDir});
		validator.setSiblingFiles(
				Qd::moduleSiblings(path, moduleFiles, [this](const std::string& file) { return qdImportsOf(file); }));
		if (validator.validate(root, path.c_str(), true, false) != 0) {
			return;
		}
		modules.push_back({ns, root});
		collect(root, ns, fs::path(path).parent_path().string());
	}

	void collect(Qd::IAstNode* root, const std::string& importerNs, const std::string& importerDir) {
		forEachUse(root, [&](const std::string& name) {
			if (isQdFile(name)) {
				// `use helper.qd` in a module is part of that module; in the script, a module
				// of its own, named after the file
				Qd::ModuleSearch fileSearch;
				fileSearch.sourceDir = importerDir;
				const std::string path = Qd::findModuleFile(name, fileSearch);
				if (!path.empty()) {
					const bool inModule = !importerNs.empty();
					loadFile(path, inModule ? importerNs : fs::path(name).stem().string(),
							inModule ? Qd::moduleFiles(fs::path(path).parent_path().string())
									 : std::vector<std::string>{});
				}
				return;
			}
			if (!mProcessedModules.insert(name).second) {
				return;
			}
			// Not found or ambiguous: the validator has reported it
			const Qd::ModuleLocation location = Qd::findModule(name, mSearch);
			if (location.directory.empty()) {
				return;
			}
			const std::vector<std::string> files = Qd::moduleFiles(location.directory);
			for (const std::string& file : files) {
				loadFile(file, location.namespaceName, files);
			}
		});
	}

	std::string mTempDir;
	Qd::ModuleSearch mSearch;
	std::vector<std::unique_ptr<Qd::Ast>> mOwners;
	std::unordered_map<std::string, Qd::IAstNode*> mParsed;
	std::unordered_set<std::string> mProcessedModules;
	std::set<std::pair<std::string, std::string>> mLoadedFiles; // namespace, canonical path
};

// The -I flags that find quadrate/qd/qd.h and what it includes
static std::string stubIncludeFlags() {
	fs::path installed = fs::path(findLibraryDir()) / ".." / "include";
	if (fs::exists(installed / "quadrate" / "qd" / "qd.h")) {
		return " -I" + shellQuote(fs::canonical(installed).string());
	}
	std::string flags;
	if (MESON_BUILD_ROOT[0] != '\0') {
		// Development build: the headers are in the source tree
		const fs::path sourceRoot = fs::path(MESON_BUILD_ROOT).parent_path().parent_path();
		if (fs::exists(sourceRoot / "lib" / "qd" / "include" / "quadrate" / "qd" / "qd.h")) {
			flags += " -I" + shellQuote((sourceRoot / "lib" / "qd" / "include").string());
		}
		if (fs::exists(sourceRoot / "lib" / "rt" / "include" / "quadrate" / "rt" / "runtime.h")) {
			flags += " -I" + shellQuote((sourceRoot / "lib" / "rt" / "include").string());
		}
	}
	return flags;
}

// Compiled code calls a native function of a used module as usr_<ns>_<func>. These stubs
// define those symbols, each a trampoline through qd_call_native, for the modules of the
// context that have native functions, typed or not.
static std::vector<fs::path> compileNativeStubs(qd_module* mod, Qd::IAstNode* root) {
	std::unordered_set<std::string> used;
	forEachUse(root, [&](const std::string& name) { used.insert(name); });

	const auto modules = contextModules(mod->owner_ctx);
	std::vector<fs::path> objects;
	std::string includeFlags;
	for (const auto& name : used) {
		auto it = modules.find(name);
		if (it == modules.end() || it->second->native_functions.empty()) {
			continue;
		}

		std::string source = "#include <quadrate/qd/qd.h>\n";
		for (const auto& [fname, nf] : it->second->native_functions) {
			source += "int usr_" + name + "_" + fname + "(qd_context* ctx) {\n    return qd_call_native(ctx, \"" +
					  name + "\", \"" + fname + "\");\n}\n";
		}
		const fs::path stubC = mod->temp_dir / ("stub_" + name + ".c");
		const fs::path stubO = mod->temp_dir / ("stub_" + name + ".o");
		if (!writeTextFile(stubC, source)) {
			continue;
		}

		if (includeFlags.empty()) {
			includeFlags = stubIncludeFlags();
		}
		std::string errors;
		const int status =
				runCommand(std::string(findCC()) + " -c -fPIC" + includeFlags + " " + shellQuote(stubC.string()) +
								   " -o " + shellQuote(stubO.string()) + " 2>&1",
						errors);
		if (status != 0) {
			fprintf(stderr, "qd_build: Failed to compile stub for module '%s':\n%s\n", name.c_str(), errors.c_str());
			continue;
		}
		objects.push_back(stubO);
	}
	return objects;
}

// The functions an object file defines, by the name qd_execute looks them up by
static std::unordered_map<std::string, std::string> exportedSymbols(qd_module* mod, const fs::path& object) {
	std::unordered_map<std::string, std::string> symbols;
	std::string listing;
	runCommand("nm " + shellQuote(object.string()) + " | grep ' T usr_' || true", listing);

	const std::string ownPrefix = "usr_" + mod->name + "_";
	std::istringstream lines(listing);
	std::string line;
	while (std::getline(lines, line)) {
		// "0000000000000000 T usr_package_function"
		const size_t start = line.find("usr_");
		if (start == std::string::npos) {
			continue;
		}
		const std::string symbol = line.substr(start);
		// The object holds this module's functions and every function of every module
		// it uses, and the map is keyed on the bare name -- so a script with its own
		// `parse` and a `use json` both want the key "parse", and whichever nm printed
		// second used to win. This module's own symbols take the key; a used module's
		// only fills one nothing else claims.
		if (symbol.compare(0, ownPrefix.size(), ownPrefix) == 0) {
			symbols[symbol.substr(ownPrefix.size())] = symbol;
		} else if (size_t second = symbol.find('_', 4); second != std::string::npos) {
			// usr_package_function -> function
			symbols.emplace(symbol.substr(second + 1), symbol);
		}
	}
	return symbols;
}

// The static library `libPath` and, through the .deps file beside it, what it needs in
// turn: a Quadrate library is linked and followed, anything else passed to the linker
// as written. Mirrors processLibraryDeps in generator.cc.
static void addLibraryDeps(
		const std::string& libDir, const std::string& libPath, std::set<std::string>& seen, std::string& flags) {
	if (!seen.insert(libPath).second) {
		return;
	}
	std::string depsFile = libPath;
	if (depsFile.size() > 2 && depsFile.compare(depsFile.size() - 2, 2, ".a") == 0) {
		depsFile = depsFile.substr(0, depsFile.size() - 2) + ".deps";
	}
	std::ifstream deps(depsFile);
	std::string line;
	while (std::getline(deps, line)) {
		line.erase(0, line.find_first_not_of(" \t\r\n"));
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		if (line.empty()) {
			continue;
		}
		if (line.rfind("-l", 0) == 0 && line.size() > 2) {
			const std::string depLib = findStaticLib(libDir, line.substr(2));
			if (!depLib.empty()) {
				flags += " " + depLib;
				addLibraryDeps(libDir, depLib, seen, flags);
				continue;
			}
		}
		flags += " " + line;
	}
}

// Links the object, the stubs, the runtime and the library of every module the script
// loaded into mod->so_path
static bool linkSharedLibrary(qd_module* mod, const fs::path& object, const std::vector<fs::path>& stubs,
		const std::vector<std::pair<std::string, Qd::IAstNode*>>& modules) {
	const std::string libDir = findLibraryDir();

	std::string command = std::string(findCXX()) + " -shared -fPIC " + shellQuote(object.string());
	for (const auto& stub : stubs) {
		command += " " + shellQuote(stub.string());
	}
	command += " -o " + shellQuote(mod->so_path.string());

	std::unordered_set<std::string> libraries = {"rt"};
	for (const auto& module : modules) {
		libraries.insert(module.first);
	}
	std::set<std::string> seen;
	std::string depsFlags;
	for (const auto& library : libraries) {
		const std::string libPath = findStaticLib(libDir, library);
		if (!libPath.empty()) {
			command += " -Wl,--whole-archive " + shellQuote(libPath) + " -Wl,--no-whole-archive";
			addLibraryDeps(libDir, libPath, seen, depsFlags);
		}
	}
	if (!depsFlags.empty()) {
		command += " -Wl,--start-group" + depsFlags + " -Wl,--end-group";
	}
	command += " -lm";
	if (!stubs.empty()) {
		// The stubs call qd_call_native, which resolves at dlopen time against the libqd
		// already loaded. In a meson build tree it is in a qd/ subdirectory.
		command += " -L" + shellQuote(libDir);
		if (fs::exists(fs::path(libDir) / "qd" / "libqd.so")) {
			command += " -L" + shellQuote(libDir + "/qd");
		}
		command += " -lqd";
	}

	std::string errors;
	const int status = runCommand(command + " 2>&1", errors);
	if (status == -1) {
		fprintf(stderr, "qd_build: Failed to execute linker\n");
		return false;
	}
	if (status != 0) {
		fprintf(stderr, "qd_build: Linking failed:\n%s\n", errors.c_str());
		return false;
	}
	return true;
}

// Whatever an earlier build of the module left: its library and its temp directory
static void discardBuild(qd_module* mod) {
	if (mod->dl_handle) {
		dynlib_platform_close(mod->dl_handle);
		mod->dl_handle = nullptr;
	}
	if (!mod->temp_dir.empty()) {
		std::error_code ec;
		fs::remove_all(mod->temp_dir, ec);
		mod->temp_dir.clear();
	}
	mod->symbol_map.clear();
	mod->compiled = false;
}

void qd_build(qd_module* mod) {
	if (!mod) {
		return;
	}
	discardBuild(mod);

	if (mod->scripts.empty()) {
		// Native functions alone need nothing compiled
		if (!mod->native_functions.empty()) {
			mod->compiled = true;
			return;
		}
		fprintf(stderr, "qd_build: No scripts to compile for module '%s'\n", mod->name.c_str());
		return;
	}

	try {
		std::string tempTemplate = (fs::temp_directory_path() / "qd_embed_XXXXXX").string();
		if (!mkdtemp(tempTemplate.data())) {
			fprintf(stderr, "qd_build: Failed to create temporary directory\n");
			return;
		}
		mod->temp_dir = tempTemplate;

		// The scripts as one source, a line apart
		std::string source;
		for (const auto& script : mod->scripts) {
			source += script + "\n";
		}
		const fs::path sourceFile = mod->temp_dir / "script.qd";
		if (!writeTextFile(sourceFile, source)) {
			fprintf(stderr, "qd_build: Failed to write source file\n");
			return;
		}
		writeTypedModules(mod);

		const std::string displayName = "<" + mod->name + ">";
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(source.c_str(), false, displayName.c_str());
		if (!root || ast.hasErrors()) {
			for (const auto& error : ast.getErrors()) {
				fprintf(stderr, "%s:%zu:%zu: error: %s\n", displayName.c_str(), error.line, error.column,
						error.message.c_str());
			}
			fprintf(stderr, "qd_build: Failed to parse script\n");
			return;
		}

		// Validated as a module, which is what dynamically loaded code is
		Qd::SemanticValidator validator;
		if (mod->warning_min_line > 0) {
			validator.setWarningMinLine(mod->warning_min_line);
		}
		validator.setIncludePaths({mod->temp_dir.string()});
		validator.setDisplayFilename(displayName);
		const size_t errorCount = validator.validate(root, sourceFile.string().c_str(), true, false);
		if (errorCount > 0) {
			fprintf(stderr, "qd_build: Semantic validation failed with %zu error(s)\n", errorCount);
			return;
		}

		ModuleLoader loader(mod->temp_dir);
		loader.load(root);
		const std::vector<fs::path> stubs = compileNativeStubs(mod, root);

		Qd::LlvmGenerator generator;
		generator.setOptimizationLevel(2);
		generator.setExportMode(true); // A shared library's functions are its interface
		// A used module after the modules it uses
		for (auto it = loader.modules.rbegin(); it != loader.modules.rend(); ++it) {
			generator.addModuleAST(it->first, it->second);
		}
		if (!generator.generate(root, mod->name)) {
			fprintf(stderr, "qd_build: Failed to generate LLVM IR\n");
			return;
		}
		const fs::path object = mod->temp_dir / "script.o";
		if (!generator.writeObject(object.string())) {
			fprintf(stderr, "qd_build: Failed to write object file\n");
			return;
		}

		mod->symbol_map = exportedSymbols(mod, object);
		mod->so_path = mod->temp_dir / ("lib" + mod->name + ".so");
		if (!linkSharedLibrary(mod, object, stubs, loader.modules)) {
			return;
		}

		mod->dl_handle = dynlib_platform_open(mod->so_path.c_str());
		if (!mod->dl_handle) {
			fprintf(stderr, "qd_build: Failed to load shared library: %s\n", dynlib_platform_error());
			return;
		}
		mod->compiled = true;
	} catch (const std::exception& e) {
		fprintf(stderr, "qd_build: Exception: %s\n", e.what());
	}
}

bool qd_is_compiled(qd_module* mod) {
	if (!mod) {
		return false;
	}
	return mod->compiled;
}

int qd_call_native(qd_context* ctx, const char* module_name, const char* func_name) {
	if (!ctx || !module_name || !func_name) {
		return QD_ERR_GENERIC;
	}

	auto ctx_it = g_context_modules.find(ctx);
	if (ctx_it == g_context_modules.end()) {
		return QD_ERR_GENERIC;
	}

	auto mod_it = ctx_it->second.find(module_name);
	if (mod_it == ctx_it->second.end()) {
		return QD_ERR_GENERIC;
	}

	qd_module* mod = mod_it->second;
	auto fn_it = mod->native_functions.find(func_name);
	if (fn_it == mod->native_functions.end()) {
		return QD_ERR_GENERIC;
	}

	auto& nf = fn_it->second;
	return nf.fn(ctx, nf.userdata);
}

void qd_execute(qd_context* ctx, const char* code) {
	if (!ctx || !code) {
		return;
	}

	// Parse the function call/code
	// For simple cases, we support "module::function" syntax
	// For inline code, we support direct operations like "123.34 . hello::world"

	std::string code_str(code);

	// Try to tokenize and execute the code
	// This is a simple interpreter for basic operations

	std::istringstream iss(code_str);
	std::string token;

	while (iss >> token) {
		// Check if it's a number (integer or float)
		char* endptr;

		int64_t int_val = 0;
		if (Qd::parseIntegerLiteral(token, int_val)) {
			qd_push_i(ctx, int_val);
			continue;
		}

		// Try parsing as float
		double float_val = strtod(token.c_str(), &endptr);
		if (*endptr == '\0') {
			// It's a float
			qd_push_f(ctx, float_val);
			continue;
		}

		// Check if it's a string literal
		if (token[0] == '"') {
			// Read until closing quote
			std::string str_val = token.substr(1); // Remove opening quote
			if (str_val.empty() || str_val.back() != '"') {
				// Need to read more tokens
				std::string rest;
				std::getline(iss, rest, '"');
				str_val += " " + rest;
			} else {
				str_val.pop_back(); // Remove closing quote
			}
			qd_push_s(ctx, str_val.c_str());
			continue;
		}

		// Check for built-in operations
		if (token == "print") {
			qd_print(ctx);
		} else if (token == "nl") {
			qd_nl(ctx);
		} else if (token == "dup") {
			qd_dup(ctx);
		} else if (token == "swap") {
			qd_swap(ctx);
		} else if (token == "drop") {
			qd_drop(ctx);
		} else if (token == "+") {
			qd_add(ctx);
		} else if (token == "-") {
			qd_sub(ctx);
		} else if (token == "*") {
			qd_mul(ctx);
		} else if (token == "/") {
			qd_div(ctx);
		} else if (token.find("::") != std::string::npos) {
			// Module-qualified function call
			size_t sep_pos = token.find("::");
			std::string module_name = token.substr(0, sep_pos);
			std::string func_name = token.substr(sep_pos + 2);

			// Look up module
			auto ctx_it = g_context_modules.find(ctx);
			if (ctx_it == g_context_modules.end()) {
				fprintf(stderr, "qd_execute: No modules registered for context\n");
				continue;
			}

			auto mod_it = ctx_it->second.find(module_name);
			if (mod_it == ctx_it->second.end()) {
				fprintf(stderr, "qd_execute: Module '%s' not found\n", module_name.c_str());
				continue;
			}

			qd_module* mod = mod_it->second;
			if (!mod->compiled) {
				fprintf(stderr, "qd_execute: Module '%s' not compiled\n", module_name.c_str());
				continue;
			}

			// Look up function in symbol map first
			std::string symbol_name;
			auto symbol_it = mod->symbol_map.find(func_name);
			if (symbol_it != mod->symbol_map.end()) {
				symbol_name = symbol_it->second;
			} else {
				// Fall back to expected name
				symbol_name = "usr_" + module_name + "_" + func_name;
			}

			// Try compiled (.so) function first
			typedef int (*qd_compiled_fn)(qd_context*);

			void* sym = dynlib_platform_symbol(mod->dl_handle, symbol_name.c_str());
			if (sym) {
				auto func = reinterpret_cast<qd_compiled_fn>(sym);
				int result = func(ctx);
				if (result != 0) {
					fprintf(stderr, "qd_execute: Function '%s::%s' returned error code %d\n", module_name.c_str(),
							func_name.c_str(), result);
				}
			} else {
				// Try native function
				auto native_it = mod->native_functions.find(func_name);
				if (native_it != mod->native_functions.end()) {
					auto& nf = native_it->second;
					int result = nf.fn(ctx, nf.userdata);
					if (result != 0) {
						fprintf(stderr, "qd_execute: Function '%s::%s' returned error code %d\n", module_name.c_str(),
								func_name.c_str(), result);
					}
				} else {
					fprintf(stderr, "qd_execute: Function '%s' (symbol '%s') not found in module '%s': %s\n",
							func_name.c_str(), symbol_name.c_str(), module_name.c_str(), dynlib_platform_error());
				}
			}
		} else {
			fprintf(stderr, "qd_execute: Unknown token '%s'\n", token.c_str());
		}
	}
}

// Clean up modules when context is freed (best effort)
// Note: This is a workaround since we can't intercept qd_free_context
// In production, we'd need to modify the context structure or use atexit
namespace {
	struct ContextCleaner {
		~ContextCleaner() {
			// Clean up any modules the embedder did not release explicitly via
			// qd_release_modules(). Best-effort backstop at process exit.
			std::lock_guard<std::mutex> lock(g_modules_mutex);
			for (auto& ctx_pair : g_context_modules) {
				for (auto& mod_pair : ctx_pair.second) {
					delete mod_pair.second;
				}
			}
			g_context_modules.clear();
		}
	};

	static ContextCleaner g_cleaner;
}
