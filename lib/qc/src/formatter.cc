#include "ast_node_block.h"
#include "ast_node_comment.h"
#include "ast_node_label.h"
#include <qc/ast_node_constant.h>
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
#include <qc/ast_node_program.h>
#include <qc/ast_node_return.h>
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
		case IAstNode::Type::LOOP_STATEMENT:
			formatLoop(node);
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
		case IAstNode::Type::IMPORT_STATEMENT:
			formatImport(node);
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
		case IAstNode::Type::RETURN_STATEMENT:
			formatReturn(node);
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
				// Import statements: add blank line only if next is not an import statement
				else if (current->type() == IAstNode::Type::IMPORT_STATEMENT) {
					if (next->type() != IAstNode::Type::IMPORT_STATEMENT) {
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
			if (!param->typeString().empty()) {
				write(":");
				write(param->typeString());
			}
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
			if (!param->typeString().empty()) {
				write(":");
				write(param->typeString());
			}
		}

		write(") {");
		newLine();

		// Format body - keep instructions/literals inline
		indent();
		if (func->body()) {
			const AstNodeBlock* body = static_cast<const AstNodeBlock*>(func->body());
			formatBlockInline(body);
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

	static bool isOperator(const std::string& name) {
		// Symbolic operators that should stay inline
		static const char* operators[] = {"!", "!=", "*", "+", "-", ".", "/", "<", "<=", "==", ">", ">="};
		static const size_t count = sizeof(operators) / sizeof(operators[0]);

		for (size_t i = 0; i < count; i++) {
			if (name == operators[i]) {
				return true;
			}
		}
		return false;
	}

	bool Formatter::isInlineNode(const IAstNode* node) {
		if (!node) {
			return false;
		}

		switch (node->type()) {
		case IAstNode::Type::INSTRUCTION: {
			// Only operators and literals stay inline, not named instructions
			const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(node);
			return isOperator(instr->name());
		}
		case IAstNode::Type::LITERAL:
			return true;
		case IAstNode::Type::IDENTIFIER:
		case IAstNode::Type::SCOPED_IDENTIFIER:
		case IAstNode::Type::FUNCTION_POINTER_REFERENCE:
			return false; // Function calls should break to new lines
		default:
			return false;
		}
	}

	// Helper to check if the next node is an operator
	static bool nextNodeIsOperator(const AstNodeBlock* block, size_t currentIndex) {
		if (currentIndex + 1 >= block->childCount()) {
			return false;
		}
		const IAstNode* nextNode = block->child(currentIndex + 1);
		if (nextNode && nextNode->type() == IAstNode::Type::INSTRUCTION) {
			const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(nextNode);
			return isOperator(instr->name());
		}
		return false;
	}

	void Formatter::formatBlockInline(const IAstNode* node) {
		const AstNodeBlock* block = static_cast<const AstNodeBlock*>(node);

		for (size_t i = 0; i < block->childCount(); i++) {
			const IAstNode* child = block->child(i);

			// Start a new line with inline nodes (operators and literals)
			if (isInlineNode(child)) {
				// Check if we're continuing from a previous named instruction or identifier
				// This happens when previous was a named instruction/identifier that continued because we're an
				// operator
				bool continuingLine = false;
				if (i > 0 && child->type() == IAstNode::Type::INSTRUCTION) {
					const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(child);
					if (isOperator(instr->name())) {
						const IAstNode* prevNode = block->child(i - 1);
						// If previous is a named (non-operator) instruction, identifier, or scoped identifier, it would
						// have continued to us
						if (prevNode && (prevNode->type() == IAstNode::Type::INSTRUCTION ||
												prevNode->type() == IAstNode::Type::IDENTIFIER ||
												prevNode->type() == IAstNode::Type::SCOPED_IDENTIFIER)) {
							if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
								const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
								if (!isOperator(prevInstr->name())) {
									continuingLine = true;
								}
							} else {
								// Identifiers and scoped identifiers always continue
								continuingLine = true;
							}
						}
					}
				}

				if (continuingLine) {
					write(" ");
				} else {
					writeIndent();
				}
				size_t firstInlineIndex = i;
				const IAstNode* lastInlineNode = nullptr;
				while (i < block->childCount() && isInlineNode(block->child(i))) {
					if (i > firstInlineIndex) {
						write(" ");
					}

					const IAstNode* inlineNode = block->child(i);
					if (!inlineNode) {
						break;
					}
					lastInlineNode = inlineNode;
					switch (inlineNode->type()) {
					case IAstNode::Type::INSTRUCTION: {
						const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(inlineNode);
						write(instr->name());
						// Break inline sequence after '.' operator
						if (instr->name() == ".") {
							i++;
							lastInlineNode = inlineNode;
							goto end_inline_sequence;
						}
						break;
					}
					case IAstNode::Type::LITERAL: {
						const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(inlineNode);
						write(lit->value());
						break;
					}
					default:
						break;
					}
					i++;
				}
			end_inline_sequence:
				i--; // Adjust because the for loop will increment

				// Check if last inline node was '.' operator
				bool lastWasPrint = false;
				if (lastInlineNode && lastInlineNode->type() == IAstNode::Type::INSTRUCTION) {
					const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(lastInlineNode);
					if (instr->name() == ".") {
						lastWasPrint = true;
					}
				}

				// After processing inline nodes, check what comes next
				if (i + 1 < block->childCount()) {
					const IAstNode* nextNode = block->child(i + 1);
					// If next is if/for/loop, process it on same line
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue; // Skip the newline, formatNode already added it
					}
					// If last was '.', check if next is 'nl'
					if (lastWasPrint && nextNode && nextNode->type() == IAstNode::Type::INSTRUCTION) {
						const AstNodeInstruction* nextInstr = static_cast<const AstNodeInstruction*>(nextNode);
						if (nextInstr->name() == "nl") {
							// Keep '.' and 'nl' on same line
							write(" nl");
							i++; // Skip the nl instruction
							newLine();
							continue;
						}
						// If last was '.' but next is NOT 'nl', add newline now
						newLine();
						continue;
					}

					// If next is a named instruction or identifier, don't newline - let it continue on this line
					if (nextNode && (nextNode->type() == IAstNode::Type::INSTRUCTION ||
											nextNode->type() == IAstNode::Type::IDENTIFIER ||
											nextNode->type() == IAstNode::Type::SCOPED_IDENTIFIER)) {
						// Don't newline, continue to next iteration which will process it on same line
						continue;
					}
				}

				// Add newline after '.', or after any inline sequence
				newLine();
			} else if (child->type() == IAstNode::Type::IF_STATEMENT ||
					   child->type() == IAstNode::Type::FOR_STATEMENT ||
					   child->type() == IAstNode::Type::LOOP_STATEMENT) {
				// if/for without preceding inline nodes still need indent
				writeIndent();
				formatNode(child);
			} else if (child->type() == IAstNode::Type::INSTRUCTION) {
				// Named instruction - check if we should continue from previous line or start new line
				bool needsIndent = true;

				// Check if previous node was an inline node or operator - if so, continue on same line
				if (i > 0) {
					const IAstNode* prevNode = block->child(i - 1);
					if (prevNode) {
						// If previous was a literal, we're continuing
						if (prevNode->type() == IAstNode::Type::LITERAL) {
							needsIndent = false;
						}
						// If previous was an operator, we're continuing
						else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
							const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
							// Continue if previous was operator, EXCEPT for '.' which breaks to new line
							if (isOperator(prevInstr->name()) && prevInstr->name() != ".") {
								needsIndent = false;
							}
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(child);
				write(instr->name());

				// Named instructions don't break if next is an operator
				if (nextNodeIsOperator(block, i)) {
					continue;
				}

				// If next is if/for, keep on same line
				if (i + 1 < block->childCount()) {
					const IAstNode* nextNode = block->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}

				newLine();
			} else if (child->type() == IAstNode::Type::IDENTIFIER) {
				// Function calls - check if we should continue from previous line
				bool needsIndent = true;
				if (i > 0) {
					const IAstNode* prevNode = block->child(i - 1);
					if (prevNode) {
						if (prevNode->type() == IAstNode::Type::LITERAL) {
							needsIndent = false;
						} else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
							const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
							// Continue if previous was operator or any instruction
							if (isOperator(prevInstr->name())) {
								needsIndent = false;
							} else {
								// Previous was a named instruction - continue
								needsIndent = false;
							}
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeIdentifier* ident = static_cast<const AstNodeIdentifier*>(child);
				write(ident->name());

				// Identifiers don't break if next is an operator
				if (nextNodeIsOperator(block, i)) {
					continue;
				}

				// If next is if/for, keep on same line
				if (i + 1 < block->childCount()) {
					const IAstNode* nextNode = block->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}

				newLine();
			} else if (child->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
				// Scoped identifiers - check if we should continue from previous line
				bool needsIndent = true;
				if (i > 0) {
					const IAstNode* prevNode = block->child(i - 1);
					if (prevNode) {
						if (prevNode->type() == IAstNode::Type::LITERAL) {
							needsIndent = false;
						} else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
							const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
							// Continue if previous was operator or any instruction
							if (isOperator(prevInstr->name())) {
								needsIndent = false;
							} else {
								// Previous was a named instruction - continue
								needsIndent = false;
							}
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeScopedIdentifier* scoped = static_cast<const AstNodeScopedIdentifier*>(child);
				write(scoped->scope());
				write("::");
				write(scoped->name());

				// Scoped identifiers don't break if next is an operator
				if (nextNodeIsOperator(block, i)) {
					continue;
				}

				// If next is if/for, keep on same line
				if (i + 1 < block->childCount()) {
					const IAstNode* nextNode = block->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}

				newLine();
			} else if (child->type() == IAstNode::Type::FUNCTION_POINTER_REFERENCE) {
				// Function pointers get their own line unless preceded by operator
				bool needsIndent = true;
				if (i > 0) {
					const IAstNode* prevNode = block->child(i - 1);
					if (prevNode && prevNode->type() == IAstNode::Type::INSTRUCTION) {
						const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
						if (isOperator(prevInstr->name())) {
							needsIndent = false;
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeFunctionPointerReference* funcPtr =
						static_cast<const AstNodeFunctionPointerReference*>(child);
				write("&");
				write(funcPtr->functionName());

				// If next node is an operator, continue on same line
				if (nextNodeIsOperator(block, i)) {
					continue;
				}

				// If next node is if/for, keep on same line
				if (i + 1 < block->childCount()) {
					const IAstNode* nextNode = block->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}
				newLine();
			} else {
				// Other non-inline nodes (control structures, comments, etc.)
				formatNode(child);
			}
		}
	}

	void Formatter::formatIf(const IAstNode* node) {
		const AstNodeIfStatement* ifNode = static_cast<const AstNodeIfStatement*>(node);

		write("if {");
		newLine();

		indent();
		if (ifNode->thenBody()) {
			formatBlockInline(ifNode->thenBody());
		}
		dedent();

		writeIndent();
		write("}");

		if (ifNode->elseBody()) {
			write(" else {");
			newLine();

			indent();
			formatBlockInline(ifNode->elseBody());
			dedent();

			writeIndent();
			write("}");
		}

		newLine();
	}

	void Formatter::formatFor(const IAstNode* node) {
		const AstNodeForStatement* forNode = static_cast<const AstNodeForStatement*>(node);

		write("for {");
		newLine();

		indent();
		if (forNode->body()) {
			formatBlockInline(forNode->body());
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	void Formatter::formatLoop(const IAstNode* node) {
		const AstNodeLoopStatement* loopNode = static_cast<const AstNodeLoopStatement*>(node);

		write("loop {");
		newLine();

		indent();
		if (loopNode->body()) {
			formatBlockInline(loopNode->body());
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
		const AstNodeCase* caseNode = static_cast<const AstNodeCase*>(node);

		writeIndent();
		if (caseNode->isDefault()) {
			write("default {");
		} else {
			write("case ");
			// Format case value inline
			if (caseNode->value()) {
				const IAstNode* val = caseNode->value();
				switch (val->type()) {
				case IAstNode::Type::LITERAL: {
					const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(val);
					write(lit->value());
					break;
				}
				case IAstNode::Type::IDENTIFIER: {
					const AstNodeIdentifier* ident = static_cast<const AstNodeIdentifier*>(val);
					write(ident->name());
					break;
				}
				default:
					break;
				}
			}
			write(" {");
		}
		newLine();

		// Format case body inline
		indent();
		if (caseNode->body()) {
			formatBlockInline(caseNode->body());
		}
		dedent();

		writeIndent();
		write("}");
		newLine();
	}

	// Helper to check if next node in generic parent is an operator
	static bool nextChildIsOperator(const IAstNode* parent, size_t currentIndex) {
		if (currentIndex + 1 >= parent->childCount()) {
			return false;
		}
		const IAstNode* nextNode = parent->child(currentIndex + 1);
		if (nextNode && nextNode->type() == IAstNode::Type::INSTRUCTION) {
			const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(nextNode);
			return isOperator(instr->name());
		}
		return false;
	}

	void Formatter::formatDefer(const IAstNode* node) {
		writeIndent();
		write("defer {");
		newLine();

		indent();
		// Format defer body using the same inline logic
		for (size_t i = 0; i < node->childCount(); i++) {
			const IAstNode* child = node->child(i);

			// Group inline nodes (operators and literals)
			if (isInlineNode(child)) {
				// Check if we're continuing from a previous non-inline instruction
				bool continuingLine = false;
				if (i > 0 && child->type() == IAstNode::Type::INSTRUCTION) {
					const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(child);
					if (isOperator(instr->name())) {
						const IAstNode* prevNode = node->child(i - 1);
						if (prevNode && prevNode->type() == IAstNode::Type::INSTRUCTION) {
							const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
							if (!isOperator(prevInstr->name())) {
								continuingLine = true;
							}
						}
					}
				}

				if (continuingLine) {
					write(" ");
				} else {
					writeIndent();
				}
				size_t firstInlineIndex = i;
				while (i < node->childCount() && isInlineNode(node->child(i))) {
					if (i > firstInlineIndex) {
						write(" ");
					}

					const IAstNode* inlineNode = node->child(i);
					if (!inlineNode) {
						break;
					}
					switch (inlineNode->type()) {
					case IAstNode::Type::INSTRUCTION: {
						const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(inlineNode);
						write(instr->name());
						break;
					}
					case IAstNode::Type::LITERAL: {
						const AstNodeLiteral* lit = static_cast<const AstNodeLiteral*>(inlineNode);
						write(lit->value());
						break;
					}
					default:
						break;
					}
					i++;
				}
				i--; // Adjust for the for loop increment

				// Check if next node is a named instruction, identifier, or scoped identifier - keep on same line
				if (i + 1 < node->childCount()) {
					const IAstNode* nextNode = node->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::INSTRUCTION ||
											nextNode->type() == IAstNode::Type::IDENTIFIER ||
											nextNode->type() == IAstNode::Type::SCOPED_IDENTIFIER ||
											nextNode->type() == IAstNode::Type::FUNCTION_POINTER_REFERENCE)) {
						write(" ");
						i++;

						// Write the instruction/identifier
						switch (nextNode->type()) {
						case IAstNode::Type::INSTRUCTION: {
							const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(nextNode);
							write(instr->name());
							break;
						}
						case IAstNode::Type::IDENTIFIER: {
							const AstNodeIdentifier* ident = static_cast<const AstNodeIdentifier*>(nextNode);
							write(ident->name());
							break;
						}
						case IAstNode::Type::SCOPED_IDENTIFIER: {
							const AstNodeScopedIdentifier* scoped =
									static_cast<const AstNodeScopedIdentifier*>(nextNode);
							write(scoped->scope());
							write("::");
							write(scoped->name());
							break;
						}
						case IAstNode::Type::FUNCTION_POINTER_REFERENCE: {
							const AstNodeFunctionPointerReference* funcPtr =
									static_cast<const AstNodeFunctionPointerReference*>(nextNode);
							write("&");
							write(funcPtr->functionName());
							break;
						}
						default:
							break;
						}

						// Check if next node (after the instruction we just wrote) is if/for
						if (i + 1 < node->childCount()) {
							const IAstNode* nextNext = node->child(i + 1);
							if (nextNext && (nextNext->type() == IAstNode::Type::IF_STATEMENT ||
													nextNext->type() == IAstNode::Type::FOR_STATEMENT ||
													nextNext->type() == IAstNode::Type::LOOP_STATEMENT)) {
								write(" ");
								i++;
								formatNode(nextNext);
								continue;
							}
						}

						newLine();
						continue;
					}
				}
				newLine();
			} else if (child->type() == IAstNode::Type::INSTRUCTION) {
				// Named instruction - check if next is an operator or control flow to keep on same line
				bool needsIndent = true;

				// Check if previous node ended with an operator (we're already on the same line)
				if (i > 0) {
					const IAstNode* prevNode = node->child(i - 1);
					if (prevNode && prevNode->type() == IAstNode::Type::INSTRUCTION) {
						const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
						if (isOperator(prevInstr->name())) {
							needsIndent = false;
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeInstruction* instr = static_cast<const AstNodeInstruction*>(child);
				write(instr->name());

				// If next node is an operator, continue on same line
				if (nextChildIsOperator(node, i)) {
					continue;
				}

				// If next node is if/for, keep on same line
				if (i + 1 < node->childCount()) {
					const IAstNode* nextNode = node->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}
				newLine();
			} else if (child->type() == IAstNode::Type::IDENTIFIER) {
				// Function calls get their own line unless preceded by operator
				bool needsIndent = true;
				if (i > 0) {
					const IAstNode* prevNode = node->child(i - 1);
					if (prevNode && prevNode->type() == IAstNode::Type::INSTRUCTION) {
						const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
						if (isOperator(prevInstr->name())) {
							needsIndent = false;
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeIdentifier* ident = static_cast<const AstNodeIdentifier*>(child);
				write(ident->name());

				// If next node is an operator, continue on same line
				if (nextChildIsOperator(node, i)) {
					continue;
				}

				// If next node is if/for, keep on same line
				if (i + 1 < node->childCount()) {
					const IAstNode* nextNode = node->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}
				newLine();
			} else if (child->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
				// Scoped identifiers get their own line unless preceded by operator
				bool needsIndent = true;
				if (i > 0) {
					const IAstNode* prevNode = node->child(i - 1);
					if (prevNode && prevNode->type() == IAstNode::Type::INSTRUCTION) {
						const AstNodeInstruction* prevInstr = static_cast<const AstNodeInstruction*>(prevNode);
						if (isOperator(prevInstr->name())) {
							needsIndent = false;
						}
					}
				}

				if (needsIndent) {
					writeIndent();
				} else {
					write(" ");
				}

				const AstNodeScopedIdentifier* scoped = static_cast<const AstNodeScopedIdentifier*>(child);
				write(scoped->scope());
				write("::");
				write(scoped->name());

				// If next node is an operator, continue on same line
				if (nextChildIsOperator(node, i)) {
					continue;
				}

				// If next node is if/for, keep on same line
				if (i + 1 < node->childCount()) {
					const IAstNode* nextNode = node->child(i + 1);
					if (nextNode && (nextNode->type() == IAstNode::Type::IF_STATEMENT ||
											nextNode->type() == IAstNode::Type::FOR_STATEMENT ||
											nextNode->type() == IAstNode::Type::LOOP_STATEMENT)) {
						write(" ");
						i++;
						formatNode(nextNode);
						continue;
					}
				}
				newLine();
			} else {
				// Other nodes
				formatNode(child);
			}
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

		std::string moduleName = useNode->module();

		// Wrap in quotes if the path contains:
		// - whitespace or special characters that would be invalid in the token stream
		// - forward slash (path separator) since it would be tokenized separately
		bool needsQuotes = false;
		for (char c : moduleName) {
			if (std::isspace(static_cast<unsigned char>(c)) || c == '/' || c == '(' || c == ')' || c == '[' ||
					c == ']' || c == '{' || c == '}' || c == '<' || c == '>' || c == ',' || c == ';' || c == ':' ||
					c == '!' || c == '?' || c == '*' || c == '&' || c == '|' || c == '^' || c == '%' || c == '@' ||
					c == '#' || c == '$' || c == '`' || c == '~' || c == '\\') {
				needsQuotes = true;
				break;
			}
		}

		if (needsQuotes) {
			write("\"");
			write(moduleName);
			write("\"");
		} else {
			write(moduleName);
		}

		newLine();
	}

	void Formatter::formatImport(const IAstNode* node) {
		const AstNodeImport* importNode = static_cast<const AstNodeImport*>(node);

		writeIndent();
		write("import \"");
		write(importNode->library());
		write("\" as \"");
		write(importNode->namespaceName());
		write("\" {");
		newLine();

		// Format imported function declarations
		indent();
		const auto& functions = importNode->functions();
		for (const auto* func : functions) {
			writeIndent();
			write("fn ");
			write(func->name);
			write("(");

			// Format input parameters
			const auto& inputs = func->inputParameters;
			for (size_t i = 0; i < inputs.size(); i++) {
				if (i > 0) {
					write(" ");
				}
				const AstNodeParameter* param = inputs[i];
				write(param->name());
				if (!param->typeString().empty()) {
					write(":");
					write(param->typeString());
				}
			}

			write(" -- ");

			// Format output parameters
			const auto& outputs = func->outputParameters;
			for (size_t i = 0; i < outputs.size(); i++) {
				if (i > 0) {
					write(" ");
				}
				const AstNodeParameter* param = outputs[i];
				write(param->name());
				if (!param->typeString().empty()) {
					write(":");
					write(param->typeString());
				}
			}

			write(")");
			newLine();
		}
		dedent();

		writeIndent();
		write("}");
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

	void Formatter::formatReturn(const IAstNode*) {
		writeIndent();
		write("return");
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
