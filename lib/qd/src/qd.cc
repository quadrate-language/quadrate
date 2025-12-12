#include <qd/qd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <llvmgen/generator.h>
#include <memory>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_use.h>
#include <qc/semantic_validator.h>
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

// Helper to find module file in standard locations
static std::string findModuleFile(const std::string& moduleName) {
	// Try 1: QUADRATE_ROOT environment variable
	const char* quadrateRoot = getenv("QUADRATE_ROOT");
	if (quadrateRoot) {
		std::string rootPath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
		if (fs::exists(rootPath)) {
			return rootPath;
		}
	}

	// Try 2: QUADRATE_LIBDIR (for development/testing)
	const char* libDir = getenv("QUADRATE_LIBDIR");
	if (libDir) {
		// Standard library modules are in lib/qd<name>/qd/<name>/module.qd relative to QUADRATE_LIBDIR
		// But the installed structure is different - try both
		std::string devPath = std::string(libDir) + "/../lib/qd" + moduleName + "/qd/" + moduleName + "/module.qd";
		if (fs::exists(devPath)) {
			return devPath;
		}
		// Also try installed structure
		std::string installPath = std::string(libDir) + "/../share/quadrate/" + moduleName + "/module.qd";
		if (fs::exists(installPath)) {
			return installPath;
		}
	}

	// Try 3: Standard library relative to executable
	{
		char exePathBuf[4096];
		int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
		if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
			try {
				fs::path exePath = fs::canonical(exePathBuf);
				fs::path exeDir = exePath.parent_path();
				fs::path sharePath = exeDir / ".." / "share" / "quadrate" / moduleName / "module.qd";
				if (fs::exists(sharePath)) {
					return sharePath.string();
				}
			} catch (...) {
				// Ignore errors resolving path
			}
		}
	}

	// Try 4: $HOME/quadrate directory
	const char* home = getenv("HOME");
	if (home) {
		std::string homePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
		if (fs::exists(homePath)) {
			return homePath;
		}
	}

	// Try 5: Local development paths (relative to build directory)
	fs::path buildRoot = fs::path(MESON_BUILD_ROOT).parent_path().parent_path();
	std::string devPath = (buildRoot / "lib" / ("std" + moduleName + "qd") / "qd" / moduleName / "module.qd").string();
	if (fs::exists(devPath)) {
		return devPath;
	}

	return "";
}

// Helper to find the library directory (either build or installed location)
static std::string findLibraryDir() {
	// Try 1: QUADRATE_LIBDIR environment variable (for development/testing)
	const char* libDir = getenv("QUADRATE_LIBDIR");
	if (libDir && fs::exists(libDir)) {
		return libDir;
	}

	// Try 2: Build directory (MESON_BUILD_ROOT/lib)
	fs::path buildLibPath = fs::path(MESON_BUILD_ROOT) / "lib";
	if (fs::exists(buildLibPath / "qdrt" / "libqdrt_static.a") || fs::exists(buildLibPath / "qdrt" / "libqdrt.a")) {
		return buildLibPath.string();
	}

	// Try 3: System installed location (/usr/lib)
	if (fs::exists("/usr/lib/libqdrt.a")) {
		return "/usr/lib";
	}

	// Try 4: /usr/local/lib
	if (fs::exists("/usr/local/lib/libqdrt.a")) {
		return "/usr/local/lib";
	}

	// Fallback to build directory
	return buildLibPath.string();
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
	// In installed location: lib<name>.a
	fs::path flatLib = fs::path(libDir) / ("lib" + libName + ".a");
	if (fs::exists(flatLib)) {
		return flatLib.string();
	}
	return "";
}

// Module implementation
struct qd_module {
	std::string name;
	std::vector<std::string> scripts;
	std::unordered_map<std::string, void (*)()> native_functions;
	std::unordered_map<std::string, std::string> symbol_map; // function_name -> full_symbol_name
	dynlib_handle_t dl_handle;								 // dynamic library handle
	fs::path temp_dir;
	fs::path so_path;
	bool compiled;
	size_t warning_min_line; // Minimum line for warnings (0 = no suppression)

	qd_module(const std::string& n) : name(n), dl_handle(nullptr), compiled(false), warning_min_line(0) {
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

// Global module registry (stored per-context would be better, but API doesn't support it)
static std::unordered_map<qd_context*, std::unordered_map<std::string, qd_module*>> g_context_modules;

qd_module* qd_get_module(qd_context* ctx, const char* name) {
	if (!ctx || !name) {
		return nullptr;
	}

	// Get or create module registry for this context
	auto& modules = g_context_modules[ctx];

	// Check if module already exists
	auto it = modules.find(name);
	if (it != modules.end()) {
		return it->second;
	}

	// Create new module
	qd_module* mod = new qd_module(name);
	modules[name] = mod;
	return mod;
}

void qd_add_script(qd_module* mod, const char* script) {
	if (!mod || !script) {
		return;
	}
	mod->scripts.push_back(script);
}

void qd_register_function(qd_module* mod, const char* name, void (*fn)()) {
	if (!mod || !name || !fn) {
		return;
	}
	mod->native_functions[name] = fn;
}

void qd_set_warning_min_line(qd_module* mod, size_t line) {
	if (!mod) {
		return;
	}
	mod->warning_min_line = line;
}

void qd_build(qd_module* mod) {
	if (!mod) {
		return;
	}

	if (mod->scripts.empty()) {
		fprintf(stderr, "qd_build: No scripts to compile for module '%s'\n", mod->name.c_str());
		return;
	}

	try {
		// Create temporary directory for compilation
		fs::path temp_base = fs::temp_directory_path();
		std::string temp_template = (temp_base / "qd_embed_XXXXXX").string();
		std::vector<char> temp_buf(temp_template.begin(), temp_template.end());
		temp_buf.push_back('\0');
		char* temp_path = mkdtemp(temp_buf.data());
		if (!temp_path) {
			fprintf(stderr, "qd_build: Failed to create temporary directory\n");
			return;
		}
		mod->temp_dir = temp_path;

		// Combine all scripts into one source file
		// Prepend package declaration to ensure correct symbol names
		std::string combined_source = "package " + mod->name + "\n\n";
		for (const auto& script : mod->scripts) {
			combined_source += script;
			combined_source += "\n";
		}

		// Write source to file
		fs::path source_file = mod->temp_dir / "script.qd";
		FILE* f = fopen(source_file.c_str(), "w");
		if (!f) {
			fprintf(stderr, "qd_build: Failed to write source file\n");
			return;
		}
		fwrite(combined_source.c_str(), 1, combined_source.size(), f);
		fclose(f);

		// Parse the source
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(combined_source.c_str(), false, source_file.string().c_str());
		if (!root) {
			fprintf(stderr, "qd_build: Failed to parse script\n");
			return;
		}

		// Validate semantics (pass true for isModuleFile since this is dynamically loaded code)
		Qd::SemanticValidator validator;
		if (mod->warning_min_line > 0) {
			validator.setWarningMinLine(mod->warning_min_line);
		}
		size_t error_count = validator.validate(root, source_file.string().c_str(), true, false);
		if (error_count > 0) {
			fprintf(stderr, "qd_build: Semantic validation failed with %zu error(s)\n", error_count);
			return;
		}

		// Collect use statements and load module ASTs
		std::vector<std::pair<std::string, Qd::IAstNode*>> moduleASTs;
		std::vector<std::unique_ptr<Qd::Ast>> moduleAstOwners; // Keep AST objects alive
		std::unordered_set<std::string> processedModules;

		std::function<void(Qd::IAstNode*)> collectAndLoadModules = [&](Qd::IAstNode* node) {
			if (!node) {
				return;
			}
			if (node->type() == Qd::IAstNode::Type::USE_STATEMENT) {
				auto* useNode = static_cast<Qd::AstNodeUse*>(node);
				std::string moduleName = useNode->module();

				// Skip if already processed
				if (processedModules.count(moduleName)) {
					return;
				}
				processedModules.insert(moduleName);

				// Find and load the module
				std::string modulePath = findModuleFile(moduleName);
				if (modulePath.empty()) {
					// Module not found - semantic validator should have caught this
					return;
				}

				// Read module file
				std::ifstream moduleFile(modulePath);
				if (!moduleFile.is_open()) {
					return;
				}

				moduleFile.seekg(0, std::ios::end);
				auto pos = moduleFile.tellg();
				moduleFile.seekg(0);
				if (pos < 0) {
					return;
				}

				size_t size = static_cast<size_t>(pos);
				std::string buffer(size, ' ');
				moduleFile.read(&buffer[0], static_cast<std::streamsize>(size));

				// Parse the module
				auto moduleAst = std::make_unique<Qd::Ast>();
				auto moduleRoot = moduleAst->generate(buffer.c_str(), false, modulePath.c_str());
				if (moduleRoot && !moduleAst->hasErrors()) {
					// Validate module
					Qd::SemanticValidator modValidator;
					size_t modErrors = modValidator.validate(moduleRoot, modulePath.c_str(), true, false);
					if (modErrors == 0) {
						moduleASTs.push_back({moduleName, moduleRoot});
						moduleAstOwners.push_back(std::move(moduleAst));

						// Recursively process this module's imports
						collectAndLoadModules(moduleRoot);
					}
				}
			}
			for (size_t i = 0; i < node->childCount(); i++) {
				collectAndLoadModules(node->child(i));
			}
		};
		collectAndLoadModules(root);

		// Generate LLVM IR
		Qd::LlvmGenerator generator;
		generator.setOptimizationLevel(2);

		// Add all imported modules first (in reverse order for proper dependency ordering)
		for (auto it = moduleASTs.rbegin(); it != moduleASTs.rend(); ++it) {
			generator.addModuleAST(it->first, it->second);
		}

		if (!generator.generate(root, mod->name)) {
			fprintf(stderr, "qd_build: Failed to generate LLVM IR\n");
			return;
		}

		// Write object file
		fs::path obj_file = mod->temp_dir / "script.o";
		if (!generator.writeObject(obj_file.string())) {
			fprintf(stderr, "qd_build: Failed to write object file\n");
			return;
		}

		// Link to shared library
		mod->so_path = mod->temp_dir / ("lib" + mod->name + ".so");

		// Check what symbols are in the object file and store them for lookup
		std::string nm_cmd = "nm " + obj_file.string() + " | grep ' T usr_' || true";
		FILE* nm_output = popen(nm_cmd.c_str(), "r");
		std::unordered_map<std::string, std::string> symbol_map; // function_name -> full_symbol
		if (nm_output) {
			char buffer[256];
			while (fgets(buffer, sizeof(buffer), nm_output)) {
				// Parse symbol: "0000000000000000 T usr_package_function"
				char* symbol_start = strstr(buffer, "usr_");
				if (symbol_start) {
					std::string full_symbol = symbol_start;
					// Remove newline
					if (!full_symbol.empty() && full_symbol.back() == '\n') {
						full_symbol.pop_back();
					}
					// Extract function name after the second underscore
					// usr_package_function -> function
					size_t first_us = full_symbol.find('_');
					if (first_us != std::string::npos) {
						size_t second_us = full_symbol.find('_', first_us + 1);
						if (second_us != std::string::npos) {
							std::string func_name = full_symbol.substr(second_us + 1);
							symbol_map[func_name] = full_symbol;
						}
					}
				}
			}
			pclose(nm_output);
		}

		// Store symbol map in module for later lookup
		mod->symbol_map = symbol_map;

		// Find the library directory (build or installed location)
		std::string lib_dir = findLibraryDir();

		// Use clang/gcc to link the object file into a shared library with static libs
		std::string link_cmd = "clang++ -shared -fPIC ";
		link_cmd += obj_file.string();
		link_cmd += " -o ";
		link_cmd += mod->so_path.string();

		// Link with static libraries (whole-archive to include all symbols)
		std::vector<std::string> libs = {
				"qdrt", "qdmath", "qdfmt", "qdio", "qdnet", "qdos", "qdstr", "qdtime", "qdmem", "qdstrconv"};
		for (const auto& lib : libs) {
			std::string libPath = findStaticLib(lib_dir, lib);
			if (!libPath.empty()) {
				link_cmd += " -Wl,--whole-archive " + libPath + " -Wl,--no-whole-archive";
			}
		}

		link_cmd += " -lm"; // Math library for sin, cos, etc.
		link_cmd += " 2>&1";

		FILE* link_output = popen(link_cmd.c_str(), "r");
		if (!link_output) {
			fprintf(stderr, "qd_build: Failed to execute linker\n");
			return;
		}

		char buffer[256];
		std::string link_errors;
		while (fgets(buffer, sizeof(buffer), link_output)) {
			link_errors += buffer;
		}
		int link_result = pclose(link_output);

		if (link_result != 0) {
			fprintf(stderr, "qd_build: Linking failed:\n%s\n", link_errors.c_str());
			return;
		}

		// Load the shared library
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

		// Try parsing as integer (with hex 0x and binary 0b support)
		int base = 10;
		const char* numStart = token.c_str();
		if (token.size() > 2 && token[0] == '0') {
			if (token[1] == 'x' || token[1] == 'X') {
				base = 16;
				numStart += 2;
			} else if (token[1] == 'b' || token[1] == 'B') {
				base = 2;
				numStart += 2;
			}
		}
		long long int_val = strtoll(numStart, &endptr, base);
		if (*endptr == '\0' && numStart != endptr) {
			// It's an integer
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
		if (token == ".") {
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
			if (!mod->compiled || !mod->dl_handle) {
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

			// Function signature: qd_exec_result (*)(qd_context*)
			typedef qd_exec_result (*qd_func_t)(qd_context*);

			void* sym = dynlib_platform_symbol(mod->dl_handle, symbol_name.c_str());
			qd_func_t func = reinterpret_cast<qd_func_t>(sym);
			if (!func) {
				// Check native functions
				auto native_it = mod->native_functions.find(func_name);
				if (native_it != mod->native_functions.end()) {
					// Call native function
					// Note: This assumes native functions follow the same signature
					func = reinterpret_cast<qd_func_t>(native_it->second);
				} else {
					fprintf(stderr, "qd_execute: Function '%s' (symbol '%s') not found in module '%s': %s\n",
							func_name.c_str(), symbol_name.c_str(), module_name.c_str(), dynlib_platform_error());
					continue;
				}
			}

			// Call the function
			qd_exec_result result = func(ctx);
			if (result.code != 0) {
				fprintf(stderr, "qd_execute: Function '%s::%s' returned error code %d\n", module_name.c_str(),
						func_name.c_str(), result.code);
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
			// Clean up all modules
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
