// Shared helpers for ast parser implementation files
// Not part of public API

#ifndef QD_QC_AST_PARSE_H
#define QD_QC_AST_PARSE_H

#include "ast_node_block.h"
#include "ast_node_comment.h"
#include "ast_node_label.h"
#include "source_utils.h"
#include <quadrate/qc/instructions.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_array.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_break.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_continue.h>
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_program.h>
#include <quadrate/qc/ast_node_return.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_string_interpolation.h>
#include <quadrate/qc/ast_node_struct.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/ast_node_type_alias.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/error_reporter.h>
#include <u8t/scanner.h>
#include <vector>

// RAII guard for temporary AST node vectors — deletes unflushed nodes on scope exit
class TempNodeGuard {
	std::vector<Qd::IAstNode*>& mNodes;

public:
	explicit TempNodeGuard(std::vector<Qd::IAstNode*>& nodes) : mNodes(nodes) {
	}

	~TempNodeGuard() {
		for (auto* node : mNodes) {
			delete node;
		}
	}

	// Call after flushing nodes to parent — clears without deleting
	void release() {
		mNodes.clear();
	}
};

namespace Qd {

	// Maximum recursive-descent nesting depth. The recursive parse functions
	// (block bodies, nested struct construction) have no natural bound, so
	// pathologically nested input would overflow the native stack and crash.
	// This caps the depth well below an 8 MB stack while staying far above any
	// realistic hand-written nesting.
	inline constexpr int kMaxParseDepth = 512;

	// Current recursive-descent depth, per thread (the parser is used from
	// multiple threads, e.g. the language server).
	inline thread_local int gParseDepth = 0;

	// RAII guard: increments the recursion depth on entry to a recursive parse
	// function and restores it on exit. Construct one at the top of each
	// recursive parser and bail out cleanly when exceeded() is true.
	class ParseDepthGuard {
	public:
		ParseDepthGuard() {
			++gParseDepth;
		}

		~ParseDepthGuard() {
			--gParseDepth;
		}

		ParseDepthGuard(const ParseDepthGuard&) = delete;
		ParseDepthGuard& operator=(const ParseDepthGuard&) = delete;

		bool exceeded() const {
			return gParseDepth > kMaxParseDepth;
		}
	};

} // namespace Qd

namespace Qd {

	// Source position lookup helper - combines line map and char-byte map
	struct SourceMaps {
		SourceLineMap lineMap;
		CharByteMap charByteMap;

		SourceMaps(const char* src) : lineMap(src), charByteMap(src) {
		}
	};

	// Thread-local pointer to current source maps (set at start of generate())
	// This allows helper functions to use optimized lookup without signature changes
	extern thread_local const SourceMaps* tCurrentSourceMaps;

	// Optimized charIndexToByteOffset - uses precomputed table when available
	inline size_t fastCharToByteOffset(const char* src, size_t charIndex) {
		if (tCurrentSourceMaps) {
			return tCurrentSourceMaps->charByteMap.getByteOffset(charIndex);
		}
		return charIndexToByteOffset(src, charIndex);
	}

	// Optimized calculateLineColumn - uses precomputed table when available
	inline void fastLineColumn(const char* src, size_t bytePos, size_t* line, size_t* column) {
		if (tCurrentSourceMaps) {
			tCurrentSourceMaps->lineMap.getLineColumn(bytePos, line, column);
		} else {
			Qd::fastLineColumn(src, bytePos, line, column);
		}
	}

	// Helper to set position on a node from scanner - uses optimized maps if available
	/** @brief Source line of the token the scanner is currently on (1-based). */
	inline size_t currentScannerLine(u8t_scanner* scanner, const char* src) {
		size_t charPos = u8t_scanner_token_start(scanner);
		size_t line, column;
		if (tCurrentSourceMaps) {
			size_t bytePos = tCurrentSourceMaps->charByteMap.getByteOffset(charPos);
			tCurrentSourceMaps->lineMap.getLineColumn(bytePos, &line, &column);
		} else {
			size_t bytePos = fastCharToByteOffset(src, charPos);
			fastLineColumn(src, bytePos, &line, &column);
		}
		return line;
	}

	inline void setNodePosition(IAstNode* node, u8t_scanner* scanner, const char* src) {
		size_t charPos = u8t_scanner_token_start(scanner);
		size_t line, column;

		if (tCurrentSourceMaps) {
			// O(log n) lookup using precomputed tables
			size_t bytePos = tCurrentSourceMaps->charByteMap.getByteOffset(charPos);
			tCurrentSourceMaps->lineMap.getLineColumn(bytePos, &line, &column);
		} else {
			// Fallback: O(n) lookup
			size_t bytePos = fastCharToByteOffset(src, charPos);
			fastLineColumn(src, bytePos, &line, &column);
		}
		node->setPosition(line, column);
	}

	// Check if an identifier is a built-in type name (used for unnamed parameter disambiguation)
	inline bool isTypeName(const std::string& name) {
		return name == "i64" || name == "f64" || name == "str" || name == "ptr" || name == "any";
	}

	// Helper to parse a comment (// or /* */)
	// Returns the comment node, or nullptr if not a comment
	// firstSlashPos: character position of the first slash (SIZE_MAX if no slash was seen)
	inline AstNodeComment* parseComment(u8t_scanner* scanner, const char* src, size_t firstSlashPos, char32_t token) {
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

		// Capture comment start position - use optimized maps if available
		size_t commentStartCharPos = firstSlashPos;
		size_t commentStartBytePos;
		size_t commentLine, commentColumn;
		if (tCurrentSourceMaps) {
			commentStartBytePos = tCurrentSourceMaps->charByteMap.getByteOffset(commentStartCharPos);
			tCurrentSourceMaps->lineMap.getLineColumn(commentStartBytePos, &commentLine, &commentColumn);
		} else {
			commentStartBytePos = fastCharToByteOffset(src, commentStartCharPos);
			fastLineColumn(src, commentStartBytePos, &commentLine, &commentColumn);
		}

		// Get character position and convert to byte offset for content start
		size_t charPos = u8t_scanner_token_start(scanner);
		size_t tokenLen = u8t_scanner_token_len(scanner);
		size_t bytePos;
		if (tCurrentSourceMaps) {
			bytePos = tCurrentSourceMaps->charByteMap.getByteOffset(charPos + tokenLen);
		} else {
			bytePos = fastCharToByteOffset(src, charPos + tokenLen);
		}

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

	// Helper to peek the immediate next character from source (no whitespace skip)
	// Returns the character or 0 if end of string
	inline char32_t peekNextChar(u8t_scanner* scanner, const char* src) {
		// Get current position after last token
		size_t tokenStart = u8t_scanner_token_start(scanner);
		size_t tokenLen = u8t_scanner_token_len(scanner);
		size_t pos = tokenStart + tokenLen;

		// Convert character index to byte offset
		size_t bytePos = fastCharToByteOffset(src, pos);

		if (src[bytePos] != '\0') {
			return static_cast<char32_t>(static_cast<unsigned char>(src[bytePos]));
		}
		return 0;
	}

	// Parse string interpolation: $"hello {name} is {age}" desugars to
	// sb::new "hello " sb::append name sb::append_int " is " sb::append age sb::append_int sb::finish
	// Called after '$' and the string token have both been consumed.
	inline void parseStringInterpolation(
			const std::string& raw, u8t_scanner* scanner, const char* src, std::vector<IAstNode*>& out) {
		// Create a STRING_INTERPOLATION node that preserves the template text.
		// It gets expanded to sb::new/append/finish nodes after parsing (in Ast::generate)
		// so that semantic validation and codegen see the expanded form.
		auto* node = new AstNodeStringInterpolation(raw);
		setNodePosition(node, scanner, src);
		out.push_back(node);
	}

	// Expand a STRING_INTERPOLATION node into sb::new/append/finish nodes.
	// Returns the expanded nodes (caller takes ownership).
	static inline std::vector<IAstNode*> expandStringInterpolation(AstNodeStringInterpolation* interpNode) {
		std::vector<IAstNode*> out;
		const std::string& raw = interpNode->templateString();
		size_t line = interpNode->line();
		size_t col = interpNode->column();

		auto* sbNew = new AstNodeScopedIdentifier("sb", "new");
		sbNew->setPosition(line, col);
		out.push_back(sbNew);

		std::string current;
		for (size_t i = 0; i < raw.size(); i++) {
			if (raw[i] == '{') {
				if (!current.empty()) {
					auto* lit = new AstNodeLiteral("\"" + current + "\"", AstNodeLiteral::LiteralType::STRING);
					lit->setPosition(line, col);
					out.push_back(lit);
					auto* app = new AstNodeScopedIdentifier("sb", "append");
					app->setPosition(line, col);
					out.push_back(app);
				}
				current.clear();
				size_t j = i + 1;
				std::string expr;
				while (j < raw.size() && raw[j] != '}') {
					expr += raw[j];
					j++;
				}
				i = j;
				if (!expr.empty()) {
					auto colonPos = expr.find("::");
					if (colonPos != std::string::npos) {
						std::string mod = expr.substr(0, colonPos);
						std::string func = expr.substr(colonPos + 2);
						auto* scoped = new AstNodeScopedIdentifier(mod, func);
						scoped->setPosition(line, col);
						out.push_back(scoped);
					} else {
						auto* ident = new AstNodeIdentifier(expr);
						ident->setPosition(line, col);
						out.push_back(ident);
					}
					auto* appExpr = new AstNodeScopedIdentifier("sb", "append_any");
					appExpr->setPosition(line, col);
					out.push_back(appExpr);
				}
			} else if (raw[i] == '\\' && i + 1 < raw.size()) {
				current += raw[i];
				current += raw[i + 1];
				i++;
			} else {
				current += raw[i];
			}
		}
		if (!current.empty()) {
			auto* lit = new AstNodeLiteral("\"" + current + "\"", AstNodeLiteral::LiteralType::STRING);
			lit->setPosition(line, col);
			out.push_back(lit);
			auto* app = new AstNodeScopedIdentifier("sb", "append");
			app->setPosition(line, col);
			out.push_back(app);
		}
		auto* sbFinish = new AstNodeScopedIdentifier("sb", "finish");
		sbFinish->setPosition(line, col);
		out.push_back(sbFinish);

		return out;
	}

	// Recursively walk the AST and expand all STRING_INTERPOLATION nodes in blocks
	static inline void expandAllStringInterpolations(IAstNode* node) {
		if (!node) {
			return;
		}

		if (node->type() == IAstNode::Type::BLOCK) {
			auto* block = static_cast<AstNodeBlock*>(node);
			// Walk children in reverse so indices stay valid after replacement
			for (size_t i = block->childCount(); i > 0; i--) {
				IAstNode* child = block->child(i - 1);
				if (!child) {
					// child() returns null for absent optional children (e.g. a loop with no
					// body), so this is reachable, not merely defensive.
					continue;
				}
				if (child->type() == IAstNode::Type::STRING_INTERPOLATION) {
					auto expanded = expandStringInterpolation(static_cast<AstNodeStringInterpolation*>(child));
					for (auto* n : expanded) {
						n->setParent(block);
					}
					block->replaceChildWithMany(i - 1, expanded);
				} else {
					expandAllStringInterpolations(child);
				}
			}
		} else if (node->type() == IAstNode::Type::PROGRAM) {
			// Program children can contain blocks (function bodies, test bodies)
			for (size_t i = 0; i < node->childCount(); i++) {
				expandAllStringInterpolations(node->child(i));
			}
		} else {
			// Recurse into children of other node types
			for (size_t i = 0; i < node->childCount(); i++) {
				expandAllStringInterpolations(node->child(i));
			}
		}
	}

	// Helper to peek the next non-whitespace character from source
	// Returns the character or 0 if end of string
	inline char32_t peekNextNonWhitespace(u8t_scanner* scanner, const char* src) {
		// Get current position after last token
		size_t tokenStart = u8t_scanner_token_start(scanner);
		size_t tokenLen = u8t_scanner_token_len(scanner);
		size_t pos = tokenStart + tokenLen;

		// Convert character index to byte offset
		size_t bytePos = fastCharToByteOffset(src, pos);

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

	// Helper to parse type arguments between '<' and '>'
	// Assumes '<' has already been consumed. Returns vector of type argument names.
	inline std::vector<std::string> parseTypeArguments(
			u8t_scanner* scanner, const char* src, ErrorReporter* errorReporter) {
		std::vector<std::string> typeArgs;
		while (true) {
			char32_t argToken = u8t_scanner_scan(scanner);
			if (argToken == '>') {
				break;
			}
			if (argToken == U8T_IDENTIFIER) {
				size_t n;
				const char* typeArg = u8t_scanner_token_text(scanner, &n);
				typeArgs.push_back(std::string(typeArg));
				char32_t peekComma = peekNextNonWhitespace(scanner, src);
				if (peekComma == ',') {
					u8t_scanner_scan(scanner);
				}
			} else if (argToken != ',') {
				errorReporter->reportError(scanner, "Expected type argument or '>'");
				break;
			}
		}
		return typeArgs;
	}

	// Helper to create boolean/result constant literals (true, false, Ok, Err)
	// Returns nullptr if text is not a boolean/result constant
	inline IAstNode* tryCreateBooleanLiteral(const char* text, u8t_scanner* scanner, const char* src) {
		if (strcmp(text, "true") == 0 || strcmp(text, "Ok") == 0 || strcmp(text, "false") == 0 ||
				strcmp(text, "Err") == 0) {
			IAstNode* node = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::BOOL);
			setNodePosition(node, scanner, src);
			return node;
		}
		if (strcmp(text, "null") == 0) {
			IAstNode* node = new AstNodeLiteral("null", AstNodeLiteral::LiteralType::NULL_PTR);
			setNodePosition(node, scanner, src);
			return node;
		}
		return nullptr;
	}

	// Helper to synchronize parser after an error
	// Skips tokens until a synchronization point is found
	inline void synchronize(u8t_scanner* scanner) {
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
						strcmp(text, "enum") == 0 || strcmp(text, "type") == 0 || strcmp(text, "use") == 0 ||
						strcmp(text, "import") == 0 || strcmp(text, "if") == 0 || strcmp(text, "for") == 0 ||
						strcmp(text, "while") == 0 || strcmp(text, "loop") == 0 || strcmp(text, "switch") == 0 ||
						strcmp(text, "return") == 0) {
					return;
				}
			}
		}
	}

	// Helper to check if a token is an operator alias and create the corresponding instruction node
	// Returns the instruction node if it's an operator, nullptr otherwise
	inline IAstNode* tryParseOperatorAlias(char32_t token, u8t_scanner* scanner, const char* src) {
		// Special handling for '-' to check for '->' (local variable declaration) or '--' (decrement)
		if (token == '-') {
			// Check if the next character in source is '>' (forming '->') or '-' (forming '--')
			size_t tokenStart = u8t_scanner_token_start(scanner);
			size_t tokenLen = u8t_scanner_token_len(scanner);
			size_t tokenEndChar = tokenStart + tokenLen;
			// Convert character index to byte offset for indexing into src
			size_t tokenEndByte = fastCharToByteOffset(src, tokenEndChar);
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

		// Note: << and >> are reserved for field operations (>>field / <<field)
		// They are handled in parseBlockBody, not here as operators.

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

	// Forward declarations for parse functions defined in separate compilation units

	// ast_expressions.cc
	IAstNode* parseSimpleToken(
			char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n, const char* src);
	IAstNode* parseBlockStatement(char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n,
			const char* src, bool allowControlFlow = true);

	// ast_statements.cc
	void parseBlockBody(AstNodeBlock* block, u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src,
			bool inFunctionBody = false);
	IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	IAstNode* parseLoopStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);

	// ast_declarations.cc
	IAstNode* parseFunctionDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic = false);
	IAstNode* parseAnonymousFunction(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);
	IAstNode* parseTestDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src);

	// ast_types.cc
	IAstNode* parseStructDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic = false);
	IAstNode* parseEnumDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic = false);
	IAstNode* parseTypeAliasDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic = false);
	AstNodeStructConstruction* parseStructConstruction(const std::string& structName,
			const std::vector<std::string>& typeArgs, u8t_scanner* scanner, ErrorReporter* errorReporter,
			const char* src, size_t startPos);

} // namespace Qd

#endif // QD_QC_AST_PARSE_H
