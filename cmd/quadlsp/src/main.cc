// Quadrate Language Server Protocol (LSP) implementation
// Main file containing core LSP class implementation

#include "lsp_impl.h"
#include "src/platform/process_platform.h"
#include "version.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <quadrate/cli/cli.h>
#include <quadrate/platform/platform.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_program.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/error_reporter.h>
#include <quadrate/qc/semantic_validator.h>
#include <set>
#include <sstream>

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

// Get sibling .qd files in the same directory (for directory-based namespaces)
std::vector<std::string> getSiblingQdFiles(const std::string& filePath) {
	std::vector<std::string> siblings;

	try {
		std::filesystem::path p(filePath);
		std::filesystem::path dir = p.parent_path();
		std::string filename = p.filename().string();

		if (dir.empty() || !std::filesystem::exists(dir)) {
			return siblings;
		}

		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (!entry.is_regular_file()) {
				continue;
			}
			std::string name = entry.path().filename().string();
			// Skip the file itself
			if (name == filename) {
				continue;
			}
			// Only .qd files
			if (name.size() > 3 && name.substr(name.size() - 3) == ".qd") {
				// Exclude test files
				if (name.size() > 8 && name.substr(name.size() - 8) == "_test.qd") {
					continue;
				}
				siblings.push_back(entry.path().string());
			}
		}

		// Sort for consistent ordering
		std::sort(siblings.begin(), siblings.end());
	} catch (...) {
		// Ignore errors
	}

	return siblings;
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
		// Capture workspace root URI
		if (params) {
			std::string rootUri = getJsonString(params, "rootUri");
			if (!rootUri.empty() && rootUri.substr(0, 7) == "file://") {
				workspaceRoot_ = rootUri.substr(7);
			}
		}
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
	} else if (method == "textDocument/rangeFormatting") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDoc = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDoc, "uri");
			json_t* range = getJsonObject(params, "range");
			json_t* rangeStart = getJsonObject(range, "start");
			json_t* rangeEnd = getJsonObject(range, "end");
			size_t startLine = static_cast<size_t>(json_integer_value(json_object_get(rangeStart, "line")));
			size_t startChar = static_cast<size_t>(json_integer_value(json_object_get(rangeStart, "character")));
			size_t endLine = static_cast<size_t>(json_integer_value(json_object_get(rangeEnd, "line")));
			size_t endChar = static_cast<size_t>(json_integer_value(json_object_get(rangeEnd, "character")));
			handleRangeFormatting(id, uri, startLine, startChar, endLine, endChar);
		}
	} else if (method == "textDocument/codeLens") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDoc = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDoc, "uri");
			handleCodeLens(id, uri);
		}
	} else if (method == "textDocument/prepareTypeHierarchy") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDocument = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDocument, "uri");
			json_t* position = getJsonObject(params, "position");
			size_t line = static_cast<size_t>(json_integer_value(json_object_get(position, "line")));
			size_t character = static_cast<size_t>(json_integer_value(json_object_get(position, "character")));
			handlePrepareTypeHierarchy(id, uri, line, character);
		}
	} else if (method == "typeHierarchy/supertypes") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* item = getJsonObject(params, "item");
			std::string itemData = getJsonString(item, "data");
			handleSupertypes(id, itemData);
		}
	} else if (method == "typeHierarchy/subtypes") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* item = getJsonObject(params, "item");
			std::string itemData = getJsonString(item, "data");
			handleSubtypes(id, itemData);
		}
	} else if (method == "textDocument/onTypeFormatting") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDoc = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDoc, "uri");
			json_t* position = getJsonObject(params, "position");
			size_t line = static_cast<size_t>(json_integer_value(json_object_get(position, "line")));
			size_t character = static_cast<size_t>(json_integer_value(json_object_get(position, "character")));
			std::string ch = getJsonString(params, "ch");
			handleOnTypeFormatting(id, uri, line, character, ch);
		}
	} else if (method == "textDocument/linkedEditingRange") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDoc = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDoc, "uri");
			json_t* position = getJsonObject(params, "position");
			size_t line = static_cast<size_t>(json_integer_value(json_object_get(position, "line")));
			size_t character = static_cast<size_t>(json_integer_value(json_object_get(position, "character")));
			handleLinkedEditingRange(id, uri, line, character);
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
	} else if (method == "textDocument/codeAction") {
		json_t* params = getJsonObject(root, "params");
		json_t* textDocument = getJsonObject(params, "textDocument");
		std::string uri = getJsonString(textDocument, "uri");

		json_t* range = getJsonObject(params, "range");
		json_t* rangeStart = getJsonObject(range, "start");
		json_t* rangeEnd = getJsonObject(range, "end");
		size_t startLine = static_cast<size_t>(json_integer_value(json_object_get(rangeStart, "line")));
		size_t startChar = static_cast<size_t>(json_integer_value(json_object_get(rangeStart, "character")));
		size_t endLine = static_cast<size_t>(json_integer_value(json_object_get(rangeEnd, "line")));
		size_t endChar = static_cast<size_t>(json_integer_value(json_object_get(rangeEnd, "character")));

		json_t* context = getJsonObject(params, "context");
		json_t* diagnostics = json_object_get(context, "diagnostics");

		handleCodeAction(id, uri, startLine, startChar, endLine, endChar, diagnostics);
	} else if (method == "workspace/symbol") {
		json_t* params = getJsonObject(root, "params");
		std::string query = getJsonString(params, "query");
		handleWorkspaceSymbols(id, query);
	} else if (method == "textDocument/inlayHint") {
		json_t* params = getJsonObject(root, "params");
		json_t* textDocument = getJsonObject(params, "textDocument");
		std::string uri = getJsonString(textDocument, "uri");

		json_t* range = getJsonObject(params, "range");
		json_t* rangeStart = getJsonObject(range, "start");
		json_t* rangeEnd = getJsonObject(range, "end");
		size_t startLine = static_cast<size_t>(json_integer_value(json_object_get(rangeStart, "line")));
		size_t endLine = static_cast<size_t>(json_integer_value(json_object_get(rangeEnd, "line")));

		handleInlayHints(id, uri, startLine, endLine);
	} else if (method == "textDocument/semanticTokens/full") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDocument = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDocument, "uri");
			handleSemanticTokens(id, uri);
		}
	} else if (method == "textDocument/documentLink") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDocument = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDocument, "uri");
			handleDocumentLinks(id, uri);
		}
	} else if (method == "textDocument/prepareCallHierarchy") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDocument = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDocument, "uri");
			json_t* position = getJsonObject(params, "position");
			size_t line = static_cast<size_t>(json_integer_value(json_object_get(position, "line")));
			size_t character = static_cast<size_t>(json_integer_value(json_object_get(position, "character")));
			handlePrepareCallHierarchy(id, uri, line, character);
		}
	} else if (method == "callHierarchy/incomingCalls") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* item = getJsonObject(params, "item");
			std::string itemData = getJsonString(item, "data");
			handleIncomingCalls(id, itemData);
		}
	} else if (method == "callHierarchy/outgoingCalls") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* item = getJsonObject(params, "item");
			std::string itemData = getJsonString(item, "data");
			handleOutgoingCalls(id, itemData);
		}
	} else if (method == "textDocument/selectionRange") {
		json_t* params = getJsonObject(root, "params");
		if (params) {
			json_t* textDocument = getJsonObject(params, "textDocument");
			std::string uri = getJsonString(textDocument, "uri");
			json_t* positions = json_object_get(params, "positions");
			std::vector<std::pair<size_t, size_t>> posVec;
			if (positions && json_is_array(positions)) {
				size_t posCount = json_array_size(positions);
				for (size_t i = 0; i < posCount; i++) {
					json_t* pos = json_array_get(positions, i);
					size_t posLine = static_cast<size_t>(json_integer_value(json_object_get(pos, "line")));
					size_t posChar = static_cast<size_t>(json_integer_value(json_object_get(pos, "character")));
					posVec.push_back({posLine, posChar});
				}
			}
			handleSelectionRange(id, uri, posVec);
		}
	} else if (method == "textDocument/prepareRename") {
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
					handlePrepareRename(id, uri, line, character);
				}
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
	json_t* renameOptions = json_object();
	json_object_set_new(renameOptions, "prepareProvider", json_true());
	json_object_set_new(capabilities, "renameProvider", renameOptions);

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

	// Code actions (quick fixes)
	json_t* codeActionProvider = json_object();
	json_t* codeActionKinds = json_array();
	json_array_append_new(codeActionKinds, json_string("quickfix"));
	json_array_append_new(codeActionKinds, json_string("source.organizeImports"));
	json_object_set_new(codeActionProvider, "codeActionKinds", codeActionKinds);
	json_object_set_new(capabilities, "codeActionProvider", codeActionProvider);

	// Workspace symbols
	json_object_set_new(capabilities, "workspaceSymbolProvider", json_true());

	// Inlay hints (inline type/parameter hints)
	json_t* inlayHintProvider = json_object();
	json_object_set_new(inlayHintProvider, "resolveProvider", json_false());
	json_object_set_new(capabilities, "inlayHintProvider", inlayHintProvider);

	// Semantic tokens (rich syntax highlighting)
	json_t* semanticTokensProvider = json_object();
	json_t* legend = json_object();

	// Token types: indices match the order here
	// 0=namespace, 1=type, 2=class, 3=enum, 4=interface, 5=struct,
	// 6=typeParameter, 7=parameter, 8=variable, 9=property, 10=enumMember,
	// 11=event, 12=function, 13=method, 14=macro, 15=keyword, 16=modifier,
	// 17=comment, 18=string, 19=number, 20=regexp, 21=operator
	json_t* tokenTypes = json_array();
	json_array_append_new(tokenTypes, json_string("namespace"));
	json_array_append_new(tokenTypes, json_string("type"));
	json_array_append_new(tokenTypes, json_string("class"));
	json_array_append_new(tokenTypes, json_string("enum"));
	json_array_append_new(tokenTypes, json_string("interface"));
	json_array_append_new(tokenTypes, json_string("struct"));
	json_array_append_new(tokenTypes, json_string("typeParameter"));
	json_array_append_new(tokenTypes, json_string("parameter"));
	json_array_append_new(tokenTypes, json_string("variable"));
	json_array_append_new(tokenTypes, json_string("property"));
	json_array_append_new(tokenTypes, json_string("enumMember"));
	json_array_append_new(tokenTypes, json_string("event"));
	json_array_append_new(tokenTypes, json_string("function"));
	json_array_append_new(tokenTypes, json_string("method"));
	json_array_append_new(tokenTypes, json_string("macro"));
	json_array_append_new(tokenTypes, json_string("keyword"));
	json_array_append_new(tokenTypes, json_string("modifier"));
	json_array_append_new(tokenTypes, json_string("comment"));
	json_array_append_new(tokenTypes, json_string("string"));
	json_array_append_new(tokenTypes, json_string("number"));
	json_array_append_new(tokenTypes, json_string("regexp"));
	json_array_append_new(tokenTypes, json_string("operator"));
	json_object_set_new(legend, "tokenTypes", tokenTypes);

	// Token modifiers (bitmask)
	json_t* tokenModifiers = json_array();
	json_array_append_new(tokenModifiers, json_string("declaration"));
	json_array_append_new(tokenModifiers, json_string("definition"));
	json_array_append_new(tokenModifiers, json_string("readonly"));
	json_array_append_new(tokenModifiers, json_string("static"));
	json_array_append_new(tokenModifiers, json_string("deprecated"));
	json_array_append_new(tokenModifiers, json_string("abstract"));
	json_array_append_new(tokenModifiers, json_string("async"));
	json_array_append_new(tokenModifiers, json_string("modification"));
	json_array_append_new(tokenModifiers, json_string("documentation"));
	json_array_append_new(tokenModifiers, json_string("defaultLibrary"));
	json_object_set_new(legend, "tokenModifiers", tokenModifiers);

	json_object_set_new(semanticTokensProvider, "legend", legend);
	json_object_set_new(semanticTokensProvider, "full", json_true());
	json_object_set_new(capabilities, "semanticTokensProvider", semanticTokensProvider);

	// Document links (clickable imports)
	json_t* documentLinkProvider = json_object();
	json_object_set_new(documentLinkProvider, "resolveProvider", json_false());
	json_object_set_new(capabilities, "documentLinkProvider", documentLinkProvider);

	// Call hierarchy
	json_object_set_new(capabilities, "callHierarchyProvider", json_true());

	// Selection range (smart selection expand/shrink)
	json_object_set_new(capabilities, "selectionRangeProvider", json_true());

	// Range formatting (format selection)
	json_object_set_new(capabilities, "documentRangeFormattingProvider", json_true());

	// Code lens (inline info above functions)
	json_t* codeLensProvider = json_object();
	json_object_set_new(codeLensProvider, "resolveProvider", json_false());
	json_object_set_new(capabilities, "codeLensProvider", codeLensProvider);

	// Type hierarchy (struct relationships)
	json_object_set_new(capabilities, "typeHierarchyProvider", json_true());

	// On type formatting (auto-indent)
	json_t* onTypeFormattingProvider = json_object();
	json_object_set_new(onTypeFormattingProvider, "firstTriggerCharacter", json_string("\n"));
	json_t* moreTriggers = json_array();
	json_array_append_new(moreTriggers, json_string("}"));
	json_array_append_new(moreTriggers, json_string("{"));
	json_object_set_new(onTypeFormattingProvider, "moreTriggerCharacter", moreTriggers);
	json_object_set_new(capabilities, "documentOnTypeFormattingProvider", onTypeFormattingProvider);

	// Linked editing ranges (edit multiple occurrences simultaneously)
	json_object_set_new(capabilities, "linkedEditingRangeProvider", json_true());

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
	std::string tempPath = "/tmp/quadlsp_lint_" + std::to_string(process_platform_getpid()) + ".qd";
	{
		std::ofstream tempFile(tempPath);
		if (!tempFile.good()) {
			return warnings;
		}
		tempFile << source;
	}

	// Build command
	std::string command = quadlintPath_ + " \"" + tempPath + "\" 2>&1";

	// Execute command and capture output
	char outputBuffer[65536];
	int result = process_platform_exec_capture(command.c_str(), outputBuffer, sizeof(outputBuffer));
	if (result < 0) {
		return warnings;
	}
	std::string output(outputBuffer);

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
			json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(lspColumn + ERROR_SPAN_LENGTH)));
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

		// Directory-based namespace: load sibling .qd files
		std::vector<std::string> siblingFiles = getSiblingQdFiles(filePath);
		if (!siblingFiles.empty()) {
			validator.setSiblingFiles(siblingFiles);
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
			json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(lspColumn + ERROR_SPAN_LENGTH)));
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
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document content
	auto it = documents_.find(uri);
	if (it == documents_.end()) {
		json_object_set_new(response, "result", json_array());
		sendMessage(response);
		json_decref(response);
		return;
	}

	const std::string& source = it->second;

	// Write source to temp file
	std::string tempPath = "/tmp/quadlsp_fmt_" + std::to_string(process_platform_getpid()) + ".qd";
	{
		std::ofstream tempFile(tempPath);
		if (!tempFile.good()) {
			json_object_set_new(response, "result", json_array());
			sendMessage(response);
			json_decref(response);
			return;
		}
		tempFile << source;
	}

	// Run quadfmt
	std::string command = "quadfmt \"" + tempPath + "\"" + QD_SHELL_STDERR_SUPPRESS;
	char outputBuffer[262144]; // 256KB for formatted output
	int exitCode = process_platform_exec_capture(command.c_str(), outputBuffer, sizeof(outputBuffer));
	std::string formatted(outputBuffer);
	std::remove(tempPath.c_str());

	// If quadfmt failed or output is empty, return no edits
	if (exitCode != 0 || formatted.empty()) {
		json_object_set_new(response, "result", json_array());
		sendMessage(response);
		json_decref(response);
		return;
	}

	// If no changes needed, return empty array
	if (formatted == source) {
		json_object_set_new(response, "result", json_array());
		sendMessage(response);
		json_decref(response);
		return;
	}

	// Count lines in original document
	size_t lineCount = 0;
	size_t lastLineLength = 0;
	size_t lineStart = 0;
	for (size_t i = 0; i < source.size(); i++) {
		if (source[i] == '\n') {
			lineCount++;
			lineStart = i + 1;
		}
	}
	lastLineLength = source.size() - lineStart;

	// Create a single edit replacing the entire document
	json_t* edits = json_array();
	json_t* edit = json_object();

	json_t* range = json_object();
	json_t* start = json_object();
	json_object_set_new(start, "line", json_integer(0));
	json_object_set_new(start, "character", json_integer(0));
	json_t* end = json_object();
	json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lineCount)));
	json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(lastLineLength)));
	json_object_set_new(range, "start", start);
	json_object_set_new(range, "end", end);

	json_object_set_new(edit, "range", range);
	json_object_set_new(edit, "newText", json_string(formatted.c_str()));
	json_array_append_new(edits, edit);

	json_object_set_new(response, "result", edits);
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
			{"loop", "Infinite loop.\n\n**Syntax:** `loop { ... }`\n\nUse `if { break }` for conditional exit."},
			{"free", "Free allocated memory.\n\n**Stack effect:** `ptr --`\n\nFrees memory allocated for structs or "
					 "strings. Automatically frees nested string fields in structs."},
			{"struct", "Declare a struct type.\n\n**Syntax:** `struct Name { field1:type1 field2:type2 "
					   "}`\n\nDefines a "
					   "composite data type with named fields."},
			{"enum", "Declare an enum type.\n\n**Syntax:** `enum Name { Variant1 Variant2 = 10 "
					 "}`\n\nDefines scoped named integer constants. Values auto-increment from 0 or from "
					 "the last explicit value. Access with `Name::Variant`."},
			{"pub", "Public visibility modifier.\n\n**Syntax:** `pub struct Name { ... }` or `pub fn name(...) { "
					"... "
					"}`\n\nMakes structs, enums, or functions visible to other modules."},
			{"defer", "Defer execution until scope exit.\n\n**Syntax:** `defer { ... }`\n\nExecutes code block when "
					  "function returns, useful for cleanup."},
			{"as", "Type narrowing cast.\n\n**Syntax:** `expr as TypeName`\n\nNarrows a ptr value to a specific "
				   "struct type for field access. Compile-time only, no runtime cost.\n\n**Example:** `c as "
				   "http::Ctx @body`"},
			{"null", "Null pointer constant.\n\nPushes `0` with pointer type. Use for empty pointer fields in "
					 "structs.\n\n**Example:** `Node { value = 1 next = null }`\n\n**Check:** `ptr null == if { "
					 "... }`"},
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
	// Include :: for scoped identifiers, but not single : (type annotations)
	while (start > 0) {
		char c = targetLine[start - 1];
		if (isalnum(c) || c == '_') {
			start--;
		} else if (c == ':' && start >= 2 && targetLine[start - 2] == ':') {
			// Include :: for scoped identifiers
			start -= 2;
		} else {
			break;
		}
	}

	// Move end forward to end of word
	while (end < targetLine.length()) {
		char c = targetLine[end];
		if (isalnum(c) || c == '_') {
			end++;
		} else if (c == ':' && end + 1 < targetLine.length() && targetLine[end + 1] == ':') {
			// Include :: for scoped identifiers
			end += 2;
		} else {
			break;
		}
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
		// Modules are nested in paths like git.sr.ht/~klahr/qdhttp@master
		// Use recursive iteration to find them
		for (const auto& entry : std::filesystem::recursive_directory_iterator(packagesDir)) {
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

// Helper: Find the first .qd file in a directory (excluding *_test.qd files)
static std::string findFirstQdFile(const std::string& directory) {
	try {
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
			return "";
		}

		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				std::string filename = entry.path().filename().string();
				if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".qd") {
					// Exclude test files (*_test.qd)
					if (filename.size() > 8 && filename.substr(filename.size() - 8) == "_test.qd") {
						continue;
					}
					return entry.path().string();
				}
			}
		}
	} catch (...) {
		// Ignore filesystem errors
	}
	return "";
}

std::string QuadrateLSP::resolveModulePath(const std::string& moduleName, const std::string& sourceDir) {
	std::string result;

	// Try 0: Check qd.json manifest for dependencies
	std::string manifestPath = sourceDir + "/qd.json";
	if (std::filesystem::exists(manifestPath)) {
		json_error_t error;
		json_t* root = json_load_file(manifestPath.c_str(), 0, &error);
		if (root) {
			json_t* dependencies = json_object_get(root, "dependencies");
			if (dependencies && json_is_object(dependencies)) {
				json_t* depValue = json_object_get(dependencies, moduleName.c_str());
				if (depValue) {
					std::string resolved;
					if (json_is_string(depValue)) {
						resolved = json_string_value(depValue);
					} else if (json_is_object(depValue)) {
						json_t* url = json_object_get(depValue, "url");
						if (url && json_is_string(url)) {
							resolved = json_string_value(url);
						}
					}

					if (!resolved.empty()) {
						// Check if it's a local path
						bool isPath = (resolved[0] == '/' || resolved[0] == '.' ||
									   (resolved.size() > 1 && resolved[0] == '~' && resolved[1] == '/'));

						if (isPath) {
							resolved = expandTilde(resolved);
							if (resolved[0] != '/') {
								resolved = sourceDir + "/" + resolved;
							}
							try {
								resolved = std::filesystem::weakly_canonical(resolved).string();
							} catch (...) {
							}
							if (std::filesystem::exists(resolved)) {
								// If it's a directory, find first .qd file
								if (std::filesystem::is_directory(resolved)) {
									result = findFirstQdFile(resolved);
									if (!result.empty()) {
										json_decref(root);
										return result;
									}
								} else if (std::filesystem::is_regular_file(resolved)) {
									json_decref(root);
									return resolved;
								}
							}
						}
					}
				}
			}
			json_decref(root);
		}
	}

	// Try 1: Sibling file (moduleName.qd in the same directory)
	std::string siblingFile = sourceDir + "/" + moduleName + ".qd";
	if (std::filesystem::exists(siblingFile) && std::filesystem::is_regular_file(siblingFile)) {
		return siblingFile;
	}

	// Try 2: Local path (relative to source file) - subdirectory
	result = findFirstQdFile(sourceDir + "/" + moduleName);
	if (!result.empty()) {
		return result;
	}

	// Try 2: Third-party packages directory (installed via quadpm)
	std::string packagePath = findLatestPackageVersion(moduleName);
	if (!packagePath.empty()) {
		result = findFirstQdFile(packagePath);
		if (!result.empty()) {
			return result;
		}
	}

	// Try 3: QUADRATE_ROOT environment variable
	const char* quadrateRoot = getenv("QUADRATE_ROOT");
	if (quadrateRoot) {
		result = findFirstQdFile(std::string(quadrateRoot) + "/" + moduleName);
		if (!result.empty()) {
			return result;
		}
	}

	// Try 4: Standard library relative to executable (for dist/ or installed builds)
	// This allows the LSP to find stdlib when run as dist/bin/quadlsp
	try {
#ifdef __linux__
		std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
#elif defined(__HAIKU__)
		std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe");
#else
		std::filesystem::path exePath; // Unsupported platform
#endif
		if (!exePath.empty()) {
			std::filesystem::path exeDir = exePath.parent_path();
#ifdef __HAIKU__
			std::filesystem::path shareDir = exeDir / ".." / "data" / "quadrate" / moduleName;
#else
			std::filesystem::path shareDir = exeDir / ".." / "share" / "quadrate" / moduleName;
#endif
			result = findFirstQdFile(shareDir.string());
			if (!result.empty()) {
				return result;
			}
		}
	} catch (...) {
		// Ignore errors resolving executable path
	}

	// Try 5: Installed standard library (/usr/share/quadrate/ on Linux, /boot/system/data/quadrate/ on Haiku)
#ifdef __HAIKU__
	result = findFirstQdFile("/boot/system/data/quadrate/" + moduleName);
#else
	result = findFirstQdFile("/usr/share/quadrate/" + moduleName);
#endif
	if (!result.empty()) {
		return result;
	}

	// Try 6: Standard library directories relative to current directory (for development)
	// Pattern: lib/qd{moduleName}/qd/{moduleName}/
	result = findFirstQdFile("lib/qd" + moduleName + "/qd/" + moduleName);
	if (!result.empty()) {
		return result;
	}

	// Try 7: $HOME/quadrate directory
	const char* home = getenv("HOME");
	if (home) {
		result = findFirstQdFile(std::string(home) + "/quadrate/" + moduleName);
		if (!result.empty()) {
			return result;
		}
	}

	return "";
}

void QuadrateLSP::handleCodeAction(const std::string& id, const std::string& uri, size_t startLine, size_t startChar,
		size_t endLine, size_t endChar, json_t* diagnostics) {
	(void)startChar;
	(void)endLine;
	(void)endChar;

	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* actions = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	// Process diagnostics to offer quick fixes
	if (diagnostics && json_is_array(diagnostics)) {
		size_t diagIndex;
		json_t* diag;
		json_array_foreach(diagnostics, diagIndex, diag) {
			std::string message = getJsonString(diag, "message");

			// Check for "Unknown module" error - offer to add 'use' statement
			if (message.find("Unknown module") != std::string::npos ||
					message.find("Unresolved module") != std::string::npos) {
				// Extract module name from message
				size_t quoteStart = message.find('\'');
				size_t quoteEnd = message.find('\'', quoteStart + 1);
				if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
					std::string moduleName = message.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

					// Create code action to add 'use' statement
					json_t* action = json_object();
					json_object_set_new(
							action, "title", json_string(("Add 'use " + moduleName + "' at top of file").c_str()));
					json_object_set_new(action, "kind", json_string("quickfix"));

					// Create workspace edit
					json_t* edit = json_object();
					json_t* changes = json_object();
					json_t* edits = json_array();

					json_t* textEdit = json_object();
					json_t* range = json_object();
					json_t* start = json_object();
					json_object_set_new(start, "line", json_integer(0));
					json_object_set_new(start, "character", json_integer(0));
					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(0));
					json_object_set_new(end, "character", json_integer(0));
					json_object_set_new(range, "start", start);
					json_object_set_new(range, "end", end);
					json_object_set_new(textEdit, "range", range);
					json_object_set_new(textEdit, "newText", json_string(("use " + moduleName + "\n").c_str()));
					json_array_append_new(edits, textEdit);

					json_object_set_new(changes, uri.c_str(), edits);
					json_object_set_new(edit, "changes", changes);
					json_object_set_new(action, "edit", edit);

					json_array_append_new(actions, action);
				}
			}

			// Check for "unused variable" warning - offer to remove or prefix with underscore
			if (message.find("Unused variable") != std::string::npos ||
					message.find("unused variable") != std::string::npos) {
				// Extract variable name from message
				size_t quoteStart = message.find('\'');
				size_t quoteEnd = message.find('\'', quoteStart + 1);
				if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
					std::string varName = message.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

					// Offer to prefix with underscore to suppress warning
					json_t* action = json_object();
					json_object_set_new(
							action, "title", json_string(("Rename to '_" + varName + "' to suppress warning").c_str()));
					json_object_set_new(action, "kind", json_string("quickfix"));

					// Find the variable declaration and create edit
					json_t* diagRange = json_object_get(diag, "range");
					if (diagRange) {
						json_t* diagStart = json_object_get(diagRange, "start");
						size_t diagLine = static_cast<size_t>(json_integer_value(json_object_get(diagStart, "line")));
						size_t diagCol =
								static_cast<size_t>(json_integer_value(json_object_get(diagStart, "character")));

						json_t* edit = json_object();
						json_t* changes = json_object();
						json_t* edits = json_array();

						json_t* textEdit = json_object();
						json_t* range = json_object();
						json_t* start = json_object();
						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(diagLine)));
						json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(diagCol)));
						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(diagLine)));
						json_object_set_new(
								end, "character", json_integer(static_cast<json_int_t>(diagCol + varName.length())));
						json_object_set_new(range, "start", start);
						json_object_set_new(range, "end", end);
						json_object_set_new(textEdit, "range", range);
						json_object_set_new(textEdit, "newText", json_string(("_" + varName).c_str()));
						json_array_append_new(edits, textEdit);

						json_object_set_new(changes, uri.c_str(), edits);
						json_object_set_new(edit, "changes", changes);
						json_object_set_new(action, "edit", edit);

						json_array_append_new(actions, action);
					}
				}
			}
		}
	}

	// Offer to organize imports if cursor is at the top of the file
	if (startLine < 5) {
		// Check if there are any 'use' statements in the document
		if (!documentText.empty() && documentText.find("use ") != std::string::npos) {
			json_t* action = json_object();
			json_object_set_new(action, "title", json_string("Organize imports"));
			json_object_set_new(action, "kind", json_string("source.organizeImports"));
			// This would require more complex logic to actually organize imports
			// For now, just advertise the action
			json_array_append_new(actions, action);
		}
	}

	json_object_set_new(response, "result", actions);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleInlayHints(const std::string& id, const std::string& uri, size_t startLine, size_t endLine) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* hints = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Parse the document to get AST
		Qd::Ast ast;
		Qd::IAstNode* astRoot = ast.generate(documentText.c_str(), false, nullptr);

		if (astRoot && !ast.hasErrors()) {
			(void)astRoot; // Used only for validation
			// Extract functions from the document for signature lookup
			std::vector<FunctionInfo> functions = extractFunctions(documentText);

			// Create a map for quick function lookup
			std::map<std::string, FunctionInfo> funcMap;
			for (const auto& func : functions) {
				funcMap[func.name] = func;
			}

			// Look for local variable declarations (-> syntax) to show type hints
			// Parse line by line within the requested range
			std::istringstream iss(documentText);
			std::string line;
			size_t lineNum = 0;

			while (std::getline(iss, line)) {
				if (lineNum >= startLine && lineNum <= endLine) {
					// Look for -> (local variable declaration)
					size_t arrowPos = line.find("->");
					if (arrowPos != std::string::npos) {
						// Find the variable name after ->
						size_t varStart = arrowPos + 2;
						while (varStart < line.size() && (line[varStart] == ' ' || line[varStart] == '\t')) {
							varStart++;
						}

						if (varStart < line.size() && (std::isalpha(line[varStart]) || line[varStart] == '_')) {
							size_t varEnd = varStart;
							while (varEnd < line.size() && (std::isalnum(line[varEnd]) || line[varEnd] == '_')) {
								varEnd++;
							}

							// We could infer type from context but that's complex
							// For now, just add a hint marker at the arrow
							json_t* hint = json_object();
							json_t* position = json_object();
							json_object_set_new(position, "line", json_integer(static_cast<json_int_t>(lineNum)));
							json_object_set_new(position, "character", json_integer(static_cast<json_int_t>(arrowPos)));
							json_object_set_new(hint, "position", position);
							json_object_set_new(hint, "label", json_string(": local"));
							json_object_set_new(hint, "kind", json_integer(1)); // Type hint
							json_object_set_new(hint, "paddingLeft", json_true());

							json_array_append_new(hints, hint);
						}
					}

					// Look for function calls with module prefix (module::func)
					size_t colonPos = 0;
					while ((colonPos = line.find("::", colonPos)) != std::string::npos) {
						// Check if this is followed by a function name
						size_t funcStart = colonPos + 2;
						if (funcStart < line.size() && (std::isalpha(line[funcStart]) || line[funcStart] == '_')) {
							size_t funcEnd = funcStart;
							while (funcEnd < line.size() && (std::isalnum(line[funcEnd]) || line[funcEnd] == '_')) {
								funcEnd++;
							}

							std::string funcName = line.substr(funcStart, funcEnd - funcStart);

							// Check if we have info about this function
							auto it = funcMap.find(funcName);
							if (it != funcMap.end() && !it->second.inputParams.empty()) {
								// Add parameter hint after the function name
								std::string paramHint = "(";
								for (size_t i = 0; i < it->second.inputParams.size(); i++) {
									if (i > 0) {
										paramHint += ", ";
									}
									// Extract just the parameter name from "name:type"
									std::string param = it->second.inputParams[i];
									size_t colonIdx = param.find(':');
									if (colonIdx != std::string::npos) {
										paramHint += param.substr(0, colonIdx);
									} else {
										paramHint += param;
									}
								}
								paramHint += ")";

								json_t* hint = json_object();
								json_t* position = json_object();
								json_object_set_new(position, "line", json_integer(static_cast<json_int_t>(lineNum)));
								json_object_set_new(
										position, "character", json_integer(static_cast<json_int_t>(funcEnd)));
								json_object_set_new(hint, "position", position);
								json_object_set_new(hint, "label", json_string(paramHint.c_str()));
								json_object_set_new(hint, "kind", json_integer(2)); // Parameter hint
								json_object_set_new(hint, "paddingLeft", json_true());

								json_array_append_new(hints, hint);
							}
						}
						colonPos += 2;
					}
				}
				lineNum++;
			}
		}
	}

	json_object_set_new(response, "result", hints);
	sendMessage(response);
	json_decref(response);
}

// Semantic token type indices (must match the order in handleInitialize)
enum SemanticTokenType {
	TOKEN_NAMESPACE = 0,
	TOKEN_TYPE = 1,
	TOKEN_CLASS = 2,
	TOKEN_ENUM = 3,
	TOKEN_INTERFACE = 4,
	TOKEN_STRUCT = 5,
	TOKEN_TYPE_PARAMETER = 6,
	TOKEN_PARAMETER = 7,
	TOKEN_VARIABLE = 8,
	TOKEN_PROPERTY = 9,
	TOKEN_ENUM_MEMBER = 10,
	TOKEN_EVENT = 11,
	TOKEN_FUNCTION = 12,
	TOKEN_METHOD = 13,
	TOKEN_MACRO = 14,
	TOKEN_KEYWORD = 15,
	TOKEN_MODIFIER = 16,
	TOKEN_COMMENT = 17,
	TOKEN_STRING = 18,
	TOKEN_NUMBER = 19,
	TOKEN_REGEXP = 20,
	TOKEN_OPERATOR = 21
};

// Semantic token modifier flags (bitmask)
enum SemanticTokenModifier {
	MOD_DECLARATION = 1 << 0,
	MOD_DEFINITION = 1 << 1,
	MOD_READONLY = 1 << 2,
	MOD_STATIC = 1 << 3,
	MOD_DEPRECATED = 1 << 4,
	MOD_ABSTRACT = 1 << 5,
	MOD_ASYNC = 1 << 6,
	MOD_MODIFICATION = 1 << 7,
	MOD_DOCUMENTATION = 1 << 8,
	MOD_DEFAULT_LIBRARY = 1 << 9
};

// Helper to check if a word is a Quadrate keyword
static bool isKeyword(const std::string& word) {
	static const std::set<std::string> keywords = {"fn", "if", "else", "for", "use", "struct", "enum", "const",
			"return", "break", "continue", "defer", "switch", "case", "default", "true", "false", "nil", "and", "or",
			"not", "in", "as", "test", "pub"};
	return keywords.count(word) > 0;
}

// Helper to check if a word is a built-in stack operation
static bool isBuiltinOp(const std::string& word) {
	static const std::set<std::string> builtins = {// Stack manipulation
			"dup", "drop", "swap", "over", "rot", "nip", "tuck", "pick", "roll",
			// Arithmetic
			"++", "--",
			// Comparison
			"lt", "gt", "le", "ge", "eq", "ne",
			// I/O
			"print", "nl", "read", "readln",
			// Type operations
			"typeof", "sizeof",
			// Memory
			"alloc", "free", "realloc",
			// Other
			"assert", "panic", "exit"};
	return builtins.count(word) > 0;
}

// Helper to check if a word is a type name
static bool isTypeName(const std::string& word) {
	static const std::set<std::string> types = {
			"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "str", "ptr", "void", "any"};
	return types.count(word) > 0;
}

void QuadrateLSP::handleSemanticTokens(const std::string& id, const std::string& uri) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	std::vector<int> data; // Encoded token data

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Collect struct type names for highlighting
		std::set<std::string> structTypeNames;

		// Extract struct names from current document
		std::vector<StructInfo> localStructs = extractStructs(documentText);
		for (const auto& s : localStructs) {
			structTypeNames.insert(s.name);
		}

		// Get source directory for module resolution
		std::string sourceDir;
		std::string filePath;
		if (uri.substr(0, 7) == "file://") {
			filePath = uri.substr(7);
			std::filesystem::path p(filePath);
			sourceDir = p.parent_path().string();
		}

		// Extract struct names from sibling files (same directory namespace)
		if (!filePath.empty()) {
			std::vector<std::string> siblings = getSiblingQdFiles(filePath);
			for (const auto& siblingPath : siblings) {
				std::ifstream file(siblingPath);
				if (file.good()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string siblingText = buffer.str();

					std::vector<StructInfo> siblingStructs = extractStructs(siblingText);
					for (const auto& s : siblingStructs) {
						structTypeNames.insert(s.name);
					}
				}
			}
		}

		// Extract struct names from imported modules
		std::istringstream importStream(documentText);
		std::string importLine;
		while (std::getline(importStream, importLine)) {
			size_t usePos = importLine.find("use ");
			if (usePos != std::string::npos) {
				size_t moduleStart = usePos + 4;
				while (moduleStart < importLine.size() && std::isspace(importLine[moduleStart])) {
					moduleStart++;
				}
				size_t moduleEnd = moduleStart;
				while (moduleEnd < importLine.size() &&
						(std::isalnum(static_cast<unsigned char>(importLine[moduleEnd])) ||
								importLine[moduleEnd] == '_' || importLine[moduleEnd] == '/' ||
								importLine[moduleEnd] == ':' || importLine[moduleEnd] == '-')) {
					moduleEnd++;
				}
				if (moduleEnd > moduleStart) {
					std::string moduleName = importLine.substr(moduleStart, moduleEnd - moduleStart);
					std::string modulePath = resolveModulePath(moduleName, sourceDir);
					if (!modulePath.empty()) {
						// Get the module directory and scan ALL .qd files for structs
						std::filesystem::path moduleDir = std::filesystem::path(modulePath).parent_path();
						try {
							for (const auto& entry : std::filesystem::directory_iterator(moduleDir)) {
								if (entry.is_regular_file() && entry.path().extension() == ".qd") {
									std::vector<StructInfo> moduleStructs = extractModuleStructs(entry.path().string());
									for (const auto& s : moduleStructs) {
										structTypeNames.insert(s.name);
									}
								}
							}
						} catch (...) {
							// Fallback to single file if directory iteration fails
							std::vector<StructInfo> moduleStructs = extractModuleStructs(modulePath);
							for (const auto& s : moduleStructs) {
								structTypeNames.insert(s.name);
							}
						}
					}
				}
			}
		}

		// Track position for delta encoding
		size_t prevLine = 0;
		size_t prevChar = 0;

		// Simple tokenizer that produces semantic tokens
		size_t line = 0;
		size_t col = 0;
		size_t i = 0;

		auto addToken = [&](size_t tokenLine, size_t tokenCol, size_t length, int tokenType, int modifiers) {
			int deltaLine = static_cast<int>(tokenLine - prevLine);
			int deltaChar = (deltaLine == 0) ? static_cast<int>(tokenCol - prevChar) : static_cast<int>(tokenCol);
			data.push_back(deltaLine);
			data.push_back(deltaChar);
			data.push_back(static_cast<int>(length));
			data.push_back(tokenType);
			data.push_back(modifiers);
			prevLine = tokenLine;
			prevChar = tokenCol;
		};

		while (i < documentText.size()) {
			char c = documentText[i];

			// Track line/column
			if (c == '\n') {
				line++;
				col = 0;
				i++;
				continue;
			}

			// Skip whitespace
			if (std::isspace(c)) {
				col++;
				i++;
				continue;
			}

			// Single-line comment
			if (c == '/' && i + 1 < documentText.size() && documentText[i + 1] == '/') {
				size_t startCol = col;
				size_t startI = i;
				while (i < documentText.size() && documentText[i] != '\n') {
					i++;
					col++;
				}
				addToken(line, startCol, i - startI, TOKEN_COMMENT, 0);
				continue;
			}

			// Multi-line comment
			if (c == '/' && i + 1 < documentText.size() && documentText[i + 1] == '*') {
				size_t startLine = line;
				size_t startCol = col;
				size_t length = 0;
				i += 2;
				col += 2;
				length += 2;
				while (i < documentText.size()) {
					if (documentText[i] == '*' && i + 1 < documentText.size() && documentText[i + 1] == '/') {
						i += 2;
						col += 2;
						length += 2;
						break;
					}
					if (documentText[i] == '\n') {
						// For multi-line comments, just emit token for first line
						// (Editors typically handle multi-line comments differently)
						break;
					}
					i++;
					col++;
					length++;
				}
				addToken(startLine, startCol, length, TOKEN_COMMENT, 0);
				continue;
			}

			// String literal
			if (c == '"') {
				size_t startCol = col;
				size_t startI = i;
				i++;
				col++;
				while (i < documentText.size() && documentText[i] != '"' && documentText[i] != '\n') {
					if (documentText[i] == '\\' && i + 1 < documentText.size()) {
						i += 2;
						col += 2;
					} else {
						i++;
						col++;
					}
				}
				if (i < documentText.size() && documentText[i] == '"') {
					i++;
					col++;
				}
				addToken(line, startCol, i - startI, TOKEN_STRING, 0);
				continue;
			}

			// Character literal
			if (c == '\'') {
				size_t startCol = col;
				size_t startI = i;
				i++;
				col++;
				while (i < documentText.size() && documentText[i] != '\'' && documentText[i] != '\n') {
					if (documentText[i] == '\\' && i + 1 < documentText.size()) {
						i += 2;
						col += 2;
					} else {
						i++;
						col++;
					}
				}
				if (i < documentText.size() && documentText[i] == '\'') {
					i++;
					col++;
				}
				addToken(line, startCol, i - startI, TOKEN_STRING, 0);
				continue;
			}

			// Number literal
			if (std::isdigit(c) || (c == '-' && i + 1 < documentText.size() && std::isdigit(documentText[i + 1]))) {
				size_t startCol = col;
				size_t startI = i;
				if (c == '-') {
					i++;
					col++;
				}
				// Handle hex (0x), binary (0b), octal (0o)
				if (i < documentText.size() && documentText[i] == '0' && i + 1 < documentText.size()) {
					char next = documentText[i + 1];
					if (next == 'x' || next == 'X' || next == 'b' || next == 'B' || next == 'o' || next == 'O') {
						i += 2;
						col += 2;
					}
				}
				while (i < documentText.size() &&
						(std::isxdigit(documentText[i]) || documentText[i] == '.' || documentText[i] == '_' ||
								documentText[i] == 'e' || documentText[i] == 'E')) {
					i++;
					col++;
				}
				addToken(line, startCol, i - startI, TOKEN_NUMBER, 0);
				continue;
			}

			// Identifier or keyword
			if (std::isalpha(c) || c == '_') {
				size_t startCol = col;
				size_t startI = i;
				while (i < documentText.size() && (std::isalnum(documentText[i]) || documentText[i] == '_')) {
					i++;
					col++;
				}
				std::string word = documentText.substr(startI, i - startI);

				// Determine token type
				int tokenType = TOKEN_VARIABLE;
				int modifiers = 0;

				if (isKeyword(word)) {
					tokenType = TOKEN_KEYWORD;
					// Check if this is 'fn' followed by a function name
					if (word == "fn") {
						modifiers = MOD_DECLARATION;
					}
				} else if (isTypeName(word)) {
					tokenType = TOKEN_TYPE;
				} else if (structTypeNames.count(word) > 0) {
					// User-defined struct type - use TOKEN_TYPE for consistent blue coloring
					tokenType = TOKEN_TYPE;
				} else if (isBuiltinOp(word)) {
					tokenType = TOKEN_MACRO;
					modifiers = MOD_DEFAULT_LIBRARY;
				} else {
					// Check context: is this a function definition?
					// Look back to see if preceded by 'fn'
					size_t lookback = startI;
					while (lookback > 0 && std::isspace(documentText[lookback - 1])) {
						lookback--;
					}
					if (lookback >= 2 && documentText.substr(lookback - 2, 2) == "fn") {
						tokenType = TOKEN_FUNCTION;
						modifiers = MOD_DEFINITION;
					}
					// Check if followed by :: (namespace)
					else if (i < documentText.size() && documentText[i] == ':' && i + 1 < documentText.size() &&
							 documentText[i + 1] == ':') {
						tokenType = TOKEN_NAMESPACE;
					}
					// Check if preceded by :: (function call or struct from module)
					else if (startI >= 2 && documentText[startI - 1] == ':' && documentText[startI - 2] == ':') {
						// Check if this is a struct type name - use TOKEN_TYPE for blue coloring
						if (structTypeNames.count(word) > 0) {
							tokenType = TOKEN_TYPE;
						} else {
							tokenType = TOKEN_FUNCTION;
						}
					}
					// Check if preceded by -> (local variable assignment)
					else if (startI >= 2) {
						size_t arrowCheck = startI - 1;
						while (arrowCheck > 0 && std::isspace(documentText[arrowCheck])) {
							arrowCheck--;
						}
						if (arrowCheck >= 1 && documentText[arrowCheck] == '>' && arrowCheck >= 1 &&
								documentText[arrowCheck - 1] == '-') {
							tokenType = TOKEN_VARIABLE;
							modifiers = MOD_DECLARATION;
						}
					}
					// Check if this is a struct field access (after <<)
					else if (startI > 0 && documentText[startI - 1] == '@') {
						tokenType = TOKEN_PROPERTY;
					}
				}

				addToken(line, startCol, i - startI, tokenType, modifiers);
				continue;
			}

			// Operators and punctuation
			if (c == '-' && i + 1 < documentText.size() && documentText[i + 1] == '>') {
				// Arrow operator
				addToken(line, col, 2, TOKEN_OPERATOR, 0);
				i += 2;
				col += 2;
				continue;
			}

			if (c == ':' && i + 1 < documentText.size() && documentText[i + 1] == ':') {
				// Namespace separator
				addToken(line, col, 2, TOKEN_OPERATOR, 0);
				i += 2;
				col += 2;
				continue;
			}

			if (c == '-' && i + 1 < documentText.size() && documentText[i + 1] == '-') {
				// Decrement or separator
				addToken(line, col, 2, TOKEN_OPERATOR, 0);
				i += 2;
				col += 2;
				continue;
			}

			if (c == '+' && i + 1 < documentText.size() && documentText[i + 1] == '+') {
				// Increment
				addToken(line, col, 2, TOKEN_MACRO, MOD_DEFAULT_LIBRARY);
				i += 2;
				col += 2;
				continue;
			}

			// Single character operators
			if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '=' || c == '<' || c == '>' ||
					c == '!' || c == '&' || c == '|' || c == '^' || c == '~' || c == '@') {
				addToken(line, col, 1, TOKEN_OPERATOR, 0);
				i++;
				col++;
				continue;
			}

			// Skip other characters (braces, parens, etc.)
			i++;
			col++;
		}
	}

	// Build result
	json_t* result = json_object();
	json_t* dataArray = json_array();
	for (int val : data) {
		json_array_append_new(dataArray, json_integer(val));
	}
	json_object_set_new(result, "data", dataArray);

	json_object_set_new(response, "result", result);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleDocumentLinks(const std::string& id, const std::string& uri) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* links = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
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

	if (!documentText.empty()) {
		// Parse line by line looking for 'use' statements
		std::istringstream iss(documentText);
		std::string line;
		size_t lineNum = 0;

		while (std::getline(iss, line)) {
			// Look for 'use' at the start of the line (with optional whitespace)
			size_t usePos = line.find("use");
			if (usePos != std::string::npos) {
				// Check that 'use' is preceded only by whitespace
				bool validUse = true;
				for (size_t i = 0; i < usePos; i++) {
					if (!std::isspace(static_cast<unsigned char>(line[i]))) {
						validUse = false;
						break;
					}
				}

				// Check that 'use' is followed by whitespace
				if (validUse && usePos + 3 < line.size() &&
						!std::isspace(static_cast<unsigned char>(line[usePos + 3]))) {
					validUse = false;
				}

				if (validUse) {
					// Find the module name after 'use'
					size_t moduleStart = usePos + 3;
					while (moduleStart < line.size() && std::isspace(static_cast<unsigned char>(line[moduleStart]))) {
						moduleStart++;
					}

					if (moduleStart < line.size()) {
						size_t moduleEnd = moduleStart;
						// Module names can contain alphanumeric, underscore, slash (for paths), and colons
						while (moduleEnd < line.size() &&
								(std::isalnum(static_cast<unsigned char>(line[moduleEnd])) || line[moduleEnd] == '_' ||
										line[moduleEnd] == '/' || line[moduleEnd] == ':' || line[moduleEnd] == '-')) {
							moduleEnd++;
						}

						if (moduleEnd > moduleStart) {
							std::string moduleName = line.substr(moduleStart, moduleEnd - moduleStart);

							// Resolve the module path
							std::string modulePath = resolveModulePath(moduleName, sourceDir);

							if (!modulePath.empty() && std::filesystem::exists(modulePath)) {
								// Create document link
								json_t* link = json_object();

								// Range covers the module name
								json_t* range = json_object();
								json_t* start = json_object();
								json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(lineNum)));
								json_object_set_new(
										start, "character", json_integer(static_cast<json_int_t>(moduleStart)));
								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(lineNum)));
								json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(moduleEnd)));
								json_object_set_new(range, "start", start);
								json_object_set_new(range, "end", end);
								json_object_set_new(link, "range", range);

								// Target is the file URI
								std::string targetUri = "file://" + modulePath;
								json_object_set_new(link, "target", json_string(targetUri.c_str()));

								// Tooltip
								std::string tooltip = "Open module: " + moduleName;
								json_object_set_new(link, "tooltip", json_string(tooltip.c_str()));

								json_array_append_new(links, link);
							}
						}
					}
				}
			}
			lineNum++;
		}
	}

	json_object_set_new(response, "result", links);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handlePrepareCallHierarchy(
		const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* items = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Get the word at the cursor position
		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty()) {
			// Extract functions from the document
			std::vector<FunctionInfo> functions = extractFunctions(documentText);

			// Check if the word is a function name
			for (const auto& func : functions) {
				if (func.name == word) {
					// Create a CallHierarchyItem
					json_t* item = json_object();
					json_object_set_new(item, "name", json_string(func.name.c_str()));
					json_object_set_new(item, "kind", json_integer(12)); // Function

					// Build detail string from signature
					std::string detail = "(";
					for (size_t i = 0; i < func.inputParams.size(); i++) {
						if (i > 0) {
							detail += ", ";
						}
						detail += func.inputParams[i];
					}
					detail += " -- ";
					for (size_t i = 0; i < func.outputParams.size(); i++) {
						if (i > 0) {
							detail += ", ";
						}
						detail += func.outputParams[i];
					}
					detail += ")";
					json_object_set_new(item, "detail", json_string(detail.c_str()));

					json_object_set_new(item, "uri", json_string(uri.c_str()));

					// Range is the function definition
					json_t* range = json_object();
					json_t* start = json_object();
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(func.line)));
					json_object_set_new(start, "character", json_integer(0));
					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(func.line)));
					json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(func.name.size() + 3)));
					json_object_set_new(range, "start", start);
					json_object_set_new(range, "end", end);
					json_object_set_new(item, "range", range);

					// Selection range is just the function name
					json_t* selRange = json_object();
					json_t* selStart = json_object();
					json_object_set_new(selStart, "line", json_integer(static_cast<json_int_t>(func.line)));
					json_object_set_new(selStart, "character", json_integer(3)); // After "fn "
					json_t* selEnd = json_object();
					json_object_set_new(selEnd, "line", json_integer(static_cast<json_int_t>(func.line)));
					json_object_set_new(
							selEnd, "character", json_integer(static_cast<json_int_t>(3 + func.name.size())));
					json_object_set_new(selRange, "start", selStart);
					json_object_set_new(selRange, "end", selEnd);
					json_object_set_new(item, "selectionRange", selRange);

					// Store data for incoming/outgoing calls
					// Format: "uri|funcname|line"
					std::string data = uri + "|" + func.name + "|" + std::to_string(func.line);
					json_object_set_new(item, "data", json_string(data.c_str()));

					json_array_append_new(items, item);
					break;
				}
			}
		}
	}

	json_object_set_new(response, "result", items);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleIncomingCalls(const std::string& id, const std::string& itemData) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* calls = json_array();

	// Parse itemData: "uri|funcname|line"
	std::string targetFunc;
	size_t firstPipe = itemData.find('|');
	size_t secondPipe = itemData.find('|', firstPipe + 1);
	if (firstPipe != std::string::npos && secondPipe != std::string::npos) {
		targetFunc = itemData.substr(firstPipe + 1, secondPipe - firstPipe - 1);
	}

	if (!targetFunc.empty()) {
		// Search all open documents for calls to this function
		for (const auto& [docUri, docText] : documents_) {
			std::vector<FunctionInfo> functions = extractFunctions(docText);

			for (const auto& func : functions) {
				// Skip the function itself
				if (func.name == targetFunc) {
					continue;
				}

				// Search the function body for calls to targetFunc
				// Simple approach: look for the function name followed by whitespace or newline
				std::istringstream iss(docText);
				std::string line;
				size_t lineNum = 0;
				bool inFunction = false;

				// Find the function's body range (rough estimate)
				while (std::getline(iss, line)) {
					if (lineNum == func.line) {
						inFunction = true;
					}

					if (inFunction) {
						// Look for closing brace to mark end of function (simple heuristic)
						if (line.find('}') != std::string::npos && lineNum > func.line) {
							break;
						}

						// Look for calls to targetFunc
						size_t pos = 0;
						while ((pos = line.find(targetFunc, pos)) != std::string::npos) {
							// Check it's not part of a larger identifier
							bool validStart = (pos == 0 || (!std::isalnum(static_cast<unsigned char>(line[pos - 1])) &&
																   line[pos - 1] != '_'));
							bool validEnd =
									(pos + targetFunc.size() >= line.size() ||
											(!std::isalnum(static_cast<unsigned char>(line[pos + targetFunc.size()])) &&
													line[pos + targetFunc.size()] != '_'));

							if (validStart && validEnd && lineNum > func.line) {
								// Found a call! Create incoming call item
								json_t* call = json_object();

								// Create the "from" CallHierarchyItem
								json_t* from = json_object();
								json_object_set_new(from, "name", json_string(func.name.c_str()));
								json_object_set_new(from, "kind", json_integer(12)); // Function
								json_object_set_new(from, "uri", json_string(docUri.c_str()));

								json_t* range = json_object();
								json_t* start = json_object();
								json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(func.line)));
								json_object_set_new(start, "character", json_integer(0));
								json_t* end = json_object();
								json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(func.line)));
								json_object_set_new(
										end, "character", json_integer(static_cast<json_int_t>(func.name.size() + 3)));
								json_object_set_new(range, "start", start);
								json_object_set_new(range, "end", end);
								json_object_set_new(from, "range", range);

								json_t* selRange = json_object();
								json_t* selStart = json_object();
								json_object_set_new(selStart, "line", json_integer(static_cast<json_int_t>(func.line)));
								json_object_set_new(selStart, "character", json_integer(3));
								json_t* selEnd = json_object();
								json_object_set_new(selEnd, "line", json_integer(static_cast<json_int_t>(func.line)));
								json_object_set_new(selEnd, "character",
										json_integer(static_cast<json_int_t>(3 + func.name.size())));
								json_object_set_new(selRange, "start", selStart);
								json_object_set_new(selRange, "end", selEnd);
								json_object_set_new(from, "selectionRange", selRange);

								std::string data = docUri + "|" + func.name + "|" + std::to_string(func.line);
								json_object_set_new(from, "data", json_string(data.c_str()));

								json_object_set_new(call, "from", from);

								// fromRanges - where the call appears
								json_t* fromRanges = json_array();
								json_t* callRange = json_object();
								json_t* callStart = json_object();
								json_object_set_new(callStart, "line", json_integer(static_cast<json_int_t>(lineNum)));
								json_object_set_new(callStart, "character", json_integer(static_cast<json_int_t>(pos)));
								json_t* callEnd = json_object();
								json_object_set_new(callEnd, "line", json_integer(static_cast<json_int_t>(lineNum)));
								json_object_set_new(callEnd, "character",
										json_integer(static_cast<json_int_t>(pos + targetFunc.size())));
								json_object_set_new(callRange, "start", callStart);
								json_object_set_new(callRange, "end", callEnd);
								json_array_append_new(fromRanges, callRange);
								json_object_set_new(call, "fromRanges", fromRanges);

								json_array_append_new(calls, call);
								break; // One call per function is enough
							}
							pos++;
						}
					}
					lineNum++;
				}
			}
		}
	}

	json_object_set_new(response, "result", calls);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleOutgoingCalls(const std::string& id, const std::string& itemData) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* calls = json_array();

	// Parse itemData: "uri|funcname|line"
	std::string sourceUri;
	std::string sourceFuncName;
	size_t sourceLine = 0;

	size_t firstPipe = itemData.find('|');
	size_t secondPipe = itemData.find('|', firstPipe + 1);
	if (firstPipe != std::string::npos && secondPipe != std::string::npos) {
		sourceUri = itemData.substr(0, firstPipe);
		sourceFuncName = itemData.substr(firstPipe + 1, secondPipe - firstPipe - 1);
		sourceLine = std::stoul(itemData.substr(secondPipe + 1));
	}

	if (!sourceFuncName.empty()) {
		// Get the source document
		auto docIter = documents_.find(sourceUri);
		if (docIter != documents_.end()) {
			const std::string& docText = docIter->second;
			std::vector<FunctionInfo> functions = extractFunctions(docText);

			// Build a set of known function names for quick lookup
			std::set<std::string> knownFuncs;
			for (const auto& f : functions) {
				knownFuncs.insert(f.name);
			}

			// Find the source function and scan its body
			std::istringstream iss(docText);
			std::string line;
			size_t lineNum = 0;
			bool inFunction = false;
			int braceDepth = 0;

			std::map<std::string, std::vector<std::pair<size_t, size_t>>> callSites; // funcName -> [(line, col)]

			while (std::getline(iss, line)) {
				if (lineNum == sourceLine) {
					inFunction = true;
				}

				if (inFunction) {
					// Track brace depth
					for (char ch : line) {
						if (ch == '{') {
							braceDepth++;
						}
						if (ch == '}') {
							braceDepth--;
						}
					}

					if (braceDepth == 0 && lineNum > sourceLine) {
						break; // End of function
					}

					// Look for function calls
					for (const auto& funcName : knownFuncs) {
						if (funcName == sourceFuncName) {
							continue; // Skip self
						}

						size_t pos = 0;
						while ((pos = line.find(funcName, pos)) != std::string::npos) {
							bool validStart = (pos == 0 || (!std::isalnum(static_cast<unsigned char>(line[pos - 1])) &&
																   line[pos - 1] != '_'));
							bool validEnd = (pos + funcName.size() >= line.size() ||
											 (!std::isalnum(static_cast<unsigned char>(line[pos + funcName.size()])) &&
													 line[pos + funcName.size()] != '_'));

							if (validStart && validEnd) {
								callSites[funcName].push_back({lineNum, pos});
							}
							pos++;
						}
					}
				}
				lineNum++;
			}

			// Create outgoing call items
			for (const auto& [calledFunc, sites] : callSites) {
				// Find the function info
				FunctionInfo* targetInfo = nullptr;
				for (auto& f : functions) {
					if (f.name == calledFunc) {
						targetInfo = &f;
						break;
					}
				}

				if (targetInfo) {
					json_t* call = json_object();

					// "to" CallHierarchyItem
					json_t* to = json_object();
					json_object_set_new(to, "name", json_string(targetInfo->name.c_str()));
					json_object_set_new(to, "kind", json_integer(12)); // Function
					json_object_set_new(to, "uri", json_string(sourceUri.c_str()));

					json_t* range = json_object();
					json_t* start = json_object();
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(targetInfo->line)));
					json_object_set_new(start, "character", json_integer(0));
					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(targetInfo->line)));
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(targetInfo->name.size() + 3)));
					json_object_set_new(range, "start", start);
					json_object_set_new(range, "end", end);
					json_object_set_new(to, "range", range);

					json_t* selRange = json_object();
					json_t* selStart = json_object();
					json_object_set_new(selStart, "line", json_integer(static_cast<json_int_t>(targetInfo->line)));
					json_object_set_new(selStart, "character", json_integer(3));
					json_t* selEnd = json_object();
					json_object_set_new(selEnd, "line", json_integer(static_cast<json_int_t>(targetInfo->line)));
					json_object_set_new(
							selEnd, "character", json_integer(static_cast<json_int_t>(3 + targetInfo->name.size())));
					json_object_set_new(selRange, "start", selStart);
					json_object_set_new(selRange, "end", selEnd);
					json_object_set_new(to, "selectionRange", selRange);

					std::string data = sourceUri + "|" + targetInfo->name + "|" + std::to_string(targetInfo->line);
					json_object_set_new(to, "data", json_string(data.c_str()));

					json_object_set_new(call, "to", to);

					// fromRanges
					json_t* fromRanges = json_array();
					for (const auto& [callLine, callCol] : sites) {
						json_t* callRange = json_object();
						json_t* callStart = json_object();
						json_object_set_new(callStart, "line", json_integer(static_cast<json_int_t>(callLine)));
						json_object_set_new(callStart, "character", json_integer(static_cast<json_int_t>(callCol)));
						json_t* callEnd = json_object();
						json_object_set_new(callEnd, "line", json_integer(static_cast<json_int_t>(callLine)));
						json_object_set_new(callEnd, "character",
								json_integer(static_cast<json_int_t>(callCol + calledFunc.size())));
						json_object_set_new(callRange, "start", callStart);
						json_object_set_new(callRange, "end", callEnd);
						json_array_append_new(fromRanges, callRange);
					}
					json_object_set_new(call, "fromRanges", fromRanges);

					json_array_append_new(calls, call);
				}
			}
		}
	}

	json_object_set_new(response, "result", calls);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleSelectionRange(
		const std::string& id, const std::string& uri, const std::vector<std::pair<size_t, size_t>>& positions) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* results = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Split into lines for easier processing
		std::vector<std::string> lines;
		std::istringstream iss(documentText);
		std::string lineContent;
		while (std::getline(iss, lineContent)) {
			lines.push_back(lineContent);
		}

		// Find function boundaries (line -> {startLine, endLine})
		std::vector<std::pair<size_t, size_t>> functionBounds;
		std::vector<FunctionInfo> functions = extractFunctions(documentText);
		for (const auto& func : functions) {
			// Find the closing brace of this function
			int braceDepth = 0;
			bool foundOpen = false;
			size_t endLine = func.line;
			for (size_t i = func.line; i < lines.size(); i++) {
				for (char ch : lines[i]) {
					if (ch == '{') {
						braceDepth++;
						foundOpen = true;
					} else if (ch == '}') {
						braceDepth--;
						if (foundOpen && braceDepth == 0) {
							endLine = i;
							break;
						}
					}
				}
				if (foundOpen && braceDepth == 0) {
					break;
				}
			}
			functionBounds.push_back({func.line, endLine});
		}

		// Process each requested position
		for (const auto& [posLine, posChar] : positions) {
			if (posLine >= lines.size()) {
				json_array_append_new(results, json_null());
				continue;
			}

			const std::string& line = lines[posLine];

			// Build selection ranges from innermost to outermost
			// Each range has a parent that is larger

			// 1. Word range - find word boundaries at cursor
			size_t wordStart = posChar;
			size_t wordEnd = posChar;

			if (posChar < line.size()) {
				// Find start of word
				while (wordStart > 0 &&
						(std::isalnum(static_cast<unsigned char>(line[wordStart - 1])) || line[wordStart - 1] == '_')) {
					wordStart--;
				}
				// Find end of word
				while (wordEnd < line.size() &&
						(std::isalnum(static_cast<unsigned char>(line[wordEnd])) || line[wordEnd] == '_')) {
					wordEnd++;
				}
			}

			// 2. Line range (excluding leading/trailing whitespace)
			size_t lineStart = 0;
			size_t lineEnd = line.size();
			while (lineStart < line.size() && std::isspace(static_cast<unsigned char>(line[lineStart]))) {
				lineStart++;
			}
			while (lineEnd > lineStart && std::isspace(static_cast<unsigned char>(line[lineEnd - 1]))) {
				lineEnd--;
			}

			// 3. Block range - find enclosing braces
			size_t blockStartLine = posLine;
			size_t blockEndLine = posLine;
			size_t blockStartChar = 0;
			size_t blockEndChar = 0;
			bool foundBlock = false;

			// Search backwards for opening brace
			int depth = 0;
			for (size_t i = posLine + 1; i > 0; i--) {
				const std::string& searchLine = lines[i - 1];
				size_t startIdx = (i - 1 == posLine) ? posChar : searchLine.size();
				for (size_t j = startIdx; j > 0; j--) {
					char ch = searchLine[j - 1];
					if (ch == '}') {
						depth++;
					} else if (ch == '{') {
						if (depth == 0) {
							blockStartLine = i - 1;
							blockStartChar = j - 1;
							foundBlock = true;
							break;
						}
						depth--;
					}
				}
				if (foundBlock) {
					break;
				}
			}

			// Search forwards for closing brace
			if (foundBlock) {
				depth = 1;
				for (size_t i = blockStartLine; i < lines.size(); i++) {
					const std::string& searchLine = lines[i];
					size_t startIdx = (i == blockStartLine) ? blockStartChar + 1 : 0;
					for (size_t j = startIdx; j < searchLine.size(); j++) {
						char ch = searchLine[j];
						if (ch == '{') {
							depth++;
						} else if (ch == '}') {
							depth--;
							if (depth == 0) {
								blockEndLine = i;
								blockEndChar = j + 1;
								break;
							}
						}
					}
					if (depth == 0) {
						break;
					}
				}
			}

			// 4. Function range
			size_t funcStartLine = 0;
			size_t funcEndLine = lines.size() > 0 ? lines.size() - 1 : 0;
			for (const auto& [fStart, fEnd] : functionBounds) {
				if (posLine >= fStart && posLine <= fEnd) {
					funcStartLine = fStart;
					funcEndLine = fEnd;
					break;
				}
			}

			// Build the nested SelectionRange structure (innermost first)
			// Order: word -> line -> block -> function

			// Helper to create a range object
			auto makeRange = [](size_t sLine, size_t sChar, size_t eLine, size_t eChar) -> json_t* {
				json_t* range = json_object();
				json_t* start = json_object();
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(sLine)));
				json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(sChar)));
				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(eLine)));
				json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(eChar)));
				json_object_set_new(range, "start", start);
				json_object_set_new(range, "end", end);
				return range;
			};

			// Function range (outermost, no parent)
			json_t* funcRange = json_object();
			json_object_set_new(
					funcRange, "range", makeRange(funcStartLine, 0, funcEndLine, lines[funcEndLine].size()));

			// Block range (parent is function)
			json_t* blockRange = nullptr;
			if (foundBlock) {
				blockRange = json_object();
				json_object_set_new(
						blockRange, "range", makeRange(blockStartLine, blockStartChar, blockEndLine, blockEndChar));
				json_object_set_new(blockRange, "parent", funcRange);
			} else {
				blockRange = funcRange;
			}

			// Line range (parent is block)
			json_t* lineRange = json_object();
			json_object_set_new(lineRange, "range", makeRange(posLine, lineStart, posLine, lineEnd));
			json_object_set_new(lineRange, "parent", blockRange);

			// Word range (parent is line) - innermost
			json_t* wordRange = json_object();
			json_object_set_new(wordRange, "range", makeRange(posLine, wordStart, posLine, wordEnd));
			json_object_set_new(wordRange, "parent", lineRange);

			json_array_append_new(results, wordRange);
		}
	}

	json_object_set_new(response, "result", results);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleRangeFormatting(const std::string& id, const std::string& uri, size_t startLine,
		size_t startChar, size_t endLine, size_t endChar) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* edits = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Write to temp file
		std::string tmpPathStr = (std::filesystem::temp_directory_path() / "quadlsp_fmt.qd").string();
		bool writeOk = false;
		{
			std::ofstream ofs(tmpPathStr, std::ios::binary);
			if (ofs) {
				ofs.write(documentText.c_str(), static_cast<std::streamsize>(documentText.size()));
				writeOk = ofs.good();
			}
		}
		if (writeOk) {
			// Run quadfmt
			std::string cmd = "quadfmt " + tmpPathStr + QD_SHELL_STDERR_SUPPRESS;
			char outputBuffer[262144]; // 256KB for formatted output
			int exitCode = process_platform_exec_capture(cmd.c_str(), outputBuffer, sizeof(outputBuffer));
			if (exitCode == 0) {
				std::string formattedText(outputBuffer);

				if (!formattedText.empty()) {
					// Split both original and formatted into lines
					std::vector<std::string> origLines;
					std::vector<std::string> fmtLines;

					std::istringstream origIss(documentText);
					std::string line;
					while (std::getline(origIss, line)) {
						origLines.push_back(line);
					}

					std::istringstream fmtIss(formattedText);
					while (std::getline(fmtIss, line)) {
						fmtLines.push_back(line);
					}

					// Only create edit if the range content changed
					// Compare lines in the requested range
					bool changed = false;
					if (origLines.size() == fmtLines.size()) {
						for (size_t i = startLine; i <= endLine && i < origLines.size(); i++) {
							if (origLines[i] != fmtLines[i]) {
								changed = true;
								break;
							}
						}
					} else {
						changed = true;
					}

					if (changed) {
						// Build the replacement text for the range
						std::string rangeText;
						for (size_t i = startLine; i <= endLine && i < fmtLines.size(); i++) {
							if (i > startLine) {
								rangeText += "\n";
							}
							rangeText += fmtLines[i];
						}

						// Adjust endChar if we're replacing full lines
						size_t adjustedEndChar = endChar;
						if (endLine < origLines.size()) {
							adjustedEndChar = origLines[endLine].size();
						}

						// Create edit
						json_t* edit = json_object();
						json_t* range = json_object();
						json_t* start = json_object();
						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(startLine)));
						json_object_set_new(start, "character", json_integer(0)); // Start of line
						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(endLine)));
						json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(adjustedEndChar)));
						json_object_set_new(range, "start", start);
						json_object_set_new(range, "end", end);
						json_object_set_new(edit, "range", range);
						json_object_set_new(edit, "newText", json_string(rangeText.c_str()));
						json_array_append_new(edits, edit);
					}
				}
			}
			std::filesystem::remove(tmpPathStr);
		}
	}

	// Suppress unused parameter warnings
	(void)startChar;

	json_object_set_new(response, "result", edits);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleCodeLens(const std::string& id, const std::string& uri) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* lenses = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Extract functions
		std::vector<FunctionInfo> functions = extractFunctions(documentText);

		// For each function, count references and create code lens
		for (const auto& func : functions) {
			// Count references to this function in all open documents
			size_t refCount = 0;
			for (const auto& [docUri, docText] : documents_) {
				// Search for function name as a whole word
				size_t pos = 0;
				while ((pos = docText.find(func.name, pos)) != std::string::npos) {
					// Check it's a whole word match
					bool validStart = (pos == 0 || (!std::isalnum(static_cast<unsigned char>(docText[pos - 1])) &&
														   docText[pos - 1] != '_'));
					bool validEnd = (pos + func.name.size() >= docText.size() ||
									 (!std::isalnum(static_cast<unsigned char>(docText[pos + func.name.size()])) &&
											 docText[pos + func.name.size()] != '_'));

					if (validStart && validEnd) {
						refCount++;
					}
					pos++;
				}
			}

			// Subtract 1 for the definition itself
			if (refCount > 0) {
				refCount--;
			}

			// Create code lens for reference count
			json_t* lens = json_object();

			// Range is above the function (on the function line)
			json_t* range = json_object();
			json_t* start = json_object();
			json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(func.line)));
			json_object_set_new(start, "character", json_integer(0));
			json_t* end = json_object();
			json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(func.line)));
			json_object_set_new(end, "character", json_integer(0));
			json_object_set_new(range, "start", start);
			json_object_set_new(range, "end", end);
			json_object_set_new(lens, "range", range);

			// Command to show references
			json_t* command = json_object();
			std::string title;
			if (refCount == 0) {
				title = "0 references";
			} else if (refCount == 1) {
				title = "1 reference";
			} else {
				title = std::to_string(refCount) + " references";
			}
			json_object_set_new(command, "title", json_string(title.c_str()));
			json_object_set_new(command, "command", json_string("editor.action.findReferences"));
			json_t* args = json_array();
			json_array_append_new(args, json_string(uri.c_str()));
			json_t* position = json_object();
			json_object_set_new(position, "line", json_integer(static_cast<json_int_t>(func.line)));
			json_object_set_new(position, "character", json_integer(3)); // After "fn "
			json_array_append_new(args, position);
			json_object_set_new(command, "arguments", args);
			json_object_set_new(lens, "command", command);

			json_array_append_new(lenses, lens);

			// Check if this is a test function (name starts with "test_")
			bool isTest = (func.name.size() > 5 && func.name.substr(0, 5) == "test_");

			if (isTest) {
				// Add "Run test" lens
				json_t* testLens = json_object();

				json_t* testRange = json_object();
				json_t* testStart = json_object();
				json_object_set_new(testStart, "line", json_integer(static_cast<json_int_t>(func.line)));
				json_object_set_new(testStart, "character", json_integer(0));
				json_t* testEnd = json_object();
				json_object_set_new(testEnd, "line", json_integer(static_cast<json_int_t>(func.line)));
				json_object_set_new(testEnd, "character", json_integer(0));
				json_object_set_new(testRange, "start", testStart);
				json_object_set_new(testRange, "end", testEnd);
				json_object_set_new(testLens, "range", testRange);

				json_t* testCommand = json_object();
				json_object_set_new(testCommand, "title", json_string("Run test"));
				json_object_set_new(testCommand, "command", json_string("quadrate.runTest"));
				json_t* testArgs = json_array();
				json_array_append_new(testArgs, json_string(uri.c_str()));
				json_array_append_new(testArgs, json_string(func.name.c_str()));
				json_object_set_new(testCommand, "arguments", testArgs);
				json_object_set_new(testLens, "command", testCommand);

				json_array_append_new(lenses, testLens);
			}
		}
	}

	json_object_set_new(response, "result", lenses);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handlePrepareTypeHierarchy(
		const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* items = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Get the word at cursor position
		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty()) {
			// Check if this is a struct name
			std::vector<StructInfo> structs = extractStructs(documentText);

			for (const auto& structInfo : structs) {
				if (structInfo.name == word) {
					// Create TypeHierarchyItem
					json_t* item = json_object();
					json_object_set_new(item, "name", json_string(structInfo.name.c_str()));
					json_object_set_new(item, "kind", json_integer(23)); // Struct

					// Build detail from fields
					std::string detail = "{";
					for (size_t i = 0; i < structInfo.fields.size(); i++) {
						if (i > 0) {
							detail += ", ";
						}
						detail += structInfo.fields[i].first + ":" + structInfo.fields[i].second;
					}
					detail += "}";
					json_object_set_new(item, "detail", json_string(detail.c_str()));

					json_object_set_new(item, "uri", json_string(uri.c_str()));

					// Find struct line by searching for "struct <name>"
					size_t structLine = 0;
					std::istringstream iss(documentText);
					std::string lineContent;
					size_t lineNum = 0;
					std::string searchPattern = "struct " + structInfo.name;
					while (std::getline(iss, lineContent)) {
						if (lineContent.find(searchPattern) != std::string::npos) {
							structLine = lineNum;
							break;
						}
						lineNum++;
					}

					// Range
					json_t* range = json_object();
					json_t* start = json_object();
					json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(structLine)));
					json_object_set_new(start, "character", json_integer(0));
					json_t* end = json_object();
					json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(structLine)));
					json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(searchPattern.size())));
					json_object_set_new(range, "start", start);
					json_object_set_new(range, "end", end);
					json_object_set_new(item, "range", range);

					// Selection range (just the struct name)
					json_t* selRange = json_object();
					json_t* selStart = json_object();
					json_object_set_new(selStart, "line", json_integer(static_cast<json_int_t>(structLine)));
					json_object_set_new(selStart, "character", json_integer(7)); // After "struct "
					json_t* selEnd = json_object();
					json_object_set_new(selEnd, "line", json_integer(static_cast<json_int_t>(structLine)));
					json_object_set_new(
							selEnd, "character", json_integer(static_cast<json_int_t>(7 + structInfo.name.size())));
					json_object_set_new(selRange, "start", selStart);
					json_object_set_new(selRange, "end", selEnd);
					json_object_set_new(item, "selectionRange", selRange);

					// Data for supertypes/subtypes
					std::string data = uri + "|" + structInfo.name;
					json_object_set_new(item, "data", json_string(data.c_str()));

					json_array_append_new(items, item);
					break;
				}
			}
		}
	}

	json_object_set_new(response, "result", items);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleSupertypes(const std::string& id, const std::string& itemData) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Quadrate doesn't have struct inheritance, so return empty array
	json_t* items = json_array();

	// Suppress unused parameter warning
	(void)itemData;

	json_object_set_new(response, "result", items);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleSubtypes(const std::string& id, const std::string& itemData) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Quadrate doesn't have struct inheritance, so return empty array
	json_t* items = json_array();

	// Suppress unused parameter warning
	(void)itemData;

	json_object_set_new(response, "result", items);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleOnTypeFormatting(
		const std::string& id, const std::string& uri, size_t line, size_t character, const std::string& ch) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* edits = json_array();

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	if (!documentText.empty()) {
		// Split into lines
		std::vector<std::string> lines;
		std::istringstream iss(documentText);
		std::string lineContent;
		while (std::getline(iss, lineContent)) {
			lines.push_back(lineContent);
		}

		if (ch == "\n" && line > 0 && line <= lines.size()) {
			// Newline was typed - auto-indent based on previous line
			const std::string& prevLine = lines[line - 1];

			// Count leading whitespace of previous line
			size_t indent = 0;
			while (indent < prevLine.size() && (prevLine[indent] == ' ' || prevLine[indent] == '\t')) {
				indent++;
			}

			// Check if previous line ends with '{'
			size_t lastNonSpace = prevLine.size();
			while (lastNonSpace > 0 && (prevLine[lastNonSpace - 1] == ' ' || prevLine[lastNonSpace - 1] == '\t')) {
				lastNonSpace--;
			}
			bool addIndent = (lastNonSpace > 0 && prevLine[lastNonSpace - 1] == '{');

			// Build indentation string
			std::string indentStr = prevLine.substr(0, indent);
			if (addIndent) {
				indentStr += "\t"; // Add one tab for block
			}

			// Only add edit if there's indentation to add
			if (!indentStr.empty()) {
				json_t* edit = json_object();
				json_t* range = json_object();
				json_t* start = json_object();
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(line)));
				json_object_set_new(start, "character", json_integer(0));
				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(line)));
				json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(character)));
				json_object_set_new(range, "start", start);
				json_object_set_new(range, "end", end);
				json_object_set_new(edit, "range", range);
				json_object_set_new(edit, "newText", json_string(indentStr.c_str()));
				json_array_append_new(edits, edit);
			}
		} else if (ch == "}" && line < lines.size()) {
			// Closing brace - check if we should reduce indentation
			const std::string& currentLine = lines[line];

			// Count current leading whitespace
			size_t currentIndent = 0;
			while (currentIndent < currentLine.size() &&
					(currentLine[currentIndent] == ' ' || currentLine[currentIndent] == '\t')) {
				currentIndent++;
			}

			// If there's indentation and the } is at the start (after whitespace), reduce by one tab
			if (currentIndent > 0 && character == currentIndent + 1) {
				// Find matching opening brace to determine correct indentation
				size_t targetIndent = 0;
				if (currentIndent >= 1) {
					// Simple approach: reduce by one tab
					if (currentLine[currentIndent - 1] == '\t') {
						targetIndent = currentIndent - 1;
					} else {
						// Handle spaces (assume 4 spaces = 1 tab)
						targetIndent = currentIndent > 4 ? currentIndent - 4 : 0;
					}
				}

				std::string newIndent;
				for (size_t i = 0; i < targetIndent; i++) {
					newIndent += currentLine[i];
				}

				json_t* edit = json_object();
				json_t* range = json_object();
				json_t* start = json_object();
				json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(line)));
				json_object_set_new(start, "character", json_integer(0));
				json_t* end = json_object();
				json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(line)));
				json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(currentIndent)));
				json_object_set_new(range, "start", start);
				json_object_set_new(range, "end", end);
				json_object_set_new(edit, "range", range);
				json_object_set_new(edit, "newText", json_string(newIndent.c_str()));
				json_array_append_new(edits, edit);
			}
		}
		// For '{', no special formatting needed
	}

	json_object_set_new(response, "result", edits);
	sendMessage(response);
	json_decref(response);
}

void QuadrateLSP::handleLinkedEditingRange(
		const std::string& id, const std::string& uri, size_t line, size_t character) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	// Get document text
	std::string documentText;
	auto docIter = documents_.find(uri);
	if (docIter != documents_.end()) {
		documentText = docIter->second;
	}

	json_t* result = nullptr;

	if (!documentText.empty()) {
		// Get the word at cursor position
		std::string word = getWordAtPosition(documentText, line, character);

		if (!word.empty() && !isKeyword(word) && !isBuiltinOp(word)) {
			// Find the function containing this position
			std::vector<FunctionInfo> functions = extractFunctions(documentText);
			size_t funcStartLine = 0;
			size_t funcEndLine = documentText.size();

			// Split into lines for processing
			std::vector<std::string> lines;
			std::istringstream iss(documentText);
			std::string lineContent;
			while (std::getline(iss, lineContent)) {
				lines.push_back(lineContent);
			}

			// Find enclosing function
			for (const auto& func : functions) {
				if (line >= func.line) {
					// Find function end (closing brace)
					int braceDepth = 0;
					bool foundOpen = false;
					for (size_t i = func.line; i < lines.size(); i++) {
						for (char ch : lines[i]) {
							if (ch == '{') {
								braceDepth++;
								foundOpen = true;
							} else if (ch == '}') {
								braceDepth--;
								if (foundOpen && braceDepth == 0) {
									if (line >= func.line && line <= i) {
										funcStartLine = func.line;
										funcEndLine = i;
									}
									break;
								}
							}
						}
						if (foundOpen && braceDepth == 0) {
							break;
						}
					}
				}
			}

			// Find all occurrences of the word within the function scope
			json_t* ranges = json_array();
			for (size_t i = funcStartLine; i <= funcEndLine && i < lines.size(); i++) {
				const std::string& currLine = lines[i];
				size_t pos = 0;
				while ((pos = currLine.find(word, pos)) != std::string::npos) {
					// Check it's a whole word match
					bool validStart = (pos == 0 || (!std::isalnum(static_cast<unsigned char>(currLine[pos - 1])) &&
														   currLine[pos - 1] != '_'));
					bool validEnd = (pos + word.size() >= currLine.size() ||
									 (!std::isalnum(static_cast<unsigned char>(currLine[pos + word.size()])) &&
											 currLine[pos + word.size()] != '_'));

					if (validStart && validEnd) {
						json_t* range = json_object();
						json_t* start = json_object();
						json_object_set_new(start, "line", json_integer(static_cast<json_int_t>(i)));
						json_object_set_new(start, "character", json_integer(static_cast<json_int_t>(pos)));
						json_t* end = json_object();
						json_object_set_new(end, "line", json_integer(static_cast<json_int_t>(i)));
						json_object_set_new(end, "character", json_integer(static_cast<json_int_t>(pos + word.size())));
						json_object_set_new(range, "start", start);
						json_object_set_new(range, "end", end);
						json_array_append_new(ranges, range);
					}
					pos++;
				}
			}

			// Only return result if we found multiple occurrences
			if (json_array_size(ranges) > 1) {
				result = json_object();
				json_object_set_new(result, "ranges", ranges);
			} else {
				json_decref(ranges);
			}
		}
	}

	if (result) {
		json_object_set_new(response, "result", result);
	} else {
		json_object_set_new(response, "result", json_null());
	}

	sendMessage(response);
	json_decref(response);
}

// Collect .qd files in a directory recursively using std::filesystem
std::vector<std::string> QuadrateLSP::collectWorkspaceFiles(const std::string& dir, int maxDepth) {
	std::vector<std::string> files;
	if (maxDepth <= 0 || dir.empty()) {
		return files;
	}

	try {
		std::filesystem::path dirPath(dir);
		if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
			return files;
		}

		for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
			std::string name = entry.path().filename().string();

			// Skip hidden directories and common non-source directories
			if (name[0] == '.' || name == "node_modules" || name == "build" || name == "dist" || name == "target") {
				continue;
			}

			if (entry.is_directory()) {
				// Recursively collect from subdirectory
				auto subFiles = collectWorkspaceFiles(entry.path().string(), maxDepth - 1);
				files.insert(files.end(), subFiles.begin(), subFiles.end());
			} else if (entry.is_regular_file()) {
				// Check if it's a .qd file
				if (entry.path().extension() == ".qd") {
					files.push_back(entry.path().string());
				}
			}
		}
	} catch (const std::filesystem::filesystem_error&) {
		// Silently ignore filesystem errors (permission denied, etc.)
	}

	return files;
}

void QuadrateLSP::handleWorkspaceSymbols(const std::string& id, const std::string& query) {
	json_t* response = json_object();
	json_object_set_new(response, "jsonrpc", json_string("2.0"));
	json_object_set_new(response, "id", json_integer(std::stoi(id)));

	json_t* symbols = json_array();
	std::set<std::string> processedFiles;

	// Helper lambda to add symbols from a file
	auto processFile = [&](const std::string& filePath, const std::string& docText) {
		std::string uri = "file://" + filePath;

		// Extract functions
		std::vector<FunctionInfo> functions = extractFunctions(docText);
		for (const auto& func : functions) {
			// Filter by query (case-insensitive substring match)
			std::string lowerName = func.name;
			std::string lowerQuery = query;
			std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
			std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

			if (lowerQuery.empty() || lowerName.find(lowerQuery) != std::string::npos) {
				json_t* symbol = json_object();
				json_object_set_new(symbol, "name", json_string(func.name.c_str()));
				json_object_set_new(symbol, "kind", json_integer(12)); // Function

				json_t* location = json_object();
				json_object_set_new(location, "uri", json_string(uri.c_str()));
				json_t* symRange = json_object();
				json_t* symStart = json_object();
				json_object_set_new(symStart, "line", json_integer(static_cast<json_int_t>(func.line)));
				json_object_set_new(symStart, "character", json_integer(0));
				json_t* symEnd = json_object();
				json_object_set_new(symEnd, "line", json_integer(static_cast<json_int_t>(func.line)));
				json_object_set_new(symEnd, "character", json_integer(0));
				json_object_set_new(symRange, "start", symStart);
				json_object_set_new(symRange, "end", symEnd);
				json_object_set_new(location, "range", symRange);
				json_object_set_new(symbol, "location", location);

				json_array_append_new(symbols, symbol);
			}
		}

		// Extract structs
		std::vector<StructInfo> structs = extractStructs(docText);
		for (const auto& st : structs) {
			std::string lowerName = st.name;
			std::string lowerQuery = query;
			std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
			std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

			if (lowerQuery.empty() || lowerName.find(lowerQuery) != std::string::npos) {
				json_t* symbol = json_object();
				json_object_set_new(symbol, "name", json_string(st.name.c_str()));
				json_object_set_new(symbol, "kind", json_integer(23)); // Struct

				json_t* location = json_object();
				json_object_set_new(location, "uri", json_string(uri.c_str()));
				json_t* symRange = json_object();
				json_t* symStart = json_object();
				json_object_set_new(symStart, "line", json_integer(static_cast<json_int_t>(st.line)));
				json_object_set_new(symStart, "character", json_integer(0));
				json_t* symEnd = json_object();
				json_object_set_new(symEnd, "line", json_integer(static_cast<json_int_t>(st.line)));
				json_object_set_new(symEnd, "character", json_integer(0));
				json_object_set_new(symRange, "start", symStart);
				json_object_set_new(symRange, "end", symEnd);
				json_object_set_new(location, "range", symRange);
				json_object_set_new(symbol, "location", location);

				json_array_append_new(symbols, symbol);
			}
		}
	};

	// First, search through all open documents (they have the latest content)
	for (const auto& [docUri, docText] : documents_) {
		std::string filePath = docUri;
		if (filePath.substr(0, 7) == "file://") {
			filePath = filePath.substr(7);
		}
		processedFiles.insert(filePath);
		processFile(filePath, docText);
	}

	// Then search through workspace files that aren't already open
	if (!workspaceRoot_.empty()) {
		std::vector<std::string> workspaceFiles = collectWorkspaceFiles(workspaceRoot_);
		for (const auto& filePath : workspaceFiles) {
			if (processedFiles.find(filePath) != processedFiles.end()) {
				continue; // Already processed as open document
			}

			// Read file from disk
			std::ifstream file(filePath);
			if (file.good()) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				std::string content = buffer.str();
				processFile(filePath, content);
			}
		}
	}

	json_object_set_new(response, "result", symbols);
	sendMessage(response);
	json_decref(response);
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
												docStream << param->displayString();
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
												docStream << param->displayString();
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
													docStream << param->displayString();
												}

												docStream << " -- ";

												// Output parameters
												for (size_t j = 0; j < importedFunc->outputParameters.size(); j++) {
													if (j > 0) {
														docStream << " ";
													}
													const auto* param = importedFunc->outputParameters[j];
													docStream << param->displayString();
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
													sigStream << param->displayString();
												}

												sigStream << " -- ";

												const auto& outputs = funcNode->outputParameters();
												for (size_t j = 0; j < outputs.size(); j++) {
													if (j > 0) {
														sigStream << " ";
													}
													Qd::AstNodeParameter* param =
															static_cast<Qd::AstNodeParameter*>(outputs[j]);
													sigStream << param->displayString();
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

													for (size_t j = 0; j < importedFunc->inputParameters.size(); j++) {
														if (j > 0) {
															sigStream << " ";
														}
														const auto* param = importedFunc->inputParameters[j];
														sigStream << param->displayString();
													}

													sigStream << " -- ";

													for (size_t j = 0; j < importedFunc->outputParameters.size(); j++) {
														if (j > 0) {
															sigStream << " ";
														}
														const auto* param = importedFunc->outputParameters[j];
														sigStream << param->displayString();
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
						detail << param->displayString();
					}
					detail << " -- ";
					const auto& outputs = funcNode->outputParameters();
					for (size_t j = 0; j < outputs.size(); j++) {
						if (j > 0) {
							detail << " ";
						}
						Qd::AstNodeParameter* param = static_cast<Qd::AstNodeParameter*>(outputs[j]);
						detail << param->displayString();
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
						const Qd::AstNodeStructField* field = static_cast<const Qd::AstNodeStructField*>(fields[j]);
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
					json_object_set_new(
							end, "character", json_integer(static_cast<json_int_t>(namespaceName.length())));
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
			// AST uses 1-based lines, convert to 0-based for LSP
			info.line = funcNode->line() > 0 ? funcNode->line() - 1 : 0;

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
				std::string paramStr = param->displayString();
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
				std::string paramStr = param->displayString();
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
					std::string paramStr = param->displayString();
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
					std::string paramStr = param->displayString();
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
			info.line = structNode->line();

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
	qdcli::printVersion("quadlsp");
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
