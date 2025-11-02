#include <cstring>
#include <iostream>
#include <jansson.h>
#include <map>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/error_reporter.h>
#include <string>
#include <vector>

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
			handleInitialize(id);
		} else if (method == "initialized") {
			// Nothing to do
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
			// For full sync, we could re-parse but we'll keep it simple
		} else if (method == "textDocument/didSave") {
			json_t* params = getJsonObject(root, "params");
			if (params) {
				json_t* textDoc = getJsonObject(params, "textDocument");
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					std::string text = getJsonString(params, "text");
					if (!text.empty()) {
						handleDidOpen(uri, text);
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
				if (textDoc) {
					std::string uri = getJsonString(textDoc, "uri");
					handleCompletion(id, uri);
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

	void handleInitialize(const std::string& id) {
		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		json_t* result = json_object();
		json_t* capabilities = json_object();

		json_object_set_new(capabilities, "textDocumentSync", json_integer(1)); // Full sync
		json_object_set_new(capabilities, "documentFormattingProvider", json_true());
		json_object_set_new(capabilities, "completionProvider", json_object());

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

	void publishDiagnostics(const std::string& uri, const std::string& text) {
		// Parse using Ast class
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(text.c_str(), false, nullptr);
		(void)root; // Suppress unused warning

		json_t* notification = json_object();
		json_object_set_new(notification, "jsonrpc", json_string("2.0"));
		json_object_set_new(notification, "method", json_string("textDocument/publishDiagnostics"));

		json_t* params = json_object();
		json_object_set_new(params, "uri", json_string(uri.c_str()));

		json_t* diagnostics = json_array();

		// If there are errors, show a generic diagnostic
		if (ast.hasErrors()) {
			json_t* diag = json_object();

			json_t* range = json_object();
			json_t* start = json_object();
			json_object_set_new(start, "line", json_integer(0));
			json_object_set_new(start, "character", json_integer(0));
			json_t* end = json_object();
			json_object_set_new(end, "line", json_integer(0));
			json_object_set_new(end, "character", json_integer(10));
			json_object_set_new(range, "start", start);
			json_object_set_new(range, "end", end);

			json_object_set_new(diag, "range", range);
			json_object_set_new(diag, "severity", json_integer(1)); // Error
			json_object_set_new(diag, "message", json_string("Syntax error(s) found. Check console for details."));

			json_array_append_new(diagnostics, diag);
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

	void handleCompletion(const std::string& id, const std::string& uri) {
		(void)uri; // Not used yet

		static const char* instructions[] = {"add", "sub", "mul", "div", "dup", "swap", "drop", "over", "rot", "print",
				"prints", "eq", "neq", "lt", "gt", "lte", "gte", "and", "or", "not", "inc", "dec", "abs", "sqrt", "sq",
				"sin", "cos", "tan", "asin", "acos", "atan", "ln", "log10", "pow", "min", "max", "ceil", "floor",
				"round", "if", "for", "switch", "case", "default", "break", "continue", "defer"};

		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		json_t* result = json_object();
		json_object_set_new(result, "isIncomplete", json_false());

		json_t* items = json_array();
		for (size_t i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++) {
			json_t* item = json_object();
			json_object_set_new(item, "label", json_string(instructions[i]));
			json_object_set_new(item, "kind", json_integer(3)); // Function
			json_array_append_new(items, item);
		}

		json_object_set_new(result, "items", items);
		json_object_set_new(response, "result", result);

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

	std::map<std::string, std::string> documents_;
	int messageId_;
};

int main() {
	QuadrateLSP lsp;
	lsp.run();
	return 0;
}
