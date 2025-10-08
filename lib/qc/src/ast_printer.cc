#include <qc/ast_printer.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_block.h>
#include <stdio.h>
#include <string>

namespace Qd {
	static const char* getTypeName(IAstNode::Type type) {
		switch (type) {
		case IAstNode::Type::Unknown:
			return "Unknown";
		case IAstNode::Type::Program:
			return "Program";
		case IAstNode::Type::FunctionDeclaration:
			return "FunctionDeclaration";
		case IAstNode::Type::VariableDeclaration:
			return "VariableDeclaration";
		case IAstNode::Type::ExpressionStatement:
			return "ExpressionStatement";
		case IAstNode::Type::IfStatement:
			return "IfStatement";
		case IAstNode::Type::WhileStatement:
			return "WhileStatement";
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

		// Print the current node
		printf("%s", prefix);
		printf("%s", isLast ? "└── " : "├── ");

		const char* typeName = getTypeName(node->type());
		printf("%s", typeName);

		// Print additional info based on node type
		if (node->type() == IAstNode::Type::FunctionDeclaration) {
			const AstNodeFunctionDeclaration* func = static_cast<const AstNodeFunctionDeclaration*>(node);
			printf(" '%s'", func->name().c_str());
		} else if (node->type() == IAstNode::Type::Identifier) {
			const AstNodeIdentifier* id = static_cast<const AstNodeIdentifier*>(node);
			printf(" '%s'", id->name().c_str());
		}

		printf("\n");

		// Print children
		size_t childCount = node->childCount();
		for (size_t i = 0; i < childCount; i++) {
			std::string newPrefix = prefix;
			newPrefix += isLast ? "    " : "│   ";

			IAstNode* child = node->child(i);
			printTree(child, newPrefix.c_str(), i == childCount - 1);
		}
	}
}
