#include "ast_node_block.h"
#include <qc/ast_node_constant.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_instruction.h>
#include "ast_node_label.h"
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>
#include <qc/ast_printer.h>
#include <stdio.h>
#include <string>

namespace Qd {
	static const char* getTypeName(IAstNode::Type type) {
		switch (type) {
		case IAstNode::Type::UNKNOWN:
			return "Unknown";
		case IAstNode::Type::PROGRAM:
			return "Program";
		case IAstNode::Type::BLOCK:
			return "Block";
		case IAstNode::Type::FUNCTION_DECLARATION:
			return "FunctionDeclaration";
		case IAstNode::Type::VARIABLE_DECLARATION:
			return "VariableDeclaration";
		case IAstNode::Type::EXPRESSION_STATEMENT:
			return "ExpressionStatement";
		case IAstNode::Type::IF_STATEMENT:
			return "IfStatement";
		case IAstNode::Type::FOR_STATEMENT:
			return "ForStatement";
		case IAstNode::Type::SWITCH_STATEMENT:
			return "SwitchStatement";
		case IAstNode::Type::CASE_STATEMENT:
			return "CaseStatement";
		case IAstNode::Type::RETURN_STATEMENT:
			return "ReturnStatement";
		case IAstNode::Type::BREAK_STATEMENT:
			return "BreakStatement";
		case IAstNode::Type::CONTINUE_STATEMENT:
			return "ContinueStatement";
		case IAstNode::Type::DEFER_STATEMENT:
			return "DeferStatement";
		case IAstNode::Type::BINARY_EXPRESSION:
			return "BinaryExpression";
		case IAstNode::Type::UNARY_EXPRESSION:
			return "UnaryExpression";
		case IAstNode::Type::LITERAL:
			return "Literal";
		case IAstNode::Type::IDENTIFIER:
			return "Identifier";
		case IAstNode::Type::INSTRUCTION:
			return "Instruction";
		case IAstNode::Type::SCOPED_IDENTIFIER:
			return "ScopedIdentifier";
		case IAstNode::Type::USE_STATEMENT:
			return "UseStatement";
		case IAstNode::Type::CONSTANT_DECLARATION:
			return "ConstantDeclaration";
		case IAstNode::Type::LABEL:
			return "Label";
		default:
			return "Unknown";
		}
	}

	static void escapeJsonString(const char* str) {
		while (*str) {
			switch (*str) {
			case '"':
				printf("\\\"");
				break;
			case '\\':
				printf("\\\\");
				break;
			case '\n':
				printf("\\n");
				break;
			case '\r':
				printf("\\r");
				break;
			case '\t':
				printf("\\t");
				break;
			default:
				printf("%c", *str);
				break;
			}
			str++;
		}
	}

	static void printJsonNode(const IAstNode* node);

	static void printJsonNode(const IAstNode* node) {
		if (!node) {
			printf("null");
			return;
		}

		printf("{");
		printf("\"type\":\"%s\"", getTypeName(node->type()));

		if (node->type() == IAstNode::Type::FUNCTION_DECLARATION) {
			const AstNodeFunctionDeclaration* func = static_cast<const AstNodeFunctionDeclaration*>(node);
			printf(",");
			printf("\"name\":\"");
			escapeJsonString(func->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::IDENTIFIER) {
			const AstNodeIdentifier* id = static_cast<const AstNodeIdentifier*>(node);
			printf(",");
			printf("\"name\":\"");
			escapeJsonString(id->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::INSTRUCTION) {
			const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(node);
			printf(",");
			printf("\"name\":\"");
			escapeJsonString(instr->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
			const AstNodeScopedIdentifier* scoped = static_cast<const AstNodeScopedIdentifier*>(node);
			printf(",");
			printf("\"scope\":\"");
			escapeJsonString(scoped->scope().c_str());
			printf("\",");
			printf("\"name\":\"");
			escapeJsonString(scoped->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::LITERAL) {
			const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(node);
			printf(",");
			const char* typeStr = "";
			switch (lit->literalType()) {
			case AstNodeLiteral::LiteralType::INTEGER:
				typeStr = "Integer";
				break;
			case AstNodeLiteral::LiteralType::FLOAT:
				typeStr = "Float";
				break;
			case AstNodeLiteral::LiteralType::STRING:
				typeStr = "String";
				break;
			}
			printf("\"literalType\":\"%s\",", typeStr);
			printf("\"value\":\"");
			escapeJsonString(lit->value().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::FOR_STATEMENT) {
			// For statement doesn't have extra properties to print
		} else if (node->type() == IAstNode::Type::CASE_STATEMENT) {
			const AstNodeCase* caseStmt = static_cast<const AstNodeCase*>(node);
			printf(",");
			printf("\"isDefault\":%s", caseStmt->isDefault() ? "true" : "false");
		} else if (node->type() == IAstNode::Type::VARIABLE_DECLARATION) {
			const AstNodeParameter* param = static_cast<const AstNodeParameter*>(node);
			printf(",");
			printf("\"name\":\"");
			escapeJsonString(param->name().c_str());
			printf("\",");
			printf("\"paramType\":\"");
			escapeJsonString(param->typeString().c_str());
			printf("\",");
			printf("\"isOutput\":%s", param->isOutput() ? "true" : "false");
		} else if (node->type() == IAstNode::Type::USE_STATEMENT) {
			const AstNodeUse* useStmt = static_cast<const AstNodeUse*>(node);
			printf(",");
			printf("\"module\":\"");
			escapeJsonString(useStmt->module().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::CONSTANT_DECLARATION) {
			const AstNodeConstant* constDecl = static_cast<const AstNodeConstant*>(node);
			printf(",");
			printf("\"name\":\"");
			escapeJsonString(constDecl->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::LABEL) {
			const AstNodeLabel* label = static_cast<const AstNodeLabel*>(node);
			printf(",");
			printf("\"name\":\"");
			escapeJsonString(label->name().c_str());
			printf("\"");
		}

		size_t childCount = node->childCount();
		if (childCount > 0) {
			printf(",");
			printf("\"children\":[");
			for (size_t i = 0; i < childCount; i++) {
				IAstNode* child = node->child(i);
				printJsonNode(child);
				if (i < childCount - 1) {
					printf(",");
				}
			}
			printf("]");
		}

		printf("}");
	}

	void AstPrinter::print(const IAstNode* node) {
		if (!node) {
			printf("null\n");
			return;
		}
		printJsonNode(node);
		printf("\n");
	}

	void AstPrinter::printTree(const IAstNode* node, const char*, bool) {
		// Deprecated - now using JSON output
		print(node);
	}
}
