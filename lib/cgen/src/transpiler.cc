#include <cgen/transpiler.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_comment.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_loop.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_use.h>
#include <qc/ast_printer.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <sstream>

namespace Qd {
	// Map parameter type string to qd_stack_type constant name
	static const char* mapTypeToStackType(const std::string& paramType) {
		if (paramType.empty()) {
			return "QD_STACK_TYPE_PTR"; // Untyped - skip type check
		} else if (paramType == "i") {
			return "QD_STACK_TYPE_INT";
		} else if (paramType == "f") {
			return "QD_STACK_TYPE_FLOAT";
		} else if (paramType == "s") {
			return "QD_STACK_TYPE_STR";
		} else if (paramType == "p") {
			return "QD_STACK_TYPE_PTR";
		} else {
			return "QD_STACK_TYPE_PTR"; // Unknown type - skip check
		}
	}

	// Helper to generate stack size check
	static void emitStackSizeCheck(
			std::stringstream& out, size_t required, const char* operation, const std::string& makeIndent) {
		out << makeIndent << "if (qd_stack_size(ctx->st) < " << required << ") {\n";
		out << makeIndent << "    fprintf(stderr, \"Fatal error in " << operation << ": Stack underflow (requires "
			<< required << " value" << (required != 1 ? "s" : "") << ", have %zu)\\n\", qd_stack_size(ctx->st));\n";
		out << makeIndent << "    abort();\n";
		out << makeIndent << "}\n";
	}

	// Helper to generate stack pop with error checking
	static void emitStackPop(std::stringstream& out, const std::string& varName, const char* operation,
			const std::string& makeIndent, const char* errorMsg) {
		out << makeIndent << "qd_stack_element_t " << varName << ";\n";
		out << makeIndent << "qd_stack_error " << varName << "_err = qd_stack_pop(ctx->st, &" << varName << ");\n";
		out << makeIndent << "if (" << varName << "_err != QD_STACK_OK) {\n";
		out << makeIndent << "    fprintf(stderr, \"Fatal error in " << operation << ": " << errorMsg << "\\n\");\n";
		out << makeIndent << "    abort();\n";
		out << makeIndent << "}\n";
	}

	// Helper to process escape sequences in Quadrate string literals
	// Converts \n, \r, \t, \\, \" to actual characters
	static std::string unescapeQuadrateString(const std::string& str) {
		std::string result;
		result.reserve(str.size());
		for (size_t i = 0; i < str.size(); ++i) {
			if (str[i] == '\\' && i + 1 < str.size()) {
				switch (str[i + 1]) {
				case 'n':
					result += '\n';
					++i;
					break;
				case 'r':
					result += '\r';
					++i;
					break;
				case 't':
					result += '\t';
					++i;
					break;
				case '\\':
					result += '\\';
					++i;
					break;
				case '"':
					result += '"';
					++i;
					break;
				default:
					result += str[i];
					break;
				}
			} else {
				result += str[i];
			}
		}
		return result;
	}

	// Helper to escape string literals for safe C code generation
	// Prevents code injection via malicious string literals
	static std::string escapeStringForC(const std::string& str) {
		std::string result;
		result.reserve(str.size());
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
				break;
			}
		}
		return result;
	}

	// Helper to validate module names
	// Prevents path traversal attacks in use statements
	static bool isValidModuleName(const std::string& name) {
		if (name.empty()) {
			return false;
		}
		// Check for path traversal sequences
		if (name.find("..") != std::string::npos) {
			return false;
		}
		// Check for absolute or relative paths
		if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
			return false;
		}
		// Check for leading dot (hidden files)
		if (name[0] == '.') {
			return false;
		}
		// Only allow alphanumeric characters and underscores
		for (char c : name) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
				return false;
			}
		}
		return true;
	}

	void traverse(IAstNode* node, const char* packageName, std::stringstream& out, int indent, int& varCounter,
			const std::string& currentForIterator = "", std::vector<IAstNode*>* deferStatements = nullptr,
			const std::unordered_map<std::string, bool>& throwsMap = {}) {
		if (node == nullptr) {
			return;
		}

		auto makeIndent = [](int level) { return std::string(static_cast<size_t>(level * 4), ' '); };

		switch (node->type()) {
		case IAstNode::Type::UNKNOWN:
			// Unknown node type, skip
			break;
		case IAstNode::Type::PROGRAM:
			out << "// Program\n";
			break;
		case IAstNode::Type::BLOCK:
			out << makeIndent(indent) << "{\n";
			break;
		case IAstNode::Type::FUNCTION_DECLARATION: {
			AstNodeFunctionDeclaration* funcDecl = static_cast<AstNodeFunctionDeclaration*>(node);
			out << "\n"
				<< makeIndent(indent) << "qd_exec_result usr_" << packageName << "_" << funcDecl->name()
				<< "(qd_context* ctx) {\n";

			// Generate type check for input parameters
			if (!funcDecl->inputParameters().empty()) {
				out << makeIndent(indent + 1) << "qd_stack_type input_types[] = {";
				for (size_t i = 0; i < funcDecl->inputParameters().size(); i++) {
					if (i > 0) {
						out << ", ";
					}
					AstNodeParameter* param = static_cast<AstNodeParameter*>(funcDecl->inputParameters()[i]);
					out << mapTypeToStackType(param->typeString());
				}
				out << "};\n";
				out << makeIndent(indent + 1) << "qd_check_stack(ctx, " << funcDecl->inputParameters().size()
					<< ", input_types, __func__);\n\n";
			}

			// Collect defer statements while traversing function body
			std::vector<IAstNode*> localDeferStatements;
			traverse(funcDecl->body(), packageName, out, indent + 1, varCounter, currentForIterator,
					&localDeferStatements, throwsMap);

			// Emit defer statements in reverse order (LIFO) before done label
			out << "\n" << makeIndent(indent) << "qd_lbl_done:;\n";
			for (auto it = localDeferStatements.rbegin(); it != localDeferStatements.rend(); ++it) {
				AstNodeDefer* deferNode = static_cast<AstNodeDefer*>(*it);
				// Traverse defer body without collecting more defers (nested defers not supported)
				for (size_t i = 0; i < deferNode->childCount(); i++) {
					IAstNode* child = deferNode->child(i);
					// If the child is a block, traverse its children directly to avoid extra braces
					if (child && child->type() == IAstNode::Type::BLOCK) {
						for (size_t j = 0; j < child->childCount(); j++) {
							traverse(child->child(j), packageName, out, indent + 1, varCounter, currentForIterator,
									nullptr, throwsMap);
						}
					} else {
						traverse(child, packageName, out, indent + 1, varCounter, currentForIterator, nullptr,
								throwsMap);
					}
				}
			}

			// Generate type check for output parameters
			if (!funcDecl->outputParameters().empty()) {
				out << makeIndent(indent + 1) << "qd_stack_type output_types[] = {";
				for (size_t i = 0; i < funcDecl->outputParameters().size(); i++) {
					if (i > 0) {
						out << ", ";
					}
					AstNodeParameter* param = static_cast<AstNodeParameter*>(funcDecl->outputParameters()[i]);
					out << mapTypeToStackType(param->typeString());
				}
				out << "};\n";
				out << makeIndent(indent + 1) << "qd_check_stack(ctx, " << funcDecl->outputParameters().size()
					<< ", output_types, __func__);\n";
			}

			out << makeIndent(indent + 1) << "return (qd_exec_result){0};\n";
			out << makeIndent(indent) << "}\n";
			return; // Don't traverse children again
		}
		case IAstNode::Type::VARIABLE_DECLARATION:
			// TODO: Handle variable declaration
			break;
		case IAstNode::Type::EXPRESSION_STATEMENT:
			// TODO: Handle expression statement
			break;
		case IAstNode::Type::IF_STATEMENT: {
			AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(node);
			int64_t currentVar = varCounter++;
			std::string var = "qd_var_" + std::to_string(currentVar);

			// Check stack has enough values
			emitStackSizeCheck(out, 1, "if", makeIndent(indent));

			// Pop the condition value from the stack
			emitStackPop(out, var, "if", makeIndent(indent), "Failed to pop value");

			// Check the condition (non-zero integer means true)
			out << makeIndent(indent) << "if (" << var << ".type == QD_STACK_TYPE_INT && " << var
				<< ".value.i != 0) {\n";

			// Then block
			if (ifStmt->thenBody()) {
				traverse(ifStmt->thenBody(), packageName, out, indent + 1, varCounter, currentForIterator,
						deferStatements, throwsMap);
			}
			out << makeIndent(indent) << "}";

			// Else block (if present)
			if (ifStmt->elseBody()) {
				out << " else {\n";
				traverse(ifStmt->elseBody(), packageName, out, indent + 1, varCounter, currentForIterator,
						deferStatements, throwsMap);
				out << makeIndent(indent) << "}";
			}
			out << "\n";
			return; // Don't traverse children again
		}
		case IAstNode::Type::FOR_STATEMENT: {
			AstNodeForStatement* forStmt = static_cast<AstNodeForStatement*>(node);
			int64_t currentVar = varCounter++;
			std::string varStart = "qd_var_" + std::to_string(currentVar) + "_start";
			std::string varEnd = "qd_var_" + std::to_string(currentVar) + "_end";
			std::string varStep = "qd_var_" + std::to_string(currentVar) + "_step";
			std::string varI = "qd_var_" + std::to_string(currentVar) + "_i";

			// Check stack has enough values
			emitStackSizeCheck(out, 3, "for", makeIndent(indent));

			// Pop step, end, start from the stack (in reverse order)
			emitStackPop(out, varStep, "for", makeIndent(indent), "Failed to pop step value");
			emitStackPop(out, varEnd, "for", makeIndent(indent), "Failed to pop end value");
			emitStackPop(out, varStart, "for", makeIndent(indent), "Failed to pop start value");

			// Generate C for loop with the values
			out << makeIndent(indent) << "if (" << varStart << ".type == QD_STACK_TYPE_INT && " << varEnd
				<< ".type == QD_STACK_TYPE_INT && " << varStep << ".type == QD_STACK_TYPE_INT) {\n";
			out << makeIndent(indent + 1) << "for (int64_t " << varI << " = " << varStart << ".value.i; " << varI
				<< " < " << varEnd << ".value.i; " << varI << " += " << varStep << ".value.i) {\n";

			// Loop body - pass iterator variable name for $ handling
			if (forStmt->body()) {
				traverse(forStmt->body(), packageName, out, indent + 2, varCounter, varI, deferStatements, throwsMap);
			}

			out << makeIndent(indent + 1) << "}\n";
			out << makeIndent(indent) << "}\n";
			return; // Don't traverse children again
		}
		case IAstNode::Type::LOOP_STATEMENT: {
			AstNodeLoopStatement* loopStmt = static_cast<AstNodeLoopStatement*>(node);

			// Generate infinite while loop
			out << makeIndent(indent) << "while (1) {\n";

			// Loop body (no iterator variable for infinite loop)
			if (loopStmt->body()) {
				traverse(loopStmt->body(), packageName, out, indent + 1, varCounter, "", deferStatements, throwsMap);
			}

			out << makeIndent(indent) << "}\n";
			return; // Don't traverse children again
		}
		case IAstNode::Type::SWITCH_STATEMENT:
			// TODO: Handle switch statement
			break;
		case IAstNode::Type::CASE_STATEMENT:
			// TODO: Handle case statement
			break;
		case IAstNode::Type::RETURN_STATEMENT:
			out << makeIndent(indent) << "goto qd_lbl_done;\n";
			break;
		case IAstNode::Type::BREAK_STATEMENT:
			out << makeIndent(indent) << "break;\n";
			break;
		case IAstNode::Type::CONTINUE_STATEMENT:
			out << makeIndent(indent) << "continue;\n";
			break;
		case IAstNode::Type::DEFER_STATEMENT:
			// Collect defer statement to be executed at function end
			if (deferStatements != nullptr) {
				deferStatements->push_back(node);
			}
			// Don't traverse children now - they'll be traversed when emitting defers
			return;
		case IAstNode::Type::BINARY_EXPRESSION:
			// TODO: Handle binary expression
			break;
		case IAstNode::Type::UNARY_EXPRESSION:
			// TODO: Handle unary expression
			break;
		case IAstNode::Type::LITERAL: {
			AstNodeLiteral* literal = static_cast<AstNodeLiteral*>(node);
			switch (literal->literalType()) {
			case AstNodeLiteral::LiteralType::INTEGER:
				out << makeIndent(indent) << "qd_push_i(ctx, (int64_t)" << literal->value() << ");\n";
				break;
			case AstNodeLiteral::LiteralType::FLOAT:
				out << makeIndent(indent) << "qd_push_f(ctx, (double)" << literal->value() << ");\n";
				break;
			case AstNodeLiteral::LiteralType::STRING: {
				// Extract the string content (remove surrounding quotes)
				std::string rawValue = literal->value();
				std::string content;
				if (rawValue.size() >= 2 && rawValue.front() == '"' && rawValue.back() == '"') {
					content = rawValue.substr(1, rawValue.size() - 2);
				} else {
					content = rawValue;
				}
				// Process escape sequences in Quadrate string
				std::string unescaped = unescapeQuadrateString(content);
				// Escape the content for safe C code generation
				std::string escaped = escapeStringForC(unescaped);
				out << makeIndent(indent) << "qd_push_s(ctx, \"" << escaped << "\");\n";
				break;
			}
			}
			break;
		}
		case IAstNode::Type::IDENTIFIER: {
			AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
			// Handle $ as iterator variable in for loops
			if (ident->name() == "$") {
				if (!currentForIterator.empty()) {
					out << makeIndent(indent) << "qd_push_i(ctx, " << currentForIterator << ");\n";
				} else {
					// $ outside of for loop - error or ignore?
					// For now, generate nothing (semantic validator should catch this)
				}
			} else {
				// Call the function
				out << makeIndent(indent) << "usr_" << packageName << "_" << ident->name() << "(ctx);\n";

				// Check if this function throws
				auto throwsIt = throwsMap.find(ident->name());
				bool functionThrows = (throwsIt != throwsMap.end() && throwsIt->second);

				if (functionThrows) {
					if (ident->abortOnError()) {
						// ! operator: check error and abort if set
						out << makeIndent(indent) << "if (ctx->has_error) {\n";
						out << makeIndent(indent + 1) << "fprintf(stderr, \"Fatal error: function '" << ident->name()
							<< "' failed\\n\");\n";
						out << makeIndent(indent + 1) << "abort();\n";
						out << makeIndent(indent) << "}\n";
					} else if (ident->checkError()) {
						// ? operator: push success status
						std::string varName = "qd_success_" + std::to_string(varCounter++);
						out << makeIndent(indent)
							<< "// Check error and push success status (1 = success, 0 = error)\n";
						out << makeIndent(indent) << "int64_t " << varName << " = ctx->has_error ? 0 : 1;\n";
						out << makeIndent(indent) << "ctx->has_error = false; // Clear error flag\n";
						out << makeIndent(indent) << "qd_stack_push_int(ctx->st, " << varName << ");\n";
					} else {
						// No operator: automatically push error flag (will be checked by 'if')
						std::string varName = "qd_success_" + std::to_string(varCounter++);
						out << makeIndent(indent)
							<< "// Fallible function - automatically push error status flag\n";
						out << makeIndent(indent) << "int64_t " << varName << " = ctx->has_error ? 0 : 1;\n";
						out << makeIndent(indent) << "ctx->has_error = false; // Clear error flag\n";
						out << makeIndent(indent) << "qd_stack_push_int(ctx->st, " << varName << ");\n";
					}
				}
			}
			break;
		}
		case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
			AstNodeFunctionPointerReference* funcPtr = static_cast<AstNodeFunctionPointerReference*>(node);
			// Push the function pointer onto the stack
			out << makeIndent(indent) << "qd_push_p(ctx, (void*)usr_" << packageName << "_" << funcPtr->functionName()
				<< ");\n";
			break;
		}
		case IAstNode::Type::INSTRUCTION: {
			AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(node);
			// Map aliases to their actual function names
			const char* instrName = instr->name().c_str();
			if (strcmp(instrName, ".") == 0) {
				instrName = "print"; // Forth-style print
			} else if (strcmp(instrName, "/") == 0) {
				instrName = "div"; // Division operator
			} else if (strcmp(instrName, "*") == 0) {
				instrName = "mul"; // Multiplication operator
			} else if (strcmp(instrName, "+") == 0) {
				instrName = "add"; // Addition operator
			} else if (strcmp(instrName, "-") == 0) {
				instrName = "sub"; // Subtraction operator
			} else if (strcmp(instrName, "%") == 0) {
				instrName = "mod"; // Modulo operator
			} else if (strcmp(instrName, "==") == 0) {
				instrName = "eq"; // Equality operator
			} else if (strcmp(instrName, "!=") == 0) {
				instrName = "neq"; // Not equal operator
			} else if (strcmp(instrName, "<") == 0) {
				instrName = "lt"; // Less than operator
			} else if (strcmp(instrName, ">") == 0) {
				instrName = "gt"; // Greater than operator
			} else if (strcmp(instrName, "<=") == 0) {
				instrName = "lte"; // Less than or equal operator
			} else if (strcmp(instrName, ">=") == 0) {
				instrName = "gte"; // Greater than or equal operator
			} else if (strcmp(instrName, "!") == 0) {
				instrName = "not"; // Logical not operator
			}
			out << makeIndent(indent) << "qd_" << instrName << "(ctx);\n";
			break;
		}
		case IAstNode::Type::SCOPED_IDENTIFIER: {
			AstNodeScopedIdentifier* scopedIdent = static_cast<AstNodeScopedIdentifier*>(node);
			out << makeIndent(indent) << "usr_" << scopedIdent->scope() << "_" << scopedIdent->name() << "(ctx);\n";
			break;
		}
		case IAstNode::Type::USE_STATEMENT: {
			AstNodeUse* use = static_cast<AstNodeUse*>(node);
			std::string moduleName = use->module();

			// Check if this is a .qd file import (same package)
			bool isDirectFile = moduleName.size() >= 3 && moduleName.substr(moduleName.size() - 3) == ".qd";

			if (isDirectFile) {
				// .qd file imports don't need includes - functions are in the same package
				// No output needed
			} else {
				// Validate module name to prevent path traversal attacks
				if (!isValidModuleName(moduleName)) {
					// Generate a compile-time error via invalid C code
					out << makeIndent(0) << "#error \"Invalid module name: '" << moduleName
						<< "'. Module names must be alphanumeric with underscores only.\"\n";
				} else {
					out << makeIndent(0) << "#include \"" << moduleName << "/module.h\"\n";
				}
			}
			break;
		}
		case IAstNode::Type::IMPORT_STATEMENT:
			// Import statements are handled in the main emit() function
			// They generate external declarations and wrapper functions
			break;
		case IAstNode::Type::CONSTANT_DECLARATION: {
			AstNodeConstant* constDecl = static_cast<AstNodeConstant*>(node);
			out << makeIndent(indent) << "#define " << packageName << "_" << constDecl->name() << " "
				<< constDecl->value() << "\n";
			break;
		}
		case IAstNode::Type::LABEL:
			// TODO: Handle label
			break;
		case IAstNode::Type::COMMENT: {
			// Transpile comments directly to C
			const AstNodeComment* comment = static_cast<const AstNodeComment*>(node);
			out << makeIndent(indent);
			if (comment->commentType() == AstNodeComment::CommentType::LINE) {
				out << "//" << comment->text() << "\n";
			} else {
				out << "/*" << comment->text() << "*/\n";
			}
			break;
		}
		}

		int childIndent = indent;
		if (node->type() == IAstNode::Type::BLOCK) {
			childIndent = indent + 1;
		}

		for (size_t i = 0; i < node->childCount(); i++) {
			traverse(node->child(i), packageName, out, childIndent, varCounter, currentForIterator, deferStatements,
					throwsMap);
		}

		if (node->type() == IAstNode::Type::BLOCK) {
			out << makeIndent(indent) << "}\n";
		}
	}

	void Transpiler::collectFunctionMetadata(IAstNode* node, std::unordered_map<std::string, bool>& throwsMap) const {
		if (!node) {
			return;
		}

		// Collect function declarations with their throws status
		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			AstNodeFunctionDeclaration* funcDecl = static_cast<AstNodeFunctionDeclaration*>(node);
			throwsMap[funcDecl->name()] = funcDecl->throws();
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			collectFunctionMetadata(node->child(i), throwsMap);
		}
	}

	void Transpiler::generateImportWrappers(IAstNode* node, const char* package, std::stringstream& out,
			std::unordered_set<std::string>& importedLibraries) const {
		if (!node) {
			return;
		}

		// If this is an import statement, generate wrappers
		if (node->type() == IAstNode::Type::IMPORT_STATEMENT) {
			AstNodeImport* import = static_cast<AstNodeImport*>(node);
			const std::string& namespaceName = import->namespaceName();
			const std::string& libraryName = import->library();

			// Track this library for linking
			importedLibraries.insert(libraryName);

			// Extract C function prefix from library filename (e.g., "libstdqd.so" -> "stdqd")
			std::string cPrefix = libraryName;
			// Remove "lib" prefix if present
			if (cPrefix.find("lib") == 0) {
				cPrefix = cPrefix.substr(3);
			}
			// Remove extension (.so, .a, etc.)
			size_t dotPos = cPrefix.find_last_of('.');
			if (dotPos != std::string::npos) {
				cPrefix = cPrefix.substr(0, dotPos);
			}

			out << "// Imported from " << libraryName << "\n";

			// Generate external declarations and wrappers for each function
			for (const auto* func : import->functions()) {
				// External declaration: extern qd_exec_result qd_stdqd_printf(qd_context* ctx);
				// C function name derived from library filename
				std::string cFunctionName = "qd_" + cPrefix + "_" + func->name;
				out << "extern qd_exec_result " << cFunctionName << "(qd_context* ctx);\n";

				// Wrapper function: usr_std_printf calls qd_stdqd_printf
				// Wrapper name uses the user-chosen namespace
				// Use static inline to avoid multiple definition errors when multiple modules
				// import the same library with the same namespace
				std::string wrapperName = "usr_" + namespaceName + "_" + func->name;
				out << "static inline qd_exec_result " << wrapperName << "(qd_context* ctx) {\n";
				out << "    return " << cFunctionName << "(ctx);\n";
				out << "}\n\n";
			}
		}

		// Recursively process children
		for (size_t i = 0; i < node->childCount(); i++) {
			generateImportWrappers(node->child(i), package, out, importedLibraries);
		}
	}

	std::optional<SourceFile> Transpiler::emit(
			const char* filename, const char* package, const char* source, bool verbose, bool dumpTokens) const {
		if (filename == nullptr || package == nullptr || source == nullptr) {
			return std::nullopt;
		}

		if (verbose) {
			std::cout << Colors::bold() << "quadc: " << Colors::reset() << "transpiling " << filename << std::endl;
		}

		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(source, dumpTokens, filename);

		// Check if there were any parse errors
		if (ast.hasErrors()) {
			// Parse errors were reported, do not proceed with transpilation
			return std::nullopt;
		}

		// Check if AST generation failed
		if (root == nullptr) {
			// AST is null (shouldn't happen unless out of memory)
			return std::nullopt;
		}

		// Semantic validation - catch errors before gcc
		Qd::SemanticValidator validator;
		size_t errorCount = validator.validate(root, filename);
		if (errorCount > 0) {
			// Validation failed - do not proceed with transpilation
			return std::nullopt;
		}

		std::stringstream ss;

		ss << "// This file is automatically generated by the Quadrate compiler.\n";
		ss << "// Do not edit manually.\n\n";
		ss << "#include <qdrt/runtime.h>\n";
		ss << "#include <stdio.h>\n";
		ss << "#include <stdlib.h>\n\n";

		// Collect function metadata (throws status)
		std::unordered_map<std::string, bool> throwsMap;
		collectFunctionMetadata(root, throwsMap);

		// Generate external declarations and wrappers for imported library functions
		std::unordered_set<std::string> importedLibraries;
		generateImportWrappers(root, package, ss, importedLibraries);

		// Reset counter for each compilation unit
		mVarCounter = 0;
		traverse(root, package, ss, 0, mVarCounter, "", nullptr, throwsMap);

		// Use basename for output file (filename might be a full path for validation purposes)
		std::string basename = std::filesystem::path(filename).filename().string();
		std::filesystem::path filepath = std::filesystem::path(package) / (basename + ".c");

		// Return source file with imported modules, imported libraries, and source directory
		return SourceFile{filepath, std::string(package), ss.str(), validator.importedModules(), importedLibraries,
				validator.sourceDirectory()};
	}
}
