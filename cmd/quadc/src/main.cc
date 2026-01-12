#include "module_resolver.h"
#include "options.h"
#include "parsed_module.h"
#include "temp_dir_guard.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_use.h>
#include <qc/ast_printer.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

int main(int argc, char** argv) {
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
	const bool noColors = std::getenv("NO_COLOR") != nullptr;
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
					std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset();
					std::cerr << Qd::Colors::bold() << "<stdin>:" << error.line << ":" << error.column << ":"
							  << Qd::Colors::reset() << " ";
					std::cerr << Qd::Colors::bold() << Qd::Colors::red() << "error:" << Qd::Colors::reset() << " ";
					std::cerr << Qd::Colors::bold() << error.message << Qd::Colors::reset() << std::endl;
				}
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset()
						  << "parsing failed for <stdin> with " << ast->errorCount() << " errors" << std::endl;
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

			// Collect imported modules
			std::function<void(Qd::IAstNode*)> collectImports = [&](Qd::IAstNode* node) {
				if (!node) {
					return;
				}
				// Check for USE statement nodes (type == USE_STATEMENT)
				if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
					auto* useNode = static_cast<Qd::AstNodeUse*>(node);
					module.importedModules.push_back(useNode->module());
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					collectImports(node->child(i));
				}
			};
			collectImports(root);

			parsedModules.push_back(std::move(module));
		}

		// Parse all main source files
		for (const auto& file : opts.files) {
			std::ifstream qdFile(file);
			if (!qdFile.is_open()) {
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << file
						  << ": No such file or directory" << std::endl;
				continue;
			}
			qdFile.seekg(0, std::ios::end);
			auto pos = qdFile.tellg();
			qdFile.seekg(0);
			if (pos < 0) {
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "reading " << file << std::endl;
				continue;
			}
			size_t size = static_cast<size_t>(pos);
			std::string buffer(size, ' ');
			qdFile.read(&buffer[0], static_cast<std::streamsize>(size));

			// Parse the source
			auto ast = std::make_unique<Qd::Ast>();
			auto root = ast->generate(buffer.c_str(), opts.dumpTokens, file.c_str());
			if (!root || ast->hasErrors()) {
				// Print stored parsing errors
				for (const auto& error : ast->getErrors()) {
					std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset();
					std::cerr << Qd::Colors::bold() << file << ":" << error.line << ":" << error.column << ":"
							  << Qd::Colors::reset() << " ";
					std::cerr << Qd::Colors::bold() << Qd::Colors::red() << "error:" << Qd::Colors::reset() << " ";
					std::cerr << Qd::Colors::bold() << error.message << Qd::Colors::reset() << std::endl;
				}
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << "parsing failed for " << file
						  << " with " << ast->errorCount() << " errors" << std::endl;
				return 1;
			}

			// Print AST if requested
			if (opts.dumpAst) {
				std::cout << "=== AST for " << file << " ===" << std::endl;
				Qd::AstPrinter::print(root);
				std::cout << std::endl;
			}

			// Semantic validation - catch errors before LLVM generation
			Qd::SemanticValidator validator;
			validator.setIncludePaths(opts.includePaths);
			validator.setSource(buffer.c_str());
			size_t errorCount = validator.validate(root, file.c_str(), false, opts.werror);
			if (errorCount > 0) {
				// Validation failed - do not proceed
				return 1;
			}

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

			// Collect imported modules
			std::function<void(Qd::IAstNode*)> collectImports = [&](Qd::IAstNode* node) {
				if (!node) {
					return;
				}
				// Check for USE statement nodes (type == USE_STATEMENT)
				if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
					auto* useNode = static_cast<Qd::AstNodeUse*>(node);
					module.importedModules.push_back(useNode->module());
				}
				for (size_t i = 0; i < node->childCount(); i++) {
					collectImports(node->child(i));
				}
			};
			collectImports(root);

			parsedModules.push_back(std::move(module));
		}

		// Collect all imported modules from main sources
		// Track which package each import belongs to (for .qd file imports)
		std::unordered_set<std::string> allModules;
		std::unordered_set<std::string> processedModules;
		std::unordered_map<std::string, std::string> moduleToPackage;	// moduleName -> packageName
		std::unordered_map<std::string, std::string> moduleToSourceDir; // moduleName -> sourceDirectory
		std::string sourceDirectory;
		for (const auto& module : parsedModules) {
			for (const auto& importedModule : module.importedModules) {
				allModules.insert(importedModule);

				// Check if this is a .qd file import (direct file import)
				bool isDirectFile =
						importedModule.size() >= 3 && importedModule.substr(importedModule.size() - 3) == ".qd";
				if (isDirectFile) {
					// Direct file imports: determine if this is an intra-module import or a top-level import
					// Intra-module: parent is a module directory, file inherits parent's namespace
					// Top-level: parent is standalone .qd, file gets its own namespace from filename

					// Check if parent is a module directory (doesn't end in .qd)
					bool parentIsModuleDirectory =
							!(module.name.size() >= 3 && module.name.substr(module.name.size() - 3) == ".qd");

					if (parentIsModuleDirectory) {
						// Intra-module import: use parent's package
						// e.g., split_module module imports helper.qd → helper functions are split_module::*
						moduleToPackage[importedModule] = module.package;
					} else {
						// Top-level import: derive package from imported filename
						// e.g., main.qd imports calculator.qd → calculator functions are calculator::*
						moduleToPackage[importedModule] = getPackageFromModuleName(importedModule);
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

				// Parse the module
				auto ast = std::make_unique<Qd::Ast>();
				auto root = ast->generate(buffer.c_str(), false, moduleFilePath.c_str());
				if (!root || ast->hasErrors()) {
					std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
							  << Qd::Colors::red() << "error: " << Qd::Colors::reset()
							  << "failed to parse module: " << moduleName << std::endl;
					// Print stored parse errors
					for (const auto& error : ast->getErrors()) {
						std::cerr << Qd::Colors::bold() << moduleFilePath << ":" << error.line << ":" << error.column
								  << ": " << Qd::Colors::reset() << Qd::Colors::bold() << Qd::Colors::red()
								  << "error: " << Qd::Colors::reset() << error.message << std::endl;
					}
					return 1;
				}

				// Semantic validation - catch errors before LLVM generation
				// Pass true for isModuleFile to skip reporting errors for missing nested module imports
				Qd::SemanticValidator validator;
				validator.setIncludePaths(opts.includePaths);
				validator.setSource(buffer.c_str());
				size_t errorCount = validator.validate(root, moduleFilePath.c_str(), true, opts.werror);
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
				std::string packageDir;
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
							packageDir = normalizedPackagesDir + "/" + packageDirName;
						}
					}
				}

				// Also check for native modules from include paths (e.g., -I /path/to/modules)
				// If the module's source directory has a lib/ subdirectory, treat it as a package
				if (packageDir.empty()) {
					std::string libDir = moduleFileSourceDir + "/lib";
					if (std::filesystem::exists(libDir) && std::filesystem::is_directory(libDir)) {
						packageDir = moduleFileSourceDir;
					}
				}

				ParsedModule parsedMod;
				parsedMod.name = moduleFilePath; // Store full file path for debug info
				parsedMod.package = packageName;
				parsedMod.sourceDirectory = moduleFileSourceDir;
				parsedMod.packageDirectory = packageDir;
				parsedMod.root = root;
				parsedMod.ast = std::move(ast);

				// Collect imports from this module
				std::function<void(Qd::IAstNode*)> collectImports = [&](Qd::IAstNode* node) {
					if (!node) {
						return;
					}
					if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
						auto* useNode = static_cast<Qd::AstNodeUse*>(node);
						parsedMod.importedModules.push_back(useNode->module());
					}
					for (size_t i = 0; i < node->childCount(); i++) {
						collectImports(node->child(i));
					}
				};
				collectImports(root);

				// Add any modules imported by this module to the set
				for (const auto& transitiveModule : parsedMod.importedModules) {
					if (!processedModules.count(transitiveModule)) {
						allModules.insert(transitiveModule);

						// Determine package for transitive imports
						bool isDirectFile = transitiveModule.size() >= 3 &&
											transitiveModule.substr(transitiveModule.size() - 3) == ".qd";
						if (isDirectFile) {
							// Check if importing file is a module directory (doesn't end in .qd)
							bool importerIsModuleDirectory =
									!(moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd");

							if (importerIsModuleDirectory) {
								// Intra-module import: use importer's package
								moduleToPackage[transitiveModule] = packageName;
							} else {
								// Top-level import: derive package from filename
								moduleToPackage[transitiveModule] = getPackageFromModuleName(transitiveModule);
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
				generator.addModuleAST(it->package, it->root, it->name);
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
			std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
					  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "no main module found" << std::endl;
			return 1;
		}

		// Check if main function exists in main module (unless in test mode)
		if (!opts.testMode) {
			bool hasMainFunction = false;
			for (size_t i = 0; i < mainRoot->childCount(); i++) {
				Qd::IAstNode* child = mainRoot->child(i);
				if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
					auto* funcDecl = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
					if (funcDecl->name() == "main") {
						hasMainFunction = true;
						break;
					}
				}
			}

			if (!hasMainFunction) {
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset()
						  << "no 'main' function found in main module" << std::endl;
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::cyan() << "note: " << Qd::Colors::reset()
						  << "a Quadrate program must have a 'main' function as the entry point" << std::endl;
				return 1;
			}
		} else {
			// In test mode, check if there are any tests
			bool hasTests = false;
			for (size_t i = 0; i < mainRoot->childCount(); i++) {
				Qd::IAstNode* child = mainRoot->child(i);
				if (child && child->type() == Qd::IAstNode::Type::TEST_DECLARATION) {
					hasTests = true;
					break;
				}
			}

			if (!hasTests) {
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset()
						  << "no test blocks found in --test mode" << std::endl;
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::cyan() << "note: " << Qd::Colors::reset()
						  << "use 'test \"name\" { ... }' to define tests" << std::endl;
				return 1;
			}
		}

		// Pass the actual source file path for debug info
		if (!generator.generate(mainRoot, mainSourceFile)) {
			std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
					  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "LLVM generation failed" << std::endl;
			return 1;
		}

		// Print IR to stdout if requested
		if (opts.dumpIR || opts.verbose) {
			std::cout << "=== Generated LLVM IR ===" << std::endl;
			std::cout << generator.getIRString() << std::endl;
		}

		// Write IR to file if save-temps is enabled
		if (opts.saveTemps) {
			std::string irFile = (std::filesystem::path(outputDir) / (opts.outputName + ".ll")).string();
			if (!generator.writeIR(irFile)) {
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "failed to write IR file"
						  << std::endl;
				return 1;
			}
			if (opts.verbose) {
				std::cout << "Written IR to " << irFile << std::endl;
			}
		}

		// Write executable
		if (!generator.writeExecutable(outputPath)) {
			std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
					  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "failed to create executable"
					  << std::endl;
			return 1;
		}

		if (opts.verbose) {
			std::cout << "Written executable to " << outputPath << std::endl;
		}

		// Run the program if requested
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
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "failed to execute program"
						  << std::endl;
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
				std::cerr << Qd::Colors::bold() << "quadc: " << Qd::Colors::reset() << Qd::Colors::bold()
						  << Qd::Colors::red() << "error: " << Qd::Colors::reset() << "program exited with code "
						  << exitCode << std::endl;
			}
			return exitCode;
		}
	}

	return 0;
}
