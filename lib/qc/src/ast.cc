#include "ast_node_block.h"
#include "ast_node_comment.h"
#include "ast_node_label.h"
#include "source_utils.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_anonymous_function.h>
#include <qc/ast_node_array.h>
#include <qc/ast_node_break.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_continue.h>
#include <qc/ast_node_ctx.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_function_pointer.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_import.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_local.h>
#include <qc/ast_node_loop.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_return.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_struct.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_test.h>
#include <qc/ast_node_use.h>
#include <qc/ast_node_while.h>
#include <qc/colors.h>
#include <qc/error_reporter.h>
#include <qc/instructions.h>
#include <u8t/scanner.h>
#include <vector>

namespace Qd {

	// Helper to set position on a node from scanner
	static void setNodePosition(IAstNode* node, u8t_scanner* scanner, const char* src) {
		size_t charPos = u8t_scanner_token_start(scanner);
		// Convert character position to byte position for calculateLineColumn
		size_t bytePos = charIndexToByteOffset(src, charPos);
		size_t line, column;
		calculateLineColumn(src, bytePos, &line, &column);
		node->setPosition(line, column);
	}

	// Helper to parse a comment (// or /* */)
	// Returns the comment node, or nullptr if not a comment
	// firstSlashPos: character position of the first slash (SIZE_MAX if no slash was seen)
	static AstNodeComment* parseComment(u8t_scanner* scanner, const char* src, size_t firstSlashPos, char32_t token) {
		if (firstSlashPos == SIZE_MAX) {
			return nullptr;
		}

		// Check that the current token is adjacent to the first slash (no whitespace between them)
		size_t currentTokenPos = u8t_scanner_token_start(scanner);
		if (currentTokenPos != firstSlashPos + 1) {
			// There's whitespace between the slashes - not a comment
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

		// Capture comment start position
		size_t commentStartCharPos = firstSlashPos;
		size_t commentStartBytePos = charIndexToByteOffset(src, commentStartCharPos);
		size_t commentLine, commentColumn;
		calculateLineColumn(src, commentStartBytePos, &commentLine, &commentColumn);

		// Get character position and convert to byte offset for content start
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
			// Read until */ - support nested block comments
			// Track nesting depth: /* starts a nested comment, */ ends one
			int nestingDepth = 1;
			while (*commentEnd != '\0' && *(commentEnd + 1) != '\0' && nestingDepth > 0) {
				if (*commentEnd == '/' && *(commentEnd + 1) == '*') {
					// Nested comment start
					nestingDepth++;
					commentEnd += 2;
				} else if (*commentEnd == '*' && *(commentEnd + 1) == '/') {
					// Comment end
					nestingDepth--;
					if (nestingDepth == 0) {
						break; // Found the matching close
					}
					commentEnd += 2;
				} else {
					commentEnd++;
				}
			}
		}

		std::string commentText(commentStart, static_cast<size_t>(commentEnd - commentStart));

		// Advance scanner past the comment by directly updating the internal pointer
		// This avoids the scanner interpreting special characters (like ") inside comments
		// NOTE: The scanner's _token_start tracks character positions (codepoints), not bytes
		// So we must count UTF-8 characters when advancing the position
		if (commentType == AstNodeComment::CommentType::LINE) {
			size_t skipChars = countUtf8Chars(commentStart, commentEnd);
			scanner->_str = commentEnd;
			scanner->_token_start += skipChars;
			// Set _token_len to 1 to simulate having scanned the last character
			// This matches the behavior of the original loop-based approach
			scanner->_token_len = 1;
		} else {
			// For block comments, advance past the closing */
			const char* blockEnd = commentEnd;
			if (*blockEnd == '*' && *(blockEnd + 1) == '/') {
				blockEnd += 2;
			}
			size_t skipChars = countUtf8Chars(commentStart, blockEnd);
			scanner->_str = blockEnd;
			scanner->_token_start += skipChars;
			scanner->_token_len = 1;
		}

		// Create and return comment node with pre-captured position
		AstNodeComment* comment = new AstNodeComment(commentText, commentType);
		comment->setPosition(commentLine, commentColumn);
		return comment;
	}

	static IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseWhileStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseLoopStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	static AstNodeStructConstruction* parseStructConstruction(const std::string& structName, u8t_scanner* scanner,
			ErrorReporter* errorReporter, const char* src, size_t startPos);

	// Helper to parse constant value after '=' (handles literals and env() function)
	// Returns the resolved string value, or empty string on error
	static std::string parseConstantValue(u8t_scanner* scanner, ErrorReporter* errorReporter) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		// Check for env() function call
		if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, &n);
			if (strcmp(text, "env") == 0) {
				// Expect opening paren
				token = u8t_scanner_scan(scanner);
				if (token != '(') {
					errorReporter->reportError(scanner, "Expected '(' after 'env'");
					return "";
				}

				// Expect environment variable name as string
				token = u8t_scanner_scan(scanner);
				if (token != U8T_STRING) {
					errorReporter->reportError(scanner, "Expected string argument for environment variable name");
					return "";
				}
				const char* envNameText = u8t_scanner_token_text(scanner, &n);
				// Remove quotes from string
				std::string envName(envNameText);
				if (envName.length() >= 2 && envName.front() == '"' && envName.back() == '"') {
					envName = envName.substr(1, envName.length() - 2);
				}

				// Check for comma (optional default value) or closing paren
				token = u8t_scanner_scan(scanner);
				std::string defaultValue = "";
				bool hasDefault = false;

				if (token == ',') {
					// Parse default value
					token = u8t_scanner_scan(scanner);
					if (token != U8T_STRING) {
						errorReporter->reportError(scanner, "Expected string for default value in env()");
						return "";
					}
					const char* defaultText = u8t_scanner_token_text(scanner, &n);
					defaultValue = std::string(defaultText);
					// Remove quotes from string
					if (defaultValue.length() >= 2 && defaultValue.front() == '"' && defaultValue.back() == '"') {
						defaultValue = defaultValue.substr(1, defaultValue.length() - 2);
					}
					hasDefault = true;
					token = u8t_scanner_scan(scanner);
				}

				if (token != ')') {
					errorReporter->reportError(scanner, "Expected ')' after env() arguments");
					return "";
				}

				// Look up environment variable
				const char* envValue = std::getenv(envName.c_str());
				if (envValue != nullptr) {
					// Wrap result in quotes so LLVM generator knows it's a string
					return "\"" + std::string(envValue) + "\"";
				} else if (hasDefault) {
					// Wrap default in quotes so LLVM generator knows it's a string
					return "\"" + defaultValue + "\"";
				} else {
					std::string errorMsg = "Environment variable '" + envName + "' is not set and no default provided";
					errorReporter->reportError(scanner, errorMsg.c_str());
					return "";
				}
			} else {
				errorReporter->reportError(
						scanner, "Expected literal value or env() after '=' in constant declaration");
				return "";
			}
		}

		// Handle literal values
		if (token == U8T_INTEGER || token == U8T_FLOAT) {
			const char* valueText = u8t_scanner_token_text(scanner, &n);
			return std::string(valueText);
		} else if (token == U8T_STRING) {
			// Keep quotes so LLVM generator knows it's a string
			const char* valueText = u8t_scanner_token_text(scanner, &n);
			return std::string(valueText);
		}

		errorReporter->reportError(scanner, "Expected literal value or env() after '=' in constant declaration");
		return "";
	}

	// Helper to peek the immediate next character from source (no whitespace skip)
	// Returns the character or 0 if end of string
	static char32_t peekNextChar(u8t_scanner* scanner, const char* src) {
		// Get current position after last token
		size_t tokenStart = u8t_scanner_token_start(scanner);
		size_t tokenLen = u8t_scanner_token_len(scanner);
		size_t pos = tokenStart + tokenLen;

		// Convert character index to byte offset
		size_t bytePos = charIndexToByteOffset(src, pos);

		if (src[bytePos] != '\0') {
			return static_cast<char32_t>(static_cast<unsigned char>(src[bytePos]));
		}
		return 0;
	}

	// Helper to peek the next non-whitespace character from source
	// Returns the character or 0 if end of string
	static char32_t peekNextNonWhitespace(u8t_scanner* scanner, const char* src) {
		// Get current position after last token
		size_t tokenStart = u8t_scanner_token_start(scanner);
		size_t tokenLen = u8t_scanner_token_len(scanner);
		size_t pos = tokenStart + tokenLen;

		// Convert character index to byte offset
		size_t bytePos = charIndexToByteOffset(src, pos);

		// Skip whitespace
		while (src[bytePos] != '\0') {
			char c = src[bytePos];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				bytePos++;
			} else {
				return static_cast<char32_t>(static_cast<unsigned char>(c));
			}
		}
		return 0;
	}

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
				if (strcmp(text, "fn") == 0 || strcmp(text, "const") == 0 || strcmp(text, "struct") == 0 ||
						strcmp(text, "use") == 0 || strcmp(text, "import") == 0 || strcmp(text, "if") == 0 ||
						strcmp(text, "for") == 0 || strcmp(text, "while") == 0 || strcmp(text, "loop") == 0 ||
						strcmp(text, "switch") == 0 || strcmp(text, "return") == 0 || strcmp(text, "ctx") == 0) {
					return;
				}
			}
		}
	}

	// Helper to check if a token is an operator alias and create the corresponding instruction node
	// Returns the instruction node if it's an operator, nullptr otherwise
	static IAstNode* tryParseOperatorAlias(char32_t token, u8t_scanner* scanner, const char* src) {
		// Special handling for '-' to check for '->' (local variable declaration) or '--' (decrement)
		if (token == '-') {
			// Check if the next character in source is '>' (forming '->') or '-' (forming '--')
			size_t tokenStart = u8t_scanner_token_start(scanner);
			size_t tokenLen = u8t_scanner_token_len(scanner);
			size_t tokenEndChar = tokenStart + tokenLen;
			// Convert character index to byte offset for indexing into src
			size_t tokenEndByte = charIndexToByteOffset(src, tokenEndChar);
			// If character immediately after '-' is '>', this is NOT subtraction
			if (tokenEndByte < strlen(src) && src[tokenEndByte] == '>') {
				return nullptr; // Not an operator alias, will be handled as local declaration
			}
			// Check for '--' (decrement)
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '-') {
				u8t_scanner_scan(scanner); // Consume second '-'
				IAstNode* node = new AstNodeInstruction("--");
				setNodePosition(node, scanner, src);
				return node;
			}
		}

		// Special handling for '+' to check for '++' (increment)
		if (token == '+') {
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '+') {
				u8t_scanner_scan(scanner); // Consume second '+'
				IAstNode* node = new AstNodeInstruction("++");
				setNodePosition(node, scanner, src);
				return node;
			}
		}

		// Special handling for '<' to check for '<<' (shift left)
		if (token == '<') {
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '<') {
				u8t_scanner_scan(scanner); // Consume second '<'
				IAstNode* node = new AstNodeInstruction("<<");
				setNodePosition(node, scanner, src);
				return node;
			}
		}

		// Special handling for '>' to check for '>>' (shift right)
		if (token == '>') {
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '>') {
				u8t_scanner_scan(scanner); // Consume second '>'
				IAstNode* node = new AstNodeInstruction(">>");
				setNodePosition(node, scanner, src);
				return node;
			}
		}

		// Map of operator tokens to their instruction names
		static const struct {
			char32_t token;
			const char* instruction;
		} OPERATOR_ALIASES[] = {
				{'/', "/"}, // div
				{'*', "*"}, // mul
				{'+', "+"}, // add
				{'-', "-"}, // sub
				{'%', "%"}	// mod
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
			char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n, const char* src) {
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
			// Handle boolean literals: true = 1, false = 0
			if (strcmp(text, "true") == 0) {
				IAstNode* node = new AstNodeLiteral("1", AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(node, scanner, src);
				return node;
			} else if (strcmp(text, "false") == 0) {
				IAstNode* node = new AstNodeLiteral("0", AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(node, scanner, src);
				return node;
			}
			if (isBuiltInInstruction(text)) {
				std::string instrName(text);
				// Check for generic type parameter: instruction<Type>
				// Use peekNextChar (no whitespace skip) to distinguish make<T> from len <
				char32_t nextCh = peekNextChar(scanner, src);
				if (nextCh == '<') {
					u8t_scanner_scan(scanner); // Consume '<'
					char32_t typeToken = u8t_scanner_scan(scanner);
					if (typeToken == U8T_IDENTIFIER) {
						std::string typeParam = u8t_scanner_token_text(scanner, n);
						char32_t closeAngle = u8t_scanner_scan(scanner);
						if (closeAngle == '>') {
							IAstNode* node = new AstNodeInstruction(instrName, typeParam);
							setNodePosition(node, scanner, src);
							return node;
						} else {
							errorReporter->reportError(scanner, "Expected '>' after type parameter");
							return nullptr;
						}
					} else {
						errorReporter->reportError(scanner, "Expected type name after '<'");
						return nullptr;
					}
				}
				IAstNode* node = new AstNodeInstruction(instrName);
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

		// If tryParseOperatorAlias returned nullptr for '-', check if it's '->' (local variable)
		if (token == '-') {
			size_t tokenStart = u8t_scanner_token_start(scanner);
			size_t tokenLen = u8t_scanner_token_len(scanner);
			size_t tokenEndChar = tokenStart + tokenLen;
			// Convert character index to byte offset for indexing into src
			size_t tokenEndByte = charIndexToByteOffset(src, tokenEndChar);
			if (tokenEndByte < strlen(src) && src[tokenEndByte] == '>') {
				// This is '-> variableName' (single variable binding)
				u8t_scanner_scan(scanner); // Consume '>'

				// Single identifier is required
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_IDENTIFIER) {
					const char* varName = u8t_scanner_token_text(scanner, n);
					std::vector<std::string> varNames;
					varNames.push_back(std::string(varName));

					IAstNode* node = new AstNodeLocal(varNames);
					size_t line, column;
					size_t tokenStartByte = charIndexToByteOffset(src, tokenStart);
					calculateLineColumn(src, tokenStartByte, &line, &column);
					node->setPosition(line, column);
					return node;
				}
				return nullptr;
			}
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
		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection
		bool sawColon = false;
		bool sawAt = false;
		bool sawDot = false;
		std::string dotVarName; // Variable name before the dot for field set
		std::vector<IAstNode*> tempNodes;

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle @ field access operator
			if (sawAt && token == U8T_IDENTIFIER) {
				sawAt = false;
				// Get the field name
				const char* fieldName = u8t_scanner_token_text(scanner, &n);

				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					// We have: identifier @field
					AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();

					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess(varIdent->name(), fieldName);
					setNodePosition(fieldAccess, scanner, src);
					delete varIdent;
					tempNodes.push_back(fieldAccess);
				} else if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
					// Chained field access: previous @field followed by @field2
					// Use empty varName to indicate stack-based access
					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
					setNodePosition(fieldAccess, scanner, src);
					tempNodes.push_back(fieldAccess);
				} else {
					// Stack-based field access: @field after struct construction, function call, etc.
					// Use empty varName to indicate stack-based access (pops struct from stack)
					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
					setNodePosition(fieldAccess, scanner, src);
					tempNodes.push_back(fieldAccess);
				}
				continue;
			}

			// Handle . field set operator
			if (sawDot && token == U8T_IDENTIFIER) {
				sawDot = false;
				// Get the field name
				const char* fieldName = u8t_scanner_token_text(scanner, &n);

				// Create field set node with the stored variable name
				AstNodeFieldSet* fieldSet = new AstNodeFieldSet(dotVarName, fieldName);
				setNodePosition(fieldSet, scanner, src);
				tempNodes.push_back(fieldSet);
				dotVarName.clear();
				continue;
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
						// Check for '!' or '?' suffix
						char32_t nextToken = u8t_scanner_peek(scanner);
						if (nextToken == '!') {
							u8t_scanner_scan(scanner); // Consume the '!'
							scoped->setAbortOnError(true);
						} else if (nextToken == '?') {
							u8t_scanner_scan(scanner); // Consume the '?'
							scoped->setCheckError(true);
						}
						tempNodes.push_back(scoped);
					} else {
						// No identifier after ::, put scope back
						tempNodes.push_back(scope);
					}
				}
				continue;
			}

			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(scanner, src, slashPos, token);
			if (comment != nullptr) {
				slashPos = SIZE_MAX;
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
			if (slashPos != SIZE_MAX) {
				// Add division instruction to tempNodes (for the first slash)
				AstNodeInstruction* divInstr = new AstNodeInstruction("/");
				setNodePosition(divInstr, scanner, src);
				tempNodes.push_back(divInstr);
				slashPos = SIZE_MAX;
			}

			if (token == '}') {
				// Check for incomplete operators before exiting
				if (sawDot) {
					errorReporter->reportError(scanner, "Expected field name after '.'");
				}
				if (sawAt) {
					errorReporter->reportError(scanner, "Expected field name after '@'");
				}
				break;
			}

			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
				continue; // Wait for next token to see if it's a comment
			}

			sawColon = (token == ':');
			if (sawColon) {
				continue; // Wait for next token to see if it's another colon
			}

			sawAt = (token == '@');
			if (sawAt) {
				continue; // Wait for next token to see if it's a field name
			}

			// Handle . field set operator: value variable.field
			if (token == '.') {
				// The variable must be the previous identifier in tempNodes
				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();
					dotVarName = varIdent->name();
					delete varIdent;
					sawDot = true;
					continue; // Wait for next token to get the field name
				} else {
					errorReporter->reportError(scanner, "Expected variable name before '.' in field set");
					continue;
				}
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
		// Handle local variable declaration: -> variableName (single variable only)
		// Check this early before other token processing
		if (token == '-') {
			// Check if the next character in source is '>' (forming '->')
			// We need to look at the actual source position, not the next token
			size_t tokenStart = u8t_scanner_token_start(scanner);
			size_t tokenLen = u8t_scanner_token_len(scanner);
			size_t tokenEndChar = tokenStart + tokenLen;
			// Convert character index to byte offset for indexing into src
			size_t tokenEndByte = charIndexToByteOffset(src, tokenEndChar);
			// Check if character immediately after '-' is '>'
			if (tokenEndByte < strlen(src) && src[tokenEndByte] == '>') {
				// This is a local declaration: -> varName (single variable binding)
				size_t arrowPosByte = charIndexToByteOffset(src, tokenStart);
				u8t_scanner_scan(scanner); // Consume '>'

				// Single identifier is required
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_IDENTIFIER) {
					const char* varName = u8t_scanner_token_text(scanner, n);
					std::vector<std::string> varNames;
					varNames.push_back(std::string(varName));

					IAstNode* node = new AstNodeLocal(varNames);
					size_t line, column;
					calculateLineColumn(src, arrowPosByte, &line, &column);
					node->setPosition(line, column);
					return node;
				} else {
					errorReporter->reportError(scanner, "Expected variable name after '->'");
					return nullptr;
				}
			}
			// Not a local declaration, fall through to handle as subtraction
		}

		if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, n);

			// Handle boolean literals: true = 1, false = 0
			if (strcmp(text, "true") == 0) {
				IAstNode* node = new AstNodeLiteral("1", AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(node, scanner, src);
				return node;
			} else if (strcmp(text, "false") == 0) {
				IAstNode* node = new AstNodeLiteral("0", AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(node, scanner, src);
				return node;
			}

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

			// defer is always allowed
			if (strcmp(text, "defer") == 0) {
				AstNodeDefer* deferStmt = new AstNodeDefer();
				setNodePosition(deferStmt, scanner, src);
				token = u8t_scanner_scan(scanner);

				// Check if defer has a block
				if (token == '{') {
					// Parse defer block - wrap it in a block node
					AstNodeBlock* deferBlock = new AstNodeBlock();
					setNodePosition(deferBlock, scanner, src);
					parseBlockBody(deferBlock, scanner, errorReporter, src);

					// Add the block as a child of defer
					deferBlock->setParent(deferStmt);
					deferStmt->addChild(deferBlock);
				}
				return deferStmt;
			}

			if (allowControlFlow) {
				if (strcmp(text, "if") == 0) {
					return parseIfStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "for") == 0) {
					return parseForStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "while") == 0) {
					return parseWhileStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "loop") == 0) {
					return parseLoopStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "switch") == 0) {
					return parseSwitchStatement(scanner, errorReporter, src);
				}
			}

			if (isBuiltInInstruction(text)) {
				std::string instrName(text);
				// Check for generic type parameter: instruction<Type>
				// Use peekNextChar (no whitespace skip) to distinguish make<T> from len <
				char32_t nextCh = peekNextChar(scanner, src);
				if (nextCh == '<') {
					u8t_scanner_scan(scanner); // Consume '<'
					char32_t typeToken = u8t_scanner_scan(scanner);
					if (typeToken == U8T_IDENTIFIER) {
						std::string typeParam = u8t_scanner_token_text(scanner, n);
						char32_t closeAngle = u8t_scanner_scan(scanner);
						if (closeAngle == '>') {
							IAstNode* node = new AstNodeInstruction(instrName, typeParam);
							setNodePosition(node, scanner, src);
							return node;
						} else {
							errorReporter->reportError(scanner, "Expected '>' after type parameter");
							return nullptr;
						}
					} else {
						errorReporter->reportError(scanner, "Expected type name after '<'");
						return nullptr;
					}
				}
				IAstNode* node = new AstNodeInstruction(instrName);
				setNodePosition(node, scanner, src);
				return node;
			}

			// Save identifier position for potential struct construction
			size_t identPos = u8t_scanner_token_start(scanner);
			std::string identName(text);

			// Check for scoped identifier (module::function, module::constant, or module::StructName)
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == ':') {
				// Save scope name before scanning invalidates the pointer
				std::string scopeName(text);
				u8t_scanner_scan(scanner); // Consume first ':'
				char32_t secondColon = u8t_scanner_peek(scanner);
				if (secondColon == ':') {
					u8t_scanner_scan(scanner); // Consume second ':'
					char32_t memberToken = u8t_scanner_scan(scanner);
					if (memberToken == U8T_IDENTIFIER) {
						const char* memberName = u8t_scanner_token_text(scanner, n);
						std::string fullName = scopeName + "::" + memberName;

						// Check if this is struct construction: module::StructName { ... }
						char32_t afterMember = peekNextNonWhitespace(scanner, src);
						if (afterMember == '{') {
							u8t_scanner_scan(scanner); // Consume '{'
							return parseStructConstruction(fullName, scanner, errorReporter, src, identPos);
						}

						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scopeName, memberName);
						setNodePosition(scoped, scanner, src);
						// Check for '!' or '?' suffix
						char32_t suffixToken = u8t_scanner_peek(scanner);
						if (suffixToken == '!') {
							u8t_scanner_scan(scanner); // Consume the '!'
							scoped->setAbortOnError(true);
						} else if (suffixToken == '?') {
							u8t_scanner_scan(scanner); // Consume the '?'
							scoped->setCheckError(true);
						}
						return scoped;
					}
				}
				// Not a valid scoped identifier, create regular identifier
				// Note: We already consumed the first ':', so we can't undo that.
				// This is an edge case that shouldn't normally happen.
			}

			// Check if this is struct construction: StructName { ... }
			nextToken = peekNextNonWhitespace(scanner, src);
			if (nextToken == '{') {
				u8t_scanner_scan(scanner); // Consume '{'
				return parseStructConstruction(identName, scanner, errorReporter, src, identPos);
			}

			AstNodeIdentifier* node = new AstNodeIdentifier(text);
			setNodePosition(node, scanner, src);
			// Check for '!' or '?' suffix
			nextToken = u8t_scanner_peek(scanner);
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

		// Handle array literal: [elem1 elem2 ...] (commas optional)
		if (token == '[') {
			size_t bracketPos = u8t_scanner_token_start(scanner);
			AstNodeArrayLiteral* arrNode = new AstNodeArrayLiteral();
			size_t line, column;
			calculateLineColumn(src, bracketPos, &line, &column);
			arrNode->setPosition(line, column);

			// Parse array elements until we hit ']'
			char32_t elemToken;
			while ((elemToken = u8t_scanner_scan(scanner)) != U8T_EOF) {
				if (elemToken == ']') {
					break;
				}

				// Skip commas (optional separator)
				if (elemToken == ',') {
					continue;
				}

				// Skip whitespace tokens if any
				if (elemToken == ' ' || elemToken == '\t' || elemToken == '\n' || elemToken == '\r') {
					continue;
				}

				// Parse the element
				IAstNode* elem = parseSimpleToken(elemToken, scanner, errorReporter, n, src);
				if (elem) {
					arrNode->addElement(elem);
				} else {
					// Try to parse as a nested array
					if (elemToken == '[') {
						// Recursive array literal
						IAstNode* nestedArr = parseBlockStatement(elemToken, scanner, errorReporter, n, src, false);
						if (nestedArr) {
							arrNode->addElement(nestedArr);
						}
					} else {
						// Unknown token in array literal
						errorReporter->reportError(scanner, "Unexpected token in array literal");
						break;
					}
				}
			}

			if (elemToken != ']') {
				errorReporter->reportError(scanner, "Expected ']' to close array literal");
			}

			return arrNode;
		}

		return parseSimpleToken(token, scanner, errorReporter, n, src);
	}

	Ast::~Ast() {
		if (mRoot) {
			delete mRoot;
			mRoot = nullptr;
		}
	}

	/**
	 * Parse an anonymous function: fn (params -- outputs) { body }
	 * Called when 'fn' keyword is followed by '(' (no identifier name).
	 * The 'fn' keyword has already been consumed.
	 */
	static IAstNode* parseAnonymousFunction(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		// The '(' has already been peeked but not consumed
		char32_t token = u8t_scanner_scan(scanner);
		if (token != '(') {
			errorReporter->reportError(scanner, "Expected '(' after 'fn' for anonymous function");
			return nullptr;
		}

		AstNodeAnonymousFunction* func = new AstNodeAnonymousFunction();
		setNodePosition(func, scanner, src);

		size_t n;
		bool isOutput = false;

		// Parse parameters: (input:type input2:type -- output:type)
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
				char32_t paramPeek = u8t_scanner_peek(scanner);
				if (paramPeek == ':') {
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

		// Expect '{'
		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after anonymous function signature");
			delete func;
			return nullptr;
		}

		// Parse the body
		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);
		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(func);
		func->setBody(body);

		return func;
	}

	static IAstNode* parseFunctionDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic = false) {
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected function name after 'fn'");
			synchronize(scanner);
			return nullptr;
		}

		size_t n;
		const char* name = u8t_scanner_token_text(scanner, &n);
		AstNodeFunctionDeclaration* func = new AstNodeFunctionDeclaration(name, isPublic);
		setNodePosition(func, scanner, src);

		// Check for generic type parameters: fn name<T, U>(...)
		char32_t peek = peekNextNonWhitespace(scanner, src);
		if (peek == '<') {
			u8t_scanner_scan(scanner); // Consume '<'
			while (true) {
				token = u8t_scanner_scan(scanner);
				if (token == '>') {
					break;
				}
				if (token == U8T_IDENTIFIER) {
					const char* typeParam = u8t_scanner_token_text(scanner, &n);
					func->addTypeParam(std::string(typeParam));

					// Check for comma or closing >
					peek = peekNextNonWhitespace(scanner, src);
					if (peek == ',') {
						u8t_scanner_scan(scanner); // Consume ','
					}
				} else if (token != ',') {
					errorReporter->reportError(scanner, "Expected type parameter or '>' in generic function");
					break;
				}
			}
		}

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
				char32_t paramPeek = u8t_scanner_peek(scanner);
				if (paramPeek == ':') {
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
		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection
		bool sawAt = false;
		bool sawDot = false;
		std::string dotVarName; // Variable name before the dot for field set
		bool foundClosingBrace = false;

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(scanner, src, slashPos, token);
			if (comment != nullptr) {
				slashPos = SIZE_MAX;
				tempNodes.push_back(comment);
				continue;
			}

			// If we saw a slash but it wasn't a comment, it's a division operator
			if (slashPos != SIZE_MAX) {
				// Add division instruction to tempNodes (for the first slash)
				AstNodeInstruction* divInstr = new AstNodeInstruction("/");
				setNodePosition(divInstr, scanner, src);
				tempNodes.push_back(divInstr);
				slashPos = SIZE_MAX;
			}

			if (token == '}') {
				// Check for incomplete operators before exiting
				if (sawDot) {
					errorReporter->reportError(scanner, "Expected field name after '.'");
				}
				if (sawAt) {
					errorReporter->reportError(scanner, "Expected field name after '@'");
				}
				foundClosingBrace = true;
				break;
			}

			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
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
						// Check for '!' or '?' suffix
						char32_t nextToken = u8t_scanner_peek(scanner);
						if (nextToken == '!') {
							u8t_scanner_scan(scanner); // Consume the '!'
							scoped->setAbortOnError(true);
						} else if (nextToken == '?') {
							u8t_scanner_scan(scanner); // Consume the '?'
							scoped->setCheckError(true);
						}
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

			// Handle @ field access operator
			if (sawAt && token == U8T_IDENTIFIER) {
				sawAt = false;
				// Get the field name
				const char* fieldName = u8t_scanner_token_text(scanner, &n);

				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					// We have: identifier @field
					AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();

					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess(varIdent->name(), fieldName);
					setNodePosition(fieldAccess, scanner, src);
					delete varIdent;
					tempNodes.push_back(fieldAccess);
				} else if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
					// Chained field access: previous @field followed by @field2
					// Use empty varName to indicate stack-based access
					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
					setNodePosition(fieldAccess, scanner, src);
					tempNodes.push_back(fieldAccess);
				} else {
					// Stack-based field access: @field after struct construction, function call, etc.
					// Use empty varName to indicate stack-based access (pops struct from stack)
					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
					setNodePosition(fieldAccess, scanner, src);
					tempNodes.push_back(fieldAccess);
				}
				continue;
			}

			sawAt = (token == '@');
			if (sawAt) {
				continue; // Wait for next token to see if it's a field name
			}

			// Handle . field set operator
			if (sawDot && token == U8T_IDENTIFIER) {
				sawDot = false;
				// Get the field name
				const char* fieldName = u8t_scanner_token_text(scanner, &n);

				// Create field set node with the stored variable name
				AstNodeFieldSet* fieldSet = new AstNodeFieldSet(dotVarName, fieldName);
				setNodePosition(fieldSet, scanner, src);
				tempNodes.push_back(fieldSet);
				dotVarName.clear();
				continue;
			}

			// Handle . field set operator: value variable.field
			if (token == '.') {
				// The variable must be the previous identifier in tempNodes
				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();
					dotVarName = varIdent->name();
					delete varIdent;
					sawDot = true;
					continue; // Wait for next token to get the field name
				} else {
					errorReporter->reportError(scanner, "Expected variable name before '.' in field set");
					continue;
				}
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
				} else if (strcmp(text, "while") == 0) {
					IAstNode* whileStmt = parseWhileStatement(scanner, errorReporter, src);
					if (whileStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						whileStmt->setParent(body);
						body->addChild(whileStmt);
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
						// Parse defer block using parseBlockBody for full feature support
						// (including scoped identifiers like flag::destroy)
						AstNodeBlock* deferBlock = new AstNodeBlock();
						setNodePosition(deferBlock, scanner, src);
						parseBlockBody(deferBlock, scanner, errorReporter, src);

						// Add the block as a child of defer
						deferBlock->setParent(deferStmt);
						deferStmt->addChild(deferBlock);
					}

					deferStmt->setParent(body);
					body->addChild(deferStmt);
				} else if (strcmp(text, "fn") == 0) {
					// Anonymous function: fn (params -- outputs) { body }
					// Peek to see if next non-whitespace is '(' (anonymous) or identifier (error)
					char32_t nextToken = peekNextNonWhitespace(scanner, src);
					if (nextToken == '(') {
						IAstNode* anonFunc = parseAnonymousFunction(scanner, errorReporter, src);
						if (anonFunc) {
							tempNodes.push_back(anonFunc);
						}
					} else {
						errorReporter->reportError(
								scanner, "Function declarations not allowed inside blocks. "
										 "Did you mean 'fn (...) { }' for an anonymous function?");
					}
				} else if (strcmp(text, "ctx") == 0) {
					// Parse ctx block
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					AstNodeCtx* ctxStmt = new AstNodeCtx();
					setNodePosition(ctxStmt, scanner, src);
					token = u8t_scanner_scan(scanner);

					// ctx requires a block
					if (token != '{') {
						errorReporter->reportError(scanner, "Expected '{' after 'ctx'");
						delete ctxStmt;
					} else {
						// Parse ctx block inline
						// ctx blocks can contain control flow statements
						std::vector<IAstNode*> ctxTempNodes;
						size_t ctxSlashPos = SIZE_MAX; // Position of first slash for comment detection
						bool ctxSawColon = false;
						bool ctxSawAt = false;
						bool ctxSawDot = false;
						std::string ctxDotVarName;

						while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
							// Handle @ field access operator
							if (ctxSawAt && token == U8T_IDENTIFIER) {
								ctxSawAt = false;
								const char* fieldName = u8t_scanner_token_text(scanner, &n);

								if (!ctxTempNodes.empty() &&
										ctxTempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
									AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(ctxTempNodes.back());
									ctxTempNodes.pop_back();
									AstNodeFieldAccess* fieldAccess =
											new AstNodeFieldAccess(varIdent->name(), fieldName);
									setNodePosition(fieldAccess, scanner, src);
									delete varIdent;
									ctxTempNodes.push_back(fieldAccess);
								} else if (!ctxTempNodes.empty() &&
										   ctxTempNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
									AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
									setNodePosition(fieldAccess, scanner, src);
									ctxTempNodes.push_back(fieldAccess);
								} else {
									AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
									setNodePosition(fieldAccess, scanner, src);
									ctxTempNodes.push_back(fieldAccess);
								}
								continue;
							}

							// Handle . field set operator
							if (ctxSawDot && token == U8T_IDENTIFIER) {
								ctxSawDot = false;
								const char* fieldName = u8t_scanner_token_text(scanner, &n);
								AstNodeFieldSet* fieldSet = new AstNodeFieldSet(ctxDotVarName, fieldName);
								setNodePosition(fieldSet, scanner, src);
								ctxTempNodes.push_back(fieldSet);
								ctxDotVarName.clear();
								continue;
							}

							// Handle comments
							AstNodeComment* ctxComment = parseComment(scanner, src, ctxSlashPos, token);
							if (ctxComment != nullptr) {
								ctxSlashPos = SIZE_MAX;
								for (auto* node : ctxTempNodes) {
									node->setParent(ctxStmt);
									ctxStmt->addChild(node);
								}
								ctxTempNodes.clear();
								ctxComment->setParent(ctxStmt);
								ctxStmt->addChild(ctxComment);
								continue;
							}

							if (ctxSlashPos != SIZE_MAX) {
								AstNodeInstruction* divInstr = new AstNodeInstruction("/");
								setNodePosition(divInstr, scanner, src);
								ctxTempNodes.push_back(divInstr);
								ctxSlashPos = SIZE_MAX;
							}

							if (token == '}') {
								if (ctxSawDot) {
									errorReporter->reportError(scanner, "Expected field name after '.'");
								}
								if (ctxSawAt) {
									errorReporter->reportError(scanner, "Expected field name after '@'");
								}
								break;
							}

							if (token == '/') {
								ctxSlashPos = u8t_scanner_token_start(scanner);
								continue;
							}

							ctxSawColon = (token == ':');
							if (ctxSawColon) {
								continue;
							}

							ctxSawAt = (token == '@');
							if (ctxSawAt) {
								continue;
							}

							// Handle . field set operator
							if (token == '.') {
								if (!ctxTempNodes.empty() &&
										ctxTempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
									AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(ctxTempNodes.back());
									ctxTempNodes.pop_back();
									ctxDotVarName = varIdent->name();
									delete varIdent;
									ctxSawDot = true;
									continue;
								} else {
									errorReporter->reportError(
											scanner, "Expected variable name before '.' in field set");
									continue;
								}
							}

							// Check if this token is an "else" keyword
							if (token == U8T_IDENTIFIER) {
								const char* tokenText = u8t_scanner_token_text(scanner, &n);
								if (strcmp(tokenText, "else") == 0) {
									// Flush ctxTempNodes before handling else
									for (auto* node : ctxTempNodes) {
										node->setParent(ctxStmt);
										ctxStmt->addChild(node);
									}
									ctxTempNodes.clear();

									// else must follow an if statement
									IAstNode* lastChild = (ctxStmt->childCount() > 0)
																  ? ctxStmt->child(ctxStmt->childCount() - 1)
																  : nullptr;
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

							IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, src, true);
							if (node) {
								ctxTempNodes.push_back(node);
							}
						}

						for (auto* node : ctxTempNodes) {
							node->setParent(ctxStmt);
							ctxStmt->addChild(node);
						}

						ctxStmt->setParent(body);
						body->addChild(ctxStmt);
					}
					continue; // Skip fallthrough after ctx parsing
				} else {
					// Handle boolean literals: true = 1, false = 0
					if (strcmp(text, "true") == 0) {
						IAstNode* node = new AstNodeLiteral("1", AstNodeLiteral::LiteralType::INTEGER);
						setNodePosition(node, scanner, src);
						tempNodes.push_back(node);
					} else if (strcmp(text, "false") == 0) {
						IAstNode* node = new AstNodeLiteral("0", AstNodeLiteral::LiteralType::INTEGER);
						setNodePosition(node, scanner, src);
						tempNodes.push_back(node);
					} else if (isBuiltInInstruction(text)) {
						std::string instrName(text);
						// Check for generic type parameter: instruction<Type>
						// Use peekNextChar (no whitespace skip) to distinguish make<T> from len <
						char32_t nextCh = peekNextChar(scanner, src);
						if (nextCh == '<') {
							u8t_scanner_scan(scanner); // Consume '<'
							char32_t typeToken = u8t_scanner_scan(scanner);
							if (typeToken == U8T_IDENTIFIER) {
								std::string typeParam = u8t_scanner_token_text(scanner, &n);
								char32_t closeAngle = u8t_scanner_scan(scanner);
								if (closeAngle == '>') {
									IAstNode* id = new AstNodeInstruction(instrName, typeParam);
									setNodePosition(id, scanner, src);
									tempNodes.push_back(id);
								} else {
									errorReporter->reportError(scanner, "Expected '>' after type parameter");
								}
							} else {
								errorReporter->reportError(scanner, "Expected type name after '<'");
							}
						} else {
							IAstNode* id = new AstNodeInstruction(instrName);
							setNodePosition(id, scanner, src);
							tempNodes.push_back(id);
						}
					} else {
						// Save identifier position for potential struct construction
						size_t identPos = u8t_scanner_token_start(scanner);
						std::string identName(text);

						// Check for scoped identifier or struct construction
						char32_t nextToken = u8t_scanner_peek(scanner);
						if (nextToken == ':') {
							// Save scope name
							std::string scopeName(text);
							u8t_scanner_scan(scanner); // Consume first ':'
							char32_t secondColon = u8t_scanner_peek(scanner);
							if (secondColon == ':') {
								u8t_scanner_scan(scanner); // Consume second ':'
								char32_t memberToken = u8t_scanner_scan(scanner);
								if (memberToken == U8T_IDENTIFIER) {
									const char* memberName = u8t_scanner_token_text(scanner, &n);
									std::string fullName = scopeName + "::" + memberName;

									// Check for struct construction: module::StructName { ... }
									char32_t afterMember = peekNextNonWhitespace(scanner, src);
									if (afterMember == '{') {
										u8t_scanner_scan(scanner); // Consume '{'
										AstNodeStructConstruction* structConstruct = parseStructConstruction(
												fullName, scanner, errorReporter, src, identPos);
										if (structConstruct) {
											// Push to tempNodes so @field can access it
											tempNodes.push_back(structConstruct);
										}
										continue;
									}

									AstNodeScopedIdentifier* scoped =
											new AstNodeScopedIdentifier(scopeName, memberName);
									setNodePosition(scoped, scanner, src);
									// Check for '!' or '?' suffix
									char32_t suffixToken = u8t_scanner_peek(scanner);
									if (suffixToken == '!') {
										u8t_scanner_scan(scanner);
										scoped->setAbortOnError(true);
									} else if (suffixToken == '?') {
										u8t_scanner_scan(scanner);
										scoped->setCheckError(true);
									}
									tempNodes.push_back(scoped);
									continue;
								}
							}
						}

						// Check for struct construction: StructName { ... }
						nextToken = peekNextNonWhitespace(scanner, src);
						if (nextToken == '{') {
							u8t_scanner_scan(scanner); // Consume '{'
							AstNodeStructConstruction* structConstruct =
									parseStructConstruction(identName, scanner, errorReporter, src, identPos);
							if (structConstruct) {
								// Push to tempNodes so @field can access it
								tempNodes.push_back(structConstruct);
							}
							continue;
						}

						AstNodeIdentifier* id = new AstNodeIdentifier(text);
						setNodePosition(id, scanner, src);
						// Check for '!' or '?' suffix
						nextToken = u8t_scanner_peek(scanner);
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
			} else if (token == '-') {
				// Check if this is '-> variableName' (local variable, single binding only)
				size_t tokenStart = u8t_scanner_token_start(scanner);
				size_t tokenLen = u8t_scanner_token_len(scanner);
				size_t tokenEndChar = tokenStart + tokenLen;
				// Convert character index to byte offset for indexing into src
				size_t tokenEndByte = charIndexToByteOffset(src, tokenEndChar);
				if (tokenEndByte < strlen(src) && src[tokenEndByte] == '>') {
					// This is '-> varName' (single variable binding)
					u8t_scanner_scan(scanner); // Consume '>'

					// Single identifier is required
					char32_t nextToken = u8t_scanner_scan(scanner);
					if (nextToken == U8T_IDENTIFIER) {
						const char* varName = u8t_scanner_token_text(scanner, &n);
						std::vector<std::string> varNames;
						varNames.push_back(std::string(varName));

						IAstNode* node = new AstNodeLocal(varNames);
						size_t line, column;
						// Note: calculateLineColumn expects byte position
						size_t tokenStartByte = charIndexToByteOffset(src, tokenStart);
						calculateLineColumn(src, tokenStartByte, &line, &column);
						node->setPosition(line, column);
						tempNodes.push_back(node);
					} else {
						errorReporter->reportError(scanner, "Expected variable name after '->'");
					}
				}
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
			} else if (token == '[') {
				// Handle array literal: [elem1 elem2 ...] (commas optional)
				size_t bracketPos = u8t_scanner_token_start(scanner);
				AstNodeArrayLiteral* arrNode = new AstNodeArrayLiteral();
				size_t line, column;
				calculateLineColumn(src, bracketPos, &line, &column);
				arrNode->setPosition(line, column);

				// Parse array elements until we hit ']'
				char32_t elemToken;
				while ((elemToken = u8t_scanner_scan(scanner)) != U8T_EOF) {
					if (elemToken == ']') {
						break;
					}

					// Skip commas (optional separator)
					if (elemToken == ',') {
						continue;
					}

					// Skip whitespace tokens if any
					if (elemToken == ' ' || elemToken == '\t' || elemToken == '\n' || elemToken == '\r') {
						continue;
					}

					// Parse the element
					IAstNode* elem = parseSimpleToken(elemToken, scanner, errorReporter, &n, src);
					if (elem) {
						arrNode->addElement(elem);
					} else {
						// Try to parse as a nested array
						if (elemToken == '[') {
							// Recursive array literal
							IAstNode* nestedArr =
									parseBlockStatement(elemToken, scanner, errorReporter, &n, src, false);
							if (nestedArr) {
								arrNode->addElement(nestedArr);
							}
						} else {
							// Unknown token in array literal
							errorReporter->reportError(scanner, "Unexpected token in array literal");
							break;
						}
					}
				}

				if (elemToken != ']') {
					errorReporter->reportError(scanner, "Expected ']' to close array literal");
				}

				tempNodes.push_back(arrNode);
			} else {
				// Check if this is an unrecognized token
				// Skip whitespace which may come through as character tokens
				if (token != ' ' && token != '\t' && token != '\n' && token != '\r') {
					// Check if token is in the set of valid single-char tokens we expect
					// Valid chars: { } ( ) [ ] : ; , . @ - + * / < > = ! & | ^
					bool isValidChar = (token == '{' || token == '(' || token == ')' || token == ']' || token == ';' ||
										token == ',' || token == '|' || token == '^');
					if (!isValidChar && token < 256 && token != U8T_IDENTIFIER && token != U8T_INTEGER &&
							token != U8T_FLOAT && token != U8T_STRING) {
						std::string msg = "Unexpected character '";
						msg += static_cast<char>(token);
						msg += "'";
						errorReporter->reportError(scanner, msg.c_str());
					}
				}
			}
		}

		// Check if we hit EOF without finding closing brace
		if (!foundClosingBrace) {
			errorReporter->reportError(scanner, "Expected '}' to close function body (reached end of file)");
		}

		for (auto* node : tempNodes) {
			node->setParent(body);
			body->addChild(node);
		}

		body->setParent(func);
		func->setBody(body);

		return func;
	}

	static IAstNode* parseTestDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_STRING) {
			errorReporter->reportError(scanner, "Expected test name (string) after 'test'");
			synchronize(scanner);
			return nullptr;
		}

		size_t n;
		const char* nameStr = u8t_scanner_token_text(scanner, &n);
		// Strip quotes from string literal
		std::string testName(nameStr);
		if (testName.length() >= 2 && testName.front() == '"' && testName.back() == '"') {
			testName = testName.substr(1, testName.length() - 2);
		}

		AstNodeTest* test = new AstNodeTest(testName);
		setNodePosition(test, scanner, src);

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after test name");
			synchronize(scanner);
			delete test;
			return nullptr;
		}

		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(test);
		test->setBody(body);

		return test;
	}

	static IAstNode* parseStructDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic = false) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected struct name after 'struct'");
			synchronize(scanner);
			return nullptr;
		}

		const char* name = u8t_scanner_token_text(scanner, &n);
		AstNodeStructDeclaration* structDecl = new AstNodeStructDeclaration(name, isPublic);
		setNodePosition(structDecl, scanner, src);

		// Check for generic type parameters: struct Name<T, U> { ... }
		char32_t peek = peekNextNonWhitespace(scanner, src);
		if (peek == '<') {
			u8t_scanner_scan(scanner); // Consume '<'
			while (true) {
				token = u8t_scanner_scan(scanner);
				if (token == '>') {
					break;
				}
				if (token == U8T_IDENTIFIER) {
					const char* typeParam = u8t_scanner_token_text(scanner, &n);
					structDecl->addTypeParam(std::string(typeParam));

					// Check for comma or closing >
					peek = peekNextNonWhitespace(scanner, src);
					if (peek == ',') {
						u8t_scanner_scan(scanner); // Consume ','
					}
				} else if (token != ',') {
					errorReporter->reportError(scanner, "Expected type parameter or '>' in generic struct");
					break;
				}
			}
		}

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after struct name");
			synchronize(scanner);
			delete structDecl;
			return nullptr;
		}

		// Parse struct fields
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* fieldName = u8t_scanner_token_text(scanner, &n);
				std::string fieldNameStr(fieldName);

				// Expect ':' and then type
				token = u8t_scanner_scan(scanner);
				if (token != ':') {
					errorReporter->reportError(scanner, "Expected ':' after field name");
					continue;
				}

				token = u8t_scanner_scan(scanner);
				if (token != U8T_IDENTIFIER && token != '*') {
					errorReporter->reportError(scanner, "Expected type after ':'");
					continue;
				}

				std::string fieldType;
				if (token == '*') {
					// Pointer type: *StructName
					fieldType = "*";
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* typeName = u8t_scanner_token_text(scanner, &n);
						fieldType += typeName;
					} else {
						errorReporter->reportError(scanner, "Expected type name after '*'");
						continue;
					}
				} else {
					// Regular type
					const char* typeName = u8t_scanner_token_text(scanner, &n);
					fieldType = typeName;
				}

				AstNodeStructField* field = new AstNodeStructField(fieldNameStr, fieldType);
				setNodePosition(field, scanner, src);
				field->setParent(structDecl);
				structDecl->addField(field);
			}
		}

		return structDecl;
	}

	// Parse struct construction body: StructName { field1: expr1 field2: expr2 ... }
	// Called after the opening '{' has been consumed
	static AstNodeStructConstruction* parseStructConstruction(const std::string& structName, u8t_scanner* scanner,
			ErrorReporter* errorReporter, const char* src, size_t startPos) {
		AstNodeStructConstruction* structConstruct = new AstNodeStructConstruction(structName);
		size_t line, column;
		size_t startPosByte = charIndexToByteOffset(src, startPos);
		calculateLineColumn(src, startPosByte, &line, &column);
		structConstruct->setPosition(line, column);

		std::string currentFieldName;
		std::vector<IAstNode*> currentFieldNodes;
		size_t slashPos = SIZE_MAX;
		bool sawAt = false;

		char32_t token;
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle @ field access operator
			if (sawAt && token == U8T_IDENTIFIER) {
				sawAt = false;
				size_t n;
				const char* fieldName = u8t_scanner_token_text(scanner, &n);

				if (!currentFieldNodes.empty() && currentFieldNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					// We have: identifier @field
					AstNodeIdentifier* varIdent = static_cast<AstNodeIdentifier*>(currentFieldNodes.back());
					currentFieldNodes.pop_back();

					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess(varIdent->name(), fieldName);
					setNodePosition(fieldAccess, scanner, src);
					delete varIdent;
					currentFieldNodes.push_back(fieldAccess);
				} else if (!currentFieldNodes.empty() &&
						   currentFieldNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
					// Chained field access
					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
					setNodePosition(fieldAccess, scanner, src);
					currentFieldNodes.push_back(fieldAccess);
				} else {
					// Stack-based field access
					AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
					setNodePosition(fieldAccess, scanner, src);
					currentFieldNodes.push_back(fieldAccess);
				}
				continue;
			}

			if (token == '@') {
				sawAt = true;
				continue;
			}
			// Handle comments
			AstNodeComment* comment = parseComment(scanner, src, slashPos, token);
			if (comment != nullptr) {
				slashPos = SIZE_MAX;
				delete comment; // Discard comments inside struct construction
				continue;
			}

			if (slashPos != SIZE_MAX) {
				// Previous '/' was not a comment start, treat as division
				AstNodeInstruction* divInstr = new AstNodeInstruction("/");
				setNodePosition(divInstr, scanner, src);
				currentFieldNodes.push_back(divInstr);
				slashPos = SIZE_MAX;
			}

			if (token == '}') {
				// End of struct construction
				// Save any pending field
				if (!currentFieldName.empty()) {
					structConstruct->addFieldInit(currentFieldName, std::move(currentFieldNodes));
					currentFieldNodes.clear();
				}
				break;
			}

			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
				continue;
			}

			if (token == U8T_IDENTIFIER) {
				size_t n;
				const char* text = u8t_scanner_token_text(scanner, &n);

				// Check if this identifier is followed by '=' (field name in struct construction)
				// Use peekNextNonWhitespace to allow spaces around '='
				char32_t nextToken = peekNextNonWhitespace(scanner, src);
				if (nextToken == '=') {
					// Save the field name BEFORE scanning (scan invalidates text pointer)
					std::string fieldName(text);

					// Save previous field if any
					if (!currentFieldName.empty()) {
						structConstruct->addFieldInit(currentFieldName, std::move(currentFieldNodes));
						currentFieldNodes.clear();
					}

					u8t_scanner_scan(scanner); // Consume the '='
					currentFieldName = fieldName;
					continue;
				}

				// Not a field name, parse as expression element
				// Check if it's a struct construction (nested)
				char32_t nextNonWs = peekNextNonWhitespace(scanner, src);
				if (nextNonWs == '{') {
					size_t nestedPos = u8t_scanner_token_start(scanner);
					std::string nestedName(text);
					u8t_scanner_scan(scanner); // Consume '{'
					AstNodeStructConstruction* nested =
							parseStructConstruction(nestedName, scanner, errorReporter, src, nestedPos);
					if (nested) {
						currentFieldNodes.push_back(nested);
					}
					continue;
				}

				// Check for scoped identifier (module::something)
				if (nextToken == ':') {
					// Already handled above for field names
				}

				// Regular identifier or scoped identifier
				std::string identName(text);
				nextToken = u8t_scanner_peek(scanner);
				if (nextToken == ':') {
					std::string scopeName(text);
					u8t_scanner_scan(scanner); // Consume first ':'
					char32_t secondColon = u8t_scanner_peek(scanner);
					if (secondColon == ':') {
						u8t_scanner_scan(scanner); // Consume second ':'
						char32_t memberToken = u8t_scanner_scan(scanner);
						if (memberToken == U8T_IDENTIFIER) {
							const char* memberName = u8t_scanner_token_text(scanner, &n);
							std::string fullName = scopeName + "::" + memberName;

							// Check for nested struct: module::StructName { ... }
							char32_t afterMember = peekNextNonWhitespace(scanner, src);
							if (afterMember == '{') {
								size_t nestedPos = u8t_scanner_token_start(scanner);
								u8t_scanner_scan(scanner); // Consume '{'
								AstNodeStructConstruction* nested =
										parseStructConstruction(fullName, scanner, errorReporter, src, nestedPos);
								if (nested) {
									currentFieldNodes.push_back(nested);
								}
								continue;
							}

							AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scopeName, memberName);
							setNodePosition(scoped, scanner, src);
							// Check for '!' or '?' suffix
							char32_t suffixToken = u8t_scanner_peek(scanner);
							if (suffixToken == '!') {
								u8t_scanner_scan(scanner);
								scoped->setAbortOnError(true);
							} else if (suffixToken == '?') {
								u8t_scanner_scan(scanner);
								scoped->setCheckError(true);
							}
							currentFieldNodes.push_back(scoped);
							continue;
						}
					}
				}

				// Check if it's a built-in instruction
				if (isBuiltInInstruction(text)) {
					IAstNode* instr = new AstNodeInstruction(text);
					setNodePosition(instr, scanner, src);
					currentFieldNodes.push_back(instr);
				} else {
					AstNodeIdentifier* ident = new AstNodeIdentifier(text);
					setNodePosition(ident, scanner, src);
					// Check for '!' or '?' suffix
					nextToken = u8t_scanner_peek(scanner);
					if (nextToken == '!') {
						u8t_scanner_scan(scanner);
						ident->setAbortOnError(true);
					} else if (nextToken == '?') {
						u8t_scanner_scan(scanner);
						ident->setCheckError(true);
					}
					currentFieldNodes.push_back(ident);
				}
			} else if (token == U8T_INTEGER) {
				size_t n;
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::INTEGER);
				setNodePosition(lit, scanner, src);
				currentFieldNodes.push_back(lit);
			} else if (token == U8T_FLOAT) {
				size_t n;
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::FLOAT);
				setNodePosition(lit, scanner, src);
				currentFieldNodes.push_back(lit);
			} else if (token == U8T_STRING) {
				size_t n;
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::STRING);
				setNodePosition(lit, scanner, src);
				currentFieldNodes.push_back(lit);
			} else if (token == '&') {
				// Handle '&' as function pointer reference
				size_t ampPos = u8t_scanner_token_start(scanner);
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_IDENTIFIER) {
					size_t n;
					const char* functionName = u8t_scanner_token_text(scanner, &n);
					AstNodeFunctionPointerReference* node = new AstNodeFunctionPointerReference(functionName);
					size_t ampPosByte = charIndexToByteOffset(src, ampPos);
					calculateLineColumn(src, ampPosByte, &line, &column);
					node->setPosition(line, column);
					currentFieldNodes.push_back(node);
				}
				// If not followed by identifier, silently ignore (error will be caught by semantic validator)
			} else {
				// Handle operators
				IAstNode* opNode = tryParseOperatorAlias(token, scanner, src);
				if (opNode != nullptr) {
					currentFieldNodes.push_back(opNode);
				}
			}
		}

		return structConstruct;
	}

	static IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);

		// Parse iterator name (required)
		std::string iteratorName = "it"; // Default for error recovery
		if (token == U8T_IDENTIFIER) {
			size_t n;
			const char* text = u8t_scanner_token_text(scanner, &n);
			iteratorName = std::string(text, n);
			token = u8t_scanner_scan(scanner);
		} else {
			errorReporter->reportError(scanner, "Expected iterator name after 'for' (e.g., 'for i {')");
		}

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after iterator name in 'for' loop");
			// Recovery: create empty for statement and synchronize
			AstNodeForStatement* forStmt = new AstNodeForStatement();
			forStmt->setIteratorName(iteratorName);
			setNodePosition(forStmt, scanner, src);
			AstNodeBlock* body = new AstNodeBlock();
			setNodePosition(body, scanner, src);
			body->setParent(forStmt);
			forStmt->setBody(body);
			synchronize(scanner);
			return forStmt;
		}

		AstNodeForStatement* forStmt = new AstNodeForStatement();
		forStmt->setIteratorName(iteratorName);
		setNodePosition(forStmt, scanner, src);
		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(forStmt);
		forStmt->setBody(body);

		return forStmt;
	}

	static IAstNode* parseWhileStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'while'");
			// Recovery: create empty while statement and synchronize
			AstNodeWhileStatement* whileStmt = new AstNodeWhileStatement();
			setNodePosition(whileStmt, scanner, src);
			AstNodeBlock* body = new AstNodeBlock();
			setNodePosition(body, scanner, src);
			body->setParent(whileStmt);
			whileStmt->setBody(body);
			synchronize(scanner);
			return whileStmt;
		}

		AstNodeWhileStatement* whileStmt = new AstNodeWhileStatement();
		setNodePosition(whileStmt, scanner, src);
		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(whileStmt);
		whileStmt->setBody(body);

		return whileStmt;
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
					parseBlockBody(defaultBody, scanner, errorReporter, src);

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
					// Copy the scope name before scanning more tokens (scanner buffer is reused)
					std::string scopeName(valueText, n);
					u8t_scanner_scan(scanner); // consume ':'
					char32_t doubleColon = u8t_scanner_scan(scanner);
					if (doubleColon == ':') {
						// This is a scoped identifier
						char32_t nameToken = u8t_scanner_scan(scanner);
						if (nameToken == U8T_IDENTIFIER) {
							const char* nameText = u8t_scanner_token_text(scanner, &n);
							caseValue = new AstNodeScopedIdentifier(scopeName, nameText);
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
			parseBlockBody(caseBody, scanner, errorReporter, src);

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
		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection
		while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
			size_t n;

			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(&scanner, src, slashPos, token);
			if (comment != nullptr) {
				slashPos = SIZE_MAX;
				comment->setParent(program);
				program->addChild(comment);
				continue;
			}

			// If we saw a slash but it wasn't a comment, reset the flag
			if (slashPos != SIZE_MAX) {
				slashPos = SIZE_MAX;
			}

			if (token == '/') {
				slashPos = u8t_scanner_token_start(&scanner);
				continue; // Wait for next token to see if it's a comment
			}

			switch (token) {
			case U8T_IDENTIFIER: {
				const char* text = u8t_scanner_token_text(&scanner, &n);

				if (strcmp(text, "pub") == 0) {
					// Check if next token is "fn", "const", or "struct"
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER) {
						const char* nextText = u8t_scanner_token_text(&scanner, &n);
						if (strcmp(nextText, "fn") == 0) {
							IAstNode* func = parseFunctionDeclaration(&scanner, &errorReporter, src, true);
							if (func) {
								func->setParent(program);
								program->addChild(func);
							}
						} else if (strcmp(nextText, "struct") == 0) {
							IAstNode* structDecl = parseStructDeclaration(&scanner, &errorReporter, src, true);
							if (structDecl) {
								structDecl->setParent(program);
								program->addChild(structDecl);
							}
						} else if (strcmp(nextText, "const") == 0) {
							// Parse public constant
							token = u8t_scanner_scan(&scanner);
							if (token == U8T_IDENTIFIER) {
								const char* constName = u8t_scanner_token_text(&scanner, &n);
								std::string constNameStr(constName);
								token = u8t_scanner_scan(&scanner);
								if (token == '=') {
									std::string value = parseConstantValue(&scanner, &errorReporter);
									if (!value.empty()) {
										AstNodeConstant* constDecl =
												new AstNodeConstant(constNameStr, value.c_str(), true);
										setNodePosition(constDecl, &scanner, src);
										constDecl->setParent(program);
										program->addChild(constDecl);
									}
								} else {
									errorReporter.reportError(&scanner, "Expected '=' after constant name");
								}
							} else {
								errorReporter.reportError(&scanner, "Expected constant name after 'pub const'");
								synchronize(&scanner);
							}
						} else {
							errorReporter.reportError(&scanner, "Expected 'fn', 'struct', or 'const' after 'pub'");
							synchronize(&scanner);
						}
					} else {
						errorReporter.reportError(&scanner, "Expected 'fn', 'struct', or 'const' after 'pub'");
						synchronize(&scanner);
					}
				} else if (strcmp(text, "fn") == 0) {
					IAstNode* func = parseFunctionDeclaration(&scanner, &errorReporter, src, false);
					if (func) {
						func->setParent(program);
						program->addChild(func);
					}
				} else if (strcmp(text, "struct") == 0) {
					IAstNode* structDecl = parseStructDeclaration(&scanner, &errorReporter, src, false);
					if (structDecl) {
						structDecl->setParent(program);
						program->addChild(structDecl);
					}
				} else if (strcmp(text, "use") == 0) {
					token = u8t_scanner_scan(&scanner);
					std::string moduleNameStr;

					if (token == U8T_STRING) {
						// Quoted path: use "path/to/file.qd"
						const char* pathLiteral = u8t_scanner_token_text(&scanner, &n);
						std::string path(pathLiteral);
						// Strip quotes from string literal
						if (path.length() >= 2 && path.front() == '"' && path.back() == '"') {
							moduleNameStr = path.substr(1, path.length() - 2);
						} else {
							moduleNameStr = path;
						}
					} else if (token == U8T_IDENTIFIER) {
						// Unquoted module name: use module or use module.qd
						const char* moduleName = u8t_scanner_token_text(&scanner, &n);
						moduleNameStr = std::string(moduleName);

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
					} else {
						errorReporter.reportError(&scanner, "Expected module name or quoted path after 'use'");
					}

					if (!moduleNameStr.empty()) {
						AstNodeUse* useStmt = new AstNodeUse(moduleNameStr.c_str());
						setNodePosition(useStmt, &scanner, src);
						useStmt->setParent(program);
						program->addChild(useStmt);
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
							bool isPublic = false;
							// Check for 'pub' keyword
							if (strcmp(keyword, "pub") == 0) {
								isPublic = true;
								token = u8t_scanner_scan(&scanner);
								if (token != U8T_IDENTIFIER) {
									errorReporter.reportError(&scanner, "Expected 'fn' after 'pub'");
									continue;
								}
								keyword = u8t_scanner_token_text(&scanner, &n);
							}
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
								func->isPublic = isPublic;

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

								// Check for optional '!' marker (fallible function)
								if (token == ')') {
									char32_t nextToken = u8t_scanner_peek(&scanner);
									if (nextToken == '!') {
										u8t_scanner_scan(&scanner); // Consume the '!'
										func->throws = true;
									}
									// The outer loop at line 1574 will scan the next token
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
							std::string value = parseConstantValue(&scanner, &errorReporter);
							if (!value.empty()) {
								AstNodeConstant* constDecl = new AstNodeConstant(constNameStr, value.c_str());
								setNodePosition(constDecl, &scanner, src);
								constDecl->setParent(program);
								program->addChild(constDecl);
							}
						} else {
							errorReporter.reportError(&scanner, "Expected '=' after constant name");
						}
					} else {
						errorReporter.reportError(&scanner, "Expected constant name after 'const'");
					}
				} else if (strcmp(text, "test") == 0) {
					IAstNode* testDecl = parseTestDeclaration(&scanner, &errorReporter, src);
					if (testDecl) {
						testDecl->setParent(program);
						program->addChild(testDecl);
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
