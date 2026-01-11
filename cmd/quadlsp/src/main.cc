// Quadrate Language Server Protocol (LSP) implementation
// Main file containing core LSP class implementation

#include "lsp_impl.h"
#include "version.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
#include <qc/ast_node_test.h>
#include <qc/error_reporter.h>
#include <qc/semantic_validator.h>
#include <sstream>
#include <unistd.h>

// Expand tilde (~) in file paths
std::string expandTilde(const std::string& path) {
	if (!path.empty() && path[0] == '~') {
		const char* home = getenv("HOME");
		if (home) {
			return std::string(home) + path.substr(1);
		}
	}
	return path;
}

// Load dependencies from qd.json and return include paths
std::vector<std::string> loadDependenciesFromManifest(const std::string& manifestDir) {
	std::vector<std::string> includePaths;

	std::string manifestPath = manifestDir + "/qd.json";
	json_error_t error;
	json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
	if (!root) {
		return includePaths;
	}

	json_t* dependencies = json_object_get(root, "dependencies");
	if (dependencies && json_is_object(dependencies)) {
		const char* depName;
		json_t* depValue;
		json_object_foreach(dependencies, depName, depValue) {
			std::string resolved;

			if (json_is_string(depValue)) {
				// Simple form: "name": "url" or "name": "../path"
				resolved = json_string_value(depValue);
			} else if (json_is_object(depValue)) {
				// Expanded form: { "url": "..." }
				json_t* url = json_object_get(depValue, "url");
				if (url && json_is_string(url)) {
					resolved = json_string_value(url);
				}
			}

			if (resolved.empty()) {
				continue;
			}

			// Check if it's a local path
			bool isPath =
					(resolved.size() > 0 && (resolved[0] == '/' || resolved[0] == '.' ||
													(resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/')));

			if (isPath) {
				resolved = expandTilde(resolved);
				if (resolved.size() > 0 && resolved[0] != '/') {
					resolved = manifestDir + "/" + resolved;
				}
				try {
					resolved = std::filesystem::weakly_canonical(resolved).string();
				} catch (...) {
				}
				if (std::filesystem::exists(resolved)) {
					includePaths.push_back(resolved);
				}
			} else {
				// Git URL - check if installed in packages dir via _namespaces symlink
				const char* home = getenv("HOME");
				if (home) {
					std::string namespacePath = std::string(home) + "/quadrate/modules/_namespaces/" + depName;
					if (std::filesystem::exists(namespacePath)) {
						try {
							std::string resolvedPath = std::filesystem::canonical(namespacePath).string();
							includePaths.push_back(resolvedPath);
						} catch (...) {
							// If canonical fails, try the path anyway
							includePaths.push_back(namespacePath);
						}
					}
				}
			}
		}
	}

	json_decref(root);
	return includePaths;
}

// QuadrateLSP implementation

void QuadrateLSP::run() {
	while (true) {
		std::string message = readMessage();
		if (message.empty()) {
			break;
		}

		handleMessage(message);
	}
}

std::string QuadrateLSP::readMessage() {
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

void QuadrateLSP::sendMessage(json_t* json) {
	char* message = json_dumps(json, JSON_COMPACT);
	if (message) {
		std::cout << "Content-Length: " << strlen(message) << "\r\n\r\n" << message << std::flush;
		free(message);
	}
}

std::string QuadrateLSP::getJsonString(json_t* obj, const char* key) {
	json_t* val = json_object_get(obj, key);
	if (val && json_is_string(val)) {
		return json_string_value(val);
	}
	return "";
}

json_t* QuadrateLSP::getJsonObject(json_t* obj, const char* key) {
	return json_object_get(obj, key);
}

void QuadrateLSP::handleMessage(const std::string& message) {
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

void QuadrateLSP::handleInitialize(const std::string& id, json_t* initOptions) {
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
	json_object_set_new(serverInfo, "version", json_string(QUADRATE_VERSION));
	json_object_set_new(result, "serverInfo", serverInfo);

	json_object_set_new(response, "result", result);

	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleDidOpen(const std::string& uri, const std::string& text) {
	documents_[uri] = text;
	publishDiagnostics(uri, text);
}

void QuadrateLSP::handleShutdown(const std::string& id) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));
	json_object_set_new(response, "result", json_null());
	sendMessage(response);
	json_decref(response);
}

std::string QuadrateLSP::stripAnsiCodes(const std::string& input) {
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

std::vector<QuadrateLSP::LintWarning> QuadrateLSP::runQuadlint(const std::string& source) {
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

void QuadrateLSP::publishDiagnostics(const std::string& uri, const std::string& text) {
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

		// Load include paths from qd.json if present
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
		// Check if file is in standard library or module locations
		if (filePath.find("/usr/share/quadrate/") != std::string::npos ||
				filePath.find("/.quadrate/modules/") != std::string::npos) {
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

void QuadrateLSP::handleFormatting(const std::string& id, const std::string& uri) {
	(void)uri; // Not used yet

	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));
	json_object_set_new(response, "result", json_array()); // Empty array for now

	sendMessage(response);
	json_decref(response);
}

std::string QuadrateLSP::getBuiltInDocumentation(const std::string& word) {
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

std::string QuadrateLSP::getWordAtPosition(const std::string& text, size_t line, size_t character) {
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

std::string QuadrateLSP::getPackagesDir() {
	// Check QUADRATE_PATH environment variable first
	const char* quadratePath = getenv("QUADRATE_PATH");
	if (quadratePath) {
		return std::string(quadratePath);
	}

	// Check if XDG_DATA_HOME is set
	const char* xdgDataHome = getenv("XDG_DATA_HOME");
	if (xdgDataHome) {
		return std::string(xdgDataHome) + "/quadrate/modules";
	}

	// Default to ~/quadrate/modules
	const char* home = getenv("HOME");
	if (home) {
		return std::string(home) + "/quadrate/modules";
	}

	return "";
}

std::string QuadrateLSP::findLatestPackageVersion(const std::string& moduleName) {
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

std::string QuadrateLSP::resolveModulePath(const std::string& moduleName, const std::string& sourceDir) {
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

void QuadrateLSP::handleHover(const std::string& id, const std::string& uri, size_t line, size_t character) {
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

						// Show if it's a method
						if (func.isMethod) {
							docStream << "**Method on:** `" << func.receiverType;
							if (!func.receiverTypeParams.empty()) {
								docStream << "<";
								for (size_t i = 0; i < func.receiverTypeParams.size(); i++) {
									if (i > 0) {
										docStream << ", ";
									}
									docStream << func.receiverTypeParams[i];
								}
								docStream << ">";
							}
							docStream << "`\n\n";
						}

						// Show type parameters for generic functions
						if (func.isGeneric) {
							docStream << "**Type parameters:** `<";
							for (size_t i = 0; i < func.typeParams.size(); i++) {
								if (i > 0) {
									docStream << ", ";
								}
								docStream << func.typeParams[i];
							}
							docStream << ">`\n\n";
						}

						docStream << "**" << (func.isMethod ? "Method" : "Function") << ":**\n```quadrate\n"
								  << func.signature << "\n```\n\n";
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

											// Show if it's a method
											if (funcNode->hasReceiver()) {
												docStream << "**Method on:** `" << funcNode->receiverType();
												if (funcNode->hasReceiverTypeParams()) {
													docStream << "<";
													const auto& rTypeParams = funcNode->receiverTypeParams();
													for (size_t j = 0; j < rTypeParams.size(); j++) {
														if (j > 0) {
															docStream << ", ";
														}
														docStream << rTypeParams[j];
													}
													docStream << ">";
												}
												docStream << "`\n\n";
											}

											// Show type parameters for generic functions
											if (funcNode->isGeneric()) {
												docStream << "**Type parameters:** `<";
												const auto& typeParams = funcNode->typeParams();
												for (size_t j = 0; j < typeParams.size(); j++) {
													if (j > 0) {
														docStream << ", ";
													}
													docStream << typeParams[j];
												}
												docStream << ">`\n\n";
											}

											docStream << "**" << (funcNode->hasReceiver() ? "Method" : "Function")
													  << " (from " << moduleName << "):**\n```quadrate\n";

											// Build full signature
											if (funcNode->hasReceiver()) {
												docStream << "fn (" << funcNode->receiverName() << ":"
														  << funcNode->receiverType();
												if (funcNode->hasReceiverTypeParams()) {
													docStream << "<";
													const auto& rTypeParams = funcNode->receiverTypeParams();
													for (size_t j = 0; j < rTypeParams.size(); j++) {
														if (j > 0) {
															docStream << ", ";
														}
														docStream << rTypeParams[j];
													}
													docStream << ">";
												}
												docStream << ") " << funcNode->name();
											} else {
												docStream << "fn " << funcNode->name();
											}

											// Add type parameters
											if (funcNode->isGeneric()) {
												docStream << "<";
												const auto& typeParams = funcNode->typeParams();
												for (size_t j = 0; j < typeParams.size(); j++) {
													if (j > 0) {
														docStream << ", ";
													}
													docStream << typeParams[j];
												}
												docStream << ">";
											}

											docStream << "(";

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

											docStream << ")";
											if (funcNode->throws()) {
												docStream << "!";
											}
											docStream << "\n```";

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

void QuadrateLSP::handleSignatureHelp(const std::string& id, const std::string& uri, size_t line, size_t character) {
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

void QuadrateLSP::handleDocumentSymbols(const std::string& id, const std::string& uri) {
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

					// Build detail string showing signature
					std::ostringstream detail;
					detail << "(";
					const auto& inputs = funcNode->inputParameters();
					for (size_t j = 0; j < inputs.size(); j++) {
						if (j > 0) {
							detail << " ";
						}
						Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(inputs[j]);
						detail << param->name() << ":" << param->typeString();
					}
					detail << " -- ";
					const auto& outputs = funcNode->outputParameters();
					for (size_t j = 0; j < outputs.size(); j++) {
						if (j > 0) {
							detail << " ";
						}
						Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(outputs[j]);
						detail << param->name() << ":" << param->typeString();
					}
					detail << ")";
					json_object_set_new(symbol, "detail", json_string(detail.str().c_str()));

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
				} else if (child && child->type() == Qd::IAstNode::Type::STRUCT_DECLARATION) {
					Qd::AstNodeStructDeclaration* structNode = static_cast<Qd::AstNodeStructDeclaration*>(child);

					json_t* symbol = json_object();
					json_object_set_new(symbol, "name", json_string(structNode->name().c_str()));
					json_object_set_new(symbol, "kind", json_integer(23)); // Struct kind

					// Build detail string showing fields
					std::ostringstream detail;
					detail << "{ ";
					const auto& fields = structNode->fields();
					for (size_t j = 0; j < fields.size(); j++) {
						if (j > 0) {
							detail << " ";
						}
						const Qd::AstNodeStructField* field =
								static_cast<const Qd::AstNodeStructField*>(fields[j]);
						detail << field->name() << ":" << field->typeName();
					}
					detail << " }";
					json_object_set_new(symbol, "detail", json_string(detail.str().c_str()));

					// Add range
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

					json_object_set_new(symbol, "range", range);
					json_object_set_new(symbol, "selectionRange", json_deep_copy(range));

					json_array_append_new(symbols, symbol);
				} else if (child && child->type() == Qd::IAstNode::Type::IMPORT_STATEMENT) {
					Qd::AstNodeImport* importNode = static_cast<Qd::AstNodeImport*>(child);
					std::string namespaceName = importNode->namespaceName();

					// Add the import itself as a namespace symbol
					json_t* nsSymbol = json_object();
					json_object_set_new(nsSymbol, "name", json_string(namespaceName.c_str()));
					json_object_set_new(nsSymbol, "kind", json_integer(3)); // Namespace kind

					// Add range
					json_t* range = json_object();
					json_t* start = json_object();
					size_t lspLine = (importNode->line() > 0) ? importNode->line() - 1 : 0;
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(start, "character", json_integer(0));
					json_object_set_new(range, "start", start);

					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lspLine)));
					json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(namespaceName.length())));
					json_object_set_new(range, "end", end);

					json_object_set_new(nsSymbol, "range", range);
					json_object_set_new(nsSymbol, "selectionRange", json_deep_copy(range));

					json_array_append_new(symbols, nsSymbol);
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

std::vector<FunctionInfo> QuadrateLSP::extractFunctions(const std::string& text) {
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

	// Iterate through program children looking for functions
	for (size_t i = 0; i < root->childCount(); i++) {
		Qd::IAstNode* child = root->child(i);

		if (child && child->type() == Qd::IAstNode::Type::FUNCTION_DECLARATION) {
			Qd::AstNodeFunctionDeclaration* funcNode = static_cast<Qd::AstNodeFunctionDeclaration*>(child);

			FunctionInfo info;
			info.name = funcNode->name();

			// Extract generic type parameters
			info.isGeneric = funcNode->isGeneric();
			if (info.isGeneric) {
				info.typeParams = funcNode->typeParams();
			}

			// Extract method receiver information
			info.isMethod = funcNode->hasReceiver();
			if (info.isMethod) {
				info.receiverName = funcNode->receiverName();
				info.receiverType = funcNode->receiverType();
				if (funcNode->hasReceiverTypeParams()) {
					info.receiverTypeParams = funcNode->receiverTypeParams();
				}
			}

			// Build signature parts
			std::ostringstream sigStream;

			// Include receiver in signature for methods
			if (info.isMethod) {
				sigStream << "fn (" << info.receiverName << ":" << info.receiverType;
				if (!info.receiverTypeParams.empty()) {
					sigStream << "<";
					for (size_t j = 0; j < info.receiverTypeParams.size(); j++) {
						if (j > 0) {
							sigStream << ", ";
						}
						sigStream << info.receiverTypeParams[j];
					}
					sigStream << ">";
				}
				sigStream << ") " << info.name;
			} else {
				sigStream << "fn " << info.name;
			}

			// Add type parameters to signature
			if (info.isGeneric) {
				sigStream << "<";
				for (size_t j = 0; j < info.typeParams.size(); j++) {
					if (j > 0) {
						sigStream << ", ";
					}
					sigStream << info.typeParams[j];
				}
				sigStream << ">";
			}

			sigStream << "(";

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
			if (funcNode->throws()) {
				sigStream << "!";
			}
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
			if (funcNode->throws()) {
				snippetStream << "!";
			}

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

std::vector<StructInfo> QuadrateLSP::extractStructs(const std::string& text) {
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

			// Extract generic type parameters
			info.isGeneric = structNode->isGeneric();
			if (info.isGeneric) {
				info.typeParams = structNode->typeParams();
			}

			// Build signature
			std::ostringstream sig;
			sig << "struct " << info.name;

			// Include type parameters in signature
			if (info.isGeneric) {
				sig << "<";
				for (size_t j = 0; j < info.typeParams.size(); j++) {
					if (j > 0) {
						sig << ", ";
					}
					sig << info.typeParams[j];
				}
				sig << ">";
			}

			sig << " { ";
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

// Entry point helpers

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
	std::cout << quadrate_version_string("quadlsp") << "\n";
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
