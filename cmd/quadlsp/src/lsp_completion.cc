// Completion handlers for QuadrateLSP
// Split from main.cc for maintainability

#include "lsp_impl.h"
#include <fstream>
#include <qc/ast.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
#include <sstream>

void QuadrateLSP::handleCompletion(const std::string& id, const std::string& uri, size_t line, size_t character) {
	static const char* instructions[] = {"add", "sub", "mul", "div", "dup", "swap", "drop", "over", "rot", "print",
			"prints", "eq", "neq", "lt", "gt", "lte", "gte", "and", "or", "not", "inc", "dec", "abs", "sqrt", "sq",
			"sin", "cos", "tan", "asin", "acos", "atan", "ln", "log10", "pow", "min", "max", "ceil", "floor", "round",
			"if", "for", "while", "loop", "switch", "case", "default", "break", "continue", "defer", "free", "struct",
			"pub"};

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

				// Build label with generic type params if applicable
				std::string label = func.name;
				if (func.isGeneric) {
					label += "<";
					for (size_t i = 0; i < func.typeParams.size(); i++) {
						if (i > 0) {
							label += ", ";
						}
						label += func.typeParams[i];
					}
					label += ">";
				}
				json_object_set_new(item, "label", json_string(label.c_str()));
				json_object_set_new(item, "kind", json_integer(func.isMethod ? 2 : 3)); // Method=2, Function=3

				// Add snippet with placeholders
				json_object_set_new(item, "insertTextFormat", json_integer(2)); // Snippet format
				json_object_set_new(item, "insertText", json_string(func.snippet.c_str()));

				// Add signature as detail
				json_object_set_new(item, "detail", json_string(func.signature.c_str()));

				// Build documentation
				std::ostringstream docStream;
				docStream << "**Module:** `" << modulePrefix << "`\n\n";
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
				docStream << "**Signature:**\n```quadrate\n" << func.signature << "\n```\n\n";
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

				// Build label with generic type params if applicable
				std::string structLabel = structInfo.name;
				if (structInfo.isGeneric) {
					structLabel += "<";
					for (size_t i = 0; i < structInfo.typeParams.size(); i++) {
						if (i > 0) {
							structLabel += ", ";
						}
						structLabel += structInfo.typeParams[i];
					}
					structLabel += ">";
				}
				json_object_set_new(item, "label", json_string(structLabel.c_str()));
				json_object_set_new(item, "kind", json_integer(22)); // Struct kind

				// Build documentation showing struct fields
				std::ostringstream docStream;
				docStream << "**Module:** `" << modulePrefix << "`\n\n";
				docStream << "**Struct:** `" << structLabel << "`\n\n";
				if (structInfo.isGeneric) {
					docStream << "**Type parameters:** `<";
					for (size_t i = 0; i < structInfo.typeParams.size(); i++) {
						if (i > 0) {
							docStream << ", ";
						}
						docStream << structInfo.typeParams[i];
					}
					docStream << ">`\n\n";
				}
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

				// Build label with generic type params if applicable
				std::string label = func.name;
				if (func.isGeneric) {
					label += "<";
					for (size_t i = 0; i < func.typeParams.size(); i++) {
						if (i > 0) {
							label += ", ";
						}
						label += func.typeParams[i];
					}
					label += ">";
				}
				json_object_set_new(item, "label", json_string(label.c_str()));
				json_object_set_new(item, "kind", json_integer(func.isMethod ? 2 : 3)); // Method=2, Function=3

				// Add snippet with placeholders
				json_object_set_new(item, "insertTextFormat", json_integer(2)); // Snippet format
				json_object_set_new(item, "insertText", json_string(func.snippet.c_str()));

				// Add signature as detail and documentation
				json_object_set_new(item, "detail", json_string(func.signature.c_str()));

				// Build documentation showing what needs to be on the stack
				std::ostringstream docStream;
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
				docStream << "**Signature:**\n```quadrate\n" << func.signature << "\n```\n\n";
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

			// Build label with generic type params if applicable
			std::string structLabel = structInfo.name;
			if (structInfo.isGeneric) {
				structLabel += "<";
				for (size_t i = 0; i < structInfo.typeParams.size(); i++) {
					if (i > 0) {
						structLabel += ", ";
					}
					structLabel += structInfo.typeParams[i];
				}
				structLabel += ">";
			}
			json_object_set_new(item, "label", json_string(structLabel.c_str()));
			json_object_set_new(item, "kind", json_integer(22)); // Struct kind

			// Build documentation showing struct fields
			std::ostringstream docStream;
			docStream << "**Struct:** `" << structLabel << "`\n\n";
			if (structInfo.isGeneric) {
				docStream << "**Type parameters:** `<";
				for (size_t i = 0; i < structInfo.typeParams.size(); i++) {
					if (i > 0) {
						docStream << ", ";
					}
					docStream << structInfo.typeParams[i];
				}
				docStream << ">`\n\n";
			}
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

std::string QuadrateLSP::getModulePrefixAtPosition(const std::string& text, size_t line, size_t character) {
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

std::string QuadrateLSP::getFieldAccessVariableAtPosition(const std::string& text, size_t line, size_t character) {
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

std::vector<std::pair<std::string, std::string>> QuadrateLSP::findStructFields(
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

std::vector<FunctionInfo> QuadrateLSP::extractModuleFunctions(const std::string& modulePath) {
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

std::vector<StructInfo> QuadrateLSP::extractModuleStructs(const std::string& modulePath) {
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

std::vector<ConstantInfo> QuadrateLSP::extractModuleConstants(const std::string& modulePath) {
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
