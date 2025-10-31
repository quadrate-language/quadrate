#include <cgen/transpiler.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_use.h>
#include <qc/ast_printer.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <sstream>

namespace Qd {
	static int varCounter = 0;

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

	void traverse(IAstNode* node, const char* packageName, std::stringstream& out, int indent,
			const std::string& currentForIterator = "") {
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

			traverse(funcDecl->body(), packageName, out, indent + 1, currentForIterator);
			out << "\n" << makeIndent(indent) << "qd_lbl_done:;\n";

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
			out << makeIndent(indent) << "if (qd_stack_size(ctx->st) < 1) {\n";
			out << makeIndent(indent + 1)
				<< "fprintf(stderr, \"Fatal error in if: Stack underflow (requires 1 value, have %zu)\\n\", "
				   "qd_stack_size(ctx->st));\n";
			out << makeIndent(indent + 1) << "abort();\n";
			out << makeIndent(indent) << "}\n";

			// Pop the condition value from the stack
			out << makeIndent(indent) << "qd_stack_element_t " << var << ";\n";
			out << makeIndent(indent) << "qd_stack_error " << var << "_err = qd_stack_pop(ctx->st, &" << var << ");\n";
			out << makeIndent(indent) << "if (" << var << "_err != QD_STACK_OK) {\n";
			out << makeIndent(indent + 1) << "fprintf(stderr, \"Fatal error in if: Failed to pop value\\n\");\n";
			out << makeIndent(indent + 1) << "abort();\n";
			out << makeIndent(indent) << "}\n";

			// Check the condition (non-zero integer means true)
			out << makeIndent(indent) << "if (" << var << ".type == QD_STACK_TYPE_INT && " << var
				<< ".value.i != 0) {\n";

			// Then block
			if (ifStmt->thenBody()) {
				traverse(ifStmt->thenBody(), packageName, out, indent + 1, currentForIterator);
			}
			out << makeIndent(indent) << "}";

			// Else block (if present)
			if (ifStmt->elseBody()) {
				out << " else {\n";
				traverse(ifStmt->elseBody(), packageName, out, indent + 1, currentForIterator);
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
			out << makeIndent(indent) << "if (qd_stack_size(ctx->st) < 3) {\n";
			out << makeIndent(indent + 1)
				<< "fprintf(stderr, \"Fatal error in for: Stack underflow (requires 3 values, have %zu)\\n\", "
				   "qd_stack_size(ctx->st));\n";
			out << makeIndent(indent + 1) << "abort();\n";
			out << makeIndent(indent) << "}\n";

			// Pop step, end, start from the stack (in reverse order)
			out << makeIndent(indent) << "qd_stack_element_t " << varStep << ";\n";
			out << makeIndent(indent) << "qd_stack_error " << varStep << "_err = qd_stack_pop(ctx->st, &" << varStep
				<< ");\n";
			out << makeIndent(indent) << "if (" << varStep << "_err != QD_STACK_OK) {\n";
			out << makeIndent(indent + 1) << "fprintf(stderr, \"Fatal error in for: Failed to pop step value\\n\");\n";
			out << makeIndent(indent + 1) << "abort();\n";
			out << makeIndent(indent) << "}\n";

			out << makeIndent(indent) << "qd_stack_element_t " << varEnd << ";\n";
			out << makeIndent(indent) << "qd_stack_error " << varEnd << "_err = qd_stack_pop(ctx->st, &" << varEnd
				<< ");\n";
			out << makeIndent(indent) << "if (" << varEnd << "_err != QD_STACK_OK) {\n";
			out << makeIndent(indent + 1) << "fprintf(stderr, \"Fatal error in for: Failed to pop end value\\n\");\n";
			out << makeIndent(indent + 1) << "abort();\n";
			out << makeIndent(indent) << "}\n";

			out << makeIndent(indent) << "qd_stack_element_t " << varStart << ";\n";
			out << makeIndent(indent) << "qd_stack_error " << varStart << "_err = qd_stack_pop(ctx->st, &" << varStart
				<< ");\n";
			out << makeIndent(indent) << "if (" << varStart << "_err != QD_STACK_OK) {\n";
			out << makeIndent(indent + 1) << "fprintf(stderr, \"Fatal error in for: Failed to pop start value\\n\");\n";
			out << makeIndent(indent + 1) << "abort();\n";
			out << makeIndent(indent) << "}\n";

			// Generate C for loop with the values
			out << makeIndent(indent) << "if (" << varStart << ".type == QD_STACK_TYPE_INT && " << varEnd
				<< ".type == QD_STACK_TYPE_INT && " << varStep << ".type == QD_STACK_TYPE_INT) {\n";
			out << makeIndent(indent + 1) << "for (int64_t " << varI << " = " << varStart << ".value.i; " << varI
				<< " < " << varEnd << ".value.i; " << varI << " += " << varStep << ".value.i) {\n";

			// Loop body - pass iterator variable name for $ handling
			if (forStmt->body()) {
				traverse(forStmt->body(), packageName, out, indent + 2, varI);
			}

			out << makeIndent(indent + 1) << "}\n";
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
			// TODO: Handle defer statement
			break;
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
			case AstNodeLiteral::LiteralType::STRING:
				out << makeIndent(indent) << "qd_push_s(ctx, " << literal->value() << ");\n";
				break;
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
				out << makeIndent(indent) << "usr_" << packageName << "_" << ident->name() << "(ctx);\n";
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
			// TODO: Handle use statement
			AstNodeUse* use = static_cast<AstNodeUse*>(node);
			out << makeIndent(0) << "#include \"" << use->module() << "\\module.h\"\n";
			break;
		}
		case IAstNode::Type::CONSTANT_DECLARATION: {
			AstNodeConstant* constDecl = static_cast<AstNodeConstant*>(node);
			out << makeIndent(indent) << "#define " << packageName << "_" << constDecl->name() << " "
				<< constDecl->value() << "\n";
			break;
		}
		case IAstNode::Type::LABEL:
			// TODO: Handle label
			break;
		}

		int childIndent = indent;
		if (node->type() == IAstNode::Type::BLOCK) {
			childIndent = indent + 1;
		}

		for (size_t i = 0; i < node->childCount(); i++) {
			traverse(node->child(i), packageName, out, childIndent, currentForIterator);
		}

		if (node->type() == IAstNode::Type::BLOCK) {
			out << makeIndent(indent) << "}\n";
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
		ss << "#include <quadrate/runtime/runtime.h>\n";
		ss << "#include <stdio.h>\n";
		ss << "#include <stdlib.h>\n\n";

		traverse(root, package, ss, 0);

		std::filesystem::path filepath = std::filesystem::path(package) / (std::string(filename) + ".c");
		return SourceFile{filepath, std::string(package), ss.str()};
	}
}
