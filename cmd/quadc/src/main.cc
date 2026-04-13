#include "ast_cache.h"
#include "build_cache.h"
#include "diagnostics.h"
#include "module_resolver.h"
#include "options.h"
#include "parsed_module.h"
#include "temp_dir_guard.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <quadrate/cli/cli.h>
#include <quadrate/llvmgen/generator.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_printer.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/semantic_validator.h>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Stdlib modules that have native code and require linking
// JIT execution doesn't support these - must fall back to disk compilation
static const std::unordered_set<std::string> nativeStdlibModules = {
		"fmt", "io", "math", "mem", "os", "signal", "strings", "strconv", "testing", "thread", "time", "tty"};

// Check if any of the imported modules are stdlib modules with native code
static bool hasNativeStdlibImports(const std::vector<std::string>& importedModules) {
	for (const auto& mod : importedModules) {
		if (nativeStdlibModules.count(mod) > 0) {
			return true;
		}
	}
	return false;
}

int main(int argc, char** argv) {
	// Timing helper - only active when QUADC_TIMING is set
	static bool timing = std::getenv("QUADC_TIMING") != nullptr;
	auto timeStart = std::chrono::steady_clock::now();
	auto timeLast = timeStart;
	auto printTiming = [&](const char* label) {
		if (timing) {
			auto now = std::chrono::steady_clock::now();
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeLast).count();
			std::cerr << "[TIMING] " << label << ": " << ms << "ms" << std::endl;
			timeLast = now;
		}
	};

	Options opts;

	// Show help if no arguments provided
	if (argc == 1) {
		printHelp();
		return 0;
	}

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
		manifestPaths = loadDependenciesFromManifest(sourceDir);
	}
	// Fall back to current working directory if no manifest found
	if (manifestPaths.empty()) {
		std::string cwd = std::filesystem::current_path().string();
		manifestPaths = loadDependenciesFromManifest(cwd);
	}
	for (const auto& path : manifestPaths) {
		opts.includePaths.push_back(path);
	}
	if (!manifestPaths.empty()) {
		setModuleIncludePaths(opts.includePaths);
	}

	// Configure colored output - check NO_COLOR environment variable
	const bool noColors = qdcli::noColor();
	Qd::Colors::setEnabled(!noColors);

	const std::string outputDir = createTempDir(opts.saveTemps);
	TempDirGuard tempGuard(outputDir);

	// When running, place executable in temp directory's bin subdirectory
	std::string outputPath;
	if (opts.run) {
		std::filesystem::path binDir = std::filesystem::path(outputDir) / "bin";
		std::filesystem::create_directory(binDir);
		outputPath = (binDir / opts.outputName).string();
	} else {
		outputPath = opts.outputName;
	}

	// Preserve temp files if requested
	if (opts.saveTemps) {
		tempGuard.release();
		std::cout << "Temporary files saved in: " << outputDir << std::endl;
	}

	if (!opts.files.empty() || opts.readStdin) {
		std::vector<ParsedModule> parsedModules;

		// Read from stdin if requested
		if (opts.readStdin) {
			std::string buffer;
			std::string line;
			while (std::getline(std::cin, line)) {
				buffer += line + "\n";
			}

			// Parse the source
			auto ast = std::make_unique<Qd::Ast>();
			auto root = ast->generate(buffer.c_str(), opts.dumpTokens, "<stdin>");
			if (!root || ast->hasErrors()) {
				// Print stored parsing errors
				for (const auto& error : ast->getErrors()) {
					printError("<stdin>", error.line, error.column, error.message);
				}
				printParseFailure("<stdin>", ast->errorCount());
				return 1;
			}

			// Print AST if requested
			if (opts.dumpAst) {
				std::cout << "=== AST for <stdin> ===" << std::endl;
				Qd::AstPrinter::print(root);
				std::cout << std::endl;
			}

			// Semantic validation - catch errors before LLVM generation
			Qd::SemanticValidator validator;
			validator.setIncludePaths(opts.includePaths);
			validator.setSource(buffer.c_str());
			size_t errorCount = validator.validate(root, "<stdin>", false, opts.werror);
			if (errorCount > 0) {
				// Validation failed - do not proceed
				return 1;
			}

			ParsedModule module;
			module.name = "<stdin>";
			module.package = "main";
			module.sourceDirectory = ".";
			module.root = root;
			module.ast = std::move(ast);
			module.importedModules = collectImportedModules(root);
			module.hasFFIImports = !validator.importedLibraries().empty();
			module.hasNativeStdlibModules = hasNativeStdlibImports(module.importedModules);

			parsedModules.push_back(std::move(module));
		}

		// Parse all main source files
		for (const auto& file : opts.files) {
			auto bufferOpt = readFileContents(file);
			if (!bufferOpt) {
				printError(file + ": No such file or directory");
				continue;
			}
			std::string buffer = std::move(*bufferOpt);

			printTiming("setup");

			// Parse the source
			auto ast = std::make_unique<Qd::Ast>();
			auto root = ast->generate(buffer.c_str(), opts.dumpTokens, file.c_str());
			printTiming("parse");
			if (!root || ast->hasErrors()) {
				// Print stored parsing errors
				for (const auto& error : ast->getErrors()) {
					printError(file, error.line, error.column, error.message);
				}
				printParseFailure(file, ast->errorCount());
				return 1;
			}

			// Print AST if requested
			if (opts.dumpAst) {
				std::cout << "=== AST for " << file << " ===" << std::endl;
				Qd::AstPrinter::print(root);
				std::cout << std::endl;
			}

			// Directory-based namespace: collect sibling files before validation
			std::vector<std::string> siblingFiles = getSiblingQdFiles(file);

			// Semantic validation - catch errors before LLVM generation
			Qd::SemanticValidator validator;
			validator.setIncludePaths(opts.includePaths);
			validator.setSource(buffer.c_str());
			validator.setSiblingFiles(siblingFiles);
			size_t errorCount = validator.validate(root, file.c_str(), false, opts.werror);
			printTiming("semantic");
			if (errorCount > 0) {
				// Validation failed - do not proceed
				return 1;
			}

			// Copy cached ASTs from validator to global cache for reuse in module loop
			AstCache::instance().importFromValidator(validator);

			// Get source directory for module resolution
			std::filesystem::path filePath(file);
			std::string sourceDirectory = filePath.parent_path().string();
			if (sourceDirectory.empty()) {
				sourceDirectory = ".";
			}

			ParsedModule module;
			module.name = file;
			module.package = "main";
			module.sourceDirectory = sourceDirectory;
			module.root = root;
			module.ast = std::move(ast);
			module.importedModules = collectImportedModules(root);
			module.hasFFIImports = !validator.importedLibraries().empty();
			module.hasNativeStdlibModules = hasNativeStdlibImports(module.importedModules);

			// Directory-based namespace: add sibling files to importedModules for code generator
			// (siblingFiles was already collected above for the semantic validator)
			for (const auto& sibling : siblingFiles) {
				// Add sibling as an "imported module" that will be merged into main
				// Use the full path as the module name (like use "file.qd" does)
				module.importedModules.push_back(sibling);
			}

			parsedModules.push_back(std::move(module));
		}

		// Collect all imported modules from main sources
		// Track which package each import belongs to (for .qd file imports)
		std::unordered_set<std::string> allModules;
		std::unordered_set<std::string> processedModules;
		std::unordered_map<std::string, std::string> moduleToPackage;	// moduleName -> packageName
		std::unordered_map<std::string, std::string> moduleToSourceDir; // moduleName -> sourceDirectory
		std::unordered_set<std::string> modulesToMergeIntoMain;			// modules that should merge into main
		std::string sourceDirectory;
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
			std::string packageName = moduleToPackage.count(moduleName) ? moduleToPackage[moduleName] : moduleName;
			std::string moduleSourceDir =
					moduleToSourceDir.count(moduleName) ? moduleToSourceDir[moduleName] : sourceDirectory;

			std::vector<std::string> moduleFilePaths = findModuleFiles(moduleName, moduleSourceDir);
			if (moduleFilePaths.empty()) {
				// Module files not found - skip silently (already validated)
				continue;
			}

			// Process all files for this module
			for (const auto& moduleFilePath : moduleFilePaths) {
				// Parse module file (uses cache if already parsed by semantic validator)
				Qd::Ast* ast = nullptr;
				std::string buffer;
				bool fromCache = false;

				auto parseStart = std::chrono::steady_clock::now();
				Qd::IAstNode* root = AstCache::instance().getOrParse(moduleFilePath, &ast, &buffer, &fromCache);
				auto parseEnd = std::chrono::steady_clock::now();
				if (timing) {
					auto parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(parseEnd - parseStart).count();
					std::cerr << "[TIMING] moduleLoop parse " << moduleName << ": " << parseMs << "ms" << std::endl;
				}
				if (!root) {
					printError("failed to parse module: " + moduleName);
					// Print stored parse errors if available
					if (ast) {
						for (const auto& error : ast->getErrors()) {
							printError(moduleFilePath, error.line, error.column, error.message);
						}
					}
					return 1;
				}

				// Semantic validation - catch errors before LLVM generation
				// Skip validation if AST came from cache (already validated during main file semantic analysis)
				// Only validate freshly parsed files to catch internal module errors
				size_t errorCount = 0;
				if (!fromCache) {
					auto valStart = std::chrono::steady_clock::now();
					Qd::SemanticValidator validator;
					validator.setIncludePaths(opts.includePaths);
					validator.setSource(buffer.c_str());
					errorCount = validator.validate(root, moduleFilePath.c_str(), true, opts.werror);
					auto valEnd = std::chrono::steady_clock::now();
					if (timing) {
						auto valMs = std::chrono::duration_cast<std::chrono::milliseconds>(valEnd - valStart).count();
						std::cerr << "[TIMING] moduleLoop validate " << moduleName << ": " << valMs << "ms"
								  << std::endl;
					}
				} else if (timing) {
					std::cerr << "[TIMING] moduleLoop validate " << moduleName << ": 0ms (cached)" << std::endl;
				}
				if (errorCount > 0) {
					// Validation failed - do not proceed
					return 1;
				}

				// Get module's source directory
				std::filesystem::path moduleFilePathObj(moduleFilePath);
				std::string moduleFileSourceDir = moduleFilePathObj.parent_path().string();
				if (moduleFileSourceDir.empty()) {
					moduleFileSourceDir = ".";
				}

				// Detect if this module is from a third-party package
				// Package paths look like: /path/to/packages/modulename@version/module.qd
				// Or via symlinks: /path/to/packages/_namespaces/name/module.qd
				std::string packageDir;

				// First check: If the module's source directory has a lib/ subdirectory, use it
				// This handles symlinks (like _namespaces/http) which resolve to the actual package
				{
					std::string libDir = moduleFileSourceDir + "/lib";
					if (std::filesystem::exists(libDir) && std::filesystem::is_directory(libDir)) {
						packageDir = moduleFileSourceDir;
					}
				}

				// Fallback: Extract package directory from path structure
				if (packageDir.empty()) {
					std::string packagesDir = getPackagesDir();
					if (!packagesDir.empty()) {
						std::string normalizedModulePath = std::filesystem::absolute(moduleFilePath).string();
						std::string normalizedPackagesDir = std::filesystem::absolute(packagesDir).string();

						// Check if module path starts with packages directory
						if (normalizedModulePath.size() > normalizedPackagesDir.size() &&
								normalizedModulePath.substr(0, normalizedPackagesDir.size()) == normalizedPackagesDir) {
							// Extract the package directory (e.g., /path/to/packages/color@master)
							std::string relativePath = normalizedModulePath.substr(normalizedPackagesDir.size());
							if (!relativePath.empty() && relativePath[0] == '/') {
								relativePath = relativePath.substr(1);
							}
							// Get the first path component (modulename@version)
							size_t slashPos = relativePath.find('/');
							if (slashPos != std::string::npos) {
								std::string packageDirName = relativePath.substr(0, slashPos);
								std::string candidateDir = normalizedPackagesDir + "/" + packageDirName;
								// Only use if it has a lib/ subdirectory
								if (std::filesystem::exists(candidateDir + "/lib")) {
									packageDir = candidateDir;
								}
							}
						}
					}
				}

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

		// Build cache: check if we can skip compilation entirely
		// Skip cache for stdin, test mode, JIT, and when dumping debug output
		bool useCache = !opts.readStdin && !opts.testMode && !opts.dumpAst && !opts.dumpIR && !opts.dumpTokens &&
						!opts.saveTemps && !opts.verbose;
		BuildCache buildCache;
		if (useCache) {
			// Hash all source files (main + all modules)
			for (const auto& mod : parsedModules) {
				buildCache.addSourceFile(mod.name);
			}
			// Hash compiler options that affect output
			buildCache.addOption("opt:" + std::to_string(opts.optLevel));
			buildCache.addOption("stack:" + std::to_string(opts.stackSize));
			buildCache.addOption("target:" + opts.targetTriple);
			buildCache.addOption("debug:" + std::to_string(opts.debugInfo ? 1 : 0));

			// Check cache before doing expensive codegen
			if (buildCache.restore(outputPath)) {
				if (timing) {
					auto now = std::chrono::steady_clock::now();
					auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeStart).count();
					std::cerr << "[TIMING] total (cached): " << totalMs << "ms" << std::endl;
				}
				// If running, execute the cached binary
				if (opts.run) {
					std::string cmd = outputPath;
					for (const auto& arg : opts.runArgs) {
						// Quote arguments that contain spaces or special characters
						bool needsQuote = arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos ||
										  arg.find('"') != std::string::npos || arg.find('\\') != std::string::npos ||
										  arg.find('$') != std::string::npos;
						if (needsQuote) {
							cmd += " \"";
							for (char c : arg) {
								if (c == '"' || c == '\\' || c == '$') {
									cmd += '\\';
								}
								cmd += c;
							}
							cmd += "\"";
						} else {
							cmd += " " + arg;
						}
					}
					int status = system(cmd.c_str());
					return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
				}
				return 0;
			}
		}

		// Now generate LLVM IR from all parsed modules
		Qd::LlvmGenerator generator;

		// Enable debug info if requested
		if (opts.debugInfo) {
			generator.setDebugInfo(true);
		}

		// Set optimization level
		generator.setOptimizationLevel(opts.optLevel);

		// Set stack size
		generator.setStackSize(opts.stackSize);

		// Enable test mode if requested
		if (opts.testMode) {
			generator.setTestMode(true);
			if (opts.coverage) {
				generator.setCoverageMode(true);
			}
		}

		// Set target triple for cross-compilation if specified
		if (!opts.targetTriple.empty()) {
			generator.setTargetTriple(opts.targetTriple);
		}

		// Add library search paths for third-party packages
		// Track which packages we've already added to avoid duplicates
		std::set<std::string> addedPackagePaths;
		for (const auto& module : parsedModules) {
			if (!module.packageDirectory.empty() &&
					addedPackagePaths.find(module.packageDirectory) == addedPackagePaths.end()) {
				// Add the package's lib directory to the linker search paths
				std::string libPath = module.packageDirectory + "/lib";
				if (std::filesystem::exists(libPath)) {
					generator.addLibrarySearchPath(libPath);
					addedPackagePaths.insert(module.packageDirectory);
				}
			}
		}

		// Add library search paths from -I include paths
		for (const auto& includePath : opts.includePaths) {
			std::string libPath = includePath + "/lib";
			if (std::filesystem::exists(libPath) && addedPackagePaths.find(includePath) == addedPackagePaths.end()) {
				generator.addLibrarySearchPath(libPath);
				addedPackagePaths.insert(includePath);
			}
		}

		// Add all dependency modules in REVERSE order (dependencies first)
		// Modules were loaded in breadth-first order (main first, then dependents, then their dependencies)
		// but we need to generate them depth-first (deep dependencies first, then their dependents)
		for (auto it = parsedModules.rbegin(); it != parsedModules.rend(); ++it) {
			if (it->package != "main") {
				generator.addModuleAST(it->package, it->root, it->name, it->mergeIntoMain);
			}
		}

		// Generate main module last
		Qd::IAstNode* mainRoot = nullptr;
		std::string mainSourceFile;
		for (auto& module : parsedModules) {
			if (module.package == "main") {
				mainRoot = module.root;
				mainSourceFile = module.name;
				break;
			}
		}

		if (!mainRoot) {
			printError("no main module found");
			return 1;
		}

		// Check if main function exists in main module (unless in test mode)
		if (!opts.testMode) {
			bool hasMainFunction = false;
			for (auto* child : mainRoot->children()) {
				if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
					auto* funcDecl = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
					if (funcDecl->name() == "main") {
						hasMainFunction = true;
						break;
					}
				}
			}

			if (!hasMainFunction) {
				printError("no 'main' function found in main module");
				printNote("a Quadrate program must have a 'main' function as the entry point");
				return 1;
			}
		} else {
			// In test mode, check if there are any tests
			bool hasTests = false;
			for (auto* child : mainRoot->children()) {
				if (child && child->type() == Qd::IAstNode::Type::TEST_DECLARATION) {
					hasTests = true;
					break;
				}
			}

			if (!hasTests) {
				printError("no test blocks found in --test mode");
				printNote("use 'test \"name\" { ... }' to define tests");
				return 1;
			}
		}

		printTiming("moduleResolution");

		// Pass the actual source file path for debug info
		if (!generator.generate(mainRoot, mainSourceFile)) {
			printError("LLVM generation failed");
			return 1;
		}
		printTiming("irGeneration");

		// Print IR to stdout if requested
		if (opts.dumpIR || opts.verbose) {
			std::cout << "=== Generated LLVM IR ===" << std::endl;
			std::cout << generator.getIRString() << std::endl;
		}

		// Write IR to file if save-temps is enabled
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

		// Use JIT for -r mode when not cross-compiling and --no-jit not specified
		// JIT is faster because it skips disk I/O and the external linker
		bool useJIT = opts.run && !opts.noJIT && opts.targetTriple.empty() && opts.runArgs.empty() && !opts.testMode;

		// Check if any module uses FFI imports or native stdlib modules - JIT doesn't support these
		if (useJIT) {
			for (const auto& mod : parsedModules) {
				if (mod.hasFFIImports) {
					useJIT = false;
					if (opts.verbose) {
						std::cout << "note: using disk compilation (FFI imports detected)" << std::endl;
					}
					break;
				}
				if (mod.hasNativeStdlibModules) {
					useJIT = false;
					if (opts.verbose) {
						std::cout << "note: using disk compilation (stdlib modules with native code detected)"
								  << std::endl;
					}
					break;
				}
			}
		}

		if (useJIT) {
			// JIT execution - compile and run in memory
			if (opts.verbose) {
				std::cout << "\n=== JIT Execution ===" << std::endl;
			}
			auto jitStart = std::chrono::steady_clock::now();
			int exitCode = generator.runJIT();
			if (timing) {
				auto jitEnd = std::chrono::steady_clock::now();
				auto jitMs = std::chrono::duration_cast<std::chrono::milliseconds>(jitEnd - jitStart).count();
				std::cerr << "[TIMING] jit: " << jitMs << "ms" << std::endl;
			}
			return exitCode;
		}

		// Traditional path: write executable to disk
		if (!generator.writeExecutable(outputPath)) {
			printError("failed to create executable");
			return 1;
		}

		// Store in build cache for future runs
		if (useCache) {
			buildCache.store(outputPath);
		}

		if (opts.verbose) {
			std::cout << "Written executable to " << outputPath << std::endl;
		}

		// Run the program if requested (traditional linking path)
		if (opts.run) {
			if (opts.verbose) {
				std::cout << "\n=== Running " << outputPath << " ===" << std::endl;
			}
			// Build command with arguments
			std::string cmd = outputPath;
			for (const auto& arg : opts.runArgs) {
				// Quote arguments that contain spaces or special characters
				bool needsQuote = arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos ||
								  arg.find('"') != std::string::npos || arg.find('\\') != std::string::npos ||
								  arg.find('$') != std::string::npos;
				if (needsQuote) {
					cmd += " \"";
					for (char c : arg) {
						if (c == '"' || c == '\\' || c == '$') {
							cmd += '\\';
						}
						cmd += c;
					}
					cmd += "\"";
				} else {
					cmd += " " + arg;
				}
			}
			// Execute using system() and get exit code
			int status = system(cmd.c_str());
			if (status == -1) {
				printError("failed to execute program");
				return 1;
			}
			// Check if process exited normally or was killed by signal
			int exitCode;
			if (WIFEXITED(status)) {
				exitCode = WEXITSTATUS(status);
			} else {
				// Process was terminated by a signal
				exitCode = -1;
			}
			if (exitCode != 0) {
				printError("program exited with code " + std::to_string(exitCode));
			}
			return exitCode;
		}
	}

	return 0;
}
