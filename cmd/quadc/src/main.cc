#include "ast_cache.h"
#include "build_cache.h"
#include "diagnostics.h"
#include "module_resolver.h"
#include "options.h"
#include "parsed_module.h"
#include "temp_dir_guard.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <quadrate/cli/cli.h>
#include <quadrate/cli/file_utils.h>
#include <quadrate/llvmgen/generator.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_printer.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/module_resolver.h>
#include <quadrate/qc/semantic_validator.h>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Deduplicate diagnostics by line:column:message before printing
static void printDeduplicatedErrors(const std::string& file, const auto& errors) {
	std::set<std::string> seen;
	for (const auto& error : errors) {
		std::string key = std::to_string(error.line) + ":" + std::to_string(error.column) + ":" + error.message;
		if (seen.insert(key).second) {
			printError(file, error.line, error.column, error.message);
		}
	}
}

// Stdlib modules that have native code and require linking
// JIT execution doesn't support these - must fall back to disk compilation
static const std::unordered_set<std::string> nativeStdlibModules = {"decimal", "fmt", "io", "math", "mem", "os",
		"signal", "strings", "strconv", "testing", "thread", "time", "tty"};

// Check if any of the imported modules are stdlib modules with native code
static bool hasNativeStdlibImports(const std::vector<std::string>& importedModules) {
	for (const auto& mod : importedModules) {
		if (nativeStdlibModules.count(mod) > 0) {
			return true;
		}
	}
	return false;
}

// Canonical form of a directory path, for comparing two paths that may differ in spelling
// (relative vs absolute, symlinks, trailing "."). Falls back to the input if the path
// cannot be resolved, which just means the comparison degrades to a textual one.
static std::string canonicalDir(const std::string& dir) {
	std::error_code ec;
	std::filesystem::path resolved = std::filesystem::canonical(dir, ec);
	return ec ? dir : resolved.string();
}

// Whether filePath lives directly in one of the given directories
static bool isInDirectory(const std::string& filePath, const std::unordered_set<std::string>& dirs) {
	if (dirs.empty()) {
		return false;
	}
	std::filesystem::path parent = std::filesystem::path(filePath).parent_path();
	return dirs.count(canonicalDir(parent.empty() ? "." : parent.string())) > 0;
}

static bool definesFunction(Qd::IAstNode* root, const std::string& name) {
	for (auto* child : root->children()) {
		if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION &&
				static_cast<Qd::AstNodeFunctionDeclaration*>(child)->name() == name) {
			return true;
		}
	}
	return false;
}

static bool isSameFile(const std::string& a, const std::string& b) {
	std::error_code ec;
	return std::filesystem::equivalent(a, b, ec) && !ec;
}

static bool checkOutputPath(const std::string& outputPath, const std::vector<std::string>& inputs) {
	for (const auto& input : inputs) {
		if (isSameFile(outputPath, input)) {
			printToolError("output file '" + outputPath + "' is the same as input file '" + input + "'");
			printNote("use -o to choose a different output name");
			return false;
		}
	}
	std::filesystem::path parent = std::filesystem::path(outputPath).parent_path();
	std::error_code ec;
	if (!parent.empty() && std::filesystem::exists(parent, ec) && !std::filesystem::is_directory(parent, ec)) {
		printToolError("cannot write output '" + outputPath + "': '" + parent.string() + "' is not a directory");
		return false;
	}
	if (!parent.empty() && !std::filesystem::is_directory(parent, ec)) {
		printToolError("cannot write output '" + outputPath + "': directory '" + parent.string() + "' does not exist");
		return false;
	}
	return true;
}

static int runExecutable(const std::string& path, const std::vector<std::string>& args) {
	std::vector<char*> argv;
	argv.reserve(args.size() + 2);
	argv.push_back(const_cast<char*>(path.c_str()));
	for (const auto& arg : args) {
		argv.push_back(const_cast<char*>(arg.c_str()));
	}
	argv.push_back(nullptr);

	std::cout.flush();
	std::cerr.flush();

	struct sigaction ignore = {};
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	struct sigaction oldInt = {};
	struct sigaction oldQuit = {};
	sigaction(SIGINT, &ignore, &oldInt);
	sigaction(SIGQUIT, &ignore, &oldQuit);

	pid_t pid = fork();
	if (pid < 0) {
		sigaction(SIGINT, &oldInt, nullptr);
		sigaction(SIGQUIT, &oldQuit, nullptr);
		printError(std::string("failed to execute program: ") + std::strerror(errno));
		return 1;
	}
	if (pid == 0) {
		sigaction(SIGINT, &oldInt, nullptr);
		sigaction(SIGQUIT, &oldQuit, nullptr);
		execv(argv[0], argv.data());
		int err = errno;
		std::string msg = "quadc: failed to execute '" + path + "': " + std::strerror(err) + "\n";
		ssize_t written = write(STDERR_FILENO, msg.data(), msg.size());
		(void)written;
		_exit(127);
	}

	int status = 0;
	pid_t waited;
	do {
		waited = waitpid(pid, &status, 0);
	} while (waited < 0 && errno == EINTR);
	sigaction(SIGINT, &oldInt, nullptr);
	sigaction(SIGQUIT, &oldQuit, nullptr);

	if (waited < 0) {
		printError(std::string("failed to wait for program: ") + std::strerror(errno));
		return 1;
	}
	if (WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		const char* name = strsignal(sig);
		printError(
				"program terminated by signal " + std::to_string(sig) + (name ? std::string(" (") + name + ")" : ""));
		return 128 + sig;
	}
	return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

// Phase timings on stderr, when QUADC_TIMING is set
class Timer {
public:
	bool enabled() const {
		return mEnabled;
	}

	// The time since the previous step
	void step(const char* label) {
		if (!mEnabled) {
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		std::cerr << "[TIMING] " << label << ": " << milliseconds(mLast, now) << "ms" << std::endl;
		mLast = now;
	}

	// The time since quadc started
	void total(const char* label) const {
		if (mEnabled) {
			std::cerr << "[TIMING] " << label << ": " << milliseconds(mStart, std::chrono::steady_clock::now()) << "ms"
					  << std::endl;
		}
	}

private:
	static long long milliseconds(
			std::chrono::steady_clock::time_point from, std::chrono::steady_clock::time_point to) {
		return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count());
	}

	bool mEnabled = std::getenv("QUADC_TIMING") != nullptr;
	std::chrono::steady_clock::time_point mStart = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point mLast = mStart;
};

// Parses and validates one source the command line named, the file `name` or stdin, and adds
// it to `parsedModules` as part of the main program. A file brings the files beside it, its
// directory's namespace. False, with the problem reported, when it does not compile.
static bool parseMainSource(const std::string& name, const std::string& source, bool fromStdin, const Options& opts,
		Timer& timer, std::vector<ParsedModule>& parsedModules) {
	// The files named here, not the modules they import: those are written for f64
	auto ast = std::make_unique<Qd::Ast>();
	ast->setDefaultFloatD128(opts.floatsAreD128);
	auto root = ast->generate(source.c_str(), opts.dumpTokens, name.c_str());
	timer.step("parse");
	if (!root || ast->hasErrors()) {
		printDeduplicatedErrors(name, ast->getErrors());
		printParseFailure(name, ast->errorCount());
		return false;
	}

	if (opts.dumpAst) {
		std::cout << "=== AST for " << name << " ===" << std::endl;
		Qd::AstPrinter::print(root);
		std::cout << std::endl;
	}

	const std::vector<std::string> siblingFiles =
			fromStdin ? std::vector<std::string>{} : qdcli::getSiblingQdFiles(name);

	Qd::SemanticValidator validator;
	validator.setIncludePaths(opts.includePaths);
	validator.setModuleVersionPins(opts.moduleVersions);
	validator.setSource(source.c_str());
	validator.setSiblingFiles(siblingFiles);
	validator.setFreestandingMode(opts.freestanding);
	const size_t errorCount = validator.validate(root, name.c_str(), false, opts.werror);
	timer.step("semantic");
	if (errorCount > 0) {
		return false;
	}

	// The validator's parsed modules, for the module loop to reuse
	AstCache::instance().importFromValidator(validator);

	ParsedModule module;
	module.name = name;
	module.package = "main";
	module.sourceDirectory = fromStdin ? "." : std::filesystem::path(name).parent_path().string();
	if (module.sourceDirectory.empty()) {
		module.sourceDirectory = ".";
	}
	module.root = root;
	module.ast = std::move(ast);
	module.importedModules = collectImportedModules(root);
	module.hasFFIImports = !validator.importedLibraries().empty();
	module.hasNativeStdlibModules = hasNativeStdlibImports(module.importedModules);
	// The directory's other files are merged into the program, as a `use "file.qd"` would be
	module.importedModules.insert(module.importedModules.end(), siblingFiles.begin(), siblingFiles.end());
	parsedModules.push_back(std::move(module));
	return true;
}

// The installed package a module directory belongs to, whose lib/ the linker searches: the
// directory itself when it has a lib/, else, under the packages directory, the nearest one
// above it that does. quadpm installs a package at <packages>/<host>/<user>/<repo>@<ref>, so
// the package is not simply the first directory below <packages>, which is the host. Empty
// for a module that is no package's.
static std::string packageDirectoryOf(const std::string& moduleDirectory) {
	namespace fs = std::filesystem;
	std::error_code ec;
	auto hasLib = [&ec](const fs::path& dir) { return fs::is_directory(dir / "lib", ec); };
	if (hasLib(moduleDirectory)) {
		return moduleDirectory;
	}
	const std::string packages = Qd::packagesDirectory();
	if (packages.empty()) {
		return "";
	}
	const fs::path root = fs::absolute(packages, ec).lexically_normal();
	fs::path dir = fs::absolute(moduleDirectory, ec).lexically_normal();
	const auto [rootEnd, dirIt] = std::mismatch(root.begin(), root.end(), dir.begin(), dir.end());
	if (rootEnd != root.end()) {
		return ""; // not under the packages directory
	}
	while (dir.has_parent_path() && dir != root) {
		dir = dir.parent_path();
		if (dir != root && hasLib(dir)) {
			return dir.string();
		}
	}
	return "";
}

// Validates one file of a module, alongside the module's other files, unless the validator
// has already checked it or it belongs to a main source's directory, which the main pass
// checked. False when it has errors, which are reported.
static bool validateModuleFile(const Options& opts, Timer& timer, const std::string& moduleName,
		const std::string& moduleFilePath, const std::vector<std::string>& moduleFilePaths, Qd::IAstNode* root,
		const std::string& source, bool alreadyValidated, bool ownedByMainPass) {
	// A cache hit is NOT evidence that anything checked this file: the semantic validator
	// parses module files to collect their signatures and hands those ASTs to the cache, so by
	// the time the module loop runs every module is a hit. Guarding on "came from cache" left
	// every imported module body unchecked, which is how a stdlib function that leaked three
	// stack slots per call stayed invisible. Guard on the AST actually having been validated.
	//
	// Files in a main source's own directory are its directory-namespace siblings, already
	// checked by the main-file pass, and only that pass can resolve them: a sibling may use a
	// constant defined in the file holding main(), which is not one of its siblings.
	if (alreadyValidated || ownedByMainPass) {
		if (timer.enabled()) {
			std::cerr << "[TIMING] moduleLoop validate " << moduleName << ": 0ms ("
					  << (ownedByMainPass ? "main-file sibling" : "already validated") << ")" << std::endl;
		}
		return true;
	}
	const auto start = std::chrono::steady_clock::now();
	// A file linked to this one by a `use "other.qd"`, either way, is not also a sibling:
	// loading it twice reported every function it defines as a duplicate
	auto qdFileImportsOf = [](const std::string& path) {
		std::unordered_set<std::string> names;
		Qd::IAstNode* fileRoot = AstCache::instance().getOrParse(path);
		if (!fileRoot) {
			return names;
		}
		for (const auto& imported : collectImportedModules(fileRoot)) {
			if (isQdFile(imported)) {
				names.insert(std::filesystem::path(imported).filename().string());
			}
		}
		return names;
	};
	Qd::SemanticValidator validator;
	validator.setIncludePaths(opts.includePaths);
	validator.setModuleVersionPins(opts.moduleVersions);
	validator.setSource(source.c_str());
	validator.setSiblingFiles(Qd::moduleSiblings(moduleFilePath, moduleFilePaths, qdFileImportsOf));
	validator.setFreestandingMode(opts.freestanding);
	const size_t errorCount = validator.validate(root, moduleFilePath.c_str(), true, opts.werror);
	if (timer.enabled()) {
		const auto ms =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		std::cerr << "[TIMING] moduleLoop validate " << moduleName << ": " << ms << "ms" << std::endl;
	}
	if (errorCount > 0) {
		return false;
	}
	AstCache::instance().markValidated(moduleFilePath);
	return true;
}

// Loads every module the main sources use, and the modules those use, into `parsedModules`.
// False, with the problem reported, when one does not compile.
static bool loadImportedModules(const Options& opts, Timer& timer, std::vector<ParsedModule>& parsedModules) {
	// Collect all imported modules from main sources
	// Track which package each import belongs to (for .qd file imports)
	std::unordered_set<std::string> allModules;
	std::unordered_set<std::string> processedModules;
	// Each file once per namespace: a module's own `use "helper.qd"` names a file the module
	// already holds, and loading it twice generated its functions twice
	std::set<std::pair<std::string, std::string>> loadedModuleFiles;
	std::unordered_map<std::string, std::string> moduleToPackage;	// moduleName -> packageName
	std::unordered_map<std::string, std::string> moduleToSourceDir; // moduleName -> sourceDirectory
	std::unordered_set<std::string> modulesToMergeIntoMain;			// modules that should merge into main
	std::string sourceDirectory;
	// Directories holding a main source file. Files there are directory-namespace siblings
	// owned by the main-file validation pass; the module loop must not re-validate them.
	std::unordered_set<std::string> mainFileDirs;
	for (const auto& module : parsedModules) {
		if (module.package == "main" && !module.sourceDirectory.empty()) {
			mainFileDirs.insert(canonicalDir(module.sourceDirectory));
		}
	}
	for (const auto& module : parsedModules) {
		for (const auto& importedModule : module.importedModules) {
			allModules.insert(importedModule);

			// Check if this is a .qd file import (direct file import)
			if (isQdFile(importedModule)) {
				// Direct file imports: determine if this is an intra-module import or a top-level import
				// Intra-module: parent is a module directory, file inherits parent's namespace
				// Top-level: parent is standalone .qd, file gets its own namespace from filename

				// Check if parent is a module directory (doesn't end in .qd)
				bool parentIsModuleDirectory = !isQdFile(module.name);

				if (parentIsModuleDirectory) {
					// Intra-module import: use parent's package
					// e.g., split_module module imports helper.qd → helper functions are split_module::*
					moduleToPackage[importedModule] = module.package;
				} else {
					// Top-level import: derive package from imported filename
					// e.g., main.qd imports calculator.qd → calculator functions are calculator::*
					std::string derivedPackage = getPackageFromModuleName(importedModule);
					moduleToPackage[importedModule] = derivedPackage;
					// Mark this module to be merged into main namespace (by package name)
					modulesToMergeIntoMain.insert(derivedPackage);
				}
				moduleToSourceDir[importedModule] = module.sourceDirectory;
			} else {
				// Regular module imports get their own package
				moduleToPackage[importedModule] = importedModule;
				moduleToSourceDir[importedModule] = module.sourceDirectory;
			}
		}
		// Use source directory from first source (they should all be the same for now)
		if (sourceDirectory.empty()) {
			sourceDirectory = module.sourceDirectory;
		}
	}

	// Transpile all imported modules (including transitive imports)
	// Keep processing until no new modules are discovered
	while (!allModules.empty()) {
		// Get next unprocessed module
		std::string moduleName = *allModules.begin();
		allModules.erase(allModules.begin());

		// Skip if already processed
		if (processedModules.count(moduleName)) {
			continue;
		}
		processedModules.insert(moduleName);

		// Get the package name and source directory for this module
		std::string moduleSourceDir =
				moduleToSourceDir.count(moduleName) ? moduleToSourceDir[moduleName] : sourceDirectory;
		const ResolvedModule resolved = findModuleFiles(moduleName, moduleSourceDir);
		std::string packageName =
				moduleToPackage.count(moduleName) ? moduleToPackage[moduleName] : resolved.namespaceName;

		const std::vector<std::string>& moduleFilePaths = resolved.files;
		if (moduleFilePaths.empty()) {
			// Module files not found - skip silently (already validated)
			continue;
		}
		// A full-path import resolves, and the validator checks the program against it, but
		// code generation keys a module by the name its `use` wrote, which is a path here
		if (!isQdFile(moduleName) && resolved.namespaceName != moduleName) {
			printError("use \"" + moduleName + "\": a full-path import cannot be compiled yet; write 'use " +
					   resolved.namespaceName + "', the namespace the package declares");
			return false;
		}

		// Process all files for this module
		for (const auto& moduleFilePath : moduleFilePaths) {
			std::error_code canonicalError;
			const std::string canonicalFile =
					std::filesystem::weakly_canonical(moduleFilePath, canonicalError).string();
			if (!loadedModuleFiles.emplace(packageName, canonicalError ? moduleFilePath : canonicalFile).second) {
				continue;
			}

			// Parse module file (uses cache if already parsed by semantic validator)
			Qd::Ast* ast = nullptr;
			std::string buffer;
			bool alreadyValidated = false;

			auto parseStart = std::chrono::steady_clock::now();
			Qd::IAstNode* root = AstCache::instance().getOrParse(moduleFilePath, &ast, &buffer, &alreadyValidated);
			auto parseEnd = std::chrono::steady_clock::now();
			if (timer.enabled()) {
				auto parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(parseEnd - parseStart).count();
				std::cerr << "[TIMING] moduleLoop parse " << moduleName << ": " << parseMs << "ms" << std::endl;
			}
			if (!root) {
				printError("failed to parse module: " + moduleName);
				// Print stored parse errors if available (deduplicated)
				if (ast) {
					printDeduplicatedErrors(moduleFilePath, ast->getErrors());
				}
				return false;
			}

			// Checked before code generation sees it
			if (!validateModuleFile(opts, timer, moduleName, moduleFilePath, moduleFilePaths, root, buffer,
						alreadyValidated, isInDirectory(moduleFilePath, mainFileDirs))) {
				return false;
			}

			// Get module's source directory
			std::filesystem::path moduleFilePathObj(moduleFilePath);
			std::string moduleFileSourceDir = moduleFilePathObj.parent_path().string();
			if (moduleFileSourceDir.empty()) {
				moduleFileSourceDir = ".";
			}

			const std::string packageDir = packageDirectoryOf(moduleFileSourceDir);

			ParsedModule parsedMod;
			parsedMod.name = moduleFilePath; // Store full file path for debug info
			parsedMod.package = packageName;
			parsedMod.sourceDirectory = moduleFileSourceDir;
			parsedMod.packageDirectory = packageDir;
			parsedMod.root = root;
			// Note: parsedMod.ast is nullptr - AST ownership is held by the global cache
			parsedMod.importedModules = collectImportedModules(root);
			parsedMod.hasFFIImports = hasFFIImportsInAST(root);
			parsedMod.hasNativeStdlibModules = hasNativeStdlibImports(parsedMod.importedModules);
			// Check if this module should be merged into main namespace (by package name)
			parsedMod.mergeIntoMain = modulesToMergeIntoMain.count(packageName) > 0;

			// Add any modules imported by this module to the set
			for (const auto& transitiveModule : parsedMod.importedModules) {
				if (!processedModules.count(transitiveModule)) {
					allModules.insert(transitiveModule);

					// Determine package for transitive imports
					if (isQdFile(transitiveModule)) {
						// Check if importing file is a module directory (doesn't end in .qd)
						bool importerIsModuleDirectory = !isQdFile(moduleName);

						if (importerIsModuleDirectory) {
							// Intra-module import: use importer's package
							moduleToPackage[transitiveModule] = packageName;
						} else {
							// Top-level import: derive package from filename
							std::string derivedPackage = getPackageFromModuleName(transitiveModule);
							moduleToPackage[transitiveModule] = derivedPackage;
							// If the importing module is merged, also merge this transitive import
							// This allows nested .qd file imports to be accessible without module prefix
							if (parsedMod.mergeIntoMain) {
								modulesToMergeIntoMain.insert(derivedPackage);
							}
						}
						// File imports use the importing module's source directory
						moduleToSourceDir[transitiveModule] = moduleFileSourceDir;
					} else {
						// Regular module imports get their own package and search from original source dir
						moduleToPackage[transitiveModule] = transitiveModule;
						moduleToSourceDir[transitiveModule] = sourceDirectory;
					}
				}
			}

			parsedModules.push_back(std::move(parsedMod));
		} // end for each file in module
	}
	return true;
}

// Everything that decides what quadc produces, for the build cache to key on
static void addCacheInputs(
		BuildCache& buildCache, const Options& opts, const std::vector<ParsedModule>& parsedModules) {
	for (const auto& mod : parsedModules) {
		buildCache.addSourceFile(mod.name);
		buildCache.addOption("module:" + mod.package + ":" + mod.name + ":" + (mod.mergeIntoMain ? "1" : "0"));
	}
	buildCache.addOption("opt:" + std::to_string(opts.optLevel));
	buildCache.addOption("stack:" + std::to_string(opts.stackSize));
	buildCache.addOption("target:" + opts.targetTriple);
	buildCache.addOption("debug:" + std::to_string(opts.debugInfo ? 1 : 0));
	buildCache.addOption("freestanding:" + std::to_string(opts.freestanding ? 1 : 0));
	buildCache.addOption("test:" + std::to_string(opts.testMode ? 1 : 0));
	buildCache.addOption("coverage:" + std::to_string(opts.coverage ? 1 : 0));
	buildCache.addOption("floats:" + std::string(opts.floatsAreD128 ? "d128" : "f64"));
	// A different quadc can produce different output from identical sources
	buildCache.addCompilerIdentity();
	// ...and so can a different runtime or stdlib, which is linked in
	buildCache.addStdlibIdentity();
}

// The generator, set up as the options say, with every module but the main one added. False,
// reported, for options that cannot go together.
static bool configureGenerator(
		Qd::LlvmGenerator& generator, const Options& opts, const std::vector<ParsedModule>& parsedModules) {
	if (opts.freestanding && opts.testMode) {
		printError("--freestanding is incompatible with --test");
		return false;
	}
	if (opts.debugInfo) {
		generator.setDebugInfo(true);
	}
	generator.setOptimizationLevel(opts.optLevel);
	generator.setStackSize(opts.stackSize);
	if (opts.testMode) {
		generator.setTestMode(true);
		if (opts.coverage) {
			generator.setCoverageMode(true);
		}
	}
	// Freestanding: no auto-main and no libc, a _start shim instead
	if (opts.freestanding) {
		generator.setFreestandingMode(true);
	}
	if (!opts.targetTriple.empty()) {
		generator.setTargetTriple(opts.targetTriple);
	}

	// The linker searches each package's lib directory, and each -I path's, once
	std::set<std::string> addedPackagePaths;
	for (const auto& module : parsedModules) {
		const std::string libPath = module.packageDirectory + "/lib";
		if (!module.packageDirectory.empty() && addedPackagePaths.count(module.packageDirectory) == 0 &&
				std::filesystem::exists(libPath)) {
			generator.addLibrarySearchPath(libPath);
			addedPackagePaths.insert(module.packageDirectory);
		}
	}
	for (const auto& includePath : opts.includePaths) {
		const std::string libPath = includePath + "/lib";
		if (std::filesystem::exists(libPath) && addedPackagePaths.count(includePath) == 0) {
			generator.addLibrarySearchPath(libPath);
			addedPackagePaths.insert(includePath);
		}
	}

	// Modules were loaded breadth-first, main first; they are generated depth-first, the
	// deepest dependencies first
	for (auto it = parsedModules.rbegin(); it != parsedModules.rend(); ++it) {
		if (it->package != "main") {
			generator.addModuleAST(it->package, it->root, it->name, it->mergeIntoMain);
		}
	}
	return true;
}

// Whether the program has the entry point its mode needs: one `main`, or in freestanding mode
// `_start` or `main`, or in test mode at least one test. Reported when it does not.
static bool checkEntryPoint(
		const Options& opts, const std::vector<ParsedModule>& parsedModules, Qd::IAstNode* mainRoot) {
	auto hasChild = [mainRoot](const std::function<bool(Qd::IAstNode*)>& matches) {
		for (auto* child : mainRoot->children()) {
			if (child && matches(child)) {
				return true;
			}
		}
		return false;
	};

	if (opts.testMode) {
		if (!hasChild([](Qd::IAstNode* child) { return child->type() == Qd::IAstNode::Type::TEST_DECLARATION; })) {
			printError("no test blocks found in --test mode");
			printNote("use 'test \"name\" { ... }' to define tests");
			return false;
		}
		return true;
	}

	std::vector<std::string> mainDefiners;
	for (const auto& module : parsedModules) {
		if (module.package == "main" && definesFunction(module.root, "main")) {
			mainDefiners.push_back(module.name);
		}
	}
	if (mainDefiners.size() > 1) {
		printError("multiple input files define 'main': " + mainDefiners[0] + ", " + mainDefiners[1]);
		printNote("a program has exactly one entry point; pass one main file");
		return false;
	}

	if (opts.freestanding) {
		if (!definesFunction(mainRoot, "_start") && !definesFunction(mainRoot, "main")) {
			printError("--freestanding requires '_start' or 'main' as entry function");
			return false;
		}
		return true;
	}
	if (!definesFunction(mainRoot, "main")) {
		printError("no 'main' function found in main module");
		printNote("a Quadrate program must have a 'main' function as the entry point");
		return false;
	}
	return true;
}

// Whether `-r` can run the program in memory. The JIT resolves only the runtime, so FFI imports
// and stdlib modules with native code send it to disk, as do arguments and cross-compiling.
static bool canUseJIT(const Options& opts, const std::vector<ParsedModule>& parsedModules) {
	if (!opts.run || opts.noJIT || !opts.targetTriple.empty() || !opts.runArgs.empty() || opts.testMode) {
		return false;
	}
	for (const auto& mod : parsedModules) {
		if (mod.hasFFIImports) {
			if (opts.verbose) {
				std::cout << "note: using disk compilation (FFI imports detected)" << std::endl;
			}
			return false;
		}
		if (mod.hasNativeStdlibModules) {
			if (opts.verbose) {
				std::cout << "note: using disk compilation (stdlib modules with native code detected)" << std::endl;
			}
			return false;
		}
	}
	return true;
}

// Compiles what the options name to `outputPath`, and runs it with `-r`. The exit code.
static int compile(const Options& opts, const std::string& outputDir, const std::string& outputPath, Timer& timer) {
	std::vector<ParsedModule> parsedModules;
	if (opts.readStdin) {
		std::string source;
		std::string line;
		while (std::getline(std::cin, line)) {
			source += line + "\n";
		}
		if (!parseMainSource("<stdin>", source, true, opts, timer, parsedModules)) {
			return 1;
		}
	}
	for (const auto& file : opts.files) {
		errno = 0;
		auto source = readFileContents(file);
		if (!source) {
			// A tool-level failure, not a diagnostic: the `tool: path: message` shape quadfmt,
			// quadlint, quaduses and quaddoc use, not the `quadc: error:` one
			printToolError(file + ": " + std::strerror(errno != 0 ? errno : EIO));
			return 1;
		}
		timer.step("setup");
		if (!parseMainSource(file, *source, false, opts, timer, parsedModules)) {
			return 1;
		}
	}
	if (!loadImportedModules(opts, timer, parsedModules)) {
		return 1;
	}

	// Skipped for stdin, test mode, freestanding, and whenever something is being dumped
	const bool useCache = !opts.readStdin && !opts.testMode && !opts.freestanding && !opts.dumpAst && !opts.dumpIR &&
						  !opts.dumpTokens && !opts.saveTemps && !opts.verbose;
	BuildCache buildCache;
	if (useCache) {
		addCacheInputs(buildCache, opts, parsedModules);
		if (buildCache.restore(outputPath)) {
			timer.total("total (cached)");
			return opts.run ? runExecutable(outputPath, opts.runArgs) : 0;
		}
	}

	Qd::LlvmGenerator generator;
	if (!configureGenerator(generator, opts, parsedModules)) {
		return 1;
	}
	// The first main source is the program; the others joined it as its directory's files
	auto mainIt = std::find_if(parsedModules.begin(), parsedModules.end(),
			[](const ParsedModule& module) { return module.package == "main"; });
	if (mainIt == parsedModules.end()) {
		printError("no main module found");
		return 1;
	}
	Qd::IAstNode* mainRoot = mainIt->root;
	const std::string mainSourceFile = mainIt->name;
	if (!checkEntryPoint(opts, parsedModules, mainRoot)) {
		return 1;
	}
	timer.step("moduleResolution");

	// The source file's path is what the debug info names
	if (!generator.generate(mainRoot, mainSourceFile)) {
		printError("LLVM generation failed");
		return 1;
	}
	timer.step("irGeneration");

	if (opts.dumpIR || opts.verbose) {
		std::cout << "=== Generated LLVM IR ===" << std::endl;
		std::cout << generator.getIRString() << std::endl;
	}
	if (opts.saveTemps) {
		std::string irFile = (std::filesystem::path(outputDir) / (opts.outputName + ".ll")).string();
		if (!generator.writeIR(irFile)) {
			printError("failed to write IR file");
			return 1;
		}
		if (opts.verbose) {
			std::cout << "Written IR to " << irFile << std::endl;
		}
	}

	// In memory, with no linker and no disk I/O, where it can
	if (canUseJIT(opts, parsedModules)) {
		if (opts.verbose) {
			std::cout << "\n=== JIT Execution ===" << std::endl;
		}
		if (!opts.saveTemps) {
			std::error_code ec;
			std::filesystem::remove_all(outputDir, ec);
		}
		const int exitCode = generator.runJIT();
		timer.step("jit");
		return exitCode;
	}

	// Freestanding: the object file alone. The user links it against libqdrt-freestanding.a
	// and their own boot code.
	if (opts.freestanding) {
		std::string objPath = outputPath;
		if (objPath.size() < 2 || objPath.substr(objPath.size() - 2) != ".o") {
			objPath += ".o";
		}
		if (!generator.writeObject(objPath)) {
			printError("failed to write object file");
			return 1;
		}
		if (opts.verbose) {
			std::cout << "Written object to " << objPath << std::endl;
		}
		return 0;
	}

	if (!generator.writeExecutable(outputPath)) {
		printError("failed to create executable");
		return 1;
	}
	if (useCache) {
		buildCache.store(outputPath);
	}
	if (opts.verbose) {
		std::cout << "Written executable to " << outputPath << std::endl;
	}
	if (opts.run) {
		if (opts.verbose) {
			std::cout << "\n=== Running " << outputPath << " ===" << std::endl;
		}
		return runExecutable(outputPath, opts.runArgs);
	}
	return 0;
}

int main(int argc, char** argv) {
	Timer timer;

	Options opts;

	// No arguments is a usage error, not success -- `quadc` in a script should not
	// look like a successful compile. parseArgs reports it the way every other
	// tool does (a terse line on stderr, exit 1) rather than dumping help to
	// stdout, and reaching it also lets a bare `echo 'code' | quadc` find the
	// stdin path, which an argc==1 shortcut here used to cut off.
	if (!parseArgs(argc, argv, opts)) {
		return 1;
	}

	if (opts.help) {
		printHelp();
		return 0;
	}

	if (opts.version) {
		printVersion();
		return 0;
	}

	// Set global module version pins from command-line options
	setModuleVersionPins(opts.moduleVersions);

	// Set module include paths from command-line options
	setModuleIncludePaths(opts.includePaths);

	// Load dependencies from qd.json
	// First check source file's directory, then fall back to cwd
	std::vector<std::string> manifestPaths;
	if (!opts.files.empty()) {
		std::filesystem::path sourceFile = opts.files[0];
		std::string sourceDir = sourceFile.parent_path().string();
		if (sourceDir.empty()) {
			sourceDir = ".";
		}
		sourceDir = std::filesystem::absolute(sourceDir).string();
		manifestPaths = qdcli::manifestIncludePaths(sourceDir);
	}
	// Fall back to current working directory if no manifest found
	if (manifestPaths.empty()) {
		std::string cwd = std::filesystem::current_path().string();
		manifestPaths = qdcli::manifestIncludePaths(cwd);
	}
	for (const auto& path : manifestPaths) {
		opts.includePaths.push_back(path);
	}
	if (!manifestPaths.empty()) {
		setModuleIncludePaths(opts.includePaths);
	}

	// Colours already default to off when NO_COLOR is set or output is redirected
	// (see lib/qc/src/colors.cc), so only ever force them *off* here -- forcing
	// them on would put escape sequences back into pipes.
	if (qdcli::noColor() || opts.noColor) {
		Qd::Colors::setEnabled(false);
	}

	// Without -o, name the binary after the source file, as gcc/go/rustc do.
	// It used to be a hardcoded "main" -- the entry function's name -- so
	// `quadc server.qd` produced `main`, the artefact said nothing about where it
	// came from, and compiling two sources in one directory silently clobbered
	// the first.
	if (!opts.outputNameSet && !opts.files.empty()) {
		std::filesystem::path stem = std::filesystem::path(opts.files[0]).stem();
		if (!stem.empty()) {
			opts.outputName = stem.string();
		}
	}

	const std::string outputDir = createTempDir(opts.saveTemps);
	TempDirGuard tempGuard(outputDir);

	// When running, place executable in temp directory's bin subdirectory
	std::string outputPath;
	if (opts.run) {
		std::filesystem::path binDir = std::filesystem::path(outputDir) / "bin";
		std::filesystem::create_directory(binDir);
		outputPath = (binDir / std::filesystem::path(opts.outputName).filename()).string();
	} else {
		outputPath = opts.outputName;
		std::string finalPath = outputPath;
		if (opts.freestanding && (finalPath.size() < 2 || finalPath.substr(finalPath.size() - 2) != ".o")) {
			finalPath += ".o";
		}
		if (!checkOutputPath(finalPath, opts.files)) {
			return 1;
		}
	}

	// Preserve temp files if requested
	if (opts.saveTemps) {
		tempGuard.release();
		std::cerr << "Temporary files saved in: " << outputDir << std::endl;
	}

	if (opts.files.empty() && !opts.readStdin) {
		return 0;
	}
	return compile(opts, outputDir, outputPath, timer);
}
