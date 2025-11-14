#include <cstring>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include "ast_node_block.h"
#include <qc/ast_node_break.h>
#include "ast_node_comment.h"
#include <qc/ast_node_constant.h>
#include <qc/ast_node_continue.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include "ast_node_label.h"
#include <qc/ast_node_literal.h>
#include <qc/ast_node_loop.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_return.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>
#include <qc/colors.h>
#include <qc/error_reporter.h>
#include <qc/instructions.h>
#include <u8t/scanner.h>
#include <vector>

namespace Qd {

	// Helper function to convert UTF-8 character index to byte offset
	static size_t charIndexToByteOffset(const char* src, size_t charIndex) {
		size_t byteOffset = 0;
		size_t currentCharIndex = 0;

		while (currentCharIndex < charIndex && src[byteOffset] != '\0') {
			unsigned char c = static_cast<unsigned char>(src[byteOffset]);
			if ((c & 0x80) == 0) {
				// Single-byte character (ASCII)
				byteOffset += 1;
			} else if ((c & 0xE0) == 0xC0) {
				// Two-byte character
				byteOffset += 2;
			} else if ((c & 0xF0) == 0xE0) {
				// Three-byte character
				byteOffset += 3;
			} else if ((c & 0xF8) == 0xF0) {
				// Four-byte character
				byteOffset += 4;
			} else {
				// Invalid UTF-8, skip one byte
				byteOffset += 1;
			}
			currentCharIndex++;
		}

		return byteOffset;
	}

	// Helper to calculate line and column from byte position
	static void calculateLineColumn(const char* src, size_t pos, size_t* line, size_t* column) {
		*line = 1;
		*column = 1;
		for (size_t i = 0; i < pos && src[i] != '\0'; i++) {
			if (src[i] == '\n') {
				(*line)++;
				*column = 1;
			} else {
				(*column)++;
			}
		}
	}

	// Helper to set position on a node from scanner
	static void setNodePosition(IAstNode* node, u8t_scanner* scanner, const char* src) {
		size_t pos = u8t_scanner_token_start(scanner);
		size_t line, column;
		calculateLineColumn(src, pos, &line, &column);
		node->setPosition(line, column);
	}

	// Helper to parse a comment (// or /* */)
	// Returns the comment node, or nullptr if not a comment
	static AstNodeComment* parseComment(u8t_scanner* scanner, const char* src, bool sawSlash, char32_t token) {
		if (!sawSlash) {
			return nullptr;
		}

		AstNodeComment::CommentType commentType;
		if (token == '/') {
			commentType = AstNodeComment::CommentType::LINE;
		} else if (token == '*') {
			commentType = AstNodeComment::CommentType::BLOCK;
		} else {
			return nullptr;
		}

		// Get character position and convert to byte offset
		size_t charPos = u8t_scanner_token_start(scanner);
		size_t tokenLen = u8t_scanner_token_len(scanner);
		size_t bytePos = charIndexToByteOffset(src, charPos + tokenLen);

		// Read comment text directly from source
		const char* commentStart = src + bytePos;
		const char* commentEnd = commentStart;

		if (commentType == AstNodeComment::CommentType::LINE) {
			// Read until end of line
			while (*commentEnd != '\0' && *commentEnd != '\n' && *commentEnd != '\r') {
				commentEnd++;
			}
		} else {
			// Read until */
			// Check both current and next character to avoid buffer overflow
			while (*commentEnd != '\0' && *(commentEnd + 1) != '\0') {
				if (*commentEnd == '*' && *(commentEnd + 1) == '/') {
					break;
				}
				commentEnd++;
			}
		}

		std::string commentText(commentStart, static_cast<size_t>(commentEnd - commentStart));

		// Advance scanner past the comment
		if (commentType == AstNodeComment::CommentType::LINE) {
			while (u8t_scanner_peek(scanner) != 0 && u8t_scanner_peek(scanner) != '\n' &&
					u8t_scanner_peek(scanner) != '\r') {
				u8t_scanner_scan(scanner);
			}
		} else {
			bool foundStar = false;
			while (u8t_scanner_peek(scanner) != 0) {
				char32_t c = u8t_scanner_scan(scanner);
				if (foundStar && c == '/') {
					break;
				}
				foundStar = (c == '*');
			}
		}

		// Create and return comment node
		AstNodeComment* comment = new AstNodeComment(commentText, commentType);
		setNodePosition(comment, scanner, src);
		return comment;
	}

	static IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseLoopStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);

	// Helper to synchronize parser after an error
	// Skips tokens until a synchronization point is found
	static void synchronize(u8t_scanner* scanner) {
		char32_t token;
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Stop at statement boundaries
			if (token == '}' || token == '{' || token == ';') {
				return;
			}

			// Stop at keywords that start new declarations/statements
			if (token == U8T_IDENTIFIER) {
				size_t n;
				const char* text = u8t_scanner_token_text(scanner, &n);
				if (strcmp(text, "fn") == 0 || strcmp(text, "const") == 0 || strcmp(text, "use") == 0 ||
						strcmp(text, "import") == 0 || strcmp(text, "if") == 0 || strcmp(text, "for") == 0 ||
						strcmp(text, "loop") == 0 || strcmp(text, "switch") == 0 || strcmp(text, "return") == 0) {
					return;
				}
			}
		}
	}

	// Helper to check if a token is an operator alias and create the corresponding instruction node
	// Returns the instruction node if it's an operator, nullptr otherwise
	static IAstNode* tryParseOperatorAlias(char32_t token, u8t_scanner* scanner, const char* src) {
		// Map of operator tokens to their instruction names
		static const struct {
			char32_t token;
			const char* instruction;
		} OPERATOR_ALIASES[] = {
				{'.', "."}, // print
				{'/', "/"}, // div
				{'*', "*"}, // mul
				{'+', "+"}, // add
				{'-', "-"}, // sub
				{'%', "%"}  // mod
		};
		static const size_t OPERATOR_COUNT = sizeof(OPERATOR_ALIASES) / sizeof(OPERATOR_ALIASES[0]);

		for (size_t i = 0; i < OPERATOR_COUNT; i++) {
			if (token == OPERATOR_ALIASES[i].token) {
				IAstNode* node = new AstNodeInstruction(OPERATOR_ALIASES[i].instruction);
				setNodePosition(node, scanner, src);
				return node;
			}
		}
		return nullptr;
	}

	// Helper to parse a single statement/expression token
	// Returns nullptr if token was a control keyword that was handled
	// Returns a node if it's a literal or identifier
	static IAstNode* parseSimpleToken(
			char32_t token, u8t_scanner* scanner, size_t* n, const char* src) {
		if (token == U8T_INTEGER) {
			const char* text = u8t_scanner_token_text(scanner, n);
			IAstNode* node = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::INTEGER);
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == U8T_FLOAT) {
			const char* text = u8t_scanner_token_text(scanner, n);
			IAstNode* node = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::FLOAT);
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == U8T_STRING) {
			const char* text = u8t_scanner_token_text(scanner, n);
			IAstNode* node = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::STRING);
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, n);
			if (isBuiltInInstruction(text)) {
				IAstNode* node = new AstNodeInstruction(text);
				setNodePosition(node, scanner, src);
				return node;
			}
			AstNodeIdentifier* node = new AstNodeIdentifier(text);
			setNodePosition(node, scanner, src);
			// Check for '!' or '?' suffix
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '!') {
				u8t_scanner_scan(scanner); // Consume the '!'
				node->setAbortOnError(true);
			} else if (nextToken == '?') {
				u8t_scanner_scan(scanner); // Consume the '?'
				node->setCheckError(true);
			}
			return node;
		}

		// Try to parse as operator alias
		IAstNode* opNode = tryParseOperatorAlias(token, scanner, src);
		if (opNode != nullptr) {
			return opNode;
		}

		if (token == '<') {
			// Check if next token is '=' for '<='
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '=') {
				u8t_scanner_scan(scanner); // Consume '='
				IAstNode* node = new AstNodeInstruction("<=");
				setNodePosition(node, scanner, src);
				return node;
			}
			// Handle '<' as alias for 'lt'
			IAstNode* node = new AstNodeInstruction("<");
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == '>') {
			// Check if next token is '=' for '>='
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '=') {
				u8t_scanner_scan(scanner); // Consume '='
				IAstNode* node = new AstNodeInstruction(">=");
				setNodePosition(node, scanner, src);
				return node;
			}
			// Handle '>' as alias for 'gt'
			IAstNode* node = new AstNodeInstruction(">");
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == '=') {
			// Check if next token is '=' for '=='
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '=') {
				u8t_scanner_scan(scanner); // Consume '='
				IAstNode* node = new AstNodeInstruction("==");
				setNodePosition(node, scanner, src);
				return node;
			}
			return nullptr;
		} else if (token == '!') {
			// Check if next token is '=' for '!='
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '=') {
				u8t_scanner_scan(scanner); // Consume '='
				IAstNode* node = new AstNodeInstruction("!=");
				setNodePosition(node, scanner, src);
				return node;
			}
			// '!' by itself is not a standalone operator - it must be used as a suffix
			return nullptr;
		} else if (token == '$') {
			// Handle '$' as for loop iterator variable
			IAstNode* node = new AstNodeIdentifier("$");
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == '&') {
			// Handle '&' as function pointer reference
			size_t ampPos = u8t_scanner_token_start(scanner);
			char32_t nextToken = u8t_scanner_scan(scanner);
			if (nextToken == U8T_IDENTIFIER) {
				size_t n2;
				const char* functionName = u8t_scanner_token_text(scanner, &n2);
				IAstNode* node = new AstNodeFunctionPointerReference(functionName);
				// Set position to the & token
				size_t line, column;
				calculateLineColumn(src, ampPos, &line, &column);
				node->setPosition(line, column);
				return node;
			}
			// If not followed by identifier, return nullptr (error will be handled by caller)
			return nullptr;
		}
		return nullptr;
	}

	// Forward declarations for recursive parsing
	static void parseBlockBody(
			AstNodeBlock* block, u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseBlockStatement(char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n,
			const char* src, bool allowControlFlow = true);

	// Helper function to parse a block body with proper else-handling
	static void parseBlockBody(
			AstNodeBlock* block, u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		size_t n;
		char32_t token;
		bool sawSlash = false;
		bool sawColon = false;
		std::vector<IAstNode*> tempNodes;

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle :: scope operator
			if (sawColon && token == ':') {
				// We have ::
				sawColon = false;
				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					AstNodeIdentifier* scope = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();

					// Get the next identifier after ::
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* memberName = u8t_scanner_token_text(scanner, &n);
						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scope->name(), memberName);
						setNodePosition(scoped, scanner, src);
						delete scope;
						tempNodes.push_back(scoped);
					} else {
						// No identifier after ::, put scope back
						tempNodes.push_back(scope);
					}
				}
				continue;
			}

			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(scanner, src, sawSlash, token);
			if (comment != nullptr) {
				sawSlash = false;
				// Flush tempNodes before adding comment
				for (auto* node : tempNodes) {
					node->setParent(block);
					block->addChild(node);
				}
				tempNodes.clear();
				// Add comment to block
				comment->setParent(block);
				block->addChild(comment);
				continue;
			}

			// If we saw a slash but it wasn't a comment, it's a division operator
			if (sawSlash) {
				sawSlash = false;
				// Add division instruction to tempNodes
				AstNodeInstruction* divInstr = new AstNodeInstruction("/");
				setNodePosition(divInstr, scanner, src);
				tempNodes.push_back(divInstr);
			}

			if (token == '}') {
				break;
			}

			sawSlash = (token == '/');
			if (sawSlash) {
				continue; // Wait for next token to see if it's a comment
			}

			sawColon = (token == ':');
			if (sawColon) {
				continue; // Wait for next token to see if it's another colon
			}

			// Check if this token is an "else" keyword
			if (token == U8T_IDENTIFIER) {
				const char* tokenText = u8t_scanner_token_text(scanner, &n);
				if (strcmp(tokenText, "else") == 0) {
					// Flush tempNodes before handling else
					for (auto* node : tempNodes) {
						node->setParent(block);
						block->addChild(node);
					}
					tempNodes.clear();

					// else must follow an if statement
					IAstNode* lastChild = (block->childCount() > 0) ? block->child(block->childCount() - 1) : nullptr;
					if (lastChild && lastChild->type() == IAstNode::Type::IF_STATEMENT) {
						AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(lastChild);

						// Parse else block - must have {
						token = u8t_scanner_scan(scanner);
						if (token != '{') {
							errorReporter->reportError(scanner, "Expected '{' after 'else'");
						} else {
							AstNodeBlock* elseBody = new AstNodeBlock();
							setNodePosition(elseBody, scanner, src);

							// Recursively parse the else body
							parseBlockBody(elseBody, scanner, errorReporter, src);

							elseBody->setParent(ifStmt);
							ifStmt->setElseBody(elseBody);
						}
						continue; // Skip adding else as a regular statement
					} else {
						errorReporter->reportError(scanner, "'else' without preceding 'if'");
						continue;
					}
				}
			}

			IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, src);
			if (node) {
				tempNodes.push_back(node);
			}
		}

		// Flush remaining tempNodes
		for (auto* node : tempNodes) {
			node->setParent(block);
			block->addChild(node);
		}
	}

	// Helper to parse statements inside a block (handles if, break, continue, nested structures)
	// Returns a node that should be added to the parent, or nullptr
	// allowControlFlow: if false, only allows break/continue but not if/for/switch
	static IAstNode* parseBlockStatement(char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n,
			const char* src, bool allowControlFlow) {
		if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, n);

			// break and continue are always allowed
			if (strcmp(text, "break") == 0) {
				IAstNode* node = new AstNodeBreak();
				setNodePosition(node, scanner, src);
				return node;
			} else if (strcmp(text, "continue") == 0) {
				IAstNode* node = new AstNodeContinue();
				setNodePosition(node, scanner, src);
				return node;
			}

			if (allowControlFlow) {
				if (strcmp(text, "if") == 0) {
					return parseIfStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "for") == 0) {
					return parseForStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "loop") == 0) {
					return parseLoopStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "switch") == 0) {
					return parseSwitchStatement(scanner, errorReporter, src);
				}
			}

			if (isBuiltInInstruction(text)) {
				IAstNode* node = new AstNodeInstruction(text);
				setNodePosition(node, scanner, src);
				return node;
			}
			AstNodeIdentifier* node = new AstNodeIdentifier(text);
			setNodePosition(node, scanner, src);
			// Check for '!' or '?' suffix
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '!') {
				u8t_scanner_scan(scanner); // Consume the '!'
				node->setAbortOnError(true);
			} else if (nextToken == '?') {
				u8t_scanner_scan(scanner); // Consume the '?'
				node->setCheckError(true);
			}
			return node;
		}
		if (token == '&') {
			size_t ampPos = u8t_scanner_token_start(scanner);
			char32_t nextToken = u8t_scanner_scan(scanner);
			if (nextToken == U8T_IDENTIFIER) {
				size_t n2;
				const char* functionName = u8t_scanner_token_text(scanner, &n2);
				IAstNode* node = new AstNodeFunctionPointerReference(functionName);
				// Set position to the & token
				size_t line, column;
				calculateLineColumn(src, ampPos, &line, &column);
				node->setPosition(line, column);
				return node;
			} else {
				errorReporter->reportError(scanner, "Expected function name after '&'");
				return nullptr;
			}
		}

		return parseSimpleToken(token, scanner, n, src);
	}

	Ast::~Ast() {
		if (mRoot) {
			delete mRoot;
			mRoot = nullptr;
		}
	}

	static IAstNode* parseFunctionDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected function name after 'fn'");
			synchronize(scanner);
			return nullptr;
		}

		size_t n;
		const char* name = u8t_scanner_token_text(scanner, &n);
		AstNodeFunctionDeclaration* func = new AstNodeFunctionDeclaration(name);
		setNodePosition(func, scanner, src);

		token = u8t_scanner_scan(scanner);
		if (token != '(') {
			errorReporter->reportError(scanner, "Expected '(' after function name");
			synchronize(scanner);
			delete func;
			return nullptr;
		}

		bool isOutput = false;
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == ')') {
				break;
			}

			if (token == '-') {
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == '-') {
					isOutput = true;
				}
			} else if (token == U8T_IDENTIFIER) {
				const char* paramName = u8t_scanner_token_text(scanner, &n);
				std::string paramNameStr(paramName);

				// Check if there's a type annotation
				char32_t peek = u8t_scanner_peek(scanner);
				if (peek == ':') {
					// Consume the ':'
					u8t_scanner_scan(scanner);
					// Get the type
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* paramType = u8t_scanner_token_text(scanner, &n);
						AstNodeParameter* param = new AstNodeParameter(paramNameStr, paramType, isOutput);
						setNodePosition(param, scanner, src);
						param->setParent(func);
						if (isOutput) {
							func->addOutputParameter(param);
						} else {
							func->addInputParameter(param);
						}
					}
				} else {
					// Untyped parameter - use empty string as type
					AstNodeParameter* param = new AstNodeParameter(paramNameStr, "", isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func);
					if (isOutput) {
						func->addOutputParameter(param);
					} else {
						func->addInputParameter(param);
					}
				}
			}
		}

		// Check for optional '!' marker (fallible function)
		token = u8t_scanner_scan(scanner);
		if (token == '!') {
			func->setThrows(true);
			token = u8t_scanner_scan(scanner);
		}

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after function signature");
			// Recovery: create empty body and return partial function
			AstNodeBlock* body = new AstNodeBlock();
			setNodePosition(body, scanner, src);
			body->setParent(func);
			func->setBody(body);
			synchronize(scanner);
			return func;
		}

		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		std::vector<IAstNode*> tempNodes;
		bool sawColon = false;
		bool sawSlash = false;

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(scanner, src, sawSlash, token);
			if (comment != nullptr) {
				sawSlash = false;
				tempNodes.push_back(comment);
				continue;
			}

			// If we saw a slash but it wasn't a comment, it's a division operator
			if (sawSlash) {
				sawSlash = false;
				// Add division instruction to tempNodes
				AstNodeInstruction* divInstr = new AstNodeInstruction("/");
				setNodePosition(divInstr, scanner, src);
				tempNodes.push_back(divInstr);
			}

			if (token == '}') {
				break;
			}

			sawSlash = (token == '/');
			if (sawSlash) {
				continue; // Wait for next token to see if it's a comment
			}

			// Handle :: scope operator
			if (sawColon && token == ':') {
				// We have ::
				sawColon = false;
				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					AstNodeIdentifier* scope = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();

					// Get the next identifier after ::
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* memberName = u8t_scanner_token_text(scanner, &n);
						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scope->name(), memberName);
						setNodePosition(scoped, scanner, src);
						delete scope;
						tempNodes.push_back(scoped);
					} else {
						// No identifier after ::, put tokens back
						tempNodes.push_back(scope);
						// Can't really handle this case properly without putback
					}
				}
				continue;
			}

			sawColon = (token == ':');
			if (sawColon) {
				continue; // Wait for next token to see if it's another colon
			}

			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);

				if (strcmp(text, "for") == 0) {
					IAstNode* forStmt = parseForStatement(scanner, errorReporter, src);
					if (forStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						forStmt->setParent(body);
						body->addChild(forStmt);
					}
				} else if (strcmp(text, "loop") == 0) {
					IAstNode* loopStmt = parseLoopStatement(scanner, errorReporter, src);
					if (loopStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						loopStmt->setParent(body);
						body->addChild(loopStmt);
					}
				} else if (strcmp(text, "break") == 0) {
					IAstNode* breakStmt = new AstNodeBreak();
					setNodePosition(breakStmt, scanner, src);
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					breakStmt->setParent(body);
					body->addChild(breakStmt);
				} else if (strcmp(text, "continue") == 0) {
					IAstNode* continueStmt = new AstNodeContinue();
					setNodePosition(continueStmt, scanner, src);
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					continueStmt->setParent(body);
					body->addChild(continueStmt);
				} else if (strcmp(text, "if") == 0) {
					IAstNode* ifStmt = parseIfStatement(scanner, errorReporter, src);
					if (ifStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						ifStmt->setParent(body);
						body->addChild(ifStmt);
					}
				} else if (strcmp(text, "else") == 0) {
					// else must follow an if statement
					IAstNode* lastChild = (body->childCount() > 0) ? body->child(body->childCount() - 1) : nullptr;
					if (lastChild && lastChild->type() == IAstNode::Type::IF_STATEMENT) {
						AstNodeIfStatement* ifStmt = static_cast<AstNodeIfStatement*>(lastChild);

						// Parse else block - must have {
						token = u8t_scanner_scan(scanner);
						if (token != '{') {
							errorReporter->reportError(scanner, "Expected '{' after 'else'");
						} else {
							AstNodeBlock* elseBody = new AstNodeBlock();
							setNodePosition(elseBody, scanner, src);

							// Use the recursive helper to parse the else body
							parseBlockBody(elseBody, scanner, errorReporter, src);

							elseBody->setParent(ifStmt);
							ifStmt->setElseBody(elseBody);
						}
					} else {
						errorReporter->reportError(scanner, "'else' without preceding 'if'");
					}
				} else if (strcmp(text, "switch") == 0) {
					IAstNode* switchStmt = parseSwitchStatement(scanner, errorReporter, src);
					if (switchStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						switchStmt->setParent(body);
						body->addChild(switchStmt);
					}
				} else if (strcmp(text, "return") == 0) {
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					AstNodeReturn* returnStmt = new AstNodeReturn();
					setNodePosition(returnStmt, scanner, src);
					returnStmt->setParent(body);
					body->addChild(returnStmt);
				} else if (strcmp(text, "defer") == 0) {
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					AstNodeDefer* deferStmt = new AstNodeDefer();
					setNodePosition(deferStmt, scanner, src);
					token = u8t_scanner_scan(scanner);

					// Check if defer has a block
					if (token == '{') {
						// Parse defer block
						std::vector<IAstNode*> deferNodes;

						while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
							if (token == '}') {
								break;
							}

							if (token == U8T_IDENTIFIER) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								if (isBuiltInInstruction(deferText)) {
									IAstNode* id = new AstNodeInstruction(deferText);
									setNodePosition(id, scanner, src);
									deferNodes.push_back(id);
								} else {
									AstNodeIdentifier* id = new AstNodeIdentifier(deferText);
									setNodePosition(id, scanner, src);
									// Check for '!' or '?' suffix
									char32_t nextToken = u8t_scanner_peek(scanner);
									if (nextToken == '!') {
										u8t_scanner_scan(scanner); // Consume the '!'
										id->setAbortOnError(true);
									} else if (nextToken == '?') {
										u8t_scanner_scan(scanner); // Consume the '?'
										id->setCheckError(true);
									}
									deferNodes.push_back(id);
								}
							} else if (token == U8T_INTEGER) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								AstNodeLiteral* lit =
										new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::INTEGER);
								setNodePosition(lit, scanner, src);
								deferNodes.push_back(lit);
							} else if (token == U8T_FLOAT) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::FLOAT);
								setNodePosition(lit, scanner, src);
								deferNodes.push_back(lit);
							} else if (token == U8T_STRING) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								AstNodeLiteral* lit =
										new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::STRING);
								setNodePosition(lit, scanner, src);
								deferNodes.push_back(lit);
							} else if (token == ':') {
								char32_t nextChar = u8t_scanner_peek(scanner);
								if (nextChar == ':') {
									u8t_scanner_scan(scanner);
									IAstNode* colonColon = new AstNodeIdentifier("::");
									setNodePosition(colonColon, scanner, src);
									deferNodes.push_back(colonColon);
								}
							} else if (token == '.' || token == '/' || token == '*' || token == '+' || token == '-' ||
									   token == '<' || token == '>' || token == '=' || token == '!') {
								// Handle character-based instructions (including multi-char ops)
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								std::string instrText(deferText);

								// Check for multi-character operators
								if (token == '<' || token == '>' || token == '=' || token == '!') {
									char32_t nextChar = u8t_scanner_peek(scanner);
									if (nextChar == '=') {
										u8t_scanner_scan(scanner);
										instrText += '=';
									}
								}

								AstNodeInstruction* instr = new AstNodeInstruction(instrText.c_str());
								setNodePosition(instr, scanner, src);
								deferNodes.push_back(instr);
							}
						}

						for (auto* node : deferNodes) {
							node->setParent(deferStmt);
							deferStmt->addChild(node);
						}
					} else {
						// Parse inline defer statement (backwards compatibility)
						std::vector<IAstNode*> deferNodes;
						bool hasSeenOperator = false;

						// Process the first token we already scanned
						if (token == U8T_IDENTIFIER) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							hasSeenOperator = true;
							IAstNode* id = isBuiltInInstruction(deferText)
												   ? static_cast<IAstNode*>(new AstNodeInstruction(deferText))
												   : static_cast<IAstNode*>(new AstNodeIdentifier(deferText));
							setNodePosition(id, scanner, src);
							deferNodes.push_back(id);
						} else if (token == U8T_INTEGER) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::INTEGER);
							setNodePosition(lit, scanner, src);
							deferNodes.push_back(lit);
						} else if (token == U8T_FLOAT) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::FLOAT);
							setNodePosition(lit, scanner, src);
							deferNodes.push_back(lit);
						} else if (token == U8T_STRING) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::STRING);
							setNodePosition(lit, scanner, src);
							deferNodes.push_back(lit);
						}

						// Continue parsing inline defer
						while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
							if (token == '}') {
								break;
							}

							if (token == U8T_IDENTIFIER) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								// Check if it's a control structure keyword - this ends the defer
								if (strcmp(deferText, "for") == 0 || strcmp(deferText, "loop") == 0 ||
										strcmp(deferText, "if") == 0 || strcmp(deferText, "switch") == 0 ||
										strcmp(deferText, "return") == 0 || strcmp(deferText, "defer") == 0 ||
										strcmp(deferText, "break") == 0 || strcmp(deferText, "continue") == 0) {
									if (isBuiltInInstruction(deferText)) {
										IAstNode* id = new AstNodeInstruction(deferText);
										setNodePosition(id, scanner, src);
										tempNodes.push_back(id);
									} else {
										AstNodeIdentifier* id = new AstNodeIdentifier(deferText);
										setNodePosition(id, scanner, src);
										char32_t nextToken = u8t_scanner_peek(scanner);
										if (nextToken == '!') {
											u8t_scanner_scan(scanner);
											id->setAbortOnError(true);
										} else if (nextToken == '?') {
											u8t_scanner_scan(scanner);
											id->setCheckError(true);
										}
										tempNodes.push_back(id);
									}
									break;
								}

								// Mark that we've seen an operator
								hasSeenOperator = true;
								if (isBuiltInInstruction(deferText)) {
									IAstNode* id = new AstNodeInstruction(deferText);
									setNodePosition(id, scanner, src);
									deferNodes.push_back(id);
								} else {
									AstNodeIdentifier* id = new AstNodeIdentifier(deferText);
									setNodePosition(id, scanner, src);
									char32_t nextToken = u8t_scanner_peek(scanner);
									if (nextToken == '!') {
										u8t_scanner_scan(scanner);
										id->setAbortOnError(true);
									}
									deferNodes.push_back(id);
								}
							} else if (token == U8T_INTEGER) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								// If we've already seen an operator and the last node was an operator,
								// this literal starts a new statement
								if (hasSeenOperator && !deferNodes.empty() &&
										deferNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
									AstNodeLiteral* lit =
											new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::INTEGER);
									setNodePosition(lit, scanner, src);
									tempNodes.push_back(lit);
									break;
								}

								IAstNode* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::INTEGER);
								setNodePosition(lit, scanner, src);
								deferNodes.push_back(lit);
							} else if (token == U8T_FLOAT) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								if (hasSeenOperator && !deferNodes.empty() &&
										deferNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
									AstNodeLiteral* lit =
											new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::FLOAT);
									setNodePosition(lit, scanner, src);
									tempNodes.push_back(lit);
									break;
								}

								IAstNode* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::FLOAT);
								setNodePosition(lit, scanner, src);
								deferNodes.push_back(lit);
							} else if (token == U8T_STRING) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								if (hasSeenOperator && !deferNodes.empty() &&
										deferNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
									AstNodeLiteral* lit =
											new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::STRING);
									setNodePosition(lit, scanner, src);
									tempNodes.push_back(lit);
									break;
								}

								IAstNode* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::STRING);
								setNodePosition(lit, scanner, src);
								deferNodes.push_back(lit);
							} else if (token == ':') {
								char32_t nextChar = u8t_scanner_peek(scanner);
								if (nextChar == ':') {
									u8t_scanner_scan(scanner);
									IAstNode* colonColon = new AstNodeIdentifier("::");
									setNodePosition(colonColon, scanner, src);
									deferNodes.push_back(colonColon);
								} else {
									break;
								}
							} else {
								break;
							}
						}

						for (auto* node : deferNodes) {
							node->setParent(deferStmt);
							deferStmt->addChild(node);
						}
					}

					deferStmt->setParent(body);
					body->addChild(deferStmt);
				} else {
					if (isBuiltInInstruction(text)) {
						IAstNode* id = new AstNodeInstruction(text);
						setNodePosition(id, scanner, src);
						tempNodes.push_back(id);
					} else {
						AstNodeIdentifier* id = new AstNodeIdentifier(text);
						setNodePosition(id, scanner, src);
						// Check for '!' or '?' suffix
						char32_t nextToken = u8t_scanner_peek(scanner);
						if (nextToken == '!') {
							u8t_scanner_scan(scanner); // Consume the '!'
							id->setAbortOnError(true);
						} else if (nextToken == '?') {
							u8t_scanner_scan(scanner); // Consume the '?'
							id->setCheckError(true);
						}
						tempNodes.push_back(id);
					}
				}
			} else if (token == U8T_INTEGER) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(lit, scanner, src);
				tempNodes.push_back(lit);
			} else if (token == U8T_FLOAT) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::FLOAT);
				setNodePosition(lit, scanner, src);
				tempNodes.push_back(lit);
			} else if (token == U8T_STRING) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::STRING);
				setNodePosition(lit, scanner, src);
				tempNodes.push_back(lit);
			}

			// Try to parse as operator alias
			IAstNode* opNode = tryParseOperatorAlias(token, scanner, src);
			if (opNode != nullptr) {
				tempNodes.push_back(opNode);
			} else if (token == '<') {
				// Check if next token is '=' for '<='
				char32_t nextToken = u8t_scanner_peek(scanner);
				if (nextToken == '=') {
					u8t_scanner_scan(scanner); // Consume '='
					AstNodeInstruction* instr = new AstNodeInstruction("<=");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				} else {
					// Handle '<' as alias for 'lt'
					AstNodeInstruction* instr = new AstNodeInstruction("<");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				}
			} else if (token == '>') {
				// Check if next token is '=' for '>='
				char32_t nextToken = u8t_scanner_peek(scanner);
				if (nextToken == '=') {
					u8t_scanner_scan(scanner); // Consume '='
					AstNodeInstruction* instr = new AstNodeInstruction(">=");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				} else {
					// Handle '>' as alias for 'gt'
					AstNodeInstruction* instr = new AstNodeInstruction(">");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				}
			} else if (token == '=') {
				// Check if next token is '=' for '=='
				char32_t nextToken = u8t_scanner_peek(scanner);
				if (nextToken == '=') {
					u8t_scanner_scan(scanner); // Consume '='
					AstNodeInstruction* instr = new AstNodeInstruction("==");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				}
			} else if (token == '!') {
				// Check if next token is '=' for '!='
				char32_t nextToken = u8t_scanner_peek(scanner);
				if (nextToken == '=') {
					u8t_scanner_scan(scanner); // Consume '='
					AstNodeInstruction* instr = new AstNodeInstruction("!=");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				} else {
					// Handle '!' as alias for 'not'
					AstNodeInstruction* instr = new AstNodeInstruction("!");
					setNodePosition(instr, scanner, src);
					tempNodes.push_back(instr);
				}
			} else if (token == '$') {
				// Handle '$' as for loop iterator variable
				AstNodeIdentifier* ident = new AstNodeIdentifier("$");
				setNodePosition(ident, scanner, src);
				tempNodes.push_back(ident);
			} else if (token == '&') {
				// Handle '&' for function pointer references
				size_t ampPos = u8t_scanner_token_start(scanner);
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_IDENTIFIER) {
					const char* functionName = u8t_scanner_token_text(scanner, &n);
					AstNodeFunctionPointerReference* funcPtr = new AstNodeFunctionPointerReference(functionName);
					size_t line, column;
					calculateLineColumn(src, ampPos, &line, &column);
					funcPtr->setPosition(line, column);
					tempNodes.push_back(funcPtr);
				} else {
					errorReporter->reportError(scanner, "Expected function name after '&'");
				}
			}
		}

		for (auto* node : tempNodes) {
			node->setParent(body);
			body->addChild(node);
		}

		body->setParent(func);
		func->setBody(body);

		return func;
	}

	static IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'for'");
			// Recovery: create empty for statement and synchronize
			AstNodeForStatement* forStmt = new AstNodeForStatement();
			setNodePosition(forStmt, scanner, src);
			AstNodeBlock* body = new AstNodeBlock();
			setNodePosition(body, scanner, src);
			body->setParent(forStmt);
			forStmt->setBody(body);
			synchronize(scanner);
			return forStmt;
		}

		AstNodeForStatement* forStmt = new AstNodeForStatement();
		setNodePosition(forStmt, scanner, src);
		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(forStmt);
		forStmt->setBody(body);

		return forStmt;
	}

	static IAstNode* parseLoopStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'loop'");
			// Recovery: create empty loop statement and synchronize
			AstNodeLoopStatement* loopStmt = new AstNodeLoopStatement();
			setNodePosition(loopStmt, scanner, src);
			AstNodeBlock* body = new AstNodeBlock();
			setNodePosition(body, scanner, src);
			body->setParent(loopStmt);
			loopStmt->setBody(body);
			synchronize(scanner);
			return loopStmt;
		}

		AstNodeLoopStatement* loopStmt = new AstNodeLoopStatement();
		setNodePosition(loopStmt, scanner, src);
		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(loopStmt);
		loopStmt->setBody(body);

		return loopStmt;
	}

	static IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'if'");
			// Recovery: create empty if statement and synchronize
			AstNodeIfStatement* ifStmt = new AstNodeIfStatement();
			setNodePosition(ifStmt, scanner, src);
			AstNodeBlock* thenBody = new AstNodeBlock();
			setNodePosition(thenBody, scanner, src);
			thenBody->setParent(ifStmt);
			ifStmt->setThenBody(thenBody);
			synchronize(scanner);
			return ifStmt;
		}

		AstNodeIfStatement* ifStmt = new AstNodeIfStatement();
		setNodePosition(ifStmt, scanner, src);
		AstNodeBlock* thenBody = new AstNodeBlock();
		setNodePosition(thenBody, scanner, src);

		// Use parseBlockBody to handle nested else clauses properly
		parseBlockBody(thenBody, scanner, errorReporter, src);

		thenBody->setParent(ifStmt);
		ifStmt->setThenBody(thenBody);

		return ifStmt;
	}

	static IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'switch'");
			// Recovery: create empty switch statement and synchronize
			AstNodeSwitchStatement* switchStmt = new AstNodeSwitchStatement();
			setNodePosition(switchStmt, scanner, src);
			synchronize(scanner);
			return switchStmt;
		}

		AstNodeSwitchStatement* switchStmt = new AstNodeSwitchStatement();
		setNodePosition(switchStmt, scanner, src);

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			// Check for '_' (wildcard/default case)
			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				if (strcmp(text, "_") == 0) {
					// Default case
					token = u8t_scanner_scan(scanner);
					if (token != '{') {
						errorReporter->reportError(scanner, "Expected '{' after '_'");
						continue;
					}

					AstNodeBlock* defaultBody = new AstNodeBlock();
					setNodePosition(defaultBody, scanner, src);
					while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
						if (token == '}') {
							break;
						}

						IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, src, false);
						if (node) {
							node->setParent(defaultBody);
							defaultBody->addChild(node);
						}
					}

					AstNodeCase* defaultCase = new AstNodeCase(nullptr, true);
					setNodePosition(defaultCase, scanner, src);
					defaultBody->setParent(defaultCase);
					defaultCase->setBody(defaultBody);
					defaultCase->setParent(switchStmt);
					switchStmt->addCase(defaultCase);
					continue;
				}
			}

			// Parse case value (no 'case' keyword)
			IAstNode* caseValue = nullptr;
			if (token == U8T_INTEGER) {
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				caseValue = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(caseValue, scanner, src);
			} else if (token == U8T_FLOAT) {
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				caseValue = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::FLOAT);
				setNodePosition(caseValue, scanner, src);
			} else if (token == U8T_STRING) {
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				caseValue = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::STRING);
				setNodePosition(caseValue, scanner, src);
			} else if (token == U8T_IDENTIFIER) {
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				// Check for scoped identifier (module::constant)
				char32_t nextToken = u8t_scanner_peek(scanner);
				if (nextToken == ':') {
					u8t_scanner_scan(scanner); // consume ':'
					char32_t doubleColon = u8t_scanner_scan(scanner);
					if (doubleColon == ':') {
						// This is a scoped identifier
						char32_t nameToken = u8t_scanner_scan(scanner);
						if (nameToken == U8T_IDENTIFIER) {
							const char* nameText = u8t_scanner_token_text(scanner, &n);
							caseValue = new AstNodeScopedIdentifier(valueText, nameText);
							setNodePosition(caseValue, scanner, src);
						}
					}
				} else {
					caseValue = isBuiltInInstruction(valueText)
										? static_cast<IAstNode*>(new AstNodeInstruction(valueText))
										: static_cast<IAstNode*>(new AstNodeIdentifier(valueText));
					setNodePosition(caseValue, scanner, src);
				}
			}

			if (!caseValue) {
				errorReporter->reportError(scanner, "Expected case value in switch statement");
				continue;
			}

			token = u8t_scanner_scan(scanner);
			if (token != '{') {
				errorReporter->reportError(scanner, "Expected '{' after case value");
				delete caseValue;
				continue;
			}

			AstNodeBlock* caseBody = new AstNodeBlock();
			setNodePosition(caseBody, scanner, src);
			while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
				if (token == '}') {
					break;
				}

				IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, src, false);
				if (node) {
					node->setParent(caseBody);
					caseBody->addChild(node);
				}
			}

			AstNodeCase* caseNode = new AstNodeCase(caseValue, false);
			setNodePosition(caseNode, scanner, src);
			caseBody->setParent(caseNode);
			caseNode->setBody(caseBody);
			caseNode->setParent(switchStmt);
			switchStmt->addCase(caseNode);
		}

		return switchStmt;
	}

	IAstNode* Ast::generate(const char* src, bool dumpTokens, const char* filename) {
		u8t_scanner scanner;
		u8t_scanner_init(&scanner, src);

		// If dumpTokens is true, scan and print all tokens, then reset the scanner
		if (dumpTokens) {
			char32_t token;
			while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
				size_t n;
				const char* text = u8t_scanner_token_text(&scanner, &n);
				size_t start = u8t_scanner_token_start(&scanner);

				std::cout << Colors::cyan() << start << Colors::reset() << " ";

				switch (token) {
				case U8T_IDENTIFIER:
					std::cout << Colors::green() << "IDENTIFIER" << Colors::reset();
					break;
				case U8T_INTEGER:
					std::cout << Colors::cyan() << "INTEGER   " << Colors::reset();
					break;
				case U8T_FLOAT:
					std::cout << Colors::cyan() << "FLOAT     " << Colors::reset();
					break;
				case U8T_STRING:
					std::cout << Colors::magenta() << "STRING    " << Colors::reset();
					break;
				default:
					// Character token
					std::cout << Colors::red() << "CHAR      " << Colors::reset();
					break;
				}

				std::cout << " \"" << text << "\"" << std::endl;
			}
			std::cout << std::endl;

			// Reset scanner for actual parsing
			u8t_scanner_init(&scanner, src);
		}

		ErrorReporter errorReporter(src, filename);
		errorReporter.setStoreErrors(true);

		if (mRoot) {
			delete mRoot;
		}
		AstProgram* program = new AstProgram();
		setNodePosition(program, &scanner, src);
		mRoot = program;

		char32_t token;
		bool sawSlash = false;
		while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
			size_t n;

			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(&scanner, src, sawSlash, token);
			if (comment != nullptr) {
				sawSlash = false;
				comment->setParent(program);
				program->addChild(comment);
				continue;
			}

			// If we saw a slash but it wasn't a comment, reset the flag
			if (sawSlash) {
				sawSlash = false;
			}

			sawSlash = (token == '/');
			if (sawSlash) {
				continue; // Wait for next token to see if it's a comment
			}

			switch (token) {
			case U8T_IDENTIFIER: {
				const char* text = u8t_scanner_token_text(&scanner, &n);

				if (strcmp(text, "fn") == 0) {
					IAstNode* func = parseFunctionDeclaration(&scanner, &errorReporter, src);
					if (func) {
						func->setParent(program);
						program->addChild(func);
					}
				} else if (strcmp(text, "use") == 0) {
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER) {
						const char* moduleName = u8t_scanner_token_text(&scanner, &n);
						std::string moduleNameStr(moduleName);

						// Check if this is a .qd file import (module.qd)
						// Peek at next token to see if it's a dot
						char32_t nextToken = u8t_scanner_peek(&scanner);
						if (nextToken == static_cast<char32_t>('.')) {
							// Consume the dot
							u8t_scanner_scan(&scanner);
							// Check if next token is 'qd'
							token = u8t_scanner_scan(&scanner);
							if (token == U8T_IDENTIFIER) {
								const char* ext = u8t_scanner_token_text(&scanner, &n);
								if (strcmp(ext, "qd") == 0) {
									moduleNameStr += ".qd";
								}
							}
						}

						AstNodeUse* useStmt = new AstNodeUse(moduleNameStr.c_str());
						setNodePosition(useStmt, &scanner, src);
						useStmt->setParent(program);
						program->addChild(useStmt);
					} else {
						errorReporter.reportError(&scanner, "Expected module name after 'use'");
					}
				} else if (strcmp(text, "import") == 0) {
					// Parse: import "libname.so" as "namespace" { fn ... }
					token = u8t_scanner_scan(&scanner);
					if (token != U8T_STRING) {
						errorReporter.reportError(&scanner, "Expected library name (string) after 'import'");
						break;
					}
					const char* libName = u8t_scanner_token_text(&scanner, &n);
					// Strip quotes from string literal
					std::string library(libName);
					if (library.length() >= 2 && library.front() == '"' && library.back() == '"') {
						library = library.substr(1, library.length() - 2);
					}

					// Expect 'as'
					token = u8t_scanner_scan(&scanner);
					if (token != U8T_IDENTIFIER) {
						errorReporter.reportError(&scanner, "Expected 'as' after library name");
						break;
					}
					const char* asKeyword = u8t_scanner_token_text(&scanner, &n);
					if (strcmp(asKeyword, "as") != 0) {
						errorReporter.reportError(&scanner, "Expected 'as' after library name");
						break;
					}

					// Expect namespace string
					token = u8t_scanner_scan(&scanner);
					if (token != U8T_STRING) {
						errorReporter.reportError(&scanner, "Expected namespace name (string) after 'as'");
						break;
					}
					const char* nsName = u8t_scanner_token_text(&scanner, &n);
					// Strip quotes from string literal
					std::string namespaceName(nsName);
					if (namespaceName.length() >= 2 && namespaceName.front() == '"' && namespaceName.back() == '"') {
						namespaceName = namespaceName.substr(1, namespaceName.length() - 2);
					}

					// Expect '{'
					token = u8t_scanner_scan(&scanner);
					if (token != '{') {
						errorReporter.reportError(&scanner, "Expected '{' after namespace name");
						break;
					}

					AstNodeImport* importStmt = new AstNodeImport(library, namespaceName);
					setNodePosition(importStmt, &scanner, src);

					// Parse function declarations
					while (true) {
						token = u8t_scanner_scan(&scanner);
						if (token == '}') {
							break;
						}
						if (token == U8T_IDENTIFIER) {
							const char* keyword = u8t_scanner_token_text(&scanner, &n);
							if (strcmp(keyword, "fn") == 0) {
								// Parse function declaration
								token = u8t_scanner_scan(&scanner);
								if (token != U8T_IDENTIFIER) {
									errorReporter.reportError(&scanner, "Expected function name after 'fn'");
									continue;
								}
								const char* funcName = u8t_scanner_token_text(&scanner, &n);
								ImportedFunction* func = new ImportedFunction();
								func->name = funcName;

								size_t funcLine, funcColumn;
								size_t pos = u8t_scanner_token_start(&scanner);
								calculateLineColumn(src, pos, &funcLine, &funcColumn);
								func->line = funcLine;
								func->column = funcColumn;

								// Expect '('
								token = u8t_scanner_scan(&scanner);
								if (token != '(') {
									errorReporter.reportError(&scanner, "Expected '(' after function name");
									delete func;
									continue;
								}

								// Parse parameters (simplified - name:type format)
								while (true) {
									token = u8t_scanner_scan(&scanner);
									if (token == ')' || token == U8T_EOF) {
										break;
									}
									if (token == '-') {
										// Check for '--' separator
										token = u8t_scanner_scan(&scanner);
										if (token == '-') {
											// Now parse output parameters
											while (true) {
												token = u8t_scanner_scan(&scanner);
												if (token == ')' || token == U8T_EOF) {
													break;
												}
												if (token == U8T_IDENTIFIER) {
													const char* paramName = u8t_scanner_token_text(&scanner, &n);
													std::string paramNameStr(paramName);
													// Expect ':'
													token = u8t_scanner_scan(&scanner);
													if (token == ':') {
														token = u8t_scanner_scan(&scanner);
														if (token == U8T_IDENTIFIER) {
															const char* paramType =
																	u8t_scanner_token_text(&scanner, &n);
															std::string paramTypeStr(paramType);
															AstNodeParameter* param = new AstNodeParameter(
																	paramNameStr, paramTypeStr, true);
															func->outputParameters.push_back(param);
														}
													}
												}
											}
											// After parsing output parameters, check if we hit ')'
											if (token == ')' || token == U8T_EOF) {
												break; // Break outer loop - we're done with all parameters
											}
										}
									}
									if (token == U8T_IDENTIFIER) {
										const char* paramName = u8t_scanner_token_text(&scanner, &n);
										std::string paramNameStr(paramName);
										// Expect ':'
										token = u8t_scanner_scan(&scanner);
										if (token == ':') {
											token = u8t_scanner_scan(&scanner);
											if (token == U8T_IDENTIFIER) {
												const char* paramType = u8t_scanner_token_text(&scanner, &n);
												std::string paramTypeStr(paramType);
												AstNodeParameter* param =
														new AstNodeParameter(paramNameStr, paramTypeStr, false);
												func->inputParameters.push_back(param);
											}
										}
									}
								}

								importStmt->addFunction(func);
							}
						}
					}

					importStmt->setParent(program);
					program->addChild(importStmt);
				} else if (strcmp(text, "const") == 0) {
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER) {
						const char* constName = u8t_scanner_token_text(&scanner, &n);
						std::string constNameStr(constName);
						token = u8t_scanner_scan(&scanner);
						if (token == '=') {
							token = u8t_scanner_scan(&scanner);
							AstNodeLiteral* value = nullptr;
							if (token == U8T_INTEGER) {
								const char* valueText = u8t_scanner_token_text(&scanner, &n);
								value = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::INTEGER);
								setNodePosition(value, &scanner, src);
							} else if (token == U8T_FLOAT) {
								const char* valueText = u8t_scanner_token_text(&scanner, &n);
								value = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::FLOAT);
								setNodePosition(value, &scanner, src);
							} else if (token == U8T_STRING) {
								const char* valueText = u8t_scanner_token_text(&scanner, &n);
								value = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::STRING);
								setNodePosition(value, &scanner, src);
							}
							if (value) {
								AstNodeConstant* constDecl = new AstNodeConstant(constNameStr, value->value().c_str());
								setNodePosition(constDecl, &scanner, src);
								delete value; // Value is copied, no longer needed
								constDecl->setParent(program);
								program->addChild(constDecl);
							}
						}
					} else {
						errorReporter.reportError(&scanner, "Expected constant name after 'const'");
					}
				}
				break;
			}
			case U8T_INTEGER:
				break;
			case U8T_STRING:
				break;
			case U8T_FLOAT:
				break;
			default:
				break;
			}
		}

		// Store the error count and details for later checking
		mErrorCount = errorReporter.errorCount();
		mErrors = errorReporter.getErrors();

		return mRoot;
	}
}
