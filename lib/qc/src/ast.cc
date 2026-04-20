#include "ast_parse.h"
#include <quadrate/qc/ast_node_global_var.h>
#include <quadrate/qc/ast_node_struct_construction.h>

namespace Qd {

	// Definition of thread-local source maps pointer
	thread_local const SourceMaps* tCurrentSourceMaps = nullptr;

	Ast::~Ast() = default;

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
	// parser. Strings are wrapped in quotes; floats contain a `.`; everything
	// else is treated as i64. Matches the set of literal kinds accepted below.
	static std::string inferTypeFromValue(const std::string& value) {
		if (!value.empty() && value.front() == '"') {
			return "str";
		}
		if (value.find('.') != std::string::npos) {
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
			errorReporter->reportError(scanner,
					hasExplicitType ? "Expected '=' after var type" : "Expected ':' or '=' after var name");
			return;
		}

		// Scan the initializer token. Identifier → const reference or struct
		// construction; literal → inline value. `env()` is intentionally not
		// supported here — consts can still use it.
		token = u8t_scanner_scan(scanner);
		std::string value;		                       // Resolved value for codegen (empty for struct-init path).
		std::string sourceExpr;                        // Original text for the formatter round-trip.
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
			value = u8t_scanner_token_text(scanner, &n);
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
		}
		setNodePosition(varDecl, scanner, src);
		varDecl->setParent(program);
		program->addChild(varDecl);
	}


	IAstNode* Ast::generate(const char* src, bool dumpTokens, const char* filename) {
		u8t_scanner scanner;
		if (!u8t_scanner_init(&scanner, src)) {
			// Invalid UTF-8 input - return empty program with error
			ErrorReporter errorReporter(src, filename);
			errorReporter.reportError(0, 0, "Invalid UTF-8 encoding in source file");
			mRoot = std::make_unique<AstProgram>();
			return mRoot.get();
		}

		// Build source position lookup tables - O(n) once, then O(log n) for each lookup
		SourceMaps sourceMaps(src);
		tCurrentSourceMaps = &sourceMaps;

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

		mRoot = std::make_unique<AstProgram>();
		setNodePosition(mRoot.get(), &scanner, src);
		AstProgram* program = static_cast<AstProgram*>(mRoot.get());

		// Check for shebang at the very beginning of the file
		// Shebang must be on the first line, starting with #!
		if (src[0] == '#' && src[1] == '!') {
			// Read until end of line
			const char* shebangEnd = src + 2;
			while (*shebangEnd != '\0' && *shebangEnd != '\n' && *shebangEnd != '\r') {
				shebangEnd++;
			}
			std::string shebangText(src + 2, static_cast<size_t>(shebangEnd - (src + 2)));
			AstNodeComment* shebang = new AstNodeComment(shebangText, AstNodeComment::CommentType::SHEBANG);
			shebang->setPosition(1, 1);
			shebang->setParent(program);
			program->addChild(shebang);

			// Advance scanner past shebang line
			size_t skipBytes = static_cast<size_t>(shebangEnd - src);
			if (*shebangEnd == '\r' && *(shebangEnd + 1) == '\n') {
				skipBytes += 2;
			} else if (*shebangEnd == '\n' || *shebangEnd == '\r') {
				skipBytes++;
			}
			scanner._str = src + skipBytes;
			scanner._token_start = skipBytes;
		}

		// Auto-inject 'use sb' if source contains $"..." string interpolation
		// and doesn't already have 'use sb'
		if (strstr(src, "$\"") != nullptr && strstr(src, "use sb") == nullptr) {
			auto* useSb = new AstNodeUse("sb");
			useSb->setPosition(0, 0);
			useSb->setParent(program);
			program->addChild(useSb);
		}

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
					// Check if next token is "fn", "inline", "packed", "const", or "struct"
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER) {
						const char* nextText = u8t_scanner_token_text(&scanner, &n);
						bool isInline = false;
						bool isPacked = false;
						if (strcmp(nextText, "inline") == 0) {
							isInline = true;
							token = u8t_scanner_scan(&scanner);
							nextText = u8t_scanner_token_text(&scanner, &n);
						} else if (strcmp(nextText, "packed") == 0) {
							isPacked = true;
							token = u8t_scanner_scan(&scanner);
							nextText = u8t_scanner_token_text(&scanner, &n);
						}
						if (strcmp(nextText, "fn") == 0) {
							IAstNode* func = parseFunctionDeclaration(&scanner, &errorReporter, src, true);
							if (func) {
								static_cast<AstNodeFunctionDeclaration*>(func)->setInline(isInline);
								func->setParent(program);
								program->addChild(func);
							}
						} else if (strcmp(nextText, "struct") == 0) {
							IAstNode* structDecl = parseStructDeclaration(&scanner, &errorReporter, src, true);
							if (structDecl) {
								if (isPacked) {
									static_cast<AstNodeStructDeclaration*>(structDecl)->setPacked(true);
								}
								structDecl->setParent(program);
								program->addChild(structDecl);
							}
						} else if (strcmp(nextText, "enum") == 0) {
							IAstNode* enumDecl = parseEnumDeclaration(&scanner, &errorReporter, src, true);
							if (enumDecl) {
								enumDecl->setParent(program);
								program->addChild(enumDecl);
							}
						} else if (strcmp(nextText, "type") == 0) {
							IAstNode* typeAlias = parseTypeAliasDeclaration(&scanner, &errorReporter, src, true);
							if (typeAlias) {
								typeAlias->setParent(program);
								program->addChild(typeAlias);
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
						} else if (strcmp(nextText, "var") == 0) {
							parseGlobalVarAfterKeyword(&scanner, &errorReporter, src, true, program);
						} else {
							errorReporter.reportError(
									&scanner, "Expected 'fn', 'struct', 'enum', 'type', 'const', or 'var' after 'pub'");
							synchronize(&scanner);
						}
					} else {
						errorReporter.reportError(
								&scanner, "Expected 'fn', 'struct', 'enum', 'type', 'const', or 'var' after 'pub'");
						synchronize(&scanner);
					}
				} else if (strcmp(text, "inline") == 0) {
					// inline fn — private inline function
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER) {
						const char* nextText = u8t_scanner_token_text(&scanner, &n);
						if (strcmp(nextText, "fn") == 0) {
							IAstNode* func = parseFunctionDeclaration(&scanner, &errorReporter, src, false);
							if (func) {
								static_cast<AstNodeFunctionDeclaration*>(func)->setInline(true);
								func->setParent(program);
								program->addChild(func);
							}
						} else {
							errorReporter.reportError(&scanner, "Expected 'fn' after 'inline'");
							synchronize(&scanner);
						}
					} else {
						errorReporter.reportError(&scanner, "Expected 'fn' after 'inline'");
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
				} else if (strcmp(text, "packed") == 0) {
					// `packed struct ...` — non-public packed struct.
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER && strcmp(u8t_scanner_token_text(&scanner, &n), "struct") == 0) {
						IAstNode* structDecl = parseStructDeclaration(&scanner, &errorReporter, src, false);
						if (structDecl) {
							static_cast<AstNodeStructDeclaration*>(structDecl)->setPacked(true);
							structDecl->setParent(program);
							program->addChild(structDecl);
						}
					} else {
						errorReporter.reportError(&scanner, "Expected 'struct' after 'packed'");
						synchronize(&scanner);
					}
				} else if (strcmp(text, "enum") == 0) {
					IAstNode* enumDecl = parseEnumDeclaration(&scanner, &errorReporter, src, false);
					if (enumDecl) {
						enumDecl->setParent(program);
						program->addChild(enumDecl);
					}
				} else if (strcmp(text, "type") == 0) {
					IAstNode* typeAlias = parseTypeAliasDeclaration(&scanner, &errorReporter, src, false);
					if (typeAlias) {
						typeAlias->setParent(program);
						program->addChild(typeAlias);
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
					size_t importSlashPos = SIZE_MAX;
					while (true) {
						token = u8t_scanner_scan(&scanner);
						if (token == '}' || token == U8T_EOF) {
							break;
						}

						// Handle comments (// and /* */) inside import block
						AstNodeComment* importComment = parseComment(&scanner, src, importSlashPos, token);
						if (importComment != nullptr) {
							importSlashPos = SIZE_MAX;
							delete importComment; // Discard comments inside import block
							continue;
						}

						// If we saw a slash but it wasn't a comment, reset
						if (importSlashPos != SIZE_MAX) {
							importSlashPos = SIZE_MAX;
						}

						// Track slashes for potential comments
						if (token == '/') {
							importSlashPos = u8t_scanner_token_start(&scanner);
							continue;
						}

						if (token == U8T_IDENTIFIER) {
							const char* keyword = u8t_scanner_token_text(&scanner, &n);
							bool isPublic = false;
							// Check for 'pub' keyword
							if (strcmp(keyword, "pub") == 0) {
								isPublic = true;
								token = u8t_scanner_scan(&scanner);
								if (token != U8T_IDENTIFIER) {
									errorReporter.reportError(&scanner, "Expected 'fn' or 'const' after 'pub'");
									continue;
								}
								keyword = u8t_scanner_token_text(&scanner, &n);
							}
							if (strcmp(keyword, "inline") == 0) {
								// 'inline' is accepted for consistency but is a no-op
								// in import blocks (imported functions have no body to inline).
								token = u8t_scanner_scan(&scanner);
								if (token != U8T_IDENTIFIER) {
									errorReporter.reportError(&scanner, "Expected 'fn' after 'inline'");
									continue;
								}
								keyword = u8t_scanner_token_text(&scanner, &n);
							}
							if (strcmp(keyword, "fn") == 0) {
								// Parse function declaration
								token = u8t_scanner_scan(&scanner);
								if (token != U8T_IDENTIFIER) {
									errorReporter.reportError(&scanner, "Expected 'fn' after 'pub'");
									continue;
								}
								const char* funcName = u8t_scanner_token_text(&scanner, &n);
								auto func = std::make_unique<ImportedFunction>();
								func->name = funcName;
								func->isPublic = isPublic;

								size_t funcLine, funcColumn;
								size_t pos = u8t_scanner_token_start(&scanner);
								fastLineColumn(src, pos, &funcLine, &funcColumn);
								func->line = funcLine;
								func->column = funcColumn;

								// Expect '('
								token = u8t_scanner_scan(&scanner);
								if (token != '(') {
									errorReporter.reportError(&scanner, "Expected '(' after function name");
									continue; // func auto-deleted by unique_ptr
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
													char32_t paramPeek = u8t_scanner_peek(&scanner);
													if (paramPeek == ':') {
														// Named typed parameter: name:type
														token = u8t_scanner_scan(&scanner);
														token = u8t_scanner_scan(&scanner);
														if (token == U8T_IDENTIFIER) {
															const char* paramType =
																	u8t_scanner_token_text(&scanner, &n);
															std::string paramTypeStr(paramType);
															AstNodeParameter* param = new AstNodeParameter(
																	paramNameStr, paramTypeStr, true);
															func->outputParameters.emplace_back(param);
														}
													} else if (isTypeName(paramNameStr)) {
														// Unnamed typed parameter: i64
														AstNodeParameter* param =
																new AstNodeParameter("", paramNameStr, true);
														func->outputParameters.emplace_back(param);
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
										char32_t paramPeek = u8t_scanner_peek(&scanner);
										if (paramPeek == ':') {
											// Named typed parameter: name:type
											token = u8t_scanner_scan(&scanner); // consume ':'
											token = u8t_scanner_scan(&scanner);
											if (token == U8T_IDENTIFIER) {
												const char* paramType = u8t_scanner_token_text(&scanner, &n);
												std::string paramTypeStr(paramType);
												// Check for qualified type name (module::Type)
												char32_t peek1 = u8t_scanner_peek(&scanner);
												if (peek1 == ':') {
													u8t_scanner_scan(&scanner); // consume first ':'
													char32_t peek2 = u8t_scanner_peek(&scanner);
													if (peek2 == ':') {
														u8t_scanner_scan(&scanner); // consume second ':'
														token = u8t_scanner_scan(&scanner);
														if (token == U8T_IDENTIFIER) {
															const char* structName =
																	u8t_scanner_token_text(&scanner, &n);
															paramTypeStr = paramTypeStr + "::" + structName;
														}
													}
												}
												AstNodeParameter* param =
														new AstNodeParameter(paramNameStr, paramTypeStr, false);
												func->inputParameters.emplace_back(param);
											}
										} else if (isTypeName(paramNameStr)) {
											// Unnamed typed parameter: i64
											AstNodeParameter* param = new AstNodeParameter("", paramNameStr, false);
											func->inputParameters.emplace_back(param);
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

								importStmt->addFunction(func.release());
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
				} else if (strcmp(text, "var") == 0) {
					parseGlobalVarAfterKeyword(&scanner, &errorReporter, src, false, program);
				} else if (strcmp(text, "test") == 0) {
					IAstNode* testDecl = parseTestDeclaration(&scanner, &errorReporter, src);
					if (testDecl) {
						testDecl->setParent(program);
						program->addChild(testDecl);
					}
				} else {
					std::string msg = "Unexpected identifier '" + std::string(text) +
									  "' at top level. Expected 'fn', 'struct', 'const', 'use', or 'test'";
					errorReporter.reportError(&scanner, msg.c_str());
				}
				break;
			}
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
				break;
			}
		}

		// Expand STRING_INTERPOLATION nodes before semantic validation sees them
		expandAllStringInterpolations(mRoot.get());

		// Store the error count and details for later checking
		mErrorCount = errorReporter.errorCount();
		mErrors = errorReporter.getErrors();

		// Clear thread-local source maps pointer
		tCurrentSourceMaps = nullptr;

		return mRoot.get();
	}
}
