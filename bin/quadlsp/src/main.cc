#include <cstring>
#include <iostream>
#include <map>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/error_reporter.h>
#include <sstream>
#include <string>
#include <vector>

// Simple JSON builder (minimal implementation for LSP)
class JsonBuilder {
public:
	JsonBuilder() {
	}

	void startObject() {
		json_ << "{";
		first_ = true;
	}

	void endObject() {
		json_ << "}";
		first_ = false; // Content was added
	}

	void startArray() {
		json_ << "[";
		first_ = true;
	}

	void endArray() {
		json_ << "]";
		first_ = false; // Content was added
	}

	void addKey(const std::string& key) {
		if (!first_) {
			json_ << ",";
		}
		json_ << "\"" << escapeString(key) << "\":";
		first_ = false;
	}

	void addString(const std::string& value) {
		json_ << "\"" << escapeString(value) << "\"";
	}

	void addNumber(int value) {
		json_ << value;
	}

	void addBool(bool value) {
		json_ << (value ? "true" : "false");
	}

	void addNull() {
		json_ << "null";
	}

	void addRaw(const std::string& raw) {
		json_ << raw;
	}

	std::string toString() {
		return json_.str();
	}

	void reset() {
		json_.str("");
		json_.clear();
		first_ = true;
	}

	void setNotFirst() {
		first_ = false;
	}

private:
	std::string escapeString(const std::string& str) {
		std::string result;
		for (char c : str) {
			switch (c) {
			case '"':
				result += "\\\"";
				break;
			case '\\':
				result += "\\\\";
				break;
			case '\n':
				result += "\\n";
				break;
			case '\r':
				result += "\\r";
				break;
			case '\t':
				result += "\\t";
				break;
			default:
				result += c;
			}
		}
		return result;
	}

	std::ostringstream json_;
	bool first_ = true;
};

// Simple JSON parser (minimal implementation for LSP)
class JsonParser {
public:
	explicit JsonParser(const std::string& json) : json_(json), pos_(0) {
	}

	std::map<std::string, std::string> parseObject() {
		std::map<std::string, std::string> result;
		skipWhitespace();
		if (json_[pos_] != '{') {
			return result;
		}
		pos_++;

		while (pos_ < json_.length()) {
			skipWhitespace();
			if (json_[pos_] == '}') {
				pos_++;
				break;
			}
			if (json_[pos_] == ',') {
				pos_++;
				continue;
			}

			// Parse key
			std::string key = parseString();
			skipWhitespace();
			if (json_[pos_] == ':') {
				pos_++;
			}
			skipWhitespace();

			// Parse value (simplified - just capture as string)
			std::string value = parseValue();
			result[key] = value;
		}

		return result;
	}

	std::string getString(const std::map<std::string, std::string>& obj, const std::string& key) {
		auto it = obj.find(key);
		if (it != obj.end()) {
			std::string val = it->second;
			// Remove quotes if present
			if (val.length() >= 2 && val[0] == '"' && val[val.length() - 1] == '"') {
				return val.substr(1, val.length() - 2);
			}
			return val;
		}
		return "";
	}

	std::map<std::string, std::string> getObject(
			const std::map<std::string, std::string>& obj, const std::string& key) {
		auto it = obj.find(key);
		if (it != obj.end()) {
			JsonParser parser(it->second);
			return parser.parseObject();
		}
		return std::map<std::string, std::string>();
	}

private:
	void skipWhitespace() {
		while (pos_ < json_.length() && std::isspace(static_cast<unsigned char>(json_[pos_]))) {
			pos_++;
		}
	}

	std::string parseString() {
		std::string result;
		if (json_[pos_] == '"') {
			pos_++;
			while (pos_ < json_.length() && json_[pos_] != '"') {
				if (json_[pos_] == '\\' && pos_ + 1 < json_.length()) {
					pos_++;
					switch (json_[pos_]) {
					case 'n':
						result += '\n';
						break;
					case 'r':
						result += '\r';
						break;
					case 't':
						result += '\t';
						break;
					default:
						result += json_[pos_];
					}
				} else {
					result += json_[pos_];
				}
				pos_++;
			}
			if (json_[pos_] == '"') {
				pos_++;
			}
		}
		return result;
	}

	std::string parseValue() {
		skipWhitespace();
		size_t start = pos_;

		if (json_[pos_] == '"') {
			// String value
			pos_++;
			while (pos_ < json_.length() && json_[pos_] != '"') {
				if (json_[pos_] == '\\') {
					pos_++;
				}
				pos_++;
			}
			pos_++;
			return json_.substr(start, pos_ - start);
		} else if (json_[pos_] == '{') {
			// Object value
			int braceCount = 0;
			while (pos_ < json_.length()) {
				if (json_[pos_] == '{') {
					braceCount++;
				} else if (json_[pos_] == '}') {
					braceCount--;
					if (braceCount == 0) {
						pos_++;
						return json_.substr(start, pos_ - start);
					}
				}
				pos_++;
			}
		} else if (json_[pos_] == '[') {
			// Array value
			int bracketCount = 0;
			while (pos_ < json_.length()) {
				if (json_[pos_] == '[') {
					bracketCount++;
				} else if (json_[pos_] == ']') {
					bracketCount--;
					if (bracketCount == 0) {
						pos_++;
						return json_.substr(start, pos_ - start);
					}
				}
				pos_++;
			}
		} else {
			// Number, bool, or null
			while (pos_ < json_.length() && json_[pos_] != ',' && json_[pos_] != '}' && json_[pos_] != ']') {
				pos_++;
			}
			return json_.substr(start, pos_ - start);
		}

		return "";
	}

	const std::string& json_;
	size_t pos_;
};

// LSP Server
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

	void sendMessage(const std::string& message) {
		std::cout << "Content-Length: " << message.length() << "\r\n\r\n" << message << std::flush;
	}

	void handleMessage(const std::string& message) {
		JsonParser parser(message);
		auto obj = parser.parseObject();

		std::string method = parser.getString(obj, "method");
		std::string id = parser.getString(obj, "id");

		if (method == "initialize") {
			handleInitialize(id);
		} else if (method == "initialized") {
			// Nothing to do
		} else if (method == "textDocument/didOpen") {
			auto params = parser.getObject(obj, "params");
			auto textDoc = parser.getObject(params, "textDocument");
			std::string uri = parser.getString(textDoc, "uri");
			std::string text = parser.getString(textDoc, "text");
			handleDidOpen(uri, text);
		} else if (method == "textDocument/didChange") {
			auto params = parser.getObject(obj, "params");
			auto textDoc = parser.getObject(params, "textDocument");
			std::string uri = parser.getString(textDoc, "uri");
			// Get content changes (simplified - assume full sync)
			std::string changes = parser.getString(params, "contentChanges");
			// For now, we'll re-parse on save
		} else if (method == "textDocument/didSave") {
			auto params = parser.getObject(obj, "params");
			auto textDoc = parser.getObject(params, "textDocument");
			std::string uri = parser.getString(textDoc, "uri");
			std::string text = parser.getString(params, "text");
			if (!text.empty()) {
				handleDidOpen(uri, text);
			}
		} else if (method == "textDocument/formatting") {
			auto params = parser.getObject(obj, "params");
			auto textDoc = parser.getObject(params, "textDocument");
			std::string uri = parser.getString(textDoc, "uri");
			handleFormatting(id, uri);
		} else if (method == "textDocument/completion") {
			auto params = parser.getObject(obj, "params");
			auto textDoc = parser.getObject(params, "textDocument");
			std::string uri = parser.getString(textDoc, "uri");
			handleCompletion(id, uri);
		} else if (method == "shutdown") {
			handleShutdown(id);
		} else if (method == "exit") {
			exit(0);
		}
	}

	void handleInitialize(const std::string& id) {
		JsonBuilder json;
		json.startObject();
		json.addKey("jsonrpc");
		json.addString("2.0");
		json.addKey("id");
		json.addNumber(std::stoi(id));
		json.addKey("result");
		json.startObject();
		json.addKey("capabilities");
		json.startObject();
		json.addKey("textDocumentSync");
		json.addNumber(1); // Full sync
		json.addKey("documentFormattingProvider");
		json.addBool(true);
		json.addKey("completionProvider");
		json.startObject();
		json.endObject();
		json.endObject();
		json.addKey("serverInfo");
		json.startObject();
		json.addKey("name");
		json.addString("quadlsp");
		json.addKey("version");
		json.addString("0.1.0");
		json.endObject();
		json.endObject();
		json.endObject();

		sendMessage(json.toString());
	}

	void handleDidOpen(const std::string& uri, const std::string& text) {
		documents_[uri] = text;
		publishDiagnostics(uri, text);
	}

	void publishDiagnostics(const std::string& uri, const std::string& text) {
		// Parse using Ast class
		// Note: Ast::generate creates its own ErrorReporter internally and prints errors to stderr
		// For a production LSP, we'd want to modify Ast to accept an ErrorReporter parameter
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(text.c_str(), false, nullptr);

		JsonBuilder json;
		json.startObject();
		json.addKey("jsonrpc");
		json.addString("2.0");
		json.addKey("method");
		json.addString("textDocument/publishDiagnostics");
		json.addKey("params");
		json.startObject();
		json.addKey("uri");
		json.addString(uri);
		json.addKey("diagnostics");
		json.startArray();

		// For now, if there are errors, show a generic diagnostic
		// TODO: Modify Ast class to accept custom ErrorReporter for detailed diagnostics
		if (ast.hasErrors()) {
			json.startObject();
			json.addKey("range");
			json.startObject();
			json.addKey("start");
			json.startObject();
			json.addKey("line");
			json.addNumber(0);
			json.addKey("character");
			json.addNumber(0);
			json.endObject();
			json.addKey("end");
			json.startObject();
			json.addKey("line");
			json.addNumber(0);
			json.addKey("character");
			json.addNumber(10);
			json.endObject();
			json.endObject();
			json.addKey("severity");
			json.addNumber(1); // Error
			json.addKey("message");
			json.addString("Syntax error(s) found. Check console for details.");
			json.endObject();
		}

		json.endArray();
		json.endObject();
		json.endObject();

		sendMessage(json.toString());

		// Ast destructor will clean up the root node
		(void)root; // Suppress unused warning
	}

	void handleFormatting(const std::string& id, const std::string& uri) {
		(void)uri; // Not used yet
		// For formatting, we'd need to integrate with quadfmt
		// For now, return empty edit list
		JsonBuilder json;
		json.startObject();
		json.addKey("jsonrpc");
		json.addString("2.0");
		json.addKey("id");
		json.addNumber(std::stoi(id));
		json.addKey("result");
		json.startArray();
		json.endArray();
		json.endObject();

		sendMessage(json.toString());
	}

	void handleCompletion(const std::string& id, const std::string& uri) {
		(void)uri; // Not used yet
		// Return built-in instruction completions
		static const char* instructions[] = {"add", "sub", "mul", "div", "dup", "swap", "drop", "over", "rot", "print",
				"prints", "eq", "neq", "lt", "gt", "lte", "gte", "and", "or", "not", "inc", "dec", "abs", "sqrt", "sq",
				"sin", "cos", "tan", "asin", "acos", "atan", "ln", "log10", "pow", "min", "max", "ceil", "floor",
				"round", "if", "for", "switch", "case", "default", "break", "continue", "defer"};

		JsonBuilder json;
		json.startObject();
		json.addKey("jsonrpc");
		json.addString("2.0");
		json.addKey("id");
		json.addNumber(std::stoi(id));
		json.addKey("result");
		json.startObject();
		json.addKey("isIncomplete");
		json.addBool(false);
		json.addKey("items");
		json.startArray();

		bool firstItem = true;
		for (size_t i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++) {
			if (!firstItem) {
				json.addRaw(",");
			}
			firstItem = false;
			json.startObject();
			json.addKey("label");
			json.addString(instructions[i]);
			json.addKey("kind");
			json.addNumber(3); // Function
			json.endObject();
		}

		json.endArray();
		json.endObject();
		json.endObject();

		sendMessage(json.toString());
	}

	void handleShutdown(const std::string& id) {
		JsonBuilder json;
		json.startObject();
		json.addKey("jsonrpc");
		json.addString("2.0");
		json.addKey("id");
		json.addNumber(std::stoi(id));
		json.addKey("result");
		json.addNull();
		json.endObject();

		sendMessage(json.toString());
	}

	std::map<std::string, std::string> documents_;
	int messageId_;
};

int main() {
	QuadrateLSP lsp;
	lsp.run();
	return 0;
}
