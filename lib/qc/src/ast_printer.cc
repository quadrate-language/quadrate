#include <qc/ast_node_block.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
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
		default:
			return "Unknown";
		}
	}

	void AstPrinter::print(const IAstNode* node) {
		if (!node) {
			printf("(null)\n");
			return;
		}
		printTree(node, "", true);
	}

	void AstPrinter::printTree(const IAstNode* node, const char* prefix, bool isLast) {
		if (!node) {
			return;
		}

		printf("%s", prefix);
		printf("%s", isLast ? "└── " : "├── ");

		const char* typeName = getTypeName(node->type());
		printf("%s", typeName);

		if (node->type() == IAstNode::Type::FunctionDeclaration) {
			const AstNodeFunctionDeclaration* func = static_cast<const AstNodeFunctionDeclaration*>(node);
			printf(" '%s'", func->name().c_str());
		} else if (node->type() == IAstNode::Type::Identifier) {
			const AstNodeIdentifier* id = static_cast<const AstNodeIdentifier*>(node);
			printf(" '%s'", id->name().c_str());
		} else if (node->type() == IAstNode::Type::Literal) {
			const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(node);
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
			printf(" %s '%s'", typeStr, lit->value().c_str());
		} else if (node->type() == IAstNode::Type::ForStatement) {
			const AstNodeForStatement* forStmt = static_cast<const AstNodeForStatement*>(node);
			printf(" '%s'", forStmt->loopVar().c_str());
		} else if (node->type() == IAstNode::Type::CaseStatement) {
			const AstNodeCase* caseStmt = static_cast<const AstNodeCase*>(node);
			if (caseStmt->isDefault()) {
				printf(" (default)");
			}
		} else if (node->type() == IAstNode::Type::VariableDeclaration) {
			const AstNodeParameter* param = static_cast<const AstNodeParameter*>(node);
			printf(" '%s:%s' (%s)", param->name().c_str(), param->typeString().c_str(), param->isOutput() ? "output" : "input");
		} else if (node->type() == IAstNode::Type::UseStatement) {
			const AstNodeUse* useStmt = static_cast<const AstNodeUse*>(node);
			printf(" '%s'", useStmt->module().c_str());
		}

		printf("\n");

		size_t childCount = node->childCount();
		for (size_t i = 0; i < childCount; i++) {
			std::string newPrefix = prefix;
			newPrefix += isLast ? "    " : "│   ";

			IAstNode* child = node->child(i);
			printTree(child, newPrefix.c_str(), i == childCount - 1);
		}
	}
}
