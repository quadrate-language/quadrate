#include "constant_info.h"
#include "function_info.h"
#include "struct_info.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <jansson.h>
#include <map>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
#include <qc/ast_node_test.h>
#include <qc/error_reporter.h>
#include <qc/semantic_validator.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

// Default error span length in characters for diagnostic highlighting
static const int ERROR_SPAN_LENGTH = 10;

// Expand tilde (~) in file paths
static std::string expandTilde(const std::string& path) {
	if (!path.empty() && path[0] == '~') {
		const char* home = getenv("HOME");
		if (home) {
			return std::string(home) + path.substr(1);
		}
	}
	return path;
}

// Load dependencies from quadrate.toml and return include paths
static std::vector<std::string> loadDependenciesFromManifest(const std::string& manifestDir) {
	std::vector<std::string> includePaths;

	std::string manifestPath = manifestDir + "/quadrate.toml";
	std::ifstream file(manifestPath);
	if (!file.is_open()) {
		return includePaths;
	}

	std::string line;
	bool inDepsSection = false;
	std::string expandedDepName;
	std::string currentUrl;

	while (std::getline(file, line)) {
		// Trim whitespace
		line.erase(0, line.find_first_not_of(" \t\r\n"));
		if (!line.empty()) {
			line.erase(line.find_last_not_of(" \t\r\n") + 1);
		}

		// Check for [dependencies] section
		if (line == "[dependencies]") {
			if (!expandedDepName.empty() && !currentUrl.empty()) {
				std::string resolved = currentUrl;
				bool isPath = (resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
				               (resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));
				if (isPath) {
					resolved = expandTilde(resolved);
					if (resolved.size() > 0 && resolved[0] != '/') {
						resolved = manifestDir + "/" + resolved;
					}
					try {
						resolved = std::filesystem::weakly_canonical(resolved).string();
					} catch (...) {}
					if (std::filesystem::exists(resolved)) {
						includePaths.push_back(resolved);
					}
				}
			}
			inDepsSection = true;
			expandedDepName = "";
			currentUrl = "";
			continue;
		}

		// Check for [dependencies.name] section
		if (line.size() > 15 && line.substr(0, 14) == "[dependencies.") {
			if (!expandedDepName.empty() && !currentUrl.empty()) {
				std::string resolved = currentUrl;
				bool isPath = (resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
				               (resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));
				if (isPath) {
					resolved = expandTilde(resolved);
					if (resolved.size() > 0 && resolved[0] != '/') {
						resolved = manifestDir + "/" + resolved;
					}
					try {
						resolved = std::filesystem::weakly_canonical(resolved).string();
					} catch (...) {}
					if (std::filesystem::exists(resolved)) {
						includePaths.push_back(resolved);
					}
				}
			}
			size_t endBracket = line.find(']');
			if (endBracket != std::string::npos) {
				expandedDepName = line.substr(14, endBracket - 14);
				currentUrl = "";
				inDepsSection = false;
			}
			continue;
		}

		// Check for other sections
		if (!line.empty() && line[0] == '[') {
			if (!expandedDepName.empty() && !currentUrl.empty()) {
				std::string resolved = currentUrl;
				bool isPath = (resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
				               (resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));
				if (isPath) {
					resolved = expandTilde(resolved);
					if (resolved.size() > 0 && resolved[0] != '/') {
						resolved = manifestDir + "/" + resolved;
					}
					try {
						resolved = std::filesystem::weakly_canonical(resolved).string();
					} catch (...) {}
					if (std::filesystem::exists(resolved)) {
						includePaths.push_back(resolved);
					}
				}
				expandedDepName = "";
				currentUrl = "";
			}
			inDepsSection = false;
			continue;
		}

		// Skip comments and empty lines
		if (line.empty() || line[0] == '#') {
			continue;
		}

		// Parse key = value
		size_t eqPos = line.find('=');
		if (eqPos != std::string::npos) {
			std::string key = line.substr(0, eqPos);
			std::string value = line.substr(eqPos + 1);

			key.erase(0, key.find_first_not_of(" \t"));
			key.erase(key.find_last_not_of(" \t") + 1);
			value.erase(0, value.find_first_not_of(" \t"));
			value.erase(value.find_last_not_of(" \t") + 1);

			if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"') {
				value = value.substr(1, value.size() - 2);
			}

			if (!expandedDepName.empty()) {
				if (key == "url") {
					currentUrl = value;
				}
			} else if (inDepsSection) {
				std::string resolved = value;
				bool isPath = (resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
				               (resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));

				if (isPath) {
					resolved = expandTilde(resolved);
					if (resolved.size() > 0 && resolved[0] != '/') {
						resolved = manifestDir + "/" + resolved;
					}
					try {
						resolved = std::filesystem::weakly_canonical(resolved).string();
					} catch (...) {}
					if (std::filesystem::exists(resolved)) {
						includePaths.push_back(resolved);
					}
				}
			}
		}
	}

	// Handle any pending expanded dependency
	if (!expandedDepName.empty() && !currentUrl.empty()) {
		std::string resolved = currentUrl;
		bool isPath = (resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
		               (resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));
		if (isPath) {
			resolved = expandTilde(resolved);
			if (resolved.size() > 0 && resolved[0] != '/') {
				resolved = manifestDir + "/" + resolved;
			}
			try {
				resolved = std::filesystem::weakly_canonical(resolved).string();
			} catch (...) {}
			if (std::filesystem::exists(resolved)) {
				includePaths.push_back(resolved);
			}
		}
	}

	return includePaths;
}

// LSP Server using jansson for JSON handling
class QuadrateLSP {
public:
	QuadrateLSP() : messageId_(0) {
	}

	void run() {
		while (true) {
			std::string message = readMessage();
			if (message.empty()) {
				break;
			}

			handleMessage(message);
		}
	}

private:
	std::string readMessage() {
		std::string line;
		size_t contentLength = 0;

		// Read headers
		while (std::getline(std::cin, line)) {
			if (line == "\r" || line == "\r\n" || line == "\n" || line.empty()) {
				break;
			}
			if (line.substr(0, 16) == "Content-Length: ") {
				contentLength = static_cast<size_t>(std::stoi(line.substr(16)));
			}
		}

		if (contentLength == 0) {
			return "";
		}

		// Read content
		std::string content;
		content.resize(contentLength);
		std::cin.read(&content[0], static_cast<std::streamsize>(contentLength));

		return content;
	}

	void sendMessage(json_t* json) {
		char* message = json_dumps(json, JSON_COMPACT);
		if (message) {
			std::cout << "Content-Length: " << strlen(message) << "\r\n\r\n" << message << std::flush;
			free(message);
		}
	}

	std::string getJsonString(json_t* obj, const char* key) {
		json_t* val = json_object_get(obj, key);
		if (val && json_is_string(val)) {
			return json_string_value(val);
		}
		return "";
	}

	json_t* getJsonObject(json_t* obj, const char* key) {
		return json_object_get(obj, key);
	}

	void handleMessage(const std::string& message) {
		json_error_t error;
		json_t* root = json_loads(message.c_str(), 0, &error);

		if (!root) {
			return; // Invalid JSON, ignore
		}

		std::string method = getJsonString(root, "method");
		std::string id = getJsonString(root, "id");

		// If id is not string, try integer
		if (id.empty()) {
			json_t* id_json = json_object_get(root, "id");
			if (id_json && json_is_integer(id_json)) {
				id = std::to_string(json_integer_value(id_json));
			}
		}

		if (method == "initialize") {
			json_t* params = getJsonObject(root, "params");
			json_t* initOptions = params ? getJsonObject(params, "initializationOptions") : nullptr;
			handleInitialize(id, initOptions);
		} else if (method == "initialized") {
			// Nothing to do
		} else if (method == "workspace/didChangeConfiguration") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* settings = getJsonObject(params, "settings");
				if (settings) {
					json_t* quadrate = getJsonObject(settings, "quadrate");
					if (quadrate) {
						json_t* lint = getJsonObject(quadrate, "lint");
						if (lint) {
							json_t* enabled = json_object_get(lint, "enabled");
							if (enabled && json_is_boolean(enabled)) {
								lintEnabled_ = json_boolean_value(enabled);
							}
							json_t* path = json_object_get(lint, "path");
							if (path && json_is_string(path)) {
								quadlintPath_ = json_string_value(path);
							}
						}
					}
				}
			}
		} else if (method == "textDocument/didOpen") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					std::string text = getJsonString(textDoc, "text");
					handleDidOpen(uri, text);
				}
			}
		} else if (method == "textDocument/didChange") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* contentChanges = getJsonObject(params, "contentChanges");
				if (textDoc && contentChanges && json_is_array(contentChanges)) {
					std::string uri = getJsonString(textDoc, "uri");
					// For full sync, contentChanges[0] contains the full document
					if (json_array_size(contentChanges) > 0) {
						json_t* change = json_array_get(contentChanges, 0);
						std::string text = getJsonString(change, "text");
						if (!text.empty()) {
							handleDidOpen(uri, text);
						}
					}
				}
			}
		} else if (method == "textDocument/didSave") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					std::string text = getJsonString(params, "text");
					if (!text.empty()) {
						handleDidOpen(uri, text);
					} else {
						// Fallback: use stored document content
						auto it = documents_.find(uri);
						if (it != documents_.end()) {
							publishDiagnostics(uri, it->second);
						}
					}
				}
			}
		} else if (method == "textDocument/formatting") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					handleFormatting(id, uri);
				}
			}
		} else if (method == "textDocument/completion") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				if (textDoc && position) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleCompletion(id, uri, line, character);
					}
				}
			}
		} else if (method == "textDocument/hover") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				if (textDoc && position) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleHover(id, uri, line, character);
					}
				}
			}
		} else if (method == "textDocument/signatureHelp") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				if (textDoc && position) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleSignatureHelp(id, uri, line, character);
					}
				}
			}
		} else if (method == "textDocument/documentSymbol") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					handleDocumentSymbols(id, uri);
				}
			}
		} else if (method == "textDocument/definition") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				if (textDoc && position) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleDefinition(id, uri, line, character);
					}
				}
			}
		} else if (method == "textDocument/references") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				if (textDoc && position) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleReferences(id, uri, line, character);
					}
				}
			}
		} else if (method == "textDocument/documentHighlight") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				if (textDoc && position) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleDocumentHighlight(id, uri, line, character);
					}
				}
			}
		} else if (method == "textDocument/foldingRange") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					handleFoldingRange(id, uri);
				}
			}
		} else if (method == "textDocument/rename") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				json_t* position = getJsonObject(params, "position");
				std::string newName = getJsonString(params, "newName");
				if (textDoc && position && !newName.empty()) {
					std::string uri = getJsonString(textDoc, "uri");
					json_t* lineJson = json_object_get(position, "line");
					json_t* charJson = json_object_get(position, "character");
					if (lineJson && charJson && json_is_integer(lineJson) && json_is_integer(charJson)) {
						size_t line = static_cast<size_t>(json_integer_value(lineJson));
						size_t character = static_cast<size_t>(json_integer_value(charJson));
						handleRename(id, uri, line, character, newName);
					}
				}
			}
		} else if (method == "shutdown") {
			handleShutdown(id);
		} else if (method == "exit") {
			json_decref(root);
			exit(0);
		}

		json_decref(root);
	}

	void handleInitialize(const std::string& id, json_t* initOptions) {
		// Parse initialization options for lint settings
		if (initOptions) {
			json_t* lint = getJsonObject(initOptions, "lint");
			if (lint) {
				json_t* enabled = json_object_get(lint, "enabled");
				if (enabled && json_is_boolean(enabled)) {
					lintEnabled_ = json_boolean_value(enabled);
				}
				json_t* path = json_object_get(lint, "path");
				if (path && json_is_string(path)) {
					quadlintPath_ = json_string_value(path);
				}
			}
		}

		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		json_t* result = json_object();
		json_t* capabilities = json_object();

		// Text document sync with full content on change and save
		json_t* textDocumentSync = json_object();
		json_object_set_new(textDocumentSync, "openClose", json_true());
		json_object_set_new(textDocumentSync, "change", json_integer(1)); // Full sync
		json_t* saveOptions = json_object();
		json_object_set_new(saveOptions, "includeText", json_true());
		json_object_set_new(textDocumentSync, "save", saveOptions);
		json_object_set_new(capabilities, "textDocumentSync", textDocumentSync);
		json_object_set_new(capabilities, "documentFormattingProvider", json_true());
		json_object_set_new(capabilities, "hoverProvider", json_true());
		json_object_set_new(capabilities, "documentSymbolProvider", json_true());
		json_object_set_new(capabilities, "definitionProvider", json_true());
		json_object_set_new(capabilities, "referencesProvider", json_true());
		json_object_set_new(capabilities, "renameProvider", json_true());

		// Enable snippet support in completions
		json_t* completionProvider = json_object();
		json_object_set_new(completionProvider, "resolveProvider", json_false());
		json_t* triggerChars = json_array();
		json_array_append_new(triggerChars, json_string(":"));
		json_array_append_new(triggerChars, json_string("@"));
		json_object_set_new(completionProvider, "triggerCharacters", triggerChars);
		json_object_set_new(capabilities, "completionProvider", completionProvider);

		// Signature help for function calls
		json_t* signatureHelpProvider = json_object();
		json_t* sigTriggerChars = json_array();
		json_array_append_new(sigTriggerChars, json_string(" "));
		json_object_set_new(signatureHelpProvider, "triggerCharacters", sigTriggerChars);
		json_object_set_new(capabilities, "signatureHelpProvider", signatureHelpProvider);

		// Document highlight (highlight other occurrences of symbol under cursor)
		json_object_set_new(capabilities, "documentHighlightProvider", json_true());

		// Folding ranges (code folding for functions, blocks)
		json_object_set_new(capabilities, "foldingRangeProvider", json_true());

		json_object_set_new(result, "capabilities", capabilities);

		json_t* serverInfo = json_object();
		json_object_set_new(serverInfo, "name", json_string("quadlsp"));
		json_object_set_new(serverInfo, "version", json_string("0.1.0"));
		json_object_set_new(result, "serverInfo", serverInfo);

		json_object_set_new(response, "result", result);

		sendMessage(response);
		json_decref(response);
	}

	void handleDidOpen(const std::string& uri, const std::string& text) {
		documents_[uri] = text;
		publishDiagnostics(uri, text);
	}

	// Structure to hold lint warnings
	struct LintWarning {
		size_t line;
		size_t column;
		std::string message;
	};

	// Strip ANSI escape codes from a string
	static std::string stripAnsiCodes(const std::string& input) {
		std::string result;
		result.reserve(input.size());
		bool inEscape = false;
		for (size_t i = 0; i < input.size(); i++) {
			// Check for escape sequence start (ESC [ or just [0-9 after previous escape)
			if (input[i] == '\033' || input[i] == '\x1b') {
				inEscape = true;
				continue;
			}
			// Also handle case where [ appears at start or after stripping ESC
			if (input[i] == '[' && (i == 0 || inEscape ||
										   (i + 1 < input.size() && (isdigit(input[i + 1]) || input[i + 1] == ';' ||
																			input[i + 1] == 'm')))) {
				inEscape = true;
				continue;
			}
			if (inEscape) {
				if (input[i] == 'm') {
					inEscape = false;
				}
				continue;
			}
			result += input[i];
		}
		return result;
	}

	// Run quadlint on source content and return warnings
	std::vector<LintWarning> runQuadlint(const std::string& source) {
		std::vector<LintWarning> warnings;

		// Write source to temp file so quadlint sees current editor content
		std::string tempPath = "/tmp/quadlsp_lint_" + std::to_string(getpid()) + ".qd";
		{
			std::ofstream tempFile(tempPath);
			if (!tempFile.good()) {
				return warnings;
			}
			tempFile << source;
		}

		// Build command
		std::string command = quadlintPath_ + " \"" + tempPath + "\" 2>&1";

		FILE* pipe = popen(command.c_str(), "r");
		if (!pipe) {
			return warnings;
		}

		char buffer[512];
		std::string output;
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
			output += buffer;
		}
		pclose(pipe);

		// Parse output - format: "filepath:line:column: warning: message"
		std::istringstream stream(output);
		std::string line;
		while (std::getline(stream, line)) {
			// Skip empty lines and summary lines
			if (line.empty() || line.find("issues found") != std::string::npos) {
				continue;
			}

			// Look for the warning pattern
			// Example: /path/file.qd:4:4: warning: Unused function 'hello'
			size_t firstColon = line.find(':');
			if (firstColon == std::string::npos) {
				continue;
			}

			// Skip the filepath part (find second colon for line number)
			size_t secondColon = line.find(':', firstColon + 1);
			if (secondColon == std::string::npos) {
				continue;
			}

			size_t thirdColon = line.find(':', secondColon + 1);
			if (thirdColon == std::string::npos) {
				continue;
			}

			// Extract line number
			std::string lineStr = line.substr(firstColon + 1, secondColon - firstColon - 1);
			// Extract column number
			std::string colStr = line.substr(secondColon + 1, thirdColon - secondColon - 1);

			// Find "warning:" marker
			size_t warningPos = line.find("warning:", thirdColon);
			if (warningPos == std::string::npos) {
				continue;
			}

			// Extract message (after "warning: ")
			std::string message = line.substr(warningPos + 9); // 9 = strlen("warning: ")
			// Strip ANSI escape codes
			message = stripAnsiCodes(message);
			// Trim leading/trailing whitespace
			while (!message.empty() && (message.front() == ' ' || message.front() == '\t')) {
				message.erase(0, 1);
			}
			while (!message.empty() && (message.back() == ' ' || message.back() == '\t' || message.back() == '\n')) {
				message.pop_back();
			}

			try {
				LintWarning warning;
				warning.line = std::stoul(lineStr);
				warning.column = std::stoul(colStr);
				warning.message = message;
				warnings.push_back(warning);
			} catch (...) {
				// Ignore parse errors
			}
		}

		// Clean up temp file
		std::remove(tempPath.c_str());

		return warnings;
	}

	void publishDiagnostics(const std::string& uri, const std::string& text) {
		// Parse using Ast class
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(text.c_str(), false, nullptr);

		json_t* notification = json_object();
		json_object_set_new(notification, "jsonrpc", json_string("2.0"));
		json_object_set_new(notification, "method", json_string("textDocument/publishDiagnostics"));

		json_t* params = json_object();
		json_object_set_new(params, "uri", json_string(uri.c_str()));

		json_t* diagnostics = json_array();

		// First, show parse errors from AST
		if (ast.hasErrors()) {
			const auto& errors = ast.getErrors();
			for (const auto& error : errors) {
				json_t* diag = json_object();

				// LSP uses 0-based line and column numbers
				size_t lspLine = (error.line > 0) ? error.line - 1 : 0;
				size_t lspColumn = (error.column > 0) ? error.column - 1 : 0;

				json_t* range = json_object();
				json_t* start = json_object();
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspColumn)));
				json_t* end = json_object();
				// End at the same position plus a reasonable offset for visibility
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(
						end, "character", json_integer(static_cast<json_int_t>(lspColumn + ERROR_SPAN_LENGTH)));
				json_object_set_new(range, "start", start);
				json_object_set_new(range, "end", end);

				json_object_set_new(diag, "range", range);
				json_object_set_new(diag, "severity", json_integer(1)); // Error
				json_object_set_new(diag, "message", json_string(error.message.c_str()));

				json_array_append_new(diagnostics, diag);
			}
		}

		// If parsing succeeded, run semantic validation to catch unresolved symbols, etc.
		if (root && !ast.hasErrors()) {
			Qd::SemanticValidator validator;
			validator.setStoreErrors(true);

			// Get filename from URI for validator
			std::string filePath = uri.substr(7); // Remove "file://"

			// Load include paths from quadrate.toml if present
			std::filesystem::path fileDir = std::filesystem::path(filePath).parent_path();
			std::vector<std::string> manifestPaths = loadDependenciesFromManifest(fileDir.string());
			if (!manifestPaths.empty()) {
				validator.setIncludePaths(manifestPaths);
			}

			validator.validate(root, filePath.c_str(), false, false);

			if (validator.errorCount() > 0) {
				const auto& errors = validator.getErrors();
				for (const auto& error : errors) {
					json_t* diag = json_object();

					// LSP uses 0-based line and column numbers
					size_t lspLine = (error.line > 0) ? error.line - 1 : 0;
					size_t lspColumn = (error.column > 0) ? error.column - 1 : 0;

					json_t* range = json_object();
					json_t* start = json_object();
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspColumn)));
					json_t* end = json_object();
					// End at the same position plus a reasonable offset for visibility
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(lspColumn + ERROR_SPAN_LENGTH)));
					json_object_set_new(range, "start", start);
					json_object_set_new(range, "end", end);

					json_object_set_new(diag, "range", range);
					json_object_set_new(diag, "severity", json_integer(1)); // Error
					json_object_set_new(diag, "message", json_string(error.message.c_str()));

					json_array_append_new(diagnostics, diag);
				}
			}
		}

		// Run quadlint if enabled and no parse/semantic errors
		// Skip linting for files in standard library or installed package locations
		bool isExternalModule = false;
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			// Check if file is in standard library or package locations
			if (filePath.find("/usr/share/quadrate/") != std::string::npos ||
					filePath.find("/.quadrate/packages/") != std::string::npos) {
				isExternalModule = true;
			}
			// Also check QUADRATE_ROOT
			const char* quadrateRoot = getenv("QUADRATE_ROOT");
			if (quadrateRoot && filePath.find(quadrateRoot) == 0) {
				isExternalModule = true;
			}
		}

		if (lintEnabled_ && !isExternalModule && root && !ast.hasErrors()) {
			std::vector<LintWarning> warnings = runQuadlint(text);

			for (const auto& warning : warnings) {
				json_t* diag = json_object();

				// LSP uses 0-based line and column numbers
				size_t lspLine = (warning.line > 0) ? warning.line - 1 : 0;
				size_t lspColumn = (warning.column > 0) ? warning.column - 1 : 0;

				json_t* range = json_object();
				json_t* start = json_object();
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspColumn)));
				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(
						end, "character", json_integer(static_cast<json_int_t>(lspColumn + ERROR_SPAN_LENGTH)));
				json_object_set_new(range, "start", start);
				json_object_set_new(range, "end", end);

				json_object_set_new(diag, "range", range);
				json_object_set_new(diag, "severity", json_integer(2)); // Warning (not Error)
				json_object_set_new(diag, "source", json_string("quadlint"));
				json_object_set_new(diag, "message", json_string(warning.message.c_str()));

				json_array_append_new(diagnostics, diag);
			}
		}

		json_object_set_new(params, "diagnostics", diagnostics);
		json_object_set_new(notification, "params", params);

		sendMessage(notification);
		json_decref(notification);
	}

	void handleFormatting(const std::string& id, const std::string& uri) {
		(void)uri; // Not used yet

		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));
		json_object_set_new(response, "result", json_array()); // Empty array for now

		sendMessage(response);
		json_decref(response);
	}

	void handleCompletion(const std::string& id, const std::string& uri, size_t line, size_t character) {
		static const char* instructions[] = {"add", "sub", "mul", "div", "dup", "swap", "drop", "over", "rot", "print",
				"prints", "eq", "neq", "lt", "gt", "lte", "gte", "and", "or", "not", "inc", "dec", "abs", "sqrt", "sq",
				"sin", "cos", "tan", "asin", "acos", "atan", "ln", "log10", "pow", "min", "max", "ceil", "floor",
				"round", "if", "for", "while", "loop", "switch", "case", "default", "break", "continue", "defer",
				"free", "struct", "pub"};

		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		json_t* result = json_object();
		json_object_set_new(result, "isIncomplete", json_false());

		json_t* items = json_array();

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			// Try to read file from disk if it's a file:// URI
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		// Get source directory for module resolution
		std::string sourceDir;
		if (uri.substr(0, 7) == "file://") {
			std::string filePath = uri.substr(7);
			size_t lastSlash = filePath.rfind('/');
			if (lastSlash != std::string::npos) {
				sourceDir = filePath.substr(0, lastSlash);
			}
		}

		// Check if we're completing after field access (e.g., "v@")
		std::string fieldAccessVar = getFieldAccessVariableAtPosition(documentText, line, character);

		if (!fieldAccessVar.empty() && !documentText.empty()) {
			// Field access completion - need to find the struct type and show its fields
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors()) {
				std::string structType = findStructTypeOfVariable(root, fieldAccessVar, line);
				if (!structType.empty()) {
					std::vector<std::pair<std::string, std::string>> fields =
							findStructFields(structType, documentText, sourceDir);

					for (const auto& field : fields) {
						json_t* item = json_object();
						json_object_set_new(item, "label", json_string(field.first.c_str()));
						json_object_set_new(item, "kind", json_integer(5)); // Field kind

						// Add type as detail
						json_object_set_new(item, "detail", json_string(field.second.c_str()));

						// Build documentation
						std::ostringstream docStream;
						docStream << "**Field:** `" << field.first << "`\n\n";
						docStream << "**Type:** `" << field.second << "`\n\n";
						docStream << "**Struct:** `" << structType << "`";

						json_t* documentation = json_object();
						json_object_set_new(documentation, "kind", json_string("markdown"));
						json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
						json_object_set_new(item, "documentation", documentation);

						json_array_append_new(items, item);
					}
				}
			}
		}
		// Check if we're completing after a module prefix (e.g., "math::")
		else if (std::string modulePrefix = getModulePrefixAtPosition(documentText, line, character);
				!modulePrefix.empty()) {
			// Module-qualified completion - only show functions from that module
			// First, check for FFI imports (import "lib.a" as "namespace" { ... })
			bool foundFfiImport = false;
			if (!documentText.empty()) {
				Qd::Ast ffiAst;
				Qd::IAstNode* ffiRoot = ffiAst.generate(documentText.c_str(), false, nullptr);
				// Don't check hasErrors() - we want to find imports even in incomplete code
				if (ffiRoot && ffiRoot->type() == Qd::IAstNode::Type::PROGRAM) {
					for (size_t i = 0; i < ffiRoot->childCount(); i++) {
						Qd::IAstNode* child = ffiRoot->child(i);
						if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
							Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
							if (importNode->namespaceName() == modulePrefix) {
								foundFfiImport = true;
								// Add functions from FFI import block
								for (const auto* func : importNode->functions()) {
									json_t* item = json_object();
									json_object_set_new(item, "label", json_string(func->name.c_str()));
									json_object_set_new(item, "kind", json_integer(3)); // Function

									// Use plain text insert to avoid editor adding ()
									json_object_set_new(item, "insertTextFormat", json_integer(1)); // PlainText
									json_object_set_new(item, "insertText", json_string(func->name.c_str()));

									// Build signature
									std::ostringstream sigStream;
									sigStream << "fn " << func->name << "(";
									for (size_t j = 0; j < func->inputParameters.size(); j++) {
										if (j > 0) {
											sigStream << " ";
										}
										sigStream << func->inputParameters[j]->name() << ":"
												  << func->inputParameters[j]->typeString();
									}
									sigStream << " -- ";
									for (size_t j = 0; j < func->outputParameters.size(); j++) {
										if (j > 0) {
											sigStream << " ";
										}
										sigStream << func->outputParameters[j]->name() << ":"
												  << func->outputParameters[j]->typeString();
									}
									sigStream << ")";

									json_object_set_new(item, "detail", json_string(sigStream.str().c_str()));

									// Build documentation
									std::ostringstream docStream;
									docStream << "**FFI Function (from " << modulePrefix << ")**\n\n";
									docStream << "```quadrate\n" << sigStream.str() << "\n```";

									json_t* documentation = json_object();
									json_object_set_new(documentation, "kind", json_string("markdown"));
									json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
									json_object_set_new(item, "documentation", documentation);

									json_array_append_new(items, item);
								}
								break;
							}
						}
					}
				}
			}

			std::string modulePath = resolveModulePath(modulePrefix, sourceDir);
			if (!foundFfiImport && !modulePath.empty()) {
				// Add functions from module
				std::vector<FunctionInfo> functions = extractModuleFunctions(modulePath);

				for (const auto& func : functions) {
					json_t* item = json_object();
					json_object_set_new(item, "label", json_string(func.name.c_str()));
					json_object_set_new(item, "kind", json_integer(3)); // Function

					// Add snippet with placeholders
					json_object_set_new(item, "insertTextFormat", json_integer(2)); // Snippet format
					json_object_set_new(item, "insertText", json_string(func.snippet.c_str()));

					// Add signature as detail
					json_object_set_new(item, "detail", json_string(func.signature.c_str()));

					// Build documentation
					std::ostringstream docStream;
					docStream << "**Module:** `" << modulePrefix << "`\n\n";
					docStream << "**Function signature:**\n```quadrate\n" << func.signature << "\n```\n\n";
					if (!func.inputParams.empty()) {
						docStream << "**Stack before call:** ";
						for (size_t i = 0; i < func.inputParams.size(); i++) {
							if (i > 0) {
								docStream << ", ";
							}
							docStream << func.inputParams[i];
						}
						docStream << "\n";
					}
					if (!func.outputParams.empty()) {
						docStream << "**Stack after call:** ";
						for (size_t i = 0; i < func.outputParams.size(); i++) {
							if (i > 0) {
								docStream << ", ";
							}
							docStream << func.outputParams[i];
						}
					}

					json_t* documentation = json_object();
					json_object_set_new(documentation, "kind", json_string("markdown"));
					json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
					json_object_set_new(item, "documentation", documentation);

					json_array_append_new(items, item);
				}

				// Add structs from module
				std::vector<StructInfo> structs = extractModuleStructs(modulePath);
				for (const auto& structInfo : structs) {
					json_t* item = json_object();
					json_object_set_new(item, "label", json_string(structInfo.name.c_str()));
					json_object_set_new(item, "kind", json_integer(22)); // Struct kind

					// Build documentation showing struct fields
					std::ostringstream docStream;
					docStream << "**Module:** `" << modulePrefix << "`\n\n";
					docStream << "**Struct:** `" << structInfo.name << "`\n\n";
					docStream << "**Fields:**\n";
					for (const auto& field : structInfo.fields) {
						docStream << "- `" << field.first << ": " << field.second << "`\n";
					}

					json_t* documentation = json_object();
					json_object_set_new(documentation, "kind", json_string("markdown"));
					json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
					json_object_set_new(item, "documentation", documentation);

					json_array_append_new(items, item);
				}

				// Add constants from module
				std::vector<ConstantInfo> constants = extractModuleConstants(modulePath);
				for (const auto& constInfo : constants) {
					json_t* item = json_object();
					json_object_set_new(item, "label", json_string(constInfo.name.c_str()));
					json_object_set_new(item, "kind", json_integer(21)); // Constant kind

					// Build documentation
					std::ostringstream docStream;
					docStream << "**Module:** `" << modulePrefix << "`\n\n";
					docStream << "**Constant:** `" << constInfo.name << " = " << constInfo.value << "`";

					json_t* documentation = json_object();
					json_object_set_new(documentation, "kind", json_string("markdown"));
					json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
					json_object_set_new(item, "documentation", documentation);

					json_array_append_new(items, item);
				}
			}
		} else {
			// Regular completion - show built-ins, local functions, structs

			// Add built-in instructions
			for (size_t i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++) {
				json_t* item = json_object();
				json_object_set_new(item, "label", json_string(instructions[i]));
				json_object_set_new(item, "kind", json_integer(3)); // Function
				json_object_set_new(item, "detail", json_string("Built-in instruction"));
				json_array_append_new(items, item);
			}

			// Add user-defined functions from the current document
			if (!documentText.empty()) {
				std::vector<FunctionInfo> functions = extractFunctions(documentText);

				for (const auto& func : functions) {
					json_t* item = json_object();
					json_object_set_new(item, "label", json_string(func.name.c_str()));
					json_object_set_new(item, "kind", json_integer(3)); // Function

					// Add snippet with placeholders
					json_object_set_new(item, "insertTextFormat", json_integer(2)); // Snippet format
					json_object_set_new(item, "insertText", json_string(func.snippet.c_str()));

					// Add signature as detail and documentation
					json_object_set_new(item, "detail", json_string(func.signature.c_str()));

					// Build documentation showing what needs to be on the stack
					std::ostringstream docStream;
					docStream << "**Function signature:**\n```quadrate\n" << func.signature << "\n```\n\n";
					if (!func.inputParams.empty()) {
						docStream << "**Stack before call:** ";
						for (size_t i = 0; i < func.inputParams.size(); i++) {
							if (i > 0) {
								docStream << ", ";
							}
							docStream << func.inputParams[i];
						}
						docStream << "\n";
					}
					if (!func.outputParams.empty()) {
						docStream << "**Stack after call:** ";
						for (size_t i = 0; i < func.outputParams.size(); i++) {
							if (i > 0) {
								docStream << ", ";
							}
							docStream << func.outputParams[i];
						}
					}

					json_t* documentation = json_object();
					json_object_set_new(documentation, "kind", json_string("markdown"));
					json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
					json_object_set_new(item, "documentation", documentation);

					json_array_append_new(items, item);
				}
			}

			// Add struct completions
			std::vector<StructInfo> structs = extractStructs(documentText);
			for (const auto& structInfo : structs) {
				json_t* item = json_object();
				json_object_set_new(item, "label", json_string(structInfo.name.c_str()));
				json_object_set_new(item, "kind", json_integer(22)); // Struct kind

				// Build documentation showing struct fields
				std::ostringstream docStream;
				docStream << "**Struct:** `" << structInfo.name << "`\n\n";
				docStream << "**Fields:**\n";
				for (const auto& field : structInfo.fields) {
					docStream << "- `" << field.first << ": " << field.second << "`\n";
				}

				json_t* documentation = json_object();
				json_object_set_new(documentation, "kind", json_string("markdown"));
				json_object_set_new(documentation, "value", json_string(docStream.str().c_str()));
				json_object_set_new(item, "documentation", documentation);

				json_array_append_new(items, item);
			}
		}

		json_object_set_new(result, "items", items);
		json_object_set_new(response, "result", result);

		sendMessage(response);
		json_decref(response);
	}

	std::string getBuiltInDocumentation(const std::string& word) {
		static const std::map<std::string, std::string> docs = {
				{"add", "Add two numbers from the stack.\n\n**Stack effect:** `a b -- result`\n\nPops two values, "
						"pushes their sum."},
				{"sub", "Subtract top from second.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes `a "
						"- b`."},
				{"mul", "Multiply two numbers.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes their "
						"product."},
				{"div", "Divide second by top.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes `a / "
						"b`."},
				{"dup", "Duplicate top of stack.\n\n**Stack effect:** `a -- a a`\n\nDuplicates the top stack value."},
				{"swap", "Swap top two values.\n\n**Stack effect:** `a b -- b a`\n\nSwaps the top two stack values."},
				{"drop", "Remove top of stack.\n\n**Stack effect:** `a --`\n\nRemoves the top value from the stack."},
				{"over", "Copy second item to top.\n\n**Stack effect:** `a b -- a b a`\n\nCopies the second value to "
						 "the top."},
				{"rot", "Rotate top three items.\n\n**Stack effect:** `a b c -- b c a`\n\nRotates the top three "
						"values."},
				{"print", "Print top value.\n\n**Stack effect:** `a --`\n\nPrints the top value and removes it."},
				{"prints", "Print string.\n\n**Stack effect:** `str --`\n\nPrints a string value."},
				{"eq", "Test equality.\n\n**Stack effect:** `a b -- bool`\n\nPushes 1 if equal, 0 otherwise."},
				{"neq", "Test inequality.\n\n**Stack effect:** `a b -- bool`\n\nPushes 1 if not equal, 0 otherwise."},
				{"lt", "Less than.\n\n**Stack effect:** `a b -- bool`\n\nPushes 1 if a < b, 0 otherwise."},
				{"gt", "Greater than.\n\n**Stack effect:** `a b -- bool`\n\nPushes 1 if a > b, 0 otherwise."},
				{"lte", "Less than or equal.\n\n**Stack effect:** `a b -- bool`"},
				{"gte", "Greater than or equal.\n\n**Stack effect:** `a b -- bool`"},
				{"and", "Logical AND.\n\n**Stack effect:** `a b -- bool`"},
				{"or", "Logical OR.\n\n**Stack effect:** `a b -- bool`"},
				{"not", "Logical NOT.\n\n**Stack effect:** `a -- bool`"},
				{"abs", "Absolute value.\n\n**Stack effect:** `a -- result`"},
				{"sqrt", "Square root.\n\n**Stack effect:** `a -- result`"},
				{"sq", "Square.\n\n**Stack effect:** `a -- result`"},
				{"sin", "Sine function.\n\n**Stack effect:** `a -- result`"},
				{"cos", "Cosine function.\n\n**Stack effect:** `a -- result`"},
				{"tan", "Tangent function.\n\n**Stack effect:** `a -- result`"},
				{"if", "Conditional execution.\n\n**Syntax:** `condition if { ... } else { ... }`"},
				{"for", "Loop construct.\n\n**Syntax:** `start end step for name { ... }`"},
				{"while",
						"Conditional loop.\n\n**Syntax:** `condition while { ... next-condition }`\n\nPops and checks "
						"condition each iteration. Continues while truthy."},
				{"loop", "Infinite loop.\n\n**Syntax:** `loop { ... }`"},
				{"free",
						"Free allocated memory.\n\n**Stack effect:** `ptr --`\n\nFrees memory allocated for structs or "
						"strings. Automatically frees nested string fields in structs."},
				{"struct", "Declare a struct type.\n\n**Syntax:** `struct Name { field1:type1 field2:type2 "
						   "}`\n\nDefines a "
						   "composite data type with named fields."},
				{"pub", "Public visibility modifier.\n\n**Syntax:** `pub struct Name { ... }` or `pub fn name(...) { "
						"... "
						"}`\n\nMakes structs or functions visible to other modules."},
				{"defer",
						"Defer execution until scope exit.\n\n**Syntax:** `defer { ... }`\n\nExecutes code block when "
						"function returns, useful for cleanup."},
		};

		auto it = docs.find(word);
		if (it != docs.end()) {
			return it->second;
		}
		return "";
	}

	std::string getWordAtPosition(const std::string& text, size_t line, size_t character) {
		// Split text into lines
		std::vector<std::string> lines;
		std::istringstream stream(text);
		std::string currentLine;
		while (std::getline(stream, currentLine)) {
			lines.push_back(currentLine);
		}

		if (line >= lines.size()) {
			return "";
		}

		const std::string& targetLine = lines[line];
		if (character >= targetLine.length()) {
			return "";
		}

		// Find word boundaries
		size_t start = character;
		size_t end = character;

		// Move start backward to beginning of word
		while (start > 0 &&
				(isalnum(targetLine[start - 1]) || targetLine[start - 1] == '_' || targetLine[start - 1] == ':')) {
			start--;
		}

		// Move end forward to end of word
		while (end < targetLine.length() &&
				(isalnum(targetLine[end]) || targetLine[end] == '_' || targetLine[end] == ':')) {
			end++;
		}

		if (end > start) {
			return targetLine.substr(start, end - start);
		}
		return "";
	}

	// Get module prefix if cursor is right after "module::" (returns empty string if not)
	std::string getModulePrefixAtPosition(const std::string& text, size_t line, size_t character) {
		// Split text into lines
		std::vector<std::string> lines;
		std::istringstream stream(text);
		std::string currentLine;
		while (std::getline(stream, currentLine)) {
			lines.push_back(currentLine);
		}

		if (line >= lines.size()) {
			return "";
		}

		const std::string& targetLine = lines[line];
		if (character == 0 || character > targetLine.length()) {
			return "";
		}

		// Check if the characters before cursor end with "::"
		size_t pos = character;

		// Skip back over any partial word being typed after ::
		while (pos > 0 && (isalnum(targetLine[pos - 1]) || targetLine[pos - 1] == '_')) {
			pos--;
		}

		// Now check if we have "::" before that
		if (pos >= 2 && targetLine[pos - 1] == ':' && targetLine[pos - 2] == ':') {
			// Found "::", now extract the module name before it
			size_t colonPos = pos - 2;
			size_t moduleEnd = colonPos;
			size_t moduleStart = colonPos;

			// Move back to find start of module name
			while (moduleStart > 0 && (isalnum(targetLine[moduleStart - 1]) || targetLine[moduleStart - 1] == '_')) {
				moduleStart--;
			}

			if (moduleEnd > moduleStart) {
				return targetLine.substr(moduleStart, moduleEnd - moduleStart);
			}
		}

		return "";
	}

	// Get variable name if cursor is right after "variable@" (returns empty string if not)
	std::string getFieldAccessVariableAtPosition(const std::string& text, size_t line, size_t character) {
		// Split text into lines
		std::vector<std::string> lines;
		std::istringstream stream(text);
		std::string currentLine;
		while (std::getline(stream, currentLine)) {
			lines.push_back(currentLine);
		}

		if (line >= lines.size()) {
			return "";
		}

		const std::string& targetLine = lines[line];
		if (character == 0 || character > targetLine.length()) {
			return "";
		}

		// Check if the characters before cursor end with "@"
		size_t pos = character;

		// Skip back over any partial field name being typed after @
		while (pos > 0 && (isalnum(targetLine[pos - 1]) || targetLine[pos - 1] == '_')) {
			pos--;
		}

		// Now check if we have "@" before that
		if (pos >= 1 && targetLine[pos - 1] == '@') {
			// Found "@", now extract the variable name before it
			size_t atPos = pos - 1;
			size_t varEnd = atPos;
			size_t varStart = atPos;

			// Move back to find start of variable name
			while (varStart > 0 && (isalnum(targetLine[varStart - 1]) || targetLine[varStart - 1] == '_')) {
				varStart--;
			}

			if (varEnd > varStart) {
				return targetLine.substr(varStart, varEnd - varStart);
			}
		}

		return "";
	}

	// Find struct fields for a given struct type (handles both local and module::Struct formats)
	std::vector<std::pair<std::string, std::string>> findStructFields(
			const std::string& structType, const std::string& documentText, const std::string& sourceDir) {
		std::vector<std::pair<std::string, std::string>> fields;

		// Check if it's a module-qualified struct (module::Struct)
		size_t colonPos = structType.find("::");
		if (colonPos != std::string::npos) {
			std::string moduleName = structType.substr(0, colonPos);
			std::string structName = structType.substr(colonPos + 2);

			std::string modulePath = resolveModulePath(moduleName, sourceDir);
			if (!modulePath.empty()) {
				std::vector<StructInfo> structs = extractModuleStructs(modulePath);
				for (const auto& s : structs) {
					if (s.name == structName) {
						return s.fields;
					}
				}
			}
		} else {
			// Local struct - search in current document
			std::vector<StructInfo> structs = extractStructs(documentText);
			for (const auto& s : structs) {
				if (s.name == structType) {
					return s.fields;
				}
			}
		}

		return fields;
	}

	// Extract public functions from a module file
	std::vector<FunctionInfo> extractModuleFunctions(const std::string& modulePath) {
		std::vector<FunctionInfo> functions;

		std::ifstream file(modulePath);
		if (!file.good()) {
			return functions;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string moduleText = buffer.str();

		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

		if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
			return functions;
		}

		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

				// Only include public functions
				if (!funcNode->isPublic()) {
					continue;
				}

				FunctionInfo info;
				info.name = funcNode->name();

				// Build signature parts
				std::ostringstream sigStream;
				sigStream << "fn " << info.name << "(";

				// Extract input parameters
				const auto& inputs = funcNode->inputParameters();
				for (size_t j = 0; j < inputs.size(); j++) {
					Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(inputs[j]);
					std::string paramStr = param->name() + ":" + param->typeString();
					info.inputParams.push_back(paramStr);

					if (j > 0) {
						sigStream << " ";
					}
					sigStream << paramStr;
				}

				sigStream << " -- ";

				// Extract output parameters
				const auto& outputs = funcNode->outputParameters();
				for (size_t j = 0; j < outputs.size(); j++) {
					Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(outputs[j]);
					std::string paramStr = param->name() + ":" + param->typeString();
					info.outputParams.push_back(paramStr);

					if (j > 0) {
						sigStream << " ";
					}
					sigStream << paramStr;
				}

				sigStream << ")";
				info.signature = sigStream.str();

				// Build snippet with placeholders for input parameters
				std::ostringstream snippetStream;
				for (size_t j = 0; j < info.inputParams.size(); j++) {
					const std::string& param = info.inputParams[j];
					size_t colonPos = param.find(':');
					std::string paramName = (colonPos != std::string::npos) ? param.substr(0, colonPos) : param;

					snippetStream << "${" << (j + 1) << ":" << paramName << "}";
					if (j < info.inputParams.size() - 1) {
						snippetStream << " ";
					}
				}
				if (!info.inputParams.empty()) {
					snippetStream << " ";
				}
				snippetStream << info.name;

				info.snippet = snippetStream.str();

				functions.push_back(info);
			} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
				// Handle imported native functions (e.g., from C libraries)
				Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);

				for (const auto* importedFunc : importNode->functions()) {
					// Only include public imported functions
					if (!importedFunc->isPublic) {
						continue;
					}

					FunctionInfo info;
					info.name = importedFunc->name;

					// Build signature parts
					std::ostringstream sigStream;
					sigStream << "fn " << info.name << "(";

					// Extract input parameters
					const auto& inputs = importedFunc->inputParameters;
					for (size_t j = 0; j < inputs.size(); j++) {
						Qd::AstNodeParameter* param = inputs[j];
						std::string paramStr = param->name() + ":" + param->typeString();
						info.inputParams.push_back(paramStr);

						if (j > 0) {
							sigStream << " ";
						}
						sigStream << paramStr;
					}

					sigStream << " -- ";

					// Extract output parameters
					const auto& outputs = importedFunc->outputParameters;
					for (size_t j = 0; j < outputs.size(); j++) {
						Qd::AstNodeParameter* param = outputs[j];
						std::string paramStr = param->name() + ":" + param->typeString();
						info.outputParams.push_back(paramStr);

						if (j > 0) {
							sigStream << " ";
						}
						sigStream << paramStr;
					}

					sigStream << ")";
					info.signature = sigStream.str();

					// Build snippet with placeholders for input parameters
					std::ostringstream snippetStream;
					for (size_t j = 0; j < info.inputParams.size(); j++) {
						const std::string& param = info.inputParams[j];
						size_t colonPos = param.find(':');
						std::string paramName = (colonPos != std::string::npos) ? param.substr(0, colonPos) : param;

						snippetStream << "${" << (j + 1) << ":" << paramName << "}";
						if (j < info.inputParams.size() - 1) {
							snippetStream << " ";
						}
					}
					if (!info.inputParams.empty()) {
						snippetStream << " ";
					}
					snippetStream << info.name;

					info.snippet = snippetStream.str();

					functions.push_back(info);
				}
			}
		}

		return functions;
	}

	// Extract public structs from a module file
	std::vector<StructInfo> extractModuleStructs(const std::string& modulePath) {
		std::vector<StructInfo> structs;

		std::ifstream file(modulePath);
		if (!file.good()) {
			return structs;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string moduleText = buffer.str();

		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

		if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
			return structs;
		}

		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
				Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);

				// Only include public structs
				if (!structNode->isPublic()) {
					continue;
				}

				StructInfo info;
				info.name = structNode->name();

				// Build signature
				std::ostringstream sig;
				sig << "struct " << info.name << " { ";
				for (const auto* field : structNode->fields()) {
					const Qd::AstNodeStructField* structField = static_cast<const Qd::AstNodeStructField*>(field);
					info.fields.push_back({structField->name(), structField->typeName()});
					sig << structField->name() << ":" << structField->typeName() << " ";
				}
				sig << "}";
				info.signature = sig.str();

				structs.push_back(info);
			}
		}

		return structs;
	}

	// Extract public constants from a module file
	std::vector<ConstantInfo> extractModuleConstants(const std::string& modulePath) {
		std::vector<ConstantInfo> constants;

		std::ifstream file(modulePath);
		if (!file.good()) {
			return constants;
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string moduleText = buffer.str();

		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

		if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
			return constants;
		}

		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (child && child->type() == Qd::IAstNode::Type::CONSTANT_DECLARATION) {
				Qd::AstNodeConstant* constNode = static_cast<Qd::AstNodeConstant*>(child);

				// Only include public constants
				if (!constNode->isPublic()) {
					continue;
				}

				ConstantInfo info;
				info.name = constNode->name();
				info.value = constNode->value();

				constants.push_back(info);
			}
		}

		return constants;
	}

	void handleHover(const std::string& id, const std::string& uri, size_t line, size_t character) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			// Try to read from disk
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* result = json_null();

		if (!documentText.empty()) {
			std::string word = getWordAtPosition(documentText, line, character);

			if (!word.empty()) {
				// Check if it's a built-in instruction
				std::string doc = getBuiltInDocumentation(word);

				if (!doc.empty()) {
					// Create hover response with documentation
					result = json_object();
					json_t* contents = json_object();
					json_object_set_new(contents, "kind", json_string("markdown"));
					json_object_set_new(contents, "value", json_string(doc.c_str()));
					json_object_set_new(result, "contents", contents);
				} else {
					// Check if it's a user-defined function
					std::vector<FunctionInfo> functions = extractFunctions(documentText);
					for (const auto& func : functions) {
						if (func.name == word) {
							// Build documentation for user function
							std::ostringstream docStream;
							docStream << "**Function:** `" << func.signature << "`\n\n";
							if (!func.inputParams.empty()) {
								docStream << "**Inputs:** ";
								for (size_t i = 0; i < func.inputParams.size(); i++) {
									if (i > 0) {
										docStream << ", ";
									}
									docStream << "`" << func.inputParams[i] << "`";
								}
								docStream << "\n\n";
							}
							if (!func.outputParams.empty()) {
								docStream << "**Outputs:** ";
								for (size_t i = 0; i < func.outputParams.size(); i++) {
									if (i > 0) {
										docStream << ", ";
									}
									docStream << "`" << func.outputParams[i] << "`";
								}
							}

							result = json_object();
							json_t* contents = json_object();
							json_object_set_new(contents, "kind", json_string("markdown"));
							json_object_set_new(contents, "value", json_string(docStream.str().c_str()));
							json_object_set_new(result, "contents", contents);
							break;
						}
					}

					// Check if it's a scoped identifier (module::symbol)
					if (json_is_null(result) && word.find("::") != std::string::npos) {
						size_t colonPos = word.find("::");
						std::string moduleName = word.substr(0, colonPos);
						std::string symbolName = word.substr(colonPos + 2);

						// Get source directory from URI
						std::string filePath = uri.substr(7); // Remove "file://"
						std::string sourceDir = std::filesystem::path(filePath).parent_path().string();

						// Resolve module path
						std::string modulePath = resolveModulePath(moduleName, sourceDir);

						if (!modulePath.empty()) {
							// Read and parse the module file
							std::ifstream file(modulePath);
							if (file.good()) {
								std::stringstream buffer;
								buffer << file.rdbuf();
								std::string moduleText = buffer.str();

								Qd::Ast ast;
								Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

								if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
									// Search for the symbol
									for (size_t i = 0; i < root->childCount(); i++) {
										Qd::IAstNode* child = root->child(i);

										if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
											Qd::AstNodeFunctionDeclaration* funcNode =
													static_cast<Qd::AstNodeFunctionDeclaration*>(child);
											if (funcNode->name() == symbolName) {
												// Build function documentation
												std::ostringstream docStream;
												docStream << "**Function (from " << moduleName << "):** `fn "
														  << funcNode->name() << "(";

												// Input parameters
												const auto& inputs = funcNode->inputParameters();
												for (size_t j = 0; j < inputs.size(); j++) {
													if (j > 0) {
														docStream << " ";
													}
													Qd::AstNodeParameter* param =
															static_cast<Qd::AstNodeParameter*>(inputs[j]);
													docStream << param->name() << ":" << param->typeString();
												}

												docStream << " -- ";

												// Output parameters
												const auto& outputs = funcNode->outputParameters();
												for (size_t j = 0; j < outputs.size(); j++) {
													if (j > 0) {
														docStream << " ";
													}
													Qd::AstNodeParameter* param =
															static_cast<Qd::AstNodeParameter*>(outputs[j]);
													docStream << param->name() << ":" << param->typeString();
												}

												docStream << ")`";

												result = json_object();
												json_t* contents = json_object();
												json_object_set_new(contents, "kind", json_string("markdown"));
												json_object_set_new(
														contents, "value", json_string(docStream.str().c_str()));
												json_object_set_new(result, "contents", contents);
												break;
											}
										} else if (child && child->type() == Qd::IAstNode::Type::CONSTANT_DECLARATION) {
											Qd::AstNodeConstant* constNode = static_cast<Qd::AstNodeConstant*>(child);
											if (constNode->name() == symbolName) {
												// Build constant documentation
												std::ostringstream docStream;
												docStream << "**Constant (from " << moduleName << "):** `"
														  << constNode->name() << " = " << constNode->value() << "`";

												result = json_object();
												json_t* contents = json_object();
												json_object_set_new(contents, "kind", json_string("markdown"));
												json_object_set_new(
														contents, "value", json_string(docStream.str().c_str()));
												json_object_set_new(result, "contents", contents);
												break;
											}
										} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
											// Check for imported functions (like stdlib)
											Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
											const auto& importedFuncs = importNode->functions();
											for (const auto* importedFunc : importedFuncs) {
												if (importedFunc->name == symbolName) {
													// Build imported function documentation
													std::ostringstream docStream;
													docStream << "**Function (from " << moduleName << "):** `fn "
															  << importedFunc->name << "(";

													// Input parameters
													for (size_t j = 0; j < importedFunc->inputParameters.size(); j++) {
														if (j > 0) {
															docStream << " ";
														}
														const auto* param = importedFunc->inputParameters[j];
														docStream << param->name() << ":" << param->typeString();
													}

													docStream << " -- ";

													// Output parameters
													for (size_t j = 0; j < importedFunc->outputParameters.size(); j++) {
														if (j > 0) {
															docStream << " ";
														}
														const auto* param = importedFunc->outputParameters[j];
														docStream << param->name() << ":" << param->typeString();
													}

													docStream << ")`";

													result = json_object();
													json_t* contents = json_object();
													json_object_set_new(contents, "kind", json_string("markdown"));
													json_object_set_new(
															contents, "value", json_string(docStream.str().c_str()));
													json_object_set_new(result, "contents", contents);
													break;
												}
											}
											if (!json_is_null(result)) {
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		json_object_set_new(response, "result", result);
		sendMessage(response);
		json_decref(response);
	}

	void handleSignatureHelp(const std::string& id, const std::string& uri, size_t line, size_t character) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* result = json_null();

		if (!documentText.empty()) {
			// Get the current line up to cursor position
			std::istringstream stream(documentText);
			std::string currentLine;
			for (size_t i = 0; i <= line; i++) {
				if (!std::getline(stream, currentLine)) {
					currentLine.clear();
					break;
				}
			}

			if (!currentLine.empty() && character <= currentLine.length()) {
				// Get the text before cursor on current line
				std::string textBeforeCursor = currentLine.substr(0, character);

				// Find the last word (potential function name)
				// Look backwards for a function name
				std::string funcName;
				size_t pos = textBeforeCursor.length();

				// Skip trailing whitespace
				while (pos > 0 && std::isspace(static_cast<unsigned char>(textBeforeCursor[pos - 1]))) {
					pos--;
				}

				// Extract the word before cursor
				size_t wordEnd = pos;
				while (pos > 0 && !std::isspace(static_cast<unsigned char>(textBeforeCursor[pos - 1]))) {
					pos--;
				}

				if (wordEnd > pos) {
					funcName = textBeforeCursor.substr(pos, wordEnd - pos);
				}

				if (!funcName.empty()) {
					std::string signature;
					std::string documentation;

					// Check if it's a built-in instruction
					std::string doc = getBuiltInDocumentation(funcName);
					if (!doc.empty()) {
						// Extract signature from documentation (first line usually)
						size_t newlinePos = doc.find('\n');
						if (newlinePos != std::string::npos) {
							signature = doc.substr(0, newlinePos);
							documentation = doc;
						} else {
							signature = doc;
							documentation = doc;
						}
					} else {
						// Check user-defined functions
						std::vector<FunctionInfo> functions = extractFunctions(documentText);
						for (const auto& func : functions) {
							if (func.name == funcName) {
								signature = func.signature;
								std::ostringstream docStream;
								if (!func.inputParams.empty()) {
									docStream << "Inputs: ";
									for (size_t i = 0; i < func.inputParams.size(); i++) {
										if (i > 0) {
											docStream << ", ";
										}
										docStream << func.inputParams[i];
									}
								}
								if (!func.outputParams.empty()) {
									if (!func.inputParams.empty()) {
										docStream << " | ";
									}
									docStream << "Outputs: ";
									for (size_t i = 0; i < func.outputParams.size(); i++) {
										if (i > 0) {
											docStream << ", ";
										}
										docStream << func.outputParams[i];
									}
								}
								documentation = docStream.str();
								break;
							}
						}

						// Check for scoped identifier (module::function)
						if (signature.empty() && funcName.find("::") != std::string::npos) {
							size_t colonPos = funcName.find("::");
							std::string moduleName = funcName.substr(0, colonPos);
							std::string symbolName = funcName.substr(colonPos + 2);

							std::string filePath = uri.substr(7);
							std::string sourceDir = std::filesystem::path(filePath).parent_path().string();
							std::string modulePath = resolveModulePath(moduleName, sourceDir);

							if (!modulePath.empty()) {
								std::ifstream file(modulePath);
								if (file.good()) {
									std::stringstream buffer;
									buffer << file.rdbuf();
									std::string moduleText = buffer.str();

									Qd::Ast ast;
									Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

									if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
										for (size_t i = 0; i < root->childCount(); i++) {
											Qd::IAstNode* child = root->child(i);

											if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
												Qd::AstNodeFunctionDeclaration* funcNode =
														static_cast<Qd::AstNodeFunctionDeclaration*>(child);
												if (funcNode->name() == symbolName) {
													std::ostringstream sigStream;
													sigStream << "fn " << funcNode->name() << "(";

													const auto& inputs = funcNode->inputParameters();
													for (size_t j = 0; j < inputs.size(); j++) {
														if (j > 0) {
															sigStream << " ";
														}
														Qd::AstNodeParameter* param =
																static_cast<Qd::AstNodeParameter*>(inputs[j]);
														sigStream << param->name() << ":" << param->typeString();
													}

													sigStream << " -- ";

													const auto& outputs = funcNode->outputParameters();
													for (size_t j = 0; j < outputs.size(); j++) {
														if (j > 0) {
															sigStream << " ";
														}
														Qd::AstNodeParameter* param =
																static_cast<Qd::AstNodeParameter*>(outputs[j]);
														sigStream << param->name() << ":" << param->typeString();
													}

													sigStream << ")";
													signature = sigStream.str();
													documentation = "From module: " + moduleName;
													break;
												}
											} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
												Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
												for (const auto* importedFunc : importNode->functions()) {
													if (importedFunc->name == symbolName) {
														std::ostringstream sigStream;
														sigStream << "fn " << importedFunc->name << "(";

														for (size_t j = 0; j < importedFunc->inputParameters.size();
																j++) {
															if (j > 0) {
																sigStream << " ";
															}
															const auto* param = importedFunc->inputParameters[j];
															sigStream << param->name() << ":" << param->typeString();
														}

														sigStream << " -- ";

														for (size_t j = 0; j < importedFunc->outputParameters.size();
																j++) {
															if (j > 0) {
																sigStream << " ";
															}
															const auto* param = importedFunc->outputParameters[j];
															sigStream << param->name() << ":" << param->typeString();
														}

														sigStream << ")";
														signature = sigStream.str();
														documentation = "From module: " + moduleName;
														break;
													}
												}
												if (!signature.empty()) {
													break;
												}
											}
										}
									}
								}
							}
						}
					}

					if (!signature.empty()) {
						result = json_object();

						json_t* signatures = json_array();
						json_t* sig = json_object();
						json_object_set_new(sig, "label", json_string(signature.c_str()));
						if (!documentation.empty()) {
							json_object_set_new(sig, "documentation", json_string(documentation.c_str()));
						}
						json_array_append_new(signatures, sig);

						json_object_set_new(result, "signatures", signatures);
						json_object_set_new(result, "activeSignature", json_integer(0));
						json_object_set_new(result, "activeParameter", json_null());
					}
				}
			}
		}

		json_object_set_new(response, "result", result);
		sendMessage(response);
		json_decref(response);
	}

	void handleDocumentSymbols(const std::string& id, const std::string& uri) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			// Try to read from disk
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* symbols = json_array();

		if (!documentText.empty()) {
			// Parse the document
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
				// Iterate through program children looking for functions and imports
				for (size_t i = 0; i < root->childCount(); i++) {
					Qd::IAstNode* child = root->child(i);

					if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
						Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

						json_t* symbol = json_object();
						json_object_set_new(symbol, "name", json_string(funcNode->name().c_str()));
						json_object_set_new(symbol, "kind", json_integer(12)); // Function kind

						// Build detail string with signature
						std::ostringstream detailStream;
						detailStream << "fn " << funcNode->name() << "(";

						const auto& inputs = funcNode->inputParameters();
						for (size_t j = 0; j < inputs.size(); j++) {
							Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(inputs[j]);
							if (j > 0) {
								detailStream << " ";
							}
							detailStream << param->name() << ":" << param->typeString();
						}

						detailStream << " -- ";

						const auto& outputs = funcNode->outputParameters();
						for (size_t j = 0; j < outputs.size(); j++) {
							Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(outputs[j]);
							if (j > 0) {
								detailStream << " ";
							}
							detailStream << param->name() << ":" << param->typeString();
						}

						detailStream << ")";
						json_object_set_new(symbol, "detail", json_string(detailStream.str().c_str()));

						// Add range (line is 1-based in AST, LSP uses 0-based)
						json_t* range = json_object();
						json_t* start = json_object();
						size_t lspLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(start, "character", json_integer(0));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(funcNode->name().length())));
						json_object_set_new(range, "end", end);

						json_object_set_new(symbol, "range", range);
						json_object_set_new(symbol, "selectionRange", json_deep_copy(range));

						json_array_append_new(symbols, symbol);
					} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
						Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
						std::string namespaceName = importNode->namespaceName();

						// Add imported functions as symbols
						const auto& importedFuncs = importNode->functions();
						for (const auto* importedFunc : importedFuncs) {
							json_t* symbol = json_object();
							std::string fullName = namespaceName + "::" + importedFunc->name;
							json_object_set_new(symbol, "name", json_string(fullName.c_str()));
							json_object_set_new(symbol, "kind", json_integer(12)); // Function kind

							// Build detail string
							std::ostringstream detailStream;
							detailStream << "fn " << importedFunc->name << "(";

							const auto& inputs = importedFunc->inputParameters;
							for (size_t j = 0; j < inputs.size(); j++) {
								if (j > 0) {
									detailStream << " ";
								}
								detailStream << inputs[j]->name() << ":" << inputs[j]->typeString();
							}

							detailStream << " -- ";

							const auto& outputs = importedFunc->outputParameters;
							for (size_t j = 0; j < outputs.size(); j++) {
								if (j > 0) {
									detailStream << " ";
								}
								detailStream << outputs[j]->name() << ":" << outputs[j]->typeString();
							}

							detailStream << ") [imported from " << importNode->library() << "]";
							json_object_set_new(symbol, "detail", json_string(detailStream.str().c_str()));

							// Add range
							json_t* range = json_object();
							json_t* start = json_object();
							size_t lspLine = (importedFunc->line > 0) ? importedFunc->line - 1 : 0;
							json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(start, "character", json_integer(0));
							json_object_set_new(range, "start", start);

							json_t* end = json_object();
							json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
							json_object_set_new(end, "character",
									json_integer(static_cast<json_int_t>(importedFunc->name.length())));
							json_object_set_new(range, "end", end);

							json_object_set_new(symbol, "range", range);
							json_object_set_new(symbol, "selectionRange", json_deep_copy(range));

							json_array_append_new(symbols, symbol);
						}
					} else if (child && child->type() == Qd::IAstNode::Type::TEST_DECLARATION) {
						Qd::AstNodeTest* testNode = static_cast<Qd::AstNodeTest*>(child);

						json_t* symbol = json_object();
						json_object_set_new(symbol, "name", json_string(testNode->name().c_str()));
						json_object_set_new(symbol, "kind", json_integer(12)); // Function kind

						// Build detail string
						std::string detail = "test \"" + testNode->name() + "\"";
						json_object_set_new(symbol, "detail", json_string(detail.c_str()));

						// Add range (line is 1-based in AST, LSP uses 0-based)
						json_t* range = json_object();
						json_t* start = json_object();
						size_t lspLine = (testNode->line() > 0) ? testNode->line() - 1 : 0;
						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(start, "character", json_integer(0));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(testNode->name().length())));
						json_object_set_new(range, "end", end);

						json_object_set_new(symbol, "range", range);
						json_object_set_new(symbol, "selectionRange", json_deep_copy(range));

						json_array_append_new(symbols, symbol);
					}
				}
			}
		}

		json_object_set_new(response, "result", symbols);
		sendMessage(response);
		json_decref(response);
	}

	// Helper function to recursively find all identifiers in AST
	void findIdentifiersInNode(Qd::IAstNode* node, const std::string& targetName, std::vector<Qd::IAstNode*>& results) {
		if (!node) {
			return;
		}

		// Check if this node is a function declaration matching our target
		if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcDecl = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
			if (funcDecl->name() == targetName) {
				results.push_back(node);
			}
		}
		// Check if this node is a local variable declaration (-> varname)
		else if (node->type() == Qd::IAstNode::Type::LOCAL) {
			Qd::AstNodeLocal* local = static_cast<Qd::AstNodeLocal*>(node);
			// Check all names in the local (supports -> a b c syntax)
			for (const std::string& name : local->names()) {
				if (name == targetName) {
					results.push_back(node);
					break;
				}
			}
		}
		// Check if this node is an identifier matching our target
		else if (node->type() == Qd::IAstNode::Type::IDENTIFIER) {
			Qd::AstNodeIdentifier* ident = static_cast<Qd::AstNodeIdentifier*>(node);
			if (ident->name() == targetName) {
				results.push_back(node);
			}
		} else if (node->type() == Qd::IAstNode::Type::SCOPED_IDENTIFIER) {
			Qd::AstNodeScopedIdentifier* scoped = static_cast<Qd::AstNodeScopedIdentifier*>(node);
			std::string fullName = scoped->scope() + "::" + scoped->name();
			if (fullName == targetName || scoped->name() == targetName) {
				results.push_back(node);
			}
		}

		// Recursively search children
		for (size_t i = 0; i < node->childCount(); i++) {
			findIdentifiersInNode(node->child(i), targetName, results);
		}
	}

	// Find a local variable declaration by searching up the AST tree from a given node
	Qd::AstNodeLocal* findLocalDeclaration(Qd::IAstNode* startNode, const std::string& varName, size_t requestLine) {
		if (!startNode) {
			return nullptr;
		}

		// Walk up to find the containing function
		Qd::IAstNode* current = startNode;
		Qd::IAstNode* functionNode = nullptr;

		while (current) {
			if (current->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				functionNode = current;
				break;
			}
			current = current->parent();
		}

		if (!functionNode) {
			return nullptr;
		}

		// Recursively search for local declarations in the function
		// We need to find declarations that appear before the request line
		std::vector<Qd::AstNodeLocal*> candidates;
		std::function<void(Qd::IAstNode*)> searchLocals = [&](Qd::IAstNode* node) {
			if (!node) {
				return;
			}

			if (node->type() == Qd::IAstNode::Type::LOCAL) {
				Qd::AstNodeLocal* localNode = static_cast<Qd::AstNodeLocal*>(node);
				// Check all names (supports multiple assignment: -> a b c)
				const auto& names = localNode->names();
				for (const auto& name : names) {
					if (name == varName) {
						// Only consider declarations that appear before the request line
						size_t declLine = (localNode->line() > 0) ? localNode->line() - 1 : 0;
						if (declLine <= requestLine) {
							candidates.push_back(localNode);
						}
						break; // Found match, no need to check more names
					}
				}
			}

			// Recursively search children
			for (size_t i = 0; i < node->childCount(); i++) {
				searchLocals(node->child(i));
			}
		};

		searchLocals(functionNode);

		// Return the last declaration before the request line (closest scope)
		if (!candidates.empty()) {
			return candidates.back();
		}

		return nullptr;
	}

	// Get packages directory path (where quadpm installs packages)
	std::string getPackagesDir() {
		// Check QUADRATE_PATH environment variable first
		const char* quadratePath = getenv("QUADRATE_PATH");
		if (quadratePath) {
			return std::string(quadratePath);
		}

		// Check if XDG_DATA_HOME is set
		const char* xdgDataHome = getenv("XDG_DATA_HOME");
		if (xdgDataHome) {
			return std::string(xdgDataHome) + "/quadrate/packages";
		}

		// Default to ~/quadrate/packages
		const char* home = getenv("HOME");
		if (home) {
			return std::string(home) + "/quadrate/packages";
		}

		return "";
	}

	// Find a package in the packages directory
	std::string findLatestPackageVersion(const std::string& moduleName) {
		std::string packagesDir = getPackagesDir();
		if (packagesDir.empty() || !std::filesystem::exists(packagesDir)) {
			return "";
		}

		std::string latestPath;
		try {
			for (const auto& entry : std::filesystem::directory_iterator(packagesDir)) {
				if (!entry.is_directory()) {
					continue;
				}

				std::string dirName = entry.path().filename().string();
				std::string prefix = moduleName + "@";
				if (dirName.size() > prefix.size() && dirName.substr(0, prefix.size()) == prefix) {
					latestPath = entry.path().string();
				}
			}
		} catch (...) {
			return "";
		}

		return latestPath;
	}

	// Resolve module name to file path using the same logic as quadc
	std::string resolveModulePath(const std::string& moduleName, const std::string& sourceDir) {
		// Try 1: Local path (relative to source file)
		std::string localPath = sourceDir + "/" + moduleName + "/module.qd";
		if (std::filesystem::exists(localPath)) {
			return localPath;
		}

		// Try 2: Third-party packages directory (installed via quadpm)
		std::string packagePath = findLatestPackageVersion(moduleName);
		if (!packagePath.empty()) {
			std::string moduleFile = packagePath + "/module.qd";
			if (std::filesystem::exists(moduleFile)) {
				return moduleFile;
			}
		}

		// Try 3: QUADRATE_ROOT environment variable
		const char* quadrateRoot = getenv("QUADRATE_ROOT");
		if (quadrateRoot) {
			std::string rootPath = std::string(quadrateRoot) + "/" + moduleName + "/module.qd";
			if (std::filesystem::exists(rootPath)) {
				return rootPath;
			}
		}

		// Try 4: Installed standard library (/usr/share/quadrate/)
		std::string installedPath = "/usr/share/quadrate/" + moduleName + "/module.qd";
		if (std::filesystem::exists(installedPath)) {
			return installedPath;
		}

		// Try 5: Standard library directories relative to current directory (for development)
		std::string stdLibPath = "lib/std" + moduleName + "qd/qd/" + moduleName + "/module.qd";
		if (std::filesystem::exists(stdLibPath)) {
			return stdLibPath;
		}

		// Try 6: $HOME/quadrate directory
		const char* home = getenv("HOME");
		if (home) {
			std::string homePath = std::string(home) + "/quadrate/" + moduleName + "/module.qd";
			if (std::filesystem::exists(homePath)) {
				return homePath;
			}
		}

		return "";
	}

	// Find a function or constant definition in an external module file
	// Returns a JSON location object if found, or json_null() if not found
	json_t* findDefinitionInModule(
			const std::string& modulePath, const std::string& symbolName, const std::string& symbolType) {
		// Read the module file
		std::ifstream file(modulePath);
		if (!file.good()) {
			return json_null();
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string moduleText = buffer.str();

		// Parse the module file
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(moduleText.c_str(), false, nullptr);

		if (!root || ast.hasErrors() || root->type() != Qd::IAstNode::Type::PROGRAM) {
			return json_null();
		}

		// Search for the definition
		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (symbolType == "function" && child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
				if (funcNode->name() == symbolName) {
					// Found the function definition
					json_t* location = json_object();
					std::string moduleUri = "file://" + modulePath;
					json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(0));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(funcNode->name().length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(location, "range", range);
					return location;
				}
			} else if (symbolType == "constant" && child && child->type() == Qd::IAstNode::Type::CONSTANT_DECLARATION) {
				Qd::AstNodeConstant* constNode = static_cast<Qd::AstNodeConstant*>(child);
				if (constNode->name() == symbolName) {
					// Found the constant definition
					json_t* location = json_object();
					std::string moduleUri = "file://" + modulePath;
					json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (constNode->line() > 0) ? constNode->line() - 1 : 0;
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(0));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(constNode->name().length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(location, "range", range);
					return location;
				}
			} else if (symbolType == "struct" && child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
				Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);
				if (structNode->name() == symbolName) {
					// Found the struct definition
					json_t* location = json_object();
					std::string moduleUri = "file://" + modulePath;
					json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (structNode->line() > 0) ? structNode->line() - 1 : 0;
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(0));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(structNode->name().length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(location, "range", range);
					return location;
				}
			} else if (symbolType == "function" && child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
				// Check for imported functions (like those in stdlib modules)
				Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
				const auto& importedFuncs = importNode->functions();
				for (const auto* importedFunc : importedFuncs) {
					if (importedFunc->name == symbolName) {
						// Found the imported function declaration
						json_t* location = json_object();
						std::string moduleUri = "file://" + modulePath;
						json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

						json_t* range = json_object();
						json_t* start = json_object();
						size_t lspLine = (importedFunc->line > 0) ? importedFunc->line - 1 : 0;
						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(start, "character", json_integer(0));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(importedFunc->name.length())));
						json_object_set_new(range, "end", end);

						json_object_set_new(location, "range", range);
						return location;
					}
				}
			}
		}

		return json_null();
	}

	// Helper: Find struct type of a local variable by analyzing the function body
	std::string findStructTypeOfVariable(Qd::IAstNode* root, const std::string& varName, size_t requestLine) {
		// Find the function containing the requested line
		Qd::AstNodeFunctionDeclaration* functionNode = nullptr;
		std::function<void(Qd::IAstNode*)> searchFunction = [&](Qd::IAstNode* node) {
			if (!node) {
				return;
			}

			if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
				// Check if the requested line is within this function
				size_t funcStartLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
				// Simple heuristic: if the function starts before or at the request line, it might contain it
				if (funcStartLine <= requestLine) {
					functionNode = funcNode;
				}
			}

			for (size_t i = 0; i < node->childCount(); i++) {
				searchFunction(node->child(i));
			}
		};

		searchFunction(root);

		if (!functionNode) {
			return "";
		}

		// Find the local declaration and look for a preceding struct constructor
		// Pattern: ... StructName -> varName  OR  ... module::StructName -> varName
		std::string structType;
		std::function<void(Qd::IAstNode*)> searchConstructor = [&](Qd::IAstNode* node) {
			if (!node || !structType.empty()) {
				return;
			}

			// If this is a block, iterate through children sequentially
			if (node->type() == Qd::IAstNode::Type::BLOCK) {
				for (size_t i = 0; i < node->childCount(); i++) {
					Qd::IAstNode* child = node->child(i);
					if (!child) {
						continue;
					}

					// Look for LOCAL node with matching name
					if (child->type() == Qd::IAstNode::Type::LOCAL) {
						Qd::AstNodeLocal* localNode = static_cast<Qd::AstNodeLocal*>(child);
						// Check all names (supports multiple assignment: -> a b c)
						const auto& localNames = localNode->names();
						bool hasMatch = false;
						for (const auto& name : localNames) {
							if (name == varName) {
								hasMatch = true;
								break;
							}
						}
						if (hasMatch) {
							// Found the declaration - now look backwards for struct constructor
							// Check previous sibling
							if (i > 0) {
								Qd::IAstNode* prevSibling = node->child(i - 1);
								if (prevSibling) {
									// Check if it's a scoped identifier (module::Struct)
									if (prevSibling->type() == Qd::IAstNode::Type::SCOPED_IDENTIFIER) {
										Qd::AstNodeScopedIdentifier* scopedNode =
												static_cast<Qd::AstNodeScopedIdentifier*>(prevSibling);
										structType = scopedNode->scope() + "::" + scopedNode->name();
										return;
									}
									// Check if it's a plain identifier (Struct)
									else if (prevSibling->type() == Qd::IAstNode::Type::IDENTIFIER) {
										Qd::AstNodeIdentifier* identNode =
												static_cast<Qd::AstNodeIdentifier*>(prevSibling);
										structType = identNode->name();
										return;
									}
								}
							}
							return;
						}
					}
				}
			}

			// Recursively search children
			for (size_t i = 0; i < node->childCount(); i++) {
				searchConstructor(node->child(i));
			}
		};

		searchConstructor(functionNode);
		return structType;
	}

	// Helper function to handle Go to Definition for field access expressions (v@x)
	json_t* handleFieldAccessDefinition(
			Qd::IAstNode* root, const std::string& uri, size_t line, bool cursorOnVariable) {
		// Find the field access node at the target line
		Qd::AstNodeFieldAccess* fieldAccessNode = nullptr;
		std::function<void(Qd::IAstNode*)> searchFieldAccess = [&](Qd::IAstNode* node) {
			if (!node) {
				return;
			}

			if (node->type() == Qd::IAstNode::Type::FIELD_ACCESS) {
				Qd::AstNodeFieldAccess* faNode = static_cast<Qd::AstNodeFieldAccess*>(node);
				// Check if this field access is on the target line
				size_t nodeLine = (faNode->line() > 0) ? faNode->line() - 1 : 0;
				if (nodeLine == line) {
					fieldAccessNode = faNode;
					return;
				}
			}

			// Recursively search children
			for (size_t i = 0; i < node->childCount(); i++) {
				searchFieldAccess(node->child(i));
				if (fieldAccessNode) {
					return;
				}
			}
		};

		searchFieldAccess(root);

		if (!fieldAccessNode) {
			return json_null();
		}

		if (cursorOnVariable) {
			// User clicked on the variable name - find the local variable declaration
			std::string varName = fieldAccessNode->varName();
			Qd::AstNodeLocal* localNode = findLocalDeclaration(fieldAccessNode, varName, line);

			if (localNode) {
				// Found the local variable declaration
				json_t* location = json_object();
				json_object_set_new(location, "uri", json_string(uri.c_str()));

				json_t* range = json_object();
				json_t* start = json_object();
				size_t lspLine = (localNode->line() > 0) ? localNode->line() - 1 : 0;
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(start, "character", json_integer(0));
				json_object_set_new(range, "start", start);

				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
				json_object_set_new(
						end, "character", json_integer(static_cast<json_int_t>(localNode->name().length())));
				json_object_set_new(range, "end", end);

				json_object_set_new(location, "range", range);
				return location;
			}
		} else {
			// User clicked on the field name - find the struct field definition
			std::string varName = fieldAccessNode->varName();
			std::string fieldName = fieldAccessNode->fieldName();

			// Find the struct type of this variable
			std::string structType = findStructTypeOfVariable(root, varName, line);
			if (structType.empty()) {
				return json_null();
			}

			// Now find the struct declaration
			// First check if it's a scoped struct (module::StructName)
			size_t colonPos = structType.find("::");
			if (colonPos != std::string::npos) {
				// This is a module struct
				std::string moduleName = structType.substr(0, colonPos);
				std::string structName = structType.substr(colonPos + 2);

				// Get source directory from the current document URI
				std::string sourceDir;
				if (uri.substr(0, 7) == "file://") {
					std::string filePath = uri.substr(7);
					size_t lastSlash = filePath.find_last_of('/');
					if (lastSlash != std::string::npos) {
						sourceDir = filePath.substr(0, lastSlash);
					}
				}

				// Resolve module path
				std::string modulePath = resolveModulePath(moduleName, sourceDir);
				if (!modulePath.empty()) {
					// Parse the module file
					std::ifstream moduleFile(modulePath);
					if (moduleFile.good()) {
						std::stringstream buffer;
						buffer << moduleFile.rdbuf();
						std::string moduleContent = buffer.str();

						Qd::Ast moduleAst;
						Qd::IAstNode* moduleRoot = moduleAst.generate(moduleContent.c_str(), false, nullptr);

						if (moduleRoot && !moduleAst.hasErrors()) {
							// Find the struct in the module
							for (size_t i = 0; i < moduleRoot->childCount(); i++) {
								Qd::IAstNode* child = moduleRoot->child(i);
								if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
									Qd::AstNodeStructDeclaration* structNode =
											static_cast<Qd::AstNodeStructDeclaration*>(child);
									if (structNode->name() == structName) {
										// Found the struct - now find the field
										const auto& fields = structNode->fields();
										for (const auto* field : fields) {
											if (field->name() == fieldName) {
												// Found the field!
												json_t* location = json_object();
												std::string moduleUri = "file://" + modulePath;
												json_object_set_new(location, "uri", json_string(moduleUri.c_str()));

												json_t* range = json_object();
												json_t* start_json = json_object();
												size_t lspLine = (field->line() > 0) ? field->line() - 1 : 0;
												json_object_set_new(start_json, "line",
														json_integer(static_cast<json_int_t>(lspLine)));
												json_object_set_new(start_json, "character", json_integer(0));
												json_object_set_new(range, "start", start_json);

												json_t* end = json_object();
												json_object_set_new(
														end, "line", json_integer(static_cast<json_int_t>(lspLine)));
												json_object_set_new(end, "character",
														json_integer(static_cast<json_int_t>(field->name().length())));
												json_object_set_new(range, "end", end);

												json_object_set_new(location, "range", range);
												return location;
											}
										}
									}
								}
							}
						}
					}
				}
			} else {
				// This is a local struct in the same file
				for (size_t i = 0; i < root->childCount(); i++) {
					Qd::IAstNode* child = root->child(i);
					if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
						Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);
						if (structNode->name() == structType) {
							// Found the struct - now find the field
							const auto& fields = structNode->fields();
							for (const auto* field : fields) {
								if (field->name() == fieldName) {
									// Found the field!
									json_t* location = json_object();
									json_object_set_new(location, "uri", json_string(uri.c_str()));

									json_t* range = json_object();
									json_t* start_json = json_object();
									size_t lspLine = (field->line() > 0) ? field->line() - 1 : 0;
									json_object_set_new(
											start_json, "line", json_integer(static_cast<json_int_t>(lspLine)));
									json_object_set_new(start_json, "character", json_integer(0));
									json_object_set_new(range, "start", start_json);

									json_t* end = json_object();
									json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
									json_object_set_new(end, "character",
											json_integer(static_cast<json_int_t>(field->name().length())));
									json_object_set_new(range, "end", end);

									json_object_set_new(location, "range", range);
									return location;
								}
							}
						}
					}
				}
			}
		}

		return json_null();
	}

	void handleDefinition(const std::string& id, const std::string& uri, size_t line, size_t character) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			// Try to read from disk
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* result = json_null();

		if (!documentText.empty()) {
			// First check if we're on an import statement line (use <module>)
			// This is done before AST parsing to work even with parse errors
			std::vector<std::string> docLines;
			{
				std::istringstream lineStream(documentText);
				std::string docLine;
				while (std::getline(lineStream, docLine)) {
					docLines.push_back(docLine);
				}
			}
			if (line < docLines.size()) {
				const std::string& targetLine = docLines[line];
				// Check if line starts with "use "
				size_t usePos = targetLine.find("use ");
				if (usePos != std::string::npos && usePos < 10) { // Allow some leading whitespace
					// Extract module name after "use "
					size_t moduleStart = usePos + 4;
					while (moduleStart < targetLine.length() && isspace(targetLine[moduleStart])) {
						moduleStart++;
					}
					size_t moduleEnd = moduleStart;
					while (moduleEnd < targetLine.length() &&
							(isalnum(targetLine[moduleEnd]) || targetLine[moduleEnd] == '_')) {
						moduleEnd++;
					}
					// Only navigate if cursor is on the module name, not on "use" keyword
					if (moduleEnd > moduleStart && character >= moduleStart && character < moduleEnd) {
						std::string moduleName = targetLine.substr(moduleStart, moduleEnd - moduleStart);
						std::string sourceDir = std::filesystem::path(uri.substr(7)).parent_path().string();
						std::string modulePath = resolveModulePath(moduleName, sourceDir);

						if (!modulePath.empty()) {
							json_t* location = json_object();
							json_object_set_new(location, "uri", json_string(("file://" + modulePath).c_str()));

							json_t* range = json_object();
							json_t* start = json_object();
							json_object_set_new(start, "line", json_integer(0));
							json_object_set_new(start, "character", json_integer(0));
							json_object_set_new(range, "start", start);

							json_t* end = json_object();
							json_object_set_new(end, "line", json_integer(0));
							json_object_set_new(end, "character", json_integer(0));
							json_object_set_new(range, "end", end);

							json_object_set_new(location, "range", range);
							result = location;

							json_object_set_new(response, "result", result);
							sendMessage(response);
							json_decref(response);
							return;
						}
					}
				}
			}

			std::string word = getWordAtPosition(documentText, line, character);

			if (!word.empty()) {
				// Parse the document
				Qd::Ast ast;
				Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

				if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
					// Check if we're in a field access expression (v@x)
					// Find the character at the cursor position to see if @ is nearby
					std::vector<std::string> lines;
					std::istringstream stream(documentText);
					std::string currentLine;
					while (std::getline(stream, currentLine)) {
						lines.push_back(currentLine);
					}

					if (line < lines.size()) {
						const std::string& targetLine = lines[line];
						// Look for @ before or after the cursor
						bool inFieldAccess = false;
						bool cursorOnVariable = false;

						// Search for @ near the cursor
						for (size_t i = 0; i < targetLine.length(); i++) {
							if (targetLine[i] == '@') {
								// Check if cursor is near this @
								if (i >= character) {
									// @ is at or after cursor - cursor might be on variable
									if (i - character <= word.length()) {
										inFieldAccess = true;
										cursorOnVariable = true;
										break;
									}
								} else {
									// @ is before cursor - cursor might be on field name
									if (character - i <= word.length() + 1) {
										inFieldAccess = true;
										cursorOnVariable = false;
										break;
									}
								}
							}
						}

						if (inFieldAccess) {
							// Find the field access node at this location
							result = handleFieldAccessDefinition(root, uri, line, cursorOnVariable);
							if (!json_is_null(result)) {
								json_object_set_new(response, "result", result);
								sendMessage(response);
								json_decref(response);
								return;
							}
						}
					}

					// Search for function or struct declaration matching the word
					for (size_t i = 0; i < root->childCount(); i++) {
						Qd::IAstNode* child = root->child(i);

						if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
							Qd::AstNodeFunctionDeclaration* funcNode =
									static_cast<Qd::AstNodeFunctionDeclaration*>(child);

							if (funcNode->name() == word) {
								// Found the definition
								json_t* location = json_object();
								json_object_set_new(location, "uri", json_string(uri.c_str()));

								json_t* range = json_object();
								json_t* start = json_object();
								size_t lspLine = (funcNode->line() > 0) ? funcNode->line() - 1 : 0;
								json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(start, "character", json_integer(0));
								json_object_set_new(range, "start", start);

								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(end, "character",
										json_integer(static_cast<json_int_t>(funcNode->name().length())));
								json_object_set_new(range, "end", end);

								json_object_set_new(location, "range", range);
								result = location;
								break;
							}
						} else if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
							Qd::AstNodeStructDeclaration* structNode =
									static_cast<Qd::AstNodeStructDeclaration*>(child);

							if (structNode->name() == word) {
								// Found the struct definition
								json_t* location = json_object();
								json_object_set_new(location, "uri", json_string(uri.c_str()));

								json_t* range = json_object();
								json_t* start = json_object();
								size_t lspLine = (structNode->line() > 0) ? structNode->line() - 1 : 0;
								json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(start, "character", json_integer(0));
								json_object_set_new(range, "start", start);

								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(end, "character",
										json_integer(static_cast<json_int_t>(structNode->name().length())));
								json_object_set_new(range, "end", end);

								json_object_set_new(location, "range", range);
								result = location;
								break;
							}
						} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
							// Check imported functions
							Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
							std::string namespaceName = importNode->namespaceName();

							const auto& importedFuncs = importNode->functions();
							for (const auto* importedFunc : importedFuncs) {
								std::string fullName = namespaceName + "::" + importedFunc->name;
								if (fullName == word || importedFunc->name == word) {
									// Found the imported function declaration
									json_t* location = json_object();
									json_object_set_new(location, "uri", json_string(uri.c_str()));

									json_t* range = json_object();
									json_t* start = json_object();
									size_t lspLine = (importedFunc->line > 0) ? importedFunc->line - 1 : 0;
									json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
									json_object_set_new(start, "character", json_integer(0));
									json_object_set_new(range, "start", start);

									json_t* end = json_object();
									json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
									json_object_set_new(end, "character",
											json_integer(static_cast<json_int_t>(importedFunc->name.length())));
									json_object_set_new(range, "end", end);

									json_object_set_new(location, "range", range);
									result = location;
									break;
								}
							}
							if (!json_is_null(result)) {
								break;
							}
						}
					}

					// If no function or import found, try searching for local variable declarations
					if (json_is_null(result)) {
						// Find the identifier node at the cursor position
						std::vector<Qd::IAstNode*> identifiers;
						findIdentifiersInNode(root, word, identifiers);

						// Find the identifier at the requested line
						Qd::IAstNode* targetIdentifier = nullptr;
						for (auto* node : identifiers) {
							size_t nodeLine = (node->line() > 0) ? node->line() - 1 : 0;
							if (nodeLine == line && node->type() == Qd::IAstNode::Type::IDENTIFIER) {
								targetIdentifier = node;
								break;
							}
						}

						if (targetIdentifier) {
							// Search for the local variable declaration
							Qd::AstNodeLocal* localDecl = findLocalDeclaration(targetIdentifier, word, line);

							if (localDecl) {
								// Found the local variable declaration
								json_t* location = json_object();
								json_object_set_new(location, "uri", json_string(uri.c_str()));

								json_t* range = json_object();
								json_t* start = json_object();
								size_t lspLine = (localDecl->line() > 0) ? localDecl->line() - 1 : 0;
								json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(start, "character", json_integer(0));
								json_object_set_new(range, "start", start);

								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
								json_object_set_new(end, "character",
										json_integer(static_cast<json_int_t>(localDecl->name().length())));
								json_object_set_new(range, "end", end);

								json_object_set_new(location, "range", range);
								result = location;
							}
						}
					}

					// If still not found, try scoped identifiers (module::symbol)
					if (json_is_null(result) && word.find("::") != std::string::npos) {
						// Extract module name and symbol name
						size_t colonPos = word.find("::");
						std::string moduleName = word.substr(0, colonPos);
						std::string symbolName = word.substr(colonPos + 2);

						// Get source directory from URI
						std::string filePath = uri.substr(7); // Remove "file://"
						std::string sourceDir = std::filesystem::path(filePath).parent_path().string();

						// Resolve module path
						std::string modulePath = resolveModulePath(moduleName, sourceDir);

						if (!modulePath.empty()) {
							// Try to find the symbol as a function first, then as a constant, then as a struct
							result = findDefinitionInModule(modulePath, symbolName, "function");
							if (json_is_null(result)) {
								result = findDefinitionInModule(modulePath, symbolName, "constant");
							}
							if (json_is_null(result)) {
								result = findDefinitionInModule(modulePath, symbolName, "struct");
							}
						}
					}
				}
			}
		}

		json_object_set_new(response, "result", result);
		sendMessage(response);
		json_decref(response);
	}

	void handleFoldingRange(const std::string& id, const std::string& uri) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* ranges = json_array();

		if (!documentText.empty()) {
			// Parse the document
			Qd::Ast ast;
			Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

			if (root && !ast.hasErrors() && root->type() == Qd::IAstNode::Type::PROGRAM) {
				// Helper lambda to recursively find folding ranges
				std::function<void(Qd::IAstNode*)> findFoldingRanges = [&](Qd::IAstNode* node) {
					if (!node) {
						return;
					}

					size_t startLine = 0;
					size_t endLine = 0;
					std::string kind;

					// Check for foldable constructs
					if (node->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
						Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(node);
						startLine = funcNode->line() > 0 ? funcNode->line() - 1 : 0;

						// Find the end line by looking at the last instruction
						if (funcNode->childCount() > 0) {
							Qd::IAstNode* lastChild = funcNode->child(funcNode->childCount() - 1);
							if (lastChild) {
								endLine = lastChild->line() > 0 ? lastChild->line() - 1 : startLine;
							}
						}
						// If no children, just fold the declaration line
						if (endLine <= startLine) {
							endLine = startLine;
						}
						kind = "region";
					} else if (node->type() == Qd::IAstNode::Type::IF_STATEMENT) {
						startLine = node->line() > 0 ? node->line() - 1 : 0;
						// Find end by traversing children
						std::function<size_t(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) -> size_t {
							if (!n) {
								return 0;
							}
							size_t maxLine = n->line() > 0 ? n->line() - 1 : 0;
							for (size_t i = 0; i < n->childCount(); i++) {
								size_t childMax = findMaxLine(n->child(i));
								if (childMax > maxLine) {
									maxLine = childMax;
								}
							}
							return maxLine;
						};
						endLine = findMaxLine(node);
						kind = "region";
					} else if (node->type() == Qd::IAstNode::Type::WHILE_STATEMENT ||
							   node->type() == Qd::IAstNode::Type::FOR_STATEMENT ||
							   node->type() == Qd::IAstNode::Type::LOOP_STATEMENT) {
						startLine = node->line() > 0 ? node->line() - 1 : 0;
						std::function<size_t(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) -> size_t {
							if (!n) {
								return 0;
							}
							size_t maxLine = n->line() > 0 ? n->line() - 1 : 0;
							for (size_t i = 0; i < n->childCount(); i++) {
								size_t childMax = findMaxLine(n->child(i));
								if (childMax > maxLine) {
									maxLine = childMax;
								}
							}
							return maxLine;
						};
						endLine = findMaxLine(node);
						kind = "region";
					} else if (node->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
						// Import blocks can be folded
						startLine = node->line() > 0 ? node->line() - 1 : 0;
						// For imports, we'd need to find the closing brace
						// For now, just don't fold single-line imports
						kind = "imports";
					} else if (node->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
						startLine = node->line() > 0 ? node->line() - 1 : 0;
						std::function<size_t(Qd::IAstNode*)> findMaxLine = [&](Qd::IAstNode* n) -> size_t {
							if (!n) {
								return 0;
							}
							size_t maxLine = n->line() > 0 ? n->line() - 1 : 0;
							for (size_t i = 0; i < n->childCount(); i++) {
								size_t childMax = findMaxLine(n->child(i));
								if (childMax > maxLine) {
									maxLine = childMax;
								}
							}
							return maxLine;
						};
						endLine = findMaxLine(node);
						kind = "region";
					}

					// Add folding range if we found a valid one
					if (!kind.empty() && endLine > startLine) {
						json_t* range = json_object();
						json_object_set_new(range, "startLine", json_integer(static_cast<json_int_t>(startLine)));
						json_object_set_new(range, "endLine", json_integer(static_cast<json_int_t>(endLine)));
						json_object_set_new(range, "kind", json_string(kind.c_str()));
						json_array_append_new(ranges, range);
					}

					// Recurse into children
					for (size_t i = 0; i < node->childCount(); i++) {
						findFoldingRanges(node->child(i));
					}
				};

				// Process all top-level nodes
				for (size_t i = 0; i < root->childCount(); i++) {
					findFoldingRanges(root->child(i));
				}
			}
		}

		json_object_set_new(response, "result", ranges);
		sendMessage(response);
		json_decref(response);
	}

	void handleDocumentHighlight(const std::string& id, const std::string& uri, size_t line, size_t character) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* highlights = json_array();

		if (!documentText.empty()) {
			std::string word = getWordAtPosition(documentText, line, character);

			if (!word.empty()) {
				// Parse the document
				Qd::Ast ast;
				Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

				if (root && !ast.hasErrors()) {
					// Find all references to this identifier
					std::vector<Qd::IAstNode*> references;
					findIdentifiersInNode(root, word, references);

					for (Qd::IAstNode* ref : references) {
						json_t* highlight = json_object();

						json_t* range = json_object();
						json_t* start = json_object();
						size_t lspLine = (ref->line() > 0) ? ref->line() - 1 : 0;
						size_t lspCol = (ref->column() > 0) ? ref->column() - 1 : 0;

						// For function declarations, find the actual column of the function name
						if (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
							std::istringstream lineStream(documentText);
							std::string lineText;
							for (size_t i = 0; i <= lspLine; i++) {
								if (!std::getline(lineStream, lineText)) {
									lineText.clear();
									break;
								}
							}
							size_t fnPos = lineText.find("fn ");
							if (fnPos != std::string::npos) {
								lspCol = fnPos + 3;
							}
						}

						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspCol)));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(lspCol + word.length())));
						json_object_set_new(range, "end", end);

						json_object_set_new(highlight, "range", range);

						// DocumentHighlightKind: 1 = Text, 2 = Read, 3 = Write
						// For function declarations, use Write; for references, use Read
						int kind = (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) ? 3 : 2;
						json_object_set_new(highlight, "kind", json_integer(kind));

						json_array_append_new(highlights, highlight);
					}
				}
			}
		}

		json_object_set_new(response, "result", highlights);
		sendMessage(response);
		json_decref(response);
	}

	void handleReferences(const std::string& id, const std::string& uri, size_t line, size_t character) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			// Try to read from disk
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* locations = json_array();

		if (!documentText.empty()) {
			std::string word = getWordAtPosition(documentText, line, character);

			if (!word.empty()) {
				// Parse the document
				Qd::Ast ast;
				Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

				if (root && !ast.hasErrors()) {
					// Find all references to this identifier
					std::vector<Qd::IAstNode*> references;
					findIdentifiersInNode(root, word, references);

					for (Qd::IAstNode* ref : references) {
						json_t* location = json_object();
						json_object_set_new(location, "uri", json_string(uri.c_str()));

						json_t* range = json_object();
						json_t* start = json_object();
						size_t lspLine = (ref->line() > 0) ? ref->line() - 1 : 0;
						size_t lspCol = (ref->column() > 0) ? ref->column() - 1 : 0;

						// For function declarations, find the actual column of the function name
						if (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
							std::istringstream lineStream(documentText);
							std::string lineText;
							for (size_t i = 0; i <= lspLine; i++) {
								if (!std::getline(lineStream, lineText)) {
									lineText.clear();
									break;
								}
							}
							size_t fnPos = lineText.find("fn ");
							if (fnPos != std::string::npos) {
								lspCol = fnPos + 3;
							}
						}

						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspCol)));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(lspCol + word.length())));
						json_object_set_new(range, "end", end);

						json_object_set_new(location, "range", range);
						json_array_append_new(locations, location);
					}
				}
			}
		}

		json_object_set_new(response, "result", locations);
		sendMessage(response);
		json_decref(response);
	}

	void handleRename(
			const std::string& id, const std::string& uri, size_t line, size_t character, const std::string& newName) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		// Get document text
		std::string documentText;
		auto docIter = documents_.find(uri);
		if (docIter != documents_.end()) {
			documentText = docIter->second;
		} else {
			// Try to read from disk
			if (uri.substr(0, 7) == "file://") {
				std::string filePath = uri.substr(7);
				std::ifstream file(filePath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					documentText = buffer.str();
				}
			}
		}

		json_t* workspaceEdit = json_object();
		json_t* changes = json_object();

		if (!documentText.empty()) {
			std::string word = getWordAtPosition(documentText, line, character);

			if (!word.empty()) {
				// Parse the document
				Qd::Ast ast;
				Qd::IAstNode* root = ast.generate(documentText.c_str(), false, nullptr);

				if (root && !ast.hasErrors()) {
					// Find the function containing the cursor position
					// LSP lines are 0-indexed, AST lines are 1-indexed
					size_t astLine = line + 1;
					Qd::IAstNode* containingFunction = nullptr;
					bool isLocalVariable = false;

					// Helper to find max line in a subtree
					std::function<size_t(Qd::IAstNode*)> getMaxLine = [&](Qd::IAstNode* node) -> size_t {
						if (!node) {
							return 0;
						}
						size_t maxLine = node->line();
						for (size_t i = 0; i < node->childCount(); i++) {
							size_t childMax = getMaxLine(node->child(i));
							if (childMax > maxLine) {
								maxLine = childMax;
							}
						}
						return maxLine;
					};

					// Find which function contains this line
					for (size_t i = 0; i < root->childCount(); i++) {
						Qd::IAstNode* child = root->child(i);
						if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
							Qd::AstNodeFunctionDeclaration* func = static_cast<Qd::AstNodeFunctionDeclaration*>(child);
							// Check if cursor line is within this function
							size_t funcEndLine = getMaxLine(child);
							if (astLine >= func->line() && astLine <= funcEndLine) {
								containingFunction = child;
								break;
							}
						}
					}

					// Check if the word is a local variable (defined with ->) in the containing function
					if (containingFunction) {
						std::function<bool(Qd::IAstNode*)> hasLocalDecl = [&](Qd::IAstNode* node) -> bool {
							if (!node) {
								return false;
							}
							if (node->type() == Qd::IAstNode::Type::LOCAL) {
								Qd::AstNodeLocal* local = static_cast<Qd::AstNodeLocal*>(node);
								for (const std::string& name : local->names()) {
									if (name == word) {
										return true;
									}
								}
							}
							for (size_t i = 0; i < node->childCount(); i++) {
								if (hasLocalDecl(node->child(i))) {
									return true;
								}
							}
							return false;
						};
						isLocalVariable = hasLocalDecl(containingFunction);
					}

					// Find all references to rename
					std::vector<Qd::IAstNode*> references;
					// If it's a local variable, only search within the containing function
					if (isLocalVariable && containingFunction) {
						findIdentifiersInNode(containingFunction, word, references);
					} else {
						// For functions and globals, search the entire file
						findIdentifiersInNode(root, word, references);
					}

					json_t* edits = json_array();

					for (Qd::IAstNode* ref : references) {
						json_t* edit = json_object();

						json_t* range = json_object();
						json_t* start = json_object();
						size_t lspLine = (ref->line() > 0) ? ref->line() - 1 : 0;
						size_t lspCol = (ref->column() > 0) ? ref->column() - 1 : 0;

						// For function declarations, find the actual column of the function name
						if (ref->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
							// Get the line text to find where the function name starts
							std::istringstream lineStream(documentText);
							std::string lineText;
							for (size_t i = 0; i <= lspLine; i++) {
								if (!std::getline(lineStream, lineText)) {
									lineText.clear();
									break;
								}
							}
							// Find "fn " in the line and then the function name
							size_t fnPos = lineText.find("fn ");
							if (fnPos != std::string::npos) {
								lspCol = fnPos + 3; // Skip "fn "
							}
						}
						// For local variable declarations (-> varname), find the actual column
						else if (ref->type() == Qd::IAstNode::Type::LOCAL) {
							Qd::AstNodeLocal* local = static_cast<Qd::AstNodeLocal*>(ref);
							std::istringstream lineStream(documentText);
							std::string lineText;
							for (size_t i = 0; i <= lspLine; i++) {
								if (!std::getline(lineStream, lineText)) {
									lineText.clear();
									break;
								}
							}
							// Find "-> " in the line followed by the variable name
							size_t arrowPos = lineText.find("-> ");
							if (arrowPos != std::string::npos) {
								// For multiple assignment (-> a b c), find the specific variable
								size_t namePos = arrowPos + 3;
								for (const std::string& name : local->names()) {
									size_t foundPos = lineText.find(name, namePos);
									if (foundPos != std::string::npos && name == word) {
										lspCol = foundPos;
										break;
									}
									// Move past this name to find the next one
									namePos = foundPos + name.length();
								}
							}
						}

						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(lspCol)));
						json_object_set_new(range, "start", start);

						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(lspCol + word.length())));
						json_object_set_new(range, "end", end);

						json_object_set_new(edit, "range", range);
						json_object_set_new(edit, "newText", json_string(newName.c_str()));

						json_array_append_new(edits, edit);
					}

					json_object_set_new(changes, uri.c_str(), edits);
				}
			}
		}

		json_object_set_new(workspaceEdit, "changes", changes);
		json_object_set_new(response, "result", workspaceEdit);
		sendMessage(response);
		json_decref(response);
	}

	void handleShutdown(const std::string& id) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));
		json_object_set_new(response, "result", json_null());

		sendMessage(response);
		json_decref(response);
	}

	std::vector<FunctionInfo> extractFunctions(const std::string& text) {
		std::vector<FunctionInfo> functions;

		// Parse the document
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(text.c_str(), false, nullptr);

		if (!root || ast.hasErrors()) {
			return functions; // Return empty on parse errors
		}

		// Root should be AstProgram
		if (root->type() != Qd::IAstNode::Type::PROGRAM) {
			return functions;
		}

		// Iterate through program children looking for function declarations and imports
		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

				FunctionInfo info;
				info.name = funcNode->name();

				// Build signature parts
				std::ostringstream sigStream;
				sigStream << "fn " << info.name << "(";

				// Extract input parameters
				const auto& inputs = funcNode->inputParameters();
				for (size_t j = 0; j < inputs.size(); j++) {
					Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(inputs[j]);
					std::string paramStr = param->name() + ":" + param->typeString();
					info.inputParams.push_back(paramStr);

					if (j > 0) {
						sigStream << " ";
					}
					sigStream << paramStr;
				}

				sigStream << " -- ";

				// Extract output parameters
				const auto& outputs = funcNode->outputParameters();
				for (size_t j = 0; j < outputs.size(); j++) {
					Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(outputs[j]);
					std::string paramStr = param->name() + ":" + param->typeString();
					info.outputParams.push_back(paramStr);

					if (j > 0) {
						sigStream << " ";
					}
					sigStream << paramStr;
				}

				sigStream << ")";
				info.signature = sigStream.str();

				// Build snippet with placeholders for input parameters
				std::ostringstream snippetStream;
				for (size_t j = 0; j < info.inputParams.size(); j++) {
					const std::string& param = info.inputParams[j];
					// Extract just the name part (before the colon)
					size_t colonPos = param.find(':');
					std::string paramName = (colonPos != std::string::npos) ? param.substr(0, colonPos) : param;

					snippetStream << "${" << (j + 1) << ":" << paramName << "}";
					if (j < info.inputParams.size() - 1) {
						snippetStream << " ";
					}
				}
				if (!info.inputParams.empty()) {
					snippetStream << " ";
				}
				snippetStream << info.name;

				info.snippet = snippetStream.str();

				functions.push_back(info);
			} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
				// Handle imported functions
				Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
				std::string namespaceName = importNode->namespaceName();

				// Iterate through imported functions
				const auto& importedFuncs = importNode->functions();
				for (const auto* importedFunc : importedFuncs) {
					FunctionInfo info;
					// Use namespace::function format
					info.name = namespaceName + "::" + importedFunc->name;

					// Build signature parts
					std::ostringstream sigStream;
					sigStream << "fn " << importedFunc->name << "(";

					// Extract input parameters
					const auto& inputs = importedFunc->inputParameters;
					for (size_t j = 0; j < inputs.size(); j++) {
						Qd::AstNodeParameter* param = inputs[j];
						std::string paramStr = param->name() + ":" + param->typeString();
						info.inputParams.push_back(paramStr);

						if (j > 0) {
							sigStream << " ";
						}
						sigStream << paramStr;
					}

					sigStream << " -- ";

					// Extract output parameters
					const auto& outputs = importedFunc->outputParameters;
					for (size_t j = 0; j < outputs.size(); j++) {
						Qd::AstNodeParameter* param = outputs[j];
						std::string paramStr = param->name() + ":" + param->typeString();
						info.outputParams.push_back(paramStr);

						if (j > 0) {
							sigStream << " ";
						}
						sigStream << paramStr;
					}

					sigStream << ")";
					info.signature = sigStream.str();

					// Build snippet with placeholders for input parameters
					std::ostringstream snippetStream;
					for (size_t j = 0; j < info.inputParams.size(); j++) {
						const std::string& param = info.inputParams[j];
						// Extract just the name part (before the colon)
						size_t colonPos = param.find(':');
						std::string paramName = (colonPos != std::string::npos) ? param.substr(0, colonPos) : param;

						snippetStream << "${" << (j + 1) << ":" << paramName << "}";
						if (j < info.inputParams.size() - 1) {
							snippetStream << " ";
						}
					}
					if (!info.inputParams.empty()) {
						snippetStream << " ";
					}
					snippetStream << info.name;

					info.snippet = snippetStream.str();

					functions.push_back(info);
				}
			}
		}

		return functions;
	}

	std::vector<StructInfo> extractStructs(const std::string& text) {
		std::vector<StructInfo> structs;

		// Parse the document
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(text.c_str(), false, nullptr);

		if (!root || ast.hasErrors()) {
			return structs; // Return empty on parse errors
		}

		// Root should be AstProgram
		if (root->type() != Qd::IAstNode::Type::PROGRAM) {
			return structs;
		}

		// Iterate through program children looking for struct declarations
		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* child = root->child(i);

			if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
				Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);

				StructInfo info;
				info.name = structNode->name();

				// Build signature
				std::ostringstream sig;
				sig << "struct " << info.name << " { ";
				for (const auto* field : structNode->fields()) {
					const Qd::AstNodeStructField* structField = static_cast<const Qd::AstNodeStructField*>(field);
					info.fields.push_back({structField->name(), structField->typeName()});
					sig << structField->name() << ":" << structField->typeName() << " ";
				}
				sig << "}";
				info.signature = sig.str();

				structs.push_back(info);
			}
		}

		return structs;
	}

	std::map<std::string, std::string> documents_;
	[[maybe_unused]] int messageId_;
	bool lintEnabled_ = true;
	std::string quadlintPath_ = "quadlint";
};

void printHelp() {
	std::cout << "quadlsp - Quadrate Language Server Protocol\n\n";
	std::cout << "Provides IDE features for Quadrate: diagnostics, completion, and hover.\n\n";
	std::cout << "Usage: quadlsp [options]\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "\n";
	std::cout << "The LSP server communicates via stdin/stdout using JSON-RPC.\n";
	std::cout << "Configure your editor to use 'quadlsp' as the language server.\n\n";
	std::cout << "Supported features:\n";
	std::cout << "  - Syntax error diagnostics\n";
	std::cout << "  - Auto-completion for built-in instructions and user functions\n";
	std::cout << "  - Hover documentation\n";
	std::cout << "  - Document symbols (outline view of functions and imports)\n";
	std::cout << "  - Go to definition (jump to function declarations)\n";
	std::cout << "  - Find references (locate all function calls)\n";
	std::cout << "  - Rename symbol (rename functions across the file)\n";
}

void printVersion() {
	std::cout << "0.1.0\n";
}

int main(int argc, char* argv[]) {
	// Check for help or version flags
	if (argc > 1) {
		std::string arg = argv[1];
		if (arg == "-h" || arg == "--help") {
			printHelp();
			return 0;
		} else if (arg == "-v" || arg == "--version") {
			printVersion();
			return 0;
		}
	}

	QuadrateLSP lsp;
	lsp.run();
	return 0;
}
