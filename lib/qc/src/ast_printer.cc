#include <qc/ast_node_block.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_label.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>
#include <qc/ast_printer.h>
#include <stdio.h>
#include <string>

namespace Qd {
	static const char* getTypeName(IAstNode::Type type) {
		switch (type) {
		case IAstNode::Type::Unknown:
			return "Unknown";
		case IAstNode::Type::Program:
			return "Program";
		case IAstNode::Type::Block:
			return "Block";
		case IAstNode::Type::FunctionDeclaration:
			return "FunctionDeclaration";
		case IAstNode::Type::VariableDeclaration:
			return "VariableDeclaration";
		case IAstNode::Type::ExpressionStatement:
			return "ExpressionStatement";
		case IAstNode::Type::IfStatement:
			return "IfStatement";
		case IAstNode::Type::ForStatement:
			return "ForStatement";
		case IAstNode::Type::SwitchStatement:
			return "SwitchStatement";
		case IAstNode::Type::CaseStatement:
			return "CaseStatement";
		case IAstNode::Type::ReturnStatement:
			return "ReturnStatement";
		case IAstNode::Type::BreakStatement:
			return "BreakStatement";
		case IAstNode::Type::BinaryExpression:
			return "BinaryExpression";
		case IAstNode::Type::UnaryExpression:
			return "UnaryExpression";
		case IAstNode::Type::Literal:
			return "Literal";
		case IAstNode::Type::Identifier:
			return "Identifier";
		case IAstNode::Type::UseStatement:
			return "UseStatement";
		case IAstNode::Type::ConstantDeclaration:
			return "ConstantDeclaration";
		case IAstNode::Type::Label:
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

	static void printJsonNode(const IAstNode* node, int indent);

	static void printIndent(int indent) {
		for (int i = 0; i < indent; i++) {
			printf("  ");
		}
	}

	static void printJsonNode(const IAstNode* node, int indent) {
		if (!node) {
			printf("null");
			return;
		}

		printf("{\n");
		printIndent(indent + 1);
		printf("\"type\": \"%s\"", getTypeName(node->type()));

		if (node->type() == IAstNode::Type::FunctionDeclaration) {
			const AstNodeFunctionDeclaration* func = static_cast<const AstNodeFunctionDeclaration*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"name\": \"");
			escapeJsonString(func->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::Identifier) {
			const AstNodeIdentifier* id = static_cast<const AstNodeIdentifier*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"name\": \"");
			escapeJsonString(id->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::Literal) {
			const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(node);
			printf(",\n");
			printIndent(indent + 1);
			const char* typeStr = "";
			switch (lit->literalType()) {
			case AstNodeLiteral::LiteralType::Integer:
				typeStr = "Integer";
				break;
			case AstNodeLiteral::LiteralType::Float:
				typeStr = "Float";
				break;
			case AstNodeLiteral::LiteralType::String:
				typeStr = "String";
				break;
			}
			printf("\"literalType\": \"%s\",\n", typeStr);
			printIndent(indent + 1);
			printf("\"value\": \"");
			escapeJsonString(lit->value().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::ForStatement) {
			const AstNodeForStatement* forStmt = static_cast<const AstNodeForStatement*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"loopVar\": \"");
			escapeJsonString(forStmt->loopVar().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::CaseStatement) {
			const AstNodeCase* caseStmt = static_cast<const AstNodeCase*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"isDefault\": %s", caseStmt->isDefault() ? "true" : "false");
		} else if (node->type() == IAstNode::Type::VariableDeclaration) {
			const AstNodeParameter* param = static_cast<const AstNodeParameter*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"name\": \"");
			escapeJsonString(param->name().c_str());
			printf("\",\n");
			printIndent(indent + 1);
			printf("\"paramType\": \"");
			escapeJsonString(param->typeString().c_str());
			printf("\",\n");
			printIndent(indent + 1);
			printf("\"isOutput\": %s", param->isOutput() ? "true" : "false");
		} else if (node->type() == IAstNode::Type::UseStatement) {
			const AstNodeUse* useStmt = static_cast<const AstNodeUse*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"module\": \"");
			escapeJsonString(useStmt->module().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::ConstantDeclaration) {
			const AstNodeConstant* constDecl = static_cast<const AstNodeConstant*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"name\": \"");
			escapeJsonString(constDecl->name().c_str());
			printf("\"");
		} else if (node->type() == IAstNode::Type::Label) {
			const AstNodeLabel* label = static_cast<const AstNodeLabel*>(node);
			printf(",\n");
			printIndent(indent + 1);
			printf("\"name\": \"");
			escapeJsonString(label->name().c_str());
			printf("\"");
		}

		size_t childCount = node->childCount();
		if (childCount > 0) {
			printf(",\n");
			printIndent(indent + 1);
			printf("\"children\": [\n");
			for (size_t i = 0; i < childCount; i++) {
				printIndent(indent + 2);
				IAstNode* child = node->child(i);
				printJsonNode(child, indent + 2);
				if (i < childCount - 1) {
					printf(",");
				}
				printf("\n");
			}
			printIndent(indent + 1);
			printf("]");
		}

		printf("\n");
		printIndent(indent);
		printf("}");
	}

	void AstPrinter::print(const IAstNode* node) {
		if (!node) {
			printf("null\n");
			return;
		}
		printJsonNode(node, 0);
		printf("\n");
	}

	void AstPrinter::printTree(const IAstNode* node, const char*, bool) {
		// Deprecated - now using JSON output
		print(node);
	}
}
