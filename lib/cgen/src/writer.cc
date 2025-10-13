#include <cgen/writer.h>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_function.h>
#include <sstream>

namespace Qd {
	void Writer::write(IAstNode* root, const char* filename) const {
		if (root == nullptr || filename == nullptr) {
			return;
		}

		std::stringstream ss;

		// Traverse the AST and generate code
		traverse(root, ss);

		printf("Writing AST to %s\n", ss.str().c_str());
	}

	void Writer::traverse(IAstNode* node, std::stringstream& out) const {
		if (node == nullptr) {
			return;
		}

		// Process current node based on its type
		switch (node->type()) {
		case IAstNode::Type::Unknown:
			// Unknown node type, skip
			break;
		case IAstNode::Type::Program:
			out << "// Program\n";
			break;
		case IAstNode::Type::Block:
			out << "{\n";
			break;
		case IAstNode::Type::FunctionDeclaration: {
			AstNodeFunctionDeclaration* funcDecl = static_cast<AstNodeFunctionDeclaration*>(node);
			out << "void " << funcDecl->name() << "(void) {\n";
			out << "    // Function body\n";
			traverse(funcDecl->body(), out);
			out << "}\n";
			break;
		}
		case IAstNode::Type::VariableDeclaration:
			// TODO: Handle variable declaration
			break;
		case IAstNode::Type::ExpressionStatement:
			// TODO: Handle expression statement
			break;
		case IAstNode::Type::IfStatement:
			// TODO: Handle if statement
			break;
		case IAstNode::Type::ForStatement:
			// TODO: Handle for statement
			break;
		case IAstNode::Type::SwitchStatement:
			// TODO: Handle switch statement
			break;
		case IAstNode::Type::CaseStatement:
			// TODO: Handle case statement
			break;
		case IAstNode::Type::ReturnStatement:
			out << "return;\n";
			break;
		case IAstNode::Type::BreakStatement:
			out << "break;\n";
			break;
		case IAstNode::Type::ContinueStatement:
			out << "continue;\n";
			break;
		case IAstNode::Type::DeferStatement:
			// TODO: Handle defer statement
			break;
		case IAstNode::Type::BinaryExpression:
			// TODO: Handle binary expression
			break;
		case IAstNode::Type::UnaryExpression:
			// TODO: Handle unary expression
			break;
		case IAstNode::Type::Literal:
			// TODO: Handle literal
			break;
		case IAstNode::Type::Identifier:
			// TODO: Handle identifier
			break;
		case IAstNode::Type::ScopedIdentifier:
			// TODO: Handle scoped identifier
			break;
		case IAstNode::Type::UseStatement:
			// TODO: Handle use statement
			break;
		case IAstNode::Type::ConstantDeclaration: {
			AstNodeConstant* constDecl = static_cast<AstNodeConstant*>(node);
			out << "#define " << constDecl->name() << " " << constDecl->value() << "\n";
			break;
		}
		case IAstNode::Type::Label:
			// TODO: Handle label
			break;
		}

		// Recursively traverse all children
		for (size_t i = 0; i < node->childCount(); i++) {
			traverse(node->child(i), out);
		}

		// Post-process node if needed
		if (node->type() == IAstNode::Type::Block) {
			out << "}\n";
		}
	}
}
