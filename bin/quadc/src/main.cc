#include "cxxopts.hpp"
#include <algorithm>
#include <cerrno>
#include <cgen/compiler.h>
#include <cgen/linker.h>
#include <cgen/process.h>
#include <cgen/source_file.h>
#include <cgen/transpiler.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <qc/colors.h>
#include <random>
#include <set>
#include <sstream>
#include <unordered_set>

#define QUADC_VERSION "0.1.0"

std::string getCompilerFlags() {
	// Optimization flags for tiny executables
	std::string opts = "-Os -flto -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-ident";

	if (std::filesystem::exists("./dist/include")) {
		return opts + " -I./dist/include";
	}
	const char* home = getenv("HOME");
	if (home) {
		std::filesystem::path localInclude = std::filesystem::path(home) / ".local" / "include";
		if (std::filesystem::exists(localInclude)) {
			return opts + " -I" + localInclude.string();
		}
	}
	return opts;
}

std::string getLinkerFlags() {
	// Optimization flags for tiny executables
	std::string opts = "-flto -Wl,--gc-sections -s";

	if (std::filesystem::exists("./dist/lib")) {
		return opts + " -L./dist/lib -Wl,-rpath,./dist/lib -lquadrate -lm -pthread";
	}
	const char* home = getenv("HOME");
	if (home) {
		std::filesystem::path localLib = std::filesystem::path(home) / ".local" / "lib";
		if (std::filesystem::exists(localLib)) {
			return opts + " -L" + localLib.string() + " -Wl,-rpath," + localLib.string() + " -lquadrate -lm -pthread";
		}
	}
	return opts + " -lquadrate -lm -pthread";
}

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

// RAII guard for automatic temp directory cleanup
class TempDirGuard {
public:
	explicit TempDirGuard(const std::string& path) : mPath(path), mShouldDelete(true) {
	}

	~TempDirGuard() {
		if (mShouldDelete && !mPath.empty()) {
			std::filesystem::remove_all(mPath);
		}
	}

	void release() {
		mShouldDelete = false;
	}

	// Prevent copying
	TempDirGuard(const TempDirGuard&) = delete;
	TempDirGuard& operator=(const TempDirGuard&) = delete;

private:
	std::string mPath;
	bool mShouldDelete;
};

// Find a module file, searching in multiple locations
// Returns the full path to the module file, or empty string if not found
std::string findModuleFile(const std::string& moduleName, const std::string& sourceDir) {
	// Check if this is a direct file import (ends with .qd)
	bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

	if (isDirectFile) {
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName;
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName;
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 3: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName;
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}
	} else {
		// Module directory import (original behavior)
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName + "/module.qd";
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 3: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}
	}

	return ""; // Not found
}

int main(int argc, char** argv) {
	cxxopts::Options options("quadc", "Quadrate compiler");
	options.add_options()("h,help", "Display help.")("v,version", "Display compiler version.")(
			"o", "Output filename", cxxopts::value<std::string>()->default_value("main"))("save-temps",
			"Save temporary files", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("no-colors",
			"Disable colored output", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("verbose",
			"Print compilation commands",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("dump-tokens", "Print tokens",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("r,run", "Run the compiled program",
			cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"shared", "Build a shared library", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"static", "Build a static library", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
			"files", "Input files", cxxopts::value<std::vector<std::string>>());

	options.parse_positional({"files"});
	auto result = options.parse(argc, argv);

	if (result.count("help") || argc == 1) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	if (result.count("version")) {
		std::cout << QUADC_VERSION << std::endl;
		return 0;
	}

	// Configure colored output
	const bool noColors = result["no-colors"].as<bool>();
	Qd::Colors::setEnabled(!noColors);

	std::string outputFilename = result["o"].as<std::string>();
	const bool shared = result["shared"].as<bool>();
	const bool statik = result["static"].as<bool>();
	const bool run = result["run"].as<bool>();

	if (shared && statik) {
		std::cerr << "quadc: cannot build both shared and static library at the same time." << std::endl;
		return 1;
	}
	if (run && (shared || statik)) {
		std::cerr << "quadc: cannot run a library" << std::endl;
		return 1;
	}
	if (shared) {
		outputFilename = "lib" + outputFilename + ".so";
	} else if (statik) {
		outputFilename = "lib" + outputFilename + ".a";
	}

	// Check if we should preserve temp files (needed for temp dir location)
	const bool saveTemps = result["save-temps"].as<bool>();

	const std::string outputDir = createTempDir(saveTemps);
	TempDirGuard tempGuard(outputDir);

	// When running, place executable in temp directory's bin subdirectory
	std::string outputPath;
	if (run) {
		std::filesystem::path binDir = std::filesystem::path(outputDir) / "bin";
		std::filesystem::create_directory(binDir);
		outputPath = (binDir / outputFilename).string();
	} else {
		outputPath = outputFilename;
	}

	// Preserve temp files if requested
	if (saveTemps) {
		tempGuard.release();
		std::cout << "Temporary files saved in: " << outputDir << std::endl;
	}

	// Check if verbose output is enabled
	const bool verbose = result["verbose"].as<bool>();

	// Check if token dumping is enabled
	const bool dumpTokens = result["dump-tokens"].as<bool>();

	if (result.count("files")) {
		auto files = result["files"].as<std::vector<std::string>>();
		Qd::Transpiler transpiler;
		std::vector<Qd::SourceFile> transpiledSources;
		for (const auto& file : files) {
			std::ifstream qdFile(file);
			if (!qdFile.is_open()) {
				std::cerr << "quadc: cannot find " << file << ": No such file or directory" << std::endl;
				continue;
			}
			qdFile.seekg(0, std::ios::end);
			auto pos = qdFile.tellg();
			qdFile.seekg(0);
			if (pos < 0) {
				std::cerr << "quadc: error reading " << file << std::endl;
				continue;
			}
			size_t size = static_cast<size_t>(pos);
			std::string buffer(size, ' ');
			qdFile.read(&buffer[0], static_cast<std::streamsize>(size));
			// Pass the full path for semantic validation (needed for local module resolution)
			if (auto ts = transpiler.emit(file.c_str(), "main", buffer.c_str(), verbose, dumpTokens)) {
				transpiledSources.push_back(*ts);
			} else {
				return 1;
			}
		}

		// Collect all imported modules from main sources
		// Track which package each import belongs to (for .qd file imports)
		std::unordered_set<std::string> allModules;
		std::unordered_set<std::string> processedModules;
		std::unordered_map<std::string, std::string> moduleToPackage;	// moduleName -> packageName
		std::unordered_map<std::string, std::string> moduleToSourceDir; // moduleName -> sourceDirectory
		std::string sourceDirectory;
		for (const auto& source : transpiledSources) {
			for (const auto& module : source.importedModules) {
				allModules.insert(module);

				// Check if this is a .qd file import
				bool isDirectFile = module.size() >= 3 && module.substr(module.size() - 3) == ".qd";
				if (isDirectFile) {
					// .qd file imports use the parent package name
					moduleToPackage[module] = source.package;
					moduleToSourceDir[module] = source.sourceDirectory;
				} else {
					// Regular module imports get their own package
					moduleToPackage[module] = module;
					moduleToSourceDir[module] = source.sourceDirectory;
				}
			}
			// Use source directory from first source (they should all be the same for now)
			if (sourceDirectory.empty()) {
				sourceDirectory = source.sourceDirectory;
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
			std::string packageName = moduleToPackage.count(moduleName) ? moduleToPackage[moduleName] : moduleName;
			std::string moduleSourceDir =
					moduleToSourceDir.count(moduleName) ? moduleToSourceDir[moduleName] : sourceDirectory;

			std::string moduleFilePath = findModuleFile(moduleName, moduleSourceDir);
			if (moduleFilePath.empty()) {
				// Module file not found - skip silently (already validated)
				continue;
			}

			// Read module file
			std::ifstream moduleFile(moduleFilePath);
			if (!moduleFile.is_open()) {
				continue;
			}
			moduleFile.seekg(0, std::ios::end);
			auto pos = moduleFile.tellg();
			moduleFile.seekg(0);
			if (pos < 0) {
				continue;
			}
			size_t size = static_cast<size_t>(pos);
			std::string buffer(size, ' ');
			moduleFile.read(&buffer[0], static_cast<std::streamsize>(size));

			// Transpile module with the determined package name
			if (auto ts = transpiler.emit(
						moduleFilePath.c_str(), packageName.c_str(), buffer.c_str(), verbose, false)) {
				// Add any modules imported by this module to the set
				for (const auto& transitiveModule : ts->importedModules) {
					if (!processedModules.count(transitiveModule)) {
						allModules.insert(transitiveModule);

						// Determine package for transitive imports
						bool isDirectFile = transitiveModule.size() >= 3 &&
											transitiveModule.substr(transitiveModule.size() - 3) == ".qd";
						if (isDirectFile) {
							// .qd file imports use the same package as the module that imported them
							moduleToPackage[transitiveModule] = packageName;
							moduleToSourceDir[transitiveModule] = ts->sourceDirectory;
						} else {
							// Regular module imports get their own package and search from original source dir
							moduleToPackage[transitiveModule] = transitiveModule;
							moduleToSourceDir[transitiveModule] = sourceDirectory;
						}
					}
				}
				transpiledSources.push_back(*ts);
			} else {
				std::cerr << "quadc: failed to transpile module: " << moduleName << std::endl;
				return 1;
			}
		}

		// Group sources by package for header generation
		std::unordered_map<std::string, std::vector<size_t>> packageSources;
		for (size_t i = 0; i < transpiledSources.size(); i++) {
			packageSources[transpiledSources[i].package].push_back(i);
		}

		for (auto& source : transpiledSources) {
			std::filesystem::path packageDir = std::filesystem::path(outputDir) / source.package;
			std::error_code ec;
			std::filesystem::create_directory(packageDir, ec);
			// Only fail if error is not "already exists"
			if (ec && ec.value() != EEXIST) {
				std::cerr << "quadc: failed to create directory " << packageDir << ": " << ec.message() << std::endl;
				return 1;
			}
			std::filesystem::path filePath = std::filesystem::path(outputDir) / source.filename;
			std::ofstream outFile(filePath);
			outFile << source.content;
			outFile.close();
		}

		// Generate header files for modules (one per package)
		for (const auto& [packageName, sourceIndices] : packageSources) {
			if (packageName == "main") {
				continue; // Skip main package
			}

			std::filesystem::path packageDir = std::filesystem::path(outputDir) / packageName;
			std::stringstream headerContent;
			std::string guardName = packageName;
			// Convert to uppercase for header guard
			std::transform(guardName.begin(), guardName.end(), guardName.begin(), ::toupper);

			headerContent << "#ifndef " << guardName << "_MODULE_H\n";
			headerContent << "#define " << guardName << "_MODULE_H\n\n";
			headerContent << "#include <runtime/runtime.h>\n\n";

			// Collect all function declarations and definitions from all sources in this package
			std::set<std::string> functionSignatures; // Regular function declarations
			std::set<std::string> externDeclarations; // Extern declarations (for imported functions)
			std::set<std::string> inlineFunctions;	  // Static inline functions with full body
			for (size_t idx : sourceIndices) {
				const auto& source = transpiledSources[idx];
				std::stringstream ss(source.content);
				std::string line;
				std::string currentFunction;
				bool inStaticInline = false;
				int braceCount = 0;

				while (std::getline(ss, line)) {
					// Capture extern declarations for imported functions
					if (line.find("extern qd_exec_result qd_") != std::string::npos) {
						std::string externDecl = line;
						externDecl.erase(0, externDecl.find_first_not_of(" \t"));
						externDeclarations.insert(externDecl);
					}
					// Check if this is the start of a static inline function
					else if (line.find("static inline qd_exec_result usr_" + packageName + "_") != std::string::npos) {
						inStaticInline = true;
						currentFunction = line + "\n";
						// Count braces in this line
						for (char c : line) {
							if (c == '{') {
								braceCount++;
							}
							if (c == '}') {
								braceCount--;
							}
						}
						if (braceCount == 0) {
							// Single-line function
							inlineFunctions.insert(currentFunction);
							inStaticInline = false;
							currentFunction.clear();
						}
					} else if (inStaticInline) {
						currentFunction += line + "\n";
						// Count braces
						for (char c : line) {
							if (c == '{') {
								braceCount++;
							}
							if (c == '}') {
								braceCount--;
							}
						}
						if (braceCount == 0) {
							// End of function
							inlineFunctions.insert(currentFunction);
							inStaticInline = false;
							currentFunction.clear();
						}
					} else if (line.find("qd_exec_result usr_" + packageName + "_") != std::string::npos) {
						// Regular function declaration
						size_t parenPos = line.find("(qd_context* ctx)");
						if (parenPos != std::string::npos) {
							std::string funcSig = line.substr(0, parenPos + 17); // +17 for "(qd_context* ctx)"
							// Remove leading whitespace
							funcSig.erase(0, funcSig.find_first_not_of(" \t"));
							functionSignatures.insert(funcSig);
						}
					}
				}
			}

			// Write extern declarations first
			for (const auto& externDecl : externDeclarations) {
				headerContent << externDecl << "\n";
			}
			if (!externDeclarations.empty()) {
				headerContent << "\n";
			}

			// Write all inline function definitions
			for (const auto& inlineFunc : inlineFunctions) {
				headerContent << inlineFunc << "\n";
			}

			// Write all regular function declarations
			for (const auto& funcSig : functionSignatures) {
				headerContent << funcSig << ";\n";
			}

			headerContent << "\n#endif // " << guardName << "_MODULE_H\n";

			// Write header file
			std::filesystem::path headerPath = packageDir / "module.h";
			std::ofstream headerFile(headerPath);
			headerFile << headerContent.str();
			headerFile.close();
		}

		// Generate main.c before compiling any sources
		if (!shared && !statik) {
			const char* mainFile = "// This file is automatically generated by the Quadrate compiler.\n"
								   "// Do not edit manually.\n\n"
								   "#include <quadrate/qd.h>\n\n"
								   "extern qd_exec_result usr_main_main(qd_context* ctx);\n\n"
								   "int main(void) {\n"
								   "    qd_context* ctx = qd_create_context(1024);\n"
								   "    qd_exec_result res = usr_main_main(ctx);\n"
								   "    qd_free_context(ctx);\n"
								   "    return res.code;\n"
								   "}\n";
			std::filesystem::path mainFilePath = std::filesystem::path(outputDir) / "main.c";
			std::ofstream mainFileStream(mainFilePath);
			mainFileStream << mainFile;
			mainFileStream.close();
		}

		std::vector<TranslationUnit> translationUnits;
		Qd::Compiler compiler;
		// Add temp directory to include path so gcc can find module headers
		std::string compilerFlags = getCompilerFlags() + " -I" + outputDir;
		for (auto& source : transpiledSources) {
			if (auto tu = compiler.compile(
						(std::filesystem::path(outputDir) / source.filename).c_str(), compilerFlags.c_str(), verbose)) {
				translationUnits.push_back(*tu);
			} else {
				std::cerr << "quadc: compilation failed for " << source.filename << std::endl;
				return 1;
			}
		}

		// Compile main.c only if it was generated (not building shared/static library)
		if (!shared && !statik) {
			std::filesystem::path mainCPath = std::filesystem::path(outputDir) / "main.c";
			if (auto tu = compiler.compile(mainCPath.string().c_str(), compilerFlags.c_str(), verbose)) {
				translationUnits.push_back(*tu);
			} else {
				std::cerr << "quadc: compilation failed for main.c" << std::endl;
				return 1;
			}
		}

		// Collect all imported libraries from all transpiled sources
		std::unordered_set<std::string> allImportedLibraries;
		for (const auto& source : transpiledSources) {
			for (const auto& lib : source.importedLibraries) {
				allImportedLibraries.insert(lib);
			}
		}

		// Convert library names to linker flags (libstdqd.so -> -lstdqd)
		std::string importedLibraryFlags;
		for (const auto& lib : allImportedLibraries) {
			// Extract library name from filename (e.g., "libstdqd.so" -> "stdqd")
			std::string libName = lib;
			if (libName.find("lib") == 0) {
				libName = libName.substr(3); // Remove "lib" prefix
			}
			// Remove .so or .a extension
			size_t dotPos = libName.find_last_of('.');
			if (dotPos != std::string::npos) {
				libName = libName.substr(0, dotPos);
			}
			importedLibraryFlags += " -l" + libName;
		}

		Qd::Linker linker;
		std::string linkerFlags = getLinkerFlags() + importedLibraryFlags;
		bool linkSuccess = linker.link(translationUnits, outputPath.c_str(), linkerFlags.c_str(), verbose);
		if (!linkSuccess) {
			std::cerr << "quadc: linking failed" << std::endl;
			return 1;
		}

		// Run the program if requested
		if (run) {
			// Execute the compiled binary safely
			std::vector<std::string> args; // No arguments to the program
			int exitCode = Qd::executeProcess(outputPath, args);
			if (exitCode != 0) {
				std::cerr << "quadc: program exited with code " << exitCode << std::endl;
				return exitCode;
			}
		}
	}

	return 0;
}
