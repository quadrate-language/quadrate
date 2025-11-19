#include <qd/qd.h>

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/semantic_validator.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// Module implementation
struct qd_module {
	std::string name;
	std::vector<std::string> scripts;
	std::unordered_map<std::string, void (*)()> native_functions;
	std::unordered_map<std::string, std::string> symbol_map; // function_name -> full_symbol_name
	void* dl_handle; // dlopen handle
	fs::path temp_dir;
	fs::path so_path;
	bool compiled;

	qd_module(const std::string& n) : name(n), dl_handle(nullptr), compiled(false) {
	}

	~qd_module() {
		// Clean up dynamic library
		if (dl_handle) {
			dlclose(dl_handle);
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
		char temp_template[] = "/tmp/qd_embed_XXXXXX";
		char* temp_path = mkdtemp(temp_template);
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
		size_t error_count = validator.validate(root, source_file.string().c_str(), true, false);
		if (error_count > 0) {
			fprintf(stderr, "qd_build: Semantic validation failed with %zu error(s)\n", error_count);
			return;
		}

		// Generate LLVM IR
		Qd::LlvmGenerator generator;
		generator.setOptimizationLevel(2);
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

		// Use clang/gcc to link the object file into a shared library
		fs::path qdrt_lib_path = fs::path(MESON_BUILD_ROOT) / "lib" / "qdrt";
		std::string link_cmd = "clang++ -shared ";
		link_cmd += obj_file.string();
		link_cmd += " -o ";
		link_cmd += mod->so_path.string();
		link_cmd += " -L" + qdrt_lib_path.string();
		link_cmd += " -lqdrt";
		link_cmd += " -Wl,-rpath,$ORIGIN";
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
		mod->dl_handle = dlopen(mod->so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
		if (!mod->dl_handle) {
			fprintf(stderr, "qd_build: Failed to load shared library: %s\n", dlerror());
			return;
		}

		mod->compiled = true;

	} catch (const std::exception& e) {
		fprintf(stderr, "qd_build: Exception: %s\n", e.what());
	}
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

		// Try parsing as integer
		long long int_val = strtoll(token.c_str(), &endptr, 10);
		if (*endptr == '\0') {
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

			void* sym = dlsym(mod->dl_handle, symbol_name.c_str());
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
					        func_name.c_str(), symbol_name.c_str(), module_name.c_str(), dlerror());
					continue;
				}
			}

			// Call the function
			qd_exec_result result = func(ctx);
			if (result.code != 0) {
				fprintf(stderr, "qd_execute: Function '%s::%s' returned error code %d\n",
				        module_name.c_str(), func_name.c_str(), result.code);
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
