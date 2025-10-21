#include <cgen/writer.h>
#include <fstream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_function.h>
#include <sstream>

namespace Qd {
	void Writer::write(IAstNode* root, const char* packageName, const char* filename) const {
		if (root == nullptr || packageName == nullptr || filename == nullptr) {
			return;
		}

		std::stringstream ss;

		Writer::writeHeader(ss);

		// Traverse the AST and generate code
		traverse(root, packageName, ss);

		Writer::writeFooter(ss);

		// Write to file
		std::ofstream file(filename);
		file << ss.str();
		file.close();
	}

	void Writer::writeMain(const char* filename) const {
		std::ofstream file(filename);
		file << "// Generated main C code\n";
		file << "#include <runtime/runtime.h>\n\n";
		file << "extern qd_exec_result main_main(qd_context* ctx);\n\n";
		file << "int main(void) {\n";
		file << "    qd_context ctx;\n";
		file << "    main_main(&ctx);\n";
		file << "    return 0;\n";
		file << "}\n";
		file.close();
	}

	void Writer::writeHeader(std::stringstream& out) const {
		out << "// Generated C code\n";
		out << "#include <runtime/runtime.h>\n\n";
	}

	void Writer::traverse(IAstNode* node, const char* packageName, std::stringstream& out, int indent) const {
		if (node == nullptr) {
			return;
		}

		// Helper to generate indentation string
		auto makeIndent = [](int level) { return std::string(static_cast<size_t>(level * 4), ' '); };

		// Process current node based on its type
		switch (node->type()) {
		case IAstNode::Type::Unknown:
			// Unknown node type, skip
			break;
		case IAstNode::Type::Program:
			out << "// Program\n";
			break;
		case IAstNode::Type::Block:
			out << makeIndent(indent) << "{\n";
			break;
		case IAstNode::Type::FunctionDeclaration: {
			AstNodeFunctionDeclaration* funcDecl = static_cast<AstNodeFunctionDeclaration*>(node);
			out << "\n"
				<< makeIndent(indent) << "qd_exec_result " << packageName << "_" << funcDecl->name()
				<< "(qd_context* ctx) {\n";
			out << makeIndent(indent + 1) << "QD_REQUIRE_STACK(ctx, " << funcDecl->inputParameters().size() << ");\n\n";
			traverse(funcDecl->body(), packageName, out, indent + 1);
			out << "\n"
				<< makeIndent(indent) << "QD_DONE:;\n"
				<< makeIndent(indent + 1) << "QD_REQUIRE_STACK(ctx, " << funcDecl->outputParameters().size() << ");\n"
				<< makeIndent(indent + 1) << "return (qd_exec_result){0};\n";
			out << makeIndent(indent) << "}\n";
			return; // Don't traverse children again
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
			out << makeIndent(indent) << "goto QD_DONE;\n";
			break;
		case IAstNode::Type::BreakStatement:
			out << makeIndent(indent) << "break;\n";
			break;
		case IAstNode::Type::ContinueStatement:
			out << makeIndent(indent) << "continue;\n";
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
			out << makeIndent(indent) << "#define " << packageName << "_" << constDecl->name() << " "
				<< constDecl->value() << "\n";
			break;
		}
		case IAstNode::Type::Label:
			// TODO: Handle label
			break;
		}

		// Determine the indentation level for children
		int childIndent = indent;
		if (node->type() == IAstNode::Type::Block) {
			childIndent = indent + 1;
		}

		// Recursively traverse all children
		for (size_t i = 0; i < node->childCount(); i++) {
			traverse(node->child(i), packageName, out, childIndent);
		}

		// Post-process node if needed
		if (node->type() == IAstNode::Type::Block) {
			out << makeIndent(indent) << "}\n";
		}
	}

	void Writer::writeFooter(std::stringstream& out) const {
		out << "\n// End of generated code\n";
	}
}
