#include "ast_parse.h"

#include <algorithm>
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_struct_construction.h>
#include <quadrate/qc/numeric_literal.h>

namespace Qd {

	// Definition of thread-local source maps pointer
	thread_local const SourceMaps* tCurrentSourceMaps = nullptr;

	Ast::~Ast() = default;

	// Consumes the run of `inline` / `stack` modifiers that may precede `fn`, in any
	// order, and returns the keyword that follows them. `text` is the identifier the
	// caller has already scanned; `token` is updated alongside it so the caller's own
	// dispatch still sees the current token. Returns nullptr if the run is not
	// followed by an identifier at all, which the caller reports.
	static const char* consumeFunctionModifiers(
			u8t_scanner* scanner, const char* text, char32_t& token, bool& isInline, bool& isStack) {
		size_t n;
		while (text != nullptr && (strcmp(text, "inline") == 0 || strcmp(text, "stack") == 0)) {
			if (strcmp(text, "inline") == 0) {
				isInline = true;
			} else {
				isStack = true;
			}
			token = u8t_scanner_scan(scanner);
			if (token != U8T_IDENTIFIER) {
				return nullptr;
			}
			text = u8t_scanner_token_text(scanner, &n);
		}
		return text;
	}

	// Helper to parse constant value after '=' (handles literals and env() function)
	// Returns the resolved string value, or empty string on error
	static std::string parseConstantValue(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
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
				noteUnterminatedString(scanner, src);
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
					noteUnterminatedString(scanner, src);
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
			if (noteDigitSeparator(scanner, errorReporter, src, token)) {
				return "";
			}
			std::string valueText = u8t_scanner_token_text(scanner, &n);
			// A constant keeps its `d`, and so its type: `const Tenth = 0.1d` is a d128.
			takeD128Suffix(scanner, errorReporter, src, token, valueText);
			return valueText;
		} else if (token == U8T_STRING) {
			// Keep quotes so LLVM generator knows it's a string
			noteUnterminatedString(scanner, src);
			const char* valueText = u8t_scanner_token_text(scanner, &n);
			return std::string(valueText);
		}

		errorReporter->reportError(scanner, "Expected literal value or env() after '=' in constant declaration");
		return "";
	}

	// Peek the next non-whitespace character in the scanner's input.
	// The built-in u8t_scanner_peek returns the raw next codepoint without
	// skipping whitespace, so `Point {` peeks as `' '` after scanning
	// `Point`. We reach past the whitespace directly via the scanner's
	// internal `_str` cursor. Returns 0 at end-of-input.
	static char32_t peekNextNonWhitespace(u8t_scanner* scanner) {
		const char* p = scanner->_str;
		while (p && *p) {
			unsigned char c = static_cast<unsigned char>(*p);
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				++p;
				continue;
			}
			return static_cast<char32_t>(c);
		}
		return 0;
	}

	// Find a previously-declared constant in `program` by name.
	// Returns the const's raw value string on success, empty string if not found.
	static std::string lookupParsedConst(AstProgram* program, const std::string& name) {
		for (size_t i = 0; i < program->childCount(); i++) {
			IAstNode* child = program->child(i);
			if (child && child->type() == IAstNode::Type::CONSTANT_DECLARATION) {
				auto* c = static_cast<AstNodeConstant*>(child);
				if (c->name() == name) {
					return std::string(c->value());
				}
			}
		}
		return "";
	}

	// Infer a type name from a raw value string produced by the initializer
	// parser. Strings are wrapped in quotes; a point or an exponent makes a float;
	// everything else is treated as i64. Matches the set of literal kinds accepted below.
	static std::string inferTypeFromValue(const std::string& value) {
		if (!value.empty() && value.front() == '"') {
			return "str";
		}
		if (isD128LiteralText(value)) {
			return "d128";
		}
		if (isFloatLiteralText(value)) {
			return "f64";
		}
		return "i64";
	}

	// Parse `var name [:type] = <literal-or-const-ref>` after the `var`
	// keyword has been consumed. The `:type` annotation is optional; when
	// omitted, the type is inferred from the initializer literal (or from the
	// value of the referenced const). Initializer itself is either a literal
	// (int, float, string) or the name of a previously-declared `const` —
	// the latter is resolved at parse time so codegen sees a plain literal.
	// `env()` is intentionally not supported here; use a const for env values.
	// Shared between the `pub var` and bare `var` paths.
	static void parseGlobalVarAfterKeyword(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic, AstProgram* program) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected variable name after 'var'");
			return;
		}
		std::string varName(u8t_scanner_token_text(scanner, &n));

		std::string typeName;
		bool hasExplicitType = false;

		token = u8t_scanner_scan(scanner);
		if (token == ':') {
			// Explicit type annotation.
			token = u8t_scanner_scan(scanner);
			if (token != U8T_IDENTIFIER) {
				errorReporter->reportError(scanner, "Expected type name after ':'");
				return;
			}
			typeName = u8t_scanner_token_text(scanner, &n);
			hasExplicitType = true;
			token = u8t_scanner_scan(scanner);
		}

		if (token != '=') {
			errorReporter->reportError(
					scanner, hasExplicitType ? "Expected '=' after var type" : "Expected ':' or '=' after var name");
			return;
		}

		// Scan the initializer token. Identifier → const reference or struct
		// construction; literal → inline value. `env()` is intentionally not
		// supported here — consts can still use it.
		token = u8t_scanner_scan(scanner);
		std::string value;		// Resolved value for codegen (empty for struct-init path).
		std::string sourceExpr; // Original text for the formatter round-trip.
		AstNodeStructConstruction* structInit = nullptr;
		if (token == U8T_IDENTIFIER) {
			std::string refName(u8t_scanner_token_text(scanner, &n));
			if (refName == "env") {
				errorReporter->reportError(
						scanner, "env() is not supported in var initializers (use a const and reference it)");
				return;
			}
			// Distinguish struct construction (`StructName { ... }`) from
			// a const reference by peeking the next non-whitespace char.
			char32_t peekCh = peekNextNonWhitespace(scanner);
			if (peekCh == U'{') {
				// Record start-of-struct offset in the source so the
				// formatter can emit the original text verbatim.
				size_t structStartOffset = static_cast<size_t>(scanner->_str - src);
				// Skip whitespace before the '{' we committed to.
				while (src[structStartOffset] == ' ' || src[structStartOffset] == '\t' ||
						src[structStartOffset] == '\n' || src[structStartOffset] == '\r') {
					++structStartOffset;
				}
				size_t braceTokenStart = u8t_scanner_token_start(scanner);
				u8t_scanner_scan(scanner); // Consume '{'
				structInit = parseStructConstruction(refName, {}, scanner, errorReporter, src, braceTokenStart);
				if (!structInit) {
					return;
				}
				// After parseStructConstruction, the scanner is positioned
				// just past the closing '}'. The end offset is the byte
				// offset of the character following it.
				size_t structEndOffset = static_cast<size_t>(scanner->_str - src);
				sourceExpr = std::string(refName) + " " +
							 std::string(src + structStartOffset, structEndOffset - structStartOffset);
				if (!hasExplicitType) {
					typeName = refName;
				}
			} else {
				value = lookupParsedConst(program, refName);
				if (value.empty()) {
					std::string msg = "Unknown constant '" + refName + "' in var initializer";
					errorReporter->reportError(scanner, msg.c_str());
					return;
				}
				sourceExpr = refName;
			}
		} else if (token == U8T_INTEGER || token == U8T_FLOAT || token == U8T_STRING) {
			if (noteDigitSeparator(scanner, errorReporter, src, token)) {
				return;
			}
			if (token == U8T_STRING) {
				noteUnterminatedString(scanner, src);
			}
			value = u8t_scanner_token_text(scanner, &n);
			takeD128Suffix(scanner, errorReporter, src, token, value);
			sourceExpr = value;
		} else {
			errorReporter->reportError(scanner, "Expected literal value or const name after '=' in var initializer");
			return;
		}

		if (!hasExplicitType && !structInit) {
			typeName = inferTypeFromValue(value);
		}

		auto* varDecl = new AstNodeGlobalVar(varName, typeName, value.c_str(), isPublic);
		varDecl->setSourceExpr(sourceExpr);
		varDecl->setHasExplicitType(hasExplicitType);
		if (structInit) {
			varDecl->setInitializerNode(structInit);
		} else if (typeName == "str" && !value.empty() && value.front() == '"') {
			// A string global runs its initializer the way a struct global does. A
			// refcounted string needs a heap header, which no LLVM static initializer can
			// express, so the literal was simply dropped: `var label = "hello"` compiled
			// clean and left the global null, and reading it pushed *nothing* -- the next
			// instruction died of a stack underflow that named the instruction rather than
			// the read.
			varDecl->setInitializerNode(new AstNodeLiteral(value, AstNodeLiteral::LiteralType::STRING));
		} else if (typeName == "d128" && isD128LiteralText(value)) {
			// The literal the compiler reads into the global's initial value
			varDecl->setInitializerNode(new AstNodeLiteral(value, AstNodeLiteral::LiteralType::D128));
		}
		setNodePosition(varDecl, scanner, src);
		varDecl->setParent(program);
		program->addChild(varDecl);
	}

	// Hands a parsed top-level node to the program. A parser that failed has already
	// reported why and returns nullptr.
	static void addToProgram(AstProgram* program, IAstNode* node) {
		if (node != nullptr) {
			node->setParent(program);
			program->addChild(node);
		}
	}

	// Strips the quotes the scanner leaves on a string token
	static std::string unquoted(const char* literal) {
		std::string text(literal);
		if (text.length() >= 2 && text.front() == '"' && text.back() == '"') {
			return text.substr(1, text.length() - 2);
		}
		return text;
	}

	// `-t`: every token of `src` with its position and kind
	static void printTokens(const char* src) {
		u8t_scanner scanner;
		u8t_scanner_init(&scanner, src);
		char32_t token;
		while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
			size_t n;
			const char* text = u8t_scanner_token_text(&scanner, &n);
			std::cout << Colors::cyan() << u8t_scanner_token_start(&scanner) << Colors::reset() << " ";
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
	}

	// A `#!` first line becomes a SHEBANG comment, and the scanner moves past it
	static void parseShebang(u8t_scanner* scanner, const char* src, AstProgram* program) {
		if (src[0] != '#' || src[1] != '!') {
			return;
		}
		const char* shebangEnd = src + 2;
		while (*shebangEnd != '\0' && *shebangEnd != '\n' && *shebangEnd != '\r') {
			shebangEnd++;
		}
		std::string shebangText(src + 2, static_cast<size_t>(shebangEnd - (src + 2)));
		AstNodeComment* shebang = new AstNodeComment(shebangText, AstNodeComment::CommentType::SHEBANG);
		shebang->setPosition(1, 1);
		addToProgram(program, shebang);

		size_t skipBytes = static_cast<size_t>(shebangEnd - src);
		if (*shebangEnd == '\r' && *(shebangEnd + 1) == '\n') {
			skipBytes += 2;
		} else if (*shebangEnd == '\n' || *shebangEnd == '\r') {
			skipBytes++;
		}
		scanner->_str = src + skipBytes;
		scanner->_token_start = skipBytes;
	}

	// `const name = value`, after the `const` keyword
	static void parseConstAfterKeyword(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic, AstProgram* program) {
		size_t n;
		if (u8t_scanner_scan(scanner) != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected constant name after 'const'");
			return;
		}
		std::string name(u8t_scanner_token_text(scanner, &n));
		if (u8t_scanner_scan(scanner) != '=') {
			errorReporter->reportError(scanner, "Expected '=' after constant name");
			return;
		}
		std::string value = parseConstantValue(scanner, errorReporter, src);
		if (!value.empty()) {
			AstNodeConstant* constDecl = new AstNodeConstant(name, value.c_str(), isPublic);
			setNodePosition(constDecl, scanner, src);
			addToProgram(program, constDecl);
		}
	}

	// `use module`, `use module.qd` or `use "path/to/file.qd"`, after the `use` keyword
	static void parseUseAfterKeyword(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, AstProgram* program) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);
		std::string moduleName;
		if (token == U8T_STRING) {
			noteUnterminatedString(scanner, src);
			moduleName = unquoted(u8t_scanner_token_text(scanner, &n));
		} else if (token == U8T_IDENTIFIER) {
			moduleName = u8t_scanner_token_text(scanner, &n);
			if (u8t_scanner_peek(scanner) == static_cast<char32_t>('.')) {
				u8t_scanner_scan(scanner);
				if (u8t_scanner_scan(scanner) == U8T_IDENTIFIER &&
						strcmp(u8t_scanner_token_text(scanner, &n), "qd") == 0) {
					moduleName += ".qd";
				}
			}
		} else {
			errorReporter->reportError(scanner, "Expected module name or quoted path after 'use'");
		}

		if (!moduleName.empty()) {
			AstNodeUse* useStmt = new AstNodeUse(moduleName.c_str());
			setNodePosition(useStmt, scanner, src);
			addToProgram(program, useStmt);
		}
	}

	// One `[pub] [inline] fn name(inputs -- outputs)[!]` of an import block, after the
	// `fn` keyword. Null when it is malformed, which is reported.
	static std::unique_ptr<ImportedFunction> parseImportedFunction(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic) {
		size_t n;
		if (u8t_scanner_scan(scanner) != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected function name after 'fn'");
			return nullptr;
		}
		auto func = std::make_unique<ImportedFunction>();
		func->name = u8t_scanner_token_text(scanner, &n);
		func->isPublic = isPublic;

		// token_start is a character index and fastLineColumn wants a byte
		// offset, so it has to go through the map the way every other call
		// site does. Without the conversion the line drifts by one for each
		// multi-byte character earlier in the file, which put every imported
		// function after the first non-ASCII doc comment on the wrong line.
		size_t funcLine, funcColumn;
		size_t pos = fastCharToByteOffset(src, u8t_scanner_token_start(scanner));
		fastLineColumn(src, pos, &funcLine, &funcColumn);
		func->line = funcLine;
		func->column = funcColumn;

		if (u8t_scanner_scan(scanner) != '(') {
			errorReporter->reportError(scanner, "Expected '(' after function name");
			return nullptr;
		}

		// An import block's list takes the same forms a named function's does,
		// so it is read by the same `parseTypeFrom`. It used to have its own
		// reader that scanned exactly one identifier after the ':', which meant
		// every other form came apart silently: `[]str` was read as two stray
		// tokens `[` `]` and then an unnamed parameter of type `str`, so
		// `pub fn args( -- args:[]str)` declared a function returning a string.
		// `fn(...)`, `*T` and, in an output, `mod::T` were lost the same way.
		// Nothing reported any of it.
		char32_t token;
		bool isOutput = false;
		while (true) {
			token = u8t_scanner_scan(scanner);
			if (token == ')' || token == U8T_EOF) {
				break;
			}
			if (token == '-') {
				token = u8t_scanner_scan(scanner);
				if (token == '-') {
					isOutput = true;
					continue;
				}
				errorReporter->reportError(scanner, "Expected '--' between inputs and outputs");
				break;
			}
			// `name:type`, or a bare type. A name is always written with its
			// type, so a lone identifier here can only be the type.
			std::string paramName;
			std::string paramType;
			if (token == U8T_IDENTIFIER && u8t_scanner_peek(scanner) == ':') {
				paramName = u8t_scanner_token_text(scanner, &n);
				u8t_scanner_scan(scanner); // consume ':'
				paramType = parseType(scanner, errorReporter);
			} else {
				paramType = parseTypeFrom(token, scanner, errorReporter);
			}
			if (paramType.empty()) {
				// parseType has already reported and consumed it; carrying on
				// would read the rest of the signature as more parameters.
				break;
			}
			auto* param = new AstNodeParameter(paramName, paramType, isOutput);
			setNodePosition(param, scanner, src);
			if (isOutput) {
				func->outputParameters.emplace_back(param);
			} else {
				func->inputParameters.emplace_back(param);
			}
		}

		// An optional '!' marks it fallible
		if (token == ')' && u8t_scanner_peek(scanner) == '!') {
			u8t_scanner_scan(scanner);
			func->throws = true;
		}
		return func;
	}

	// `import "libname.so" as "namespace" { fn ... }`, after the `import` keyword
	static void parseImportAfterKeyword(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, AstProgram* program) {
		size_t n;
		if (u8t_scanner_scan(scanner) != U8T_STRING) {
			errorReporter->reportError(scanner, "Expected library name (string) after 'import'");
			return;
		}
		noteUnterminatedString(scanner, src);
		std::string library = unquoted(u8t_scanner_token_text(scanner, &n));

		if (u8t_scanner_scan(scanner) != U8T_IDENTIFIER || strcmp(u8t_scanner_token_text(scanner, &n), "as") != 0) {
			errorReporter->reportError(scanner, "Expected 'as' after library name");
			return;
		}
		if (u8t_scanner_scan(scanner) != U8T_STRING) {
			errorReporter->reportError(scanner, "Expected namespace name (string) after 'as'");
			return;
		}
		noteUnterminatedString(scanner, src);
		std::string namespaceName = unquoted(u8t_scanner_token_text(scanner, &n));

		if (u8t_scanner_scan(scanner) != '{') {
			errorReporter->reportError(scanner, "Expected '{' after namespace name");
			return;
		}

		AstNodeImport* importStmt = new AstNodeImport(library, namespaceName);
		setNodePosition(importStmt, scanner, src);

		size_t slashPos = SIZE_MAX;
		bool skipping = false; // past an error, to the next declaration
		while (true) {
			char32_t token = u8t_scanner_scan(scanner);
			if (token == '}' || token == U8T_EOF) {
				break;
			}

			// Comments inside an import block are discarded
			AstNodeComment* comment = parseComment(scanner, src, slashPos, token);
			slashPos = SIZE_MAX;
			if (comment != nullptr) {
				delete comment;
				continue;
			}
			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
				continue;
			}

			// [pub] [inline] fn. `inline` is accepted for consistency but is a
			// no-op here: an imported function has no body to inline.
			bool isPublic = false;
			const char* keyword = token == U8T_IDENTIFIER ? u8t_scanner_token_text(scanner, &n) : nullptr;
			if (keyword != nullptr && strcmp(keyword, "pub") == 0) {
				isPublic = true;
				keyword = u8t_scanner_scan(scanner) == U8T_IDENTIFIER ? u8t_scanner_token_text(scanner, &n) : nullptr;
			}
			if (keyword != nullptr && strcmp(keyword, "inline") == 0) {
				keyword = u8t_scanner_scan(scanner) == U8T_IDENTIFIER ? u8t_scanner_token_text(scanner, &n) : nullptr;
			}
			// Anything else used to be skipped a token at a time, so a typo such as
			// `fm` dropped its declaration without a word.
			if (keyword == nullptr || strcmp(keyword, "fn") != 0) {
				if (!skipping) {
					errorReporter->reportError(scanner, "Expected 'fn' in import block");
					skipping = true;
				}
				continue;
			}
			std::unique_ptr<ImportedFunction> func = parseImportedFunction(scanner, errorReporter, src, isPublic);
			skipping = func == nullptr;
			if (func) {
				importStmt->addFunction(func.release());
			}
		}

		addToProgram(program, importStmt);
	}

	// A declaration at the top level, from its first identifier `text`:
	// `[pub] [inline|stack]... fn`, `[pub] [packed] struct`, `[pub] enum|type|const|var`,
	// and the never-exported `use`, `import` and `test`.
	static void parseTopLevelDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src,
			AstProgram* program, const char* text) {
		static const char* const AFTER_PUB = "Expected 'fn', 'struct', 'enum', 'type', 'const', or 'var' after 'pub'";
		size_t n;
		char32_t token = U8T_IDENTIFIER;
		bool isPublic = false;
		if (strcmp(text, "pub") == 0) {
			isPublic = true;
			token = u8t_scanner_scan(scanner);
			if (token != U8T_IDENTIFIER) {
				errorReporter->reportError(scanner, AFTER_PUB);
				synchronize(scanner, errorReporter, src);
				return;
			}
			text = u8t_scanner_token_text(scanner, &n);
		}

		// Modifiers apply to a `fn` only. `pub` used to accept them before anything,
		// so `pub inline struct` and `pub packed fn` compiled with the modifier dropped.
		bool isInline = false;
		bool isStack = false;
		if (strcmp(text, "inline") == 0 || strcmp(text, "stack") == 0) {
			text = consumeFunctionModifiers(scanner, text, token, isInline, isStack);
			if (text == nullptr || strcmp(text, "fn") != 0) {
				errorReporter->reportError(scanner, "Expected 'fn' after function modifier");
				synchronize(scanner, errorReporter, src);
				return;
			}
		} else if (strcmp(text, "packed") == 0) {
			if (u8t_scanner_scan(scanner) != U8T_IDENTIFIER ||
					strcmp(u8t_scanner_token_text(scanner, &n), "struct") != 0) {
				errorReporter->reportError(scanner, "Expected 'struct' after 'packed'");
				synchronize(scanner, errorReporter, src);
				return;
			}
			IAstNode* structDecl = parseStructDeclaration(scanner, errorReporter, src, isPublic);
			if (structDecl != nullptr) {
				static_cast<AstNodeStructDeclaration*>(structDecl)->setPacked(true);
			}
			addToProgram(program, structDecl);
			return;
		}

		if (strcmp(text, "fn") == 0) {
			IAstNode* func = parseFunctionDeclaration(scanner, errorReporter, src, isPublic);
			if (func != nullptr) {
				static_cast<AstNodeFunctionDeclaration*>(func)->setInline(isInline);
				static_cast<AstNodeFunctionDeclaration*>(func)->setStack(isStack);
			}
			addToProgram(program, func);
		} else if (strcmp(text, "struct") == 0) {
			addToProgram(program, parseStructDeclaration(scanner, errorReporter, src, isPublic));
		} else if (strcmp(text, "enum") == 0) {
			addToProgram(program, parseEnumDeclaration(scanner, errorReporter, src, isPublic));
		} else if (strcmp(text, "type") == 0) {
			addToProgram(program, parseTypeAliasDeclaration(scanner, errorReporter, src, isPublic));
		} else if (strcmp(text, "const") == 0) {
			parseConstAfterKeyword(scanner, errorReporter, src, isPublic, program);
		} else if (strcmp(text, "var") == 0) {
			parseGlobalVarAfterKeyword(scanner, errorReporter, src, isPublic, program);
		} else if (isPublic) {
			errorReporter->reportError(scanner, AFTER_PUB);
			synchronize(scanner, errorReporter, src);
		} else if (strcmp(text, "use") == 0) {
			parseUseAfterKeyword(scanner, errorReporter, src, program);
		} else if (strcmp(text, "import") == 0) {
			parseImportAfterKeyword(scanner, errorReporter, src, program);
		} else if (strcmp(text, "test") == 0) {
			addToProgram(program, parseTestDeclaration(scanner, errorReporter, src));
		} else {
			std::string msg = "Unexpected identifier '" + std::string(text) +
							  "' at top level. Expected 'fn', 'struct', 'const', 'use', or 'test'";
			errorReporter->reportError(scanner, msg.c_str());
		}
	}

	// Errors sorted by position. The index breaks ties, so reports at one position keep
	// the order they were made in. (An index rather than std::stable_sort: that
	// allocates a temporary buffer through libstdc++'s get_temporary_buffer, which ASan
	// reports as an alloc-dealloc mismatch and which aborts every fuzz run.)
	static std::vector<ErrorInfo> inSourceOrder(const std::vector<ErrorInfo>& errors) {
		std::vector<size_t> order(errors.size());
		for (size_t i = 0; i < order.size(); i++) {
			order[i] = i;
		}
		std::sort(order.begin(), order.end(), [&errors](size_t a, size_t b) {
			const ErrorInfo& x = errors[a];
			const ErrorInfo& y = errors[b];
			if (x.line != y.line) {
				return x.line < y.line;
			}
			if (x.column != y.column) {
				return x.column < y.column;
			}
			return a < b;
		});
		std::vector<ErrorInfo> ordered;
		ordered.reserve(errors.size());
		for (size_t i : order) {
			ordered.push_back(errors[i]);
		}
		return ordered;
	}

	IAstNode* Ast::generate(const char* src, bool dumpTokens, const char* filename) {
		// For this parse only, however it returns
		struct FloatDefault {
			explicit FloatDefault(bool on) {
				parseFloatsAsD128 = on;
			}

			~FloatDefault() {
				parseFloatsAsD128 = false;
			}

			FloatDefault(const FloatDefault&) = delete;
			FloatDefault& operator=(const FloatDefault&) = delete;
		} floatDefault(mDefaultFloatD128);

		ErrorReporter errorReporter(src, filename);
		errorReporter.setStoreErrors(true);
		mRoot = std::make_unique<AstProgram>();

		u8t_scanner scanner;
		if (!u8t_scanner_init(&scanner, src)) {
			// Invalid UTF-8 input - return an empty program, but record the error the
			// way the parsing path below does: stored rather than printed, so callers
			// see it through hasErrors()/getErrors() and report it themselves. Leaving
			// those unset made a file the scanner could not read look like a clean
			// empty one, and quadlint exited 0 on it.
			errorReporter.reportError(0, 0, "Invalid UTF-8 encoding in source file");
			mErrorCount = errorReporter.errorCount();
			mErrors = errorReporter.getErrors();
			return mRoot.get();
		}

		if (dumpTokens) {
			printTokens(src);
		}

		// Build source position lookup tables - O(n) once, then O(log n) for each lookup
		SourceMaps sourceMaps(src);
		tCurrentSourceMaps = &sourceMaps;

		// parseComment and noteUnterminatedString record an unterminated /* or " here
		// rather than reporting it themselves; reset both so a previous parse on this
		// thread cannot leak into this one.
		gUnterminatedBlockCommentLine = 0;
		gUnterminatedBlockCommentColumn = 0;
		gUnterminatedStringLine = 0;
		gUnterminatedStringColumn = 0;

		setNodePosition(mRoot.get(), &scanner, src);
		AstProgram* program = static_cast<AstProgram*>(mRoot.get());
		parseShebang(&scanner, src, program);

		// Auto-inject 'use sb' if source contains $"..." string interpolation
		// and doesn't already have 'use sb'
		if (strstr(src, "$\"") != nullptr && strstr(src, "use sb") == nullptr) {
			auto* useSb = new AstNodeUse("sb");
			useSb->setPosition(0, 0);
			addToProgram(program, useSb);
		}

		char32_t token;
		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection
		while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
			size_t n;

			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(&scanner, src, slashPos, token);
			slashPos = SIZE_MAX;
			if (comment != nullptr) {
				addToProgram(program, comment);
				continue;
			}
			if (token == '/') {
				slashPos = u8t_scanner_token_start(&scanner);
				continue; // Wait for next token to see if it's a comment
			}

			switch (token) {
			case U8T_IDENTIFIER:
				parseTopLevelDeclaration(&scanner, &errorReporter, src, program, u8t_scanner_token_text(&scanner, &n));
				break;
			case U8T_INTEGER:
				errorReporter.reportError(&scanner, "Unexpected integer literal at top level");
				break;
			case U8T_STRING:
				errorReporter.reportError(&scanner, "Unexpected string literal at top level");
				break;
			case U8T_FLOAT:
				errorReporter.reportError(&scanner, "Unexpected float literal at top level");
				break;
			default:
				if (token == '}') {
					// A stray close brace at module scope means an earlier function body was
					// closed too soon. Silently skipping it let the rest of that body parse as
					// top-level declarations -- or, when nothing followed, produced a cleanly
					// built binary in which the truncated function's remaining statements had
					// simply vanished.
					errorReporter.reportError(&scanner,
							"Unmatched '}' at top level (an earlier function body was closed by an extra "
							"'}')");
				}
				break;
			}
		}

		// A block comment the lexer ran to the end of the file looking for `*/` swallowed
		// everything after it, including the braces that closed the enclosing blocks. Left
		// unreported it read as a clean parse of a shorter program, and the formatter then
		// re-emitted the comment with a `*/` the author never wrote.
		if (gUnterminatedBlockCommentLine != 0) {
			errorReporter.reportError(gUnterminatedBlockCommentLine, gUnterminatedBlockCommentColumn,
					"Unterminated block comment (reached end of file looking for '*/')");
			gUnterminatedBlockCommentLine = 0;
			gUnterminatedBlockCommentColumn = 0;
		}

		// Same shape for a string: u8t ends the token at EOF, so `use "abc` took the rest
		// of the file as a module name and the formatter re-emitted it, growing the
		// literal by a newline on every pass.
		if (gUnterminatedStringLine != 0) {
			errorReporter.reportError(
					gUnterminatedStringLine, gUnterminatedStringColumn, "Unterminated string literal");
			gUnterminatedStringLine = 0;
			gUnterminatedStringColumn = 0;
		}

		// Expand STRING_INTERPOLATION nodes before semantic validation sees them
		expandAllStringInterpolations(mRoot.get());

		// Parsing runs left to right, so the errors come out in source order -- except the
		// two reported just above, which are only known once the whole file has been read.
		mErrorCount = errorReporter.errorCount();
		mErrors = inSourceOrder(errorReporter.getErrors());

		// Clear thread-local source maps pointer
		tCurrentSourceMaps = nullptr;

		return mRoot.get();
	}
}
