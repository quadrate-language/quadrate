#include <qc/ast_node_block.h>
#include <qc/ast_node_comment.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_label.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>
#include <qc/formatter.h>

namespace Qd {

	Formatter::Formatter() : mIndentWidth(1), mMaxLineLength(120), mCurrentIndent(0) {
	}

	void Formatter::setIndentWidth(int width) {
		mIndentWidth = width;
	}

	void Formatter::setMaxLineLength(int length) {
		mMaxLineLength = length;
	}

	std::string Formatter::format(const IAstNode* node) {
		mOutput.clear();
		mCurrentIndent = 0;
		formatNode(node);
		return mOutput;
	}

	void Formatter::formatNode(const IAstNode* node) {
		if (!node) {
			return;
		}

		switch (node->type()) {
		case IAstNode::Type::PROGRAM:
			formatProgram(node);
			break;
		case IAstNode::Type::FUNCTION_DECLARATION:
			formatFunction(node);
			break;
		case IAstNode::Type::BLOCK:
			formatBlock(node);
			break;
		case IAstNode::Type::IF_STATEMENT:
			formatIf(node);
			break;
		case IAstNode::Type::FOR_STATEMENT:
			formatFor(node);
			break;
		case IAstNode::Type::SWITCH_STATEMENT:
			formatSwitch(node);
			break;
		case IAstNode::Type::CASE_STATEMENT:
			formatCase(node);
			break;
		case IAstNode::Type::DEFER_STATEMENT:
			formatDefer(node);
			break;
		case IAstNode::Type::USE_STATEMENT:
			formatUse(node);
			break;
		case IAstNode::Type::CONSTANT_DECLARATION:
			formatConstant(node);
			break;
		case IAstNode::Type::INSTRUCTION:
			formatInstruction(node);
			break;
		case IAstNode::Type::LITERAL:
			formatLiteral(node);
			break;
		case IAstNode::Type::IDENTIFIER:
			formatIdentifier(node);
			break;
		case IAstNode::Type::SCOPED_IDENTIFIER:
			formatScopedIdentifier(node);
			break;
		case IAstNode::Type::BREAK_STATEMENT:
			formatBreak(node);
			break;
		case IAstNode::Type::CONTINUE_STATEMENT:
			formatContinue(node);
			break;
		case IAstNode::Type::COMMENT:
			formatComment(node);
			break;
		default:
			// Unknown node type, skip
			break;
		}
	}

	void Formatter::formatProgram(const IAstNode* node) {
		const AstProgram* program = static_cast<const AstProgram*>(node);

		for (size_t i = 0; i < program->childCount(); i++) {
			formatNode(program->child(i));

			// Add blank line between top-level declarations
			if (i < program->childCount() - 1) {
				const IAstNode* current = program->child(i);
				const IAstNode* next = program->child(i + 1);

				// Functions: always add blank line
				if (current->type() == IAstNode::Type::FUNCTION_DECLARATION) {
					newLine();
				}
				// Use statements: add blank line only if next is not a use statement
				else if (current->type() == IAstNode::Type::USE_STATEMENT) {
					if (next->type() != IAstNode::Type::USE_STATEMENT) {
						newLine();
					}
				}
				// Constants: add blank line only if next is not a constant
				else if (current->type() == IAstNode::Type::CONSTANT_DECLARATION) {
					if (next->type() != IAstNode::Type::CONSTANT_DECLARATION) {
						newLine();
					}
				}
			}
		}
	}

	void Formatter::formatFunction(const IAstNode* node) {
		const AstNodeFunctionDeclaration* func = static_cast<const AstNodeFunctionDeclaration*>(node);

		writeIndent();
		write("fn ");
		write(func->name());
		write("(");

		// Format input parameters
		const auto& inputs = func->inputParameters();
		for (size_t i = 0; i < inputs.size(); i++) {
			if (i > 0) {
				write(" ");
			}
			const AstNodeParameter* param = static_cast<const AstNodeParameter*>(inputs[i]);
			write(param->name());
			write(":");
			write(param->typeString());
		}

		// Format output parameters
		write(" -- ");

		const auto& outputs = func->outputParameters();
		for (size_t i = 0; i < outputs.size(); i++) {
			if (i > 0) {
				write(" ");
			}
			const AstNodeParameter* param = static_cast<const AstNodeParameter*>(outputs[i]);
			write(param->name());
			write(":");
			write(param->typeString());
		}

		write(") {");
		newLine();

		// Format body
		indent();
		if (func->body()) {
			const AstNodeBlock* body = static_cast<const AstNodeBlock*>(func->body());
			for (size_t i = 0; i < body->childCount(); i++) {
				formatNode(body->child(i));
			}
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	void Formatter::formatBlock(const IAstNode* node) {
		const AstNodeBlock* block = static_cast<const AstNodeBlock*>(node);

		for (size_t i = 0; i < block->childCount(); i++) {
			formatNode(block->child(i));
		}
	}

	void Formatter::formatIf(const IAstNode* node) {
		const AstNodeIfStatement* ifNode = static_cast<const AstNodeIfStatement*>(node);

		writeIndent();
		write("if {");
		newLine();

		indent();
		if (ifNode->thenBody()) {
			formatNode(ifNode->thenBody());
		}
		dedent();

		writeIndent();
		write("}");

		if (ifNode->elseBody()) {
			write(" else {");
			newLine();

			indent();
			formatNode(ifNode->elseBody());
			dedent();

			writeIndent();
			write("}");
		}

		newLine();
	}

	void Formatter::formatFor(const IAstNode* node) {
		const AstNodeForStatement* forNode = static_cast<const AstNodeForStatement*>(node);

		writeIndent();
		write("for {");
		newLine();

		indent();
		if (forNode->body()) {
			formatNode(forNode->body());
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	void Formatter::formatSwitch(const IAstNode* node) {
		const AstNodeSwitchStatement* switchNode = static_cast<const AstNodeSwitchStatement*>(node);

		writeIndent();
		write("switch {");
		newLine();

		indent();
		for (size_t i = 0; i < switchNode->childCount(); i++) {
			formatNode(switchNode->child(i));
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	void Formatter::formatCase(const IAstNode* node) {
		writeIndent();
		write("case ");

		// Format case value (first child)
		if (node->childCount() > 0) {
			formatNode(node->child(0));
		}

		write(" {");
		newLine();

		// Format case body
		indent();
		for (size_t i = 1; i < node->childCount(); i++) {
			formatNode(node->child(i));
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	void Formatter::formatDefer(const IAstNode* node) {
		writeIndent();
		write("defer {");
		newLine();

		indent();
		// Format all children in the defer block
		for (size_t i = 0; i < node->childCount(); i++) {
			formatNode(node->child(i));
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	void Formatter::formatUse(const IAstNode* node) {
		const AstNodeUse* useNode = static_cast<const AstNodeUse*>(node);

		writeIndent();
		write("use ");
		write(useNode->module());
		newLine();
	}

	void Formatter::formatConstant(const IAstNode* node) {
		const AstNodeConstant* constNode = static_cast<const AstNodeConstant*>(node);

		writeIndent();
		write("const ");
		write(constNode->name());
		write(" = ");
		write(constNode->value());
		newLine();
	}

	void Formatter::formatInstruction(const IAstNode* node) {
		const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(node);

		writeIndent();
		write(instr->name());
		newLine();
	}

	void Formatter::formatLiteral(const IAstNode* node) {
		const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(node);

		writeIndent();
		// value() already includes quotes for strings
		write(lit->value());
		newLine();
	}

	void Formatter::formatIdentifier(const IAstNode* node) {
		const AstNodeIdentifier* ident = static_cast<const AstNodeIdentifier*>(node);
		writeIndent();
		write(ident->name());
		newLine();
	}

	void Formatter::formatScopedIdentifier(const IAstNode* node) {
		const AstNodeScopedIdentifier* scoped = static_cast<const AstNodeScopedIdentifier*>(node);

		writeIndent();
		write(scoped->scope());
		write("::");
		write(scoped->name());
		newLine();
	}

	void Formatter::formatBreak(const IAstNode*) {
		writeIndent();
		write("break");
		newLine();
	}

	void Formatter::formatContinue(const IAstNode*) {
		writeIndent();
		write("continue");
		newLine();
	}

	void Formatter::formatComment(const IAstNode* node) {
		const AstNodeComment* comment = static_cast<const AstNodeComment*>(node);

		writeIndent();
		if (comment->commentType() == AstNodeComment::CommentType::LINE) {
			write("//");
			write(comment->text());
		} else {
			write("/*");
			write(comment->text());
			write("*/");
		}
		newLine();
	}

	void Formatter::indent() {
		mCurrentIndent++;
	}

	void Formatter::dedent() {
		if (mCurrentIndent > 0) {
			mCurrentIndent--;
		}
	}

	void Formatter::writeIndent() {
		for (int i = 0; i < mCurrentIndent * mIndentWidth; i++) {
			mOutput += '\t';
		}
	}

	void Formatter::write(const std::string& text) {
		mOutput += text;
	}

	void Formatter::writeLine(const std::string& text) {
		mOutput += text;
		mOutput += '\n';
	}

	void Formatter::newLine() {
		mOutput += '\n';
	}

}
