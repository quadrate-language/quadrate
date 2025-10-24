#include <cgen/transpiler.h>
#include <filesystem>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_use.h>
#include <qc/ast_printer.h>
#include <sstream>

namespace Qd {
	static int varCounter = 0;

	// Map parameter type string to qd_stack_type constant name
	static const char* mapTypeToStackType(const std::string& paramType) {
		if (paramType.empty()) {
			return "QD_STACK_TYPE_PTR"; // Untyped - skip type check
		} else if (paramType == "int" || paramType == "i64") {
			return "QD_STACK_TYPE_INT";
		} else if (paramType == "float" || paramType == "f64") {
			return "QD_STACK_TYPE_FLOAT";
		} else if (paramType == "str" || paramType == "string") {
			return "QD_STACK_TYPE_STR";
		} else {
			return "QD_STACK_TYPE_PTR"; // Unknown type - skip check
		}
	}

	void traverse(IAstNode* node, const char* packageName, std::stringstream& out, int indent) {
		if (node == nullptr) {
			return;
		}

		auto makeIndent = [](int level) { return std::string(static_cast<size_t>(level * 4), ' '); };

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
			<< makeIndent(indent) << "qd_exec_result usr_" << packageName << "_" << funcDecl->name()
			<< "(qd_context* ctx) {\n";

		// Generate type check for input parameters
		if (!funcDecl->inputParameters().empty()) {
			out << makeIndent(indent + 1) << "qd_stack_type input_types[] = {";
			for (size_t i = 0; i < funcDecl->inputParameters().size(); i++) {
				if (i > 0) out << ", ";
				AstNodeParameter* param = static_cast<AstNodeParameter*>(funcDecl->inputParameters()[i]);
				out << mapTypeToStackType(param->typeString());
			}
			out << "};\n";
			out << makeIndent(indent + 1) << "qd_check_stack(ctx, " << funcDecl->inputParameters().size()
				<< ", input_types, __func__);\n\n";
		}

		traverse(funcDecl->body(), packageName, out, indent + 1);
		out << "\n"
			<< makeIndent(indent) << "qd_lbl_done:;\n";

		// Generate type check for output parameters
		if (!funcDecl->outputParameters().empty()) {
			out << makeIndent(indent + 1) << "qd_stack_type output_types[] = {";
			for (size_t i = 0; i < funcDecl->outputParameters().size(); i++) {
				if (i > 0) out << ", ";
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
		case IAstNode::Type::VariableDeclaration:
			// TODO: Handle variable declaration
			break;
		case IAstNode::Type::ExpressionStatement:
			// TODO: Handle expression statement
			break;
		case IAstNode::Type::IfStatement: {
			int64_t currentVar = varCounter++;
			std::string var = "qd_var_" + std::to_string(currentVar);
			out << makeIndent(indent) << "int64_t " << var << " = qd_stack_pop_i(ctx);\n";
			out << makeIndent(indent) << "if (" << var << " != 0) {\n";
			if (node->childCount() > 0) {
				traverse(node->child(0), packageName, out, indent + 1); // Then block
			}
			out << makeIndent(indent) << "}\n";
			break;
		}
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
			out << makeIndent(indent) << "goto qd_lbl_done;\n";
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
		case IAstNode::Type::Literal: {
			AstNodeLiteral* literal = static_cast<AstNodeLiteral*>(node);
			switch (literal->literalType()) {
			case AstNodeLiteral::LiteralType::Integer:
				out << makeIndent(indent) << "qd_push_i(ctx, (int64_t)" << literal->value() << ");\n";
				break;
			case AstNodeLiteral::LiteralType::Float:
				out << makeIndent(indent) << "qd_push_f(ctx, (double)" << literal->value() << ");\n";
				break;
			case AstNodeLiteral::LiteralType::String:
				out << makeIndent(indent) << "qd_push_s(ctx, " << literal->value() << ");\n";
				break;
			}
			break;
		}
		case IAstNode::Type::Identifier: {
			AstNodeIdentifier* ident = static_cast<AstNodeIdentifier*>(node);
			out << makeIndent(indent) << "usr_" << packageName << "_" << ident->name() << "(ctx);\n";
			break;
		}
		case IAstNode::Type::Instruction: {
			AstNodeInstruction* instr = static_cast<AstNodeInstruction*>(node);
			out << makeIndent(indent) << "qd_" << instr->name() << "(ctx);\n";
			break;
		}
		case IAstNode::Type::ScopedIdentifier: {
			AstNodeScopedIdentifier* scopedIdent = static_cast<AstNodeScopedIdentifier*>(node);
			out << makeIndent(indent) << "usr_" << scopedIdent->scope() << "_" << scopedIdent->name() << "(ctx);\n";
			break;
		}
		case IAstNode::Type::UseStatement: {
			// TODO: Handle use statement
			AstNodeUse* use = static_cast<AstNodeUse*>(node);
			out << makeIndent(0) << "#include \"" << use->module() << "\\module.h\"\n";
			break;
		}
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

		int childIndent = indent;
		if (node->type() == IAstNode::Type::Block) {
			childIndent = indent + 1;
		}

		for (size_t i = 0; i < node->childCount(); i++) {
			traverse(node->child(i), packageName, out, childIndent);
		}

		if (node->type() == IAstNode::Type::Block) {
			out << makeIndent(indent) << "}\n";
		}
	}

	std::optional<SourceFile> Transpiler::emit(const char* filename, const char* package, const char* source) const {
		if (filename == nullptr || package == nullptr || source == nullptr) {
			return std::nullopt;
		}

		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(source);
		Qd::AstPrinter::print(root);

		std::stringstream ss;

		ss << "// This file is automatically generated by the Quadrate compiler.\n";
		ss << "// Do not edit manually.\n\n";
		ss << "#include <quadrate/runtime/runtime.h>\n\n";

		traverse(root, package, ss, 0);

		std::filesystem::path filepath = std::filesystem::path(package) / (std::string(filename) + ".c");
		return SourceFile{filepath, std::string(package), ss.str()};
	}
}
