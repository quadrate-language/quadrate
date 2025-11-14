#include <cstring>
#include <fstream>
#include <iostream>
#include <jansson.h>
#include <map>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/error_reporter.h>
#include <sstream>
#include <string>
#include <vector>

// Default error span length in characters for diagnostic highlighting
static const int ERROR_SPAN_LENGTH = 10;

// Structure to hold function information for completions
struct FunctionInfo {
	std::string name;
	std::vector<std::string> inputParams;  // "name:type" format
	std::vector<std::string> outputParams; // "name:type" format
	std::string signature;				   // Full signature string
	std::string snippet;				   // LSP snippet with placeholders
};

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
		json_object_set_new(capabilities, "hoverProvider", json_true());

		// Enable snippet support in completions
		json_t* completionProvider = json_object();
		json_object_set_new(completionProvider, "resolveProvider", json_false());
		json_object_set_new(capabilities, "completionProvider", completionProvider);

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

		// If there are errors, show detailed diagnostics
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
		static const char* instructions[] = {"add", "sub", "mul", "div", "dup", "swap", "drop", "over", "rot", "print",
				"prints", "eq", "neq", "lt", "gt", "lte", "gte", "and", "or", "not", "inc", "dec", "abs", "sqrt", "sq",
				"sin", "cos", "tan", "asin", "acos", "atan", "ln", "log10", "pow", "min", "max", "ceil", "floor",
				"round", "if", "for", "loop", "switch", "case", "default", "break", "continue", "defer"};

		json_t* response = json_object();
		json_object_set_new(response, "jsonrpc", json_string("2.0"));
		json_object_set_new(response, "id", json_integer(std::stoi(id)));

		json_t* result = json_object();
		json_object_set_new(result, "isIncomplete", json_false());

		json_t* items = json_array();

		// Add built-in instructions
		for (size_t i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++) {
			json_t* item = json_object();
			json_object_set_new(item, "label", json_string(instructions[i]));
			json_object_set_new(item, "kind", json_integer(3)); // Function
			json_object_set_new(item, "detail", json_string("Built-in instruction"));
			json_array_append_new(items, item);
		}

		// Add user-defined functions from the current document
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

		if (!documentText.empty()) {
			std::vector<FunctionInfo> functions = extractFunctions(documentText);

			for (const auto& func : functions) {
				json_t* item = json_object();
				json_object_set_new(item, "label", json_string(func.name.c_str()));
				json_object_set_new(item, "kind", json_integer(3)); // Function

				// Add snippet with placeholders
				json_t* insertTextFormat = json_integer(2); // Snippet format
				json_object_set_new(item, "insertTextFormat", insertTextFormat);
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

		json_object_set_new(result, "items", items);
		json_object_set_new(response, "result", result);

		sendMessage(response);
		json_decref(response);
	}

	std::string getBuiltInDocumentation(const std::string& word) {
		static const std::map<std::string, std::string> docs = {
			{"add", "Add two numbers from the stack.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes their sum."},
			{"sub", "Subtract top from second.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes `a - b`."},
			{"mul", "Multiply two numbers.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes their product."},
			{"div", "Divide second by top.\n\n**Stack effect:** `a b -- result`\n\nPops two values, pushes `a / b`."},
			{"dup", "Duplicate top of stack.\n\n**Stack effect:** `a -- a a`\n\nDuplicates the top stack value."},
			{"swap", "Swap top two values.\n\n**Stack effect:** `a b -- b a`\n\nSwaps the top two stack values."},
			{"drop", "Remove top of stack.\n\n**Stack effect:** `a --`\n\nRemoves the top value from the stack."},
			{"over", "Copy second item to top.\n\n**Stack effect:** `a b -- a b a`\n\nCopies the second value to the top."},
			{"rot", "Rotate top three items.\n\n**Stack effect:** `a b c -- b c a`\n\nRotates the top three values."},
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
			{"for", "Loop construct.\n\n**Syntax:** `start end for { ... }`"},
			{"loop", "Infinite loop.\n\n**Syntax:** `loop { ... }`"},
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
		while (start > 0 && (isalnum(targetLine[start - 1]) || targetLine[start - 1] == '_' || targetLine[start - 1] == ':')) {
			start--;
		}

		// Move end forward to end of word
		while (end < targetLine.length() && (isalnum(targetLine[end]) || targetLine[end] == '_' || targetLine[end] == ':')) {
			end++;
		}

		if (end > start) {
			return targetLine.substr(start, end - start);
		}
		return "";
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
									if (i > 0) docStream << ", ";
									docStream << "`" << func.inputParams[i] << "`";
								}
								docStream << "\n\n";
							}
							if (!func.outputParams.empty()) {
								docStream << "**Outputs:** ";
								for (size_t i = 0; i < func.outputParams.size(); i++) {
									if (i > 0) docStream << ", ";
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
				}
			}
		}

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

	std::map<std::string, std::string> documents_;
	[[maybe_unused]] int messageId_;
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
}

void printVersion() {
	std::cout << "quadlsp version 0.1.0\n";
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
