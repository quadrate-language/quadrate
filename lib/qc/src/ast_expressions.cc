#include "ast_parse.h"

namespace Qd {

	// Helper to parse a single statement/expression token
	// Returns nullptr if token was a control keyword that was handled
	// Returns a node if it's a literal or identifier
	IAstNode* parseSimpleToken(
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
			// Handle boolean/result constants (true, false, Ok, Err)
			if (auto* boolNode = tryCreateBooleanLiteral(text, scanner, src)) {
				return boolNode;
			}
			if (isBuiltInInstruction(text)) {
				std::string instrName(text);
				// Check for generic type parameter: instruction<Type> or instruction<module::Type>
				// Use peekNextChar (no whitespace skip) to distinguish make<T> from len <
				char32_t nextCh = peekNextChar(scanner, src);
				if (nextCh == '<') {
					u8t_scanner_scan(scanner); // Consume '<'
					char32_t typeToken = u8t_scanner_scan(scanner);
					if (typeToken == U8T_IDENTIFIER) {
						std::string typeParam = u8t_scanner_token_text(scanner, n);

						// Check for qualified name: module::StructName
						char32_t peekChar = u8t_scanner_peek(scanner);
						if (peekChar == ':') {
							u8t_scanner_scan(scanner); // Consume first ':'
							char32_t secondColon = u8t_scanner_peek(scanner);
							if (secondColon == ':') {
								u8t_scanner_scan(scanner); // Consume second ':'
								char32_t nameToken = u8t_scanner_scan(scanner);
								if (nameToken == U8T_IDENTIFIER) {
									const char* qualName = u8t_scanner_token_text(scanner, n);
									typeParam += "::";
									typeParam += qualName;
								} else {
									errorReporter->reportError(scanner, "Expected struct name after '::'");
									return nullptr;
								}
							}
						}

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
				node->setPropagateOnError(true);
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
			size_t tokenEndByte = fastCharToByteOffset(src, tokenEndChar);
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
					size_t tokenStartByte = fastCharToByteOffset(src, tokenStart);
					fastLineColumn(src, tokenStartByte, &line, &column);
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
			if (nextToken == '<') {
				// <<field (field read, Factor-style)
				u8t_scanner_scan(scanner); // Consume second <
				char32_t fieldStart = u8t_scanner_peek(scanner);
				if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
						fieldStart == '_') {
					char32_t identToken = u8t_scanner_scan(scanner);
					if (identToken == U8T_IDENTIFIER) {
						const char* fieldName = u8t_scanner_token_text(scanner, n);
						AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
						setNodePosition(fieldAccess, scanner, src);
						return fieldAccess;
					}
				}
				// << not followed by identifier — error
				errorReporter->reportError(scanner, "Expected field name after '<<'");
				return nullptr;
			}
			// Handle '<' as alias for 'lt'
			IAstNode* node = new AstNodeInstruction("<");
			setNodePosition(node, scanner, src);
			return node;
		} else if (token == '>') {
			char32_t nextToken = u8t_scanner_peek(scanner);
			if (nextToken == '=') {
				u8t_scanner_scan(scanner); // Consume '='
				IAstNode* node = new AstNodeInstruction(">=");
				setNodePosition(node, scanner, src);
				return node;
			}
			if (nextToken == '>') {
				// >>field (field set / write, Factor-style)
				u8t_scanner_scan(scanner); // Consume second >
				char32_t fieldStart = u8t_scanner_peek(scanner);
				if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
						fieldStart == '_') {
					char32_t identToken = u8t_scanner_scan(scanner);
					if (identToken == U8T_IDENTIFIER) {
						std::string fieldName(u8t_scanner_token_text(scanner, n));
						bool noReturn = (peekNextChar(scanner, src) == '!');
						if (noReturn) {
							u8t_scanner_scan(scanner);
						}
						AstNodeFieldSet* fieldSet = new AstNodeFieldSet("", fieldName, noReturn);
						setNodePosition(fieldSet, scanner, src);
						return fieldSet;
					}
				}
				// >> not followed by identifier — error
				errorReporter->reportError(scanner, "Expected field name after '>>'");
				return nullptr;
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
				fastLineColumn(src, ampPos, &line, &column);
				node->setPosition(line, column);
				return node;
			}
			// If not followed by identifier, return nullptr (error will be handled by caller)
			return nullptr;
		}
		return nullptr;
	}

	// Helper to parse statements inside a block (handles if, break, continue, nested structures)
	// Returns a node that should be added to the parent, or nullptr
	// allowControlFlow: if false, only allows break/continue but not if/for/switch
	IAstNode* parseBlockStatement(char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n,
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
			size_t tokenEndByte = fastCharToByteOffset(src, tokenEndChar);
			// Check if character immediately after '-' is '>'
			if (tokenEndByte < strlen(src) && src[tokenEndByte] == '>') {
				// This is a local declaration: -> varName (single variable binding)
				size_t arrowPosByte = fastCharToByteOffset(src, tokenStart);
				u8t_scanner_scan(scanner); // Consume '>'

				// Single identifier is required
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_IDENTIFIER) {
					const char* varName = u8t_scanner_token_text(scanner, n);
					std::vector<std::string> varNames;
					varNames.push_back(std::string(varName));

					IAstNode* node = new AstNodeLocal(varNames);
					size_t line, column;
					fastLineColumn(src, arrowPosByte, &line, &column);
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

			// Handle boolean/result constants (true, false, Ok, Err)
			if (auto* boolNode = tryCreateBooleanLiteral(text, scanner, src)) {
				return boolNode;
			}

			// break, continue, and return are always allowed
			if (strcmp(text, "break") == 0) {
				IAstNode* node = new AstNodeBreak();
				setNodePosition(node, scanner, src);
				return node;
			} else if (strcmp(text, "continue") == 0) {
				IAstNode* node = new AstNodeContinue();
				setNodePosition(node, scanner, src);
				return node;
			} else if (strcmp(text, "return") == 0) {
				IAstNode* node = new AstNodeReturn();
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
					errorReporter->reportError(
							scanner, "'while' has been removed; use 'loop' with 'if'/'break' instead");
					synchronize(scanner);
					return nullptr;
				} else if (strcmp(text, "loop") == 0) {
					return parseLoopStatement(scanner, errorReporter, src);
				} else if (strcmp(text, "switch") == 0) {
					return parseSwitchStatement(scanner, errorReporter, src);
				}
			}

			// Type narrowing cast: 'as TypeName'
			if (strcmp(text, "as") == 0) {
				token = u8t_scanner_scan(scanner);
				if (token == U8T_IDENTIFIER) {
					std::string typeName(u8t_scanner_token_text(scanner, n));
					// Check for qualified type: module::Type
					char32_t peekChar = u8t_scanner_peek(scanner);
					if (peekChar == ':') {
						u8t_scanner_scan(scanner); // Consume first ':'
						char32_t secondColon = u8t_scanner_peek(scanner);
						if (secondColon == ':') {
							u8t_scanner_scan(scanner); // Consume second ':'
							token = u8t_scanner_scan(scanner);
							if (token == U8T_IDENTIFIER) {
								typeName += "::";
								typeName += u8t_scanner_token_text(scanner, n);
							} else {
								errorReporter->reportError(scanner, "Expected type name after '::'");
							}
						}
					}
					AstNodeAsCast* asCast = new AstNodeAsCast(typeName);
					setNodePosition(asCast, scanner, src);
					return asCast;
				} else {
					errorReporter->reportError(scanner, "Expected type name after 'as'");
				}
			}

			if (isBuiltInInstruction(text)) {
				std::string instrName(text);
				// Check for generic type parameter: instruction<Type> or instruction<module::Type>
				// Use peekNextChar (no whitespace skip) to distinguish make<T> from len <
				char32_t nextCh = peekNextChar(scanner, src);
				if (nextCh == '<') {
					u8t_scanner_scan(scanner); // Consume '<'
					char32_t typeToken = u8t_scanner_scan(scanner);
					if (typeToken == U8T_IDENTIFIER) {
						std::string typeParam = u8t_scanner_token_text(scanner, n);

						// Check for qualified name: module::StructName
						char32_t peekChar = u8t_scanner_peek(scanner);
						if (peekChar == ':') {
							u8t_scanner_scan(scanner); // Consume first ':'
							char32_t secondColon = u8t_scanner_peek(scanner);
							if (secondColon == ':') {
								u8t_scanner_scan(scanner); // Consume second ':'
								char32_t nameToken = u8t_scanner_scan(scanner);
								if (nameToken == U8T_IDENTIFIER) {
									const char* qualName = u8t_scanner_token_text(scanner, n);
									typeParam += "::";
									typeParam += qualName;
								} else {
									errorReporter->reportError(scanner, "Expected struct name after '::'");
									return nullptr;
								}
							}
						}

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

			// Check for anonymous error literal: error { code = X message = Y }
			if (identName == "error") {
				char32_t errNextToken = peekNextNonWhitespace(scanner, src);
				if (errNextToken == '{') {
					u8t_scanner_scan(scanner); // Consume '{'
					// Parse as anonymous error struct using special name __error__
					return parseStructConstruction("__error__", {}, scanner, errorReporter, src, identPos);
				}
				// Note: error <<field is handled in parseBlockBody via <<field handler
			}

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

						// Check if this is struct construction: module::StructName { ... } or module::StructName<T> {
						// ... } Only treat '<' as generic type args if member name starts with uppercase (struct names)
						// and it's not followed by '=' (which would make it '<=' operator)
						char32_t afterMember = peekNextNonWhitespace(scanner, src);
						bool memberLooksLikeStruct =
								memberName != nullptr && memberName[0] != '\0' && std::isupper(memberName[0]);
						bool isGenericBracket = false;
						if (afterMember == '<' && memberLooksLikeStruct) {
							// Check if this is '<=' operator by looking at character after '<'
							// Look ahead in the raw string to see if '<' is followed by '='
							const char* currentPos = scanner->_str;
							// Skip whitespace and find '<'
							while (*currentPos && (*currentPos == ' ' || *currentPos == '\t' || *currentPos == '\n')) {
								currentPos++;
							}
							if (*currentPos == '<' && *(currentPos + 1) != '=') {
								isGenericBracket = true;
							}
						}
						if (isGenericBracket) {
							// Parse type arguments for generic struct construction
							u8t_scanner_scan(scanner); // Consume '<'
							auto typeArgs = parseTypeArguments(scanner, src, errorReporter);
							afterMember = peekNextNonWhitespace(scanner, src);
							if (afterMember == '{') {
								u8t_scanner_scan(scanner); // Consume '{'
								return parseStructConstruction(
										fullName, typeArgs, scanner, errorReporter, src, identPos);
							}
							errorReporter->reportError(scanner, "Expected '{' after generic type arguments");
							return nullptr;
						}
						if (afterMember == '{') {
							u8t_scanner_scan(scanner); // Consume '{'
							return parseStructConstruction(fullName, {}, scanner, errorReporter, src, identPos);
						}

						std::string memberStr(memberName);

						// Check for triple scope: module::Enum::Variant
						char32_t peekColon3 = u8t_scanner_peek(scanner);
						if (peekColon3 == ':') {
							u8t_scanner_scan(scanner); // Consume first ':'
							char32_t peekColon4 = u8t_scanner_peek(scanner);
							if (peekColon4 == ':') {
								u8t_scanner_scan(scanner); // Consume second ':'
								char32_t thirdToken = u8t_scanner_scan(scanner);
								if (thirdToken == U8T_IDENTIFIER) {
									const char* variantName = u8t_scanner_token_text(scanner, n);
									memberStr += "::";
									memberStr += variantName;
								}
							}
						}

						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scopeName, memberStr);
						setNodePosition(scoped, scanner, src);
						// Check for '!' or '?' suffix
						char32_t suffixToken = u8t_scanner_peek(scanner);
						if (suffixToken == '!') {
							u8t_scanner_scan(scanner); // Consume the '!'
							scoped->setAbortOnError(true);
						} else if (suffixToken == '?') {
							u8t_scanner_scan(scanner); // Consume the '?'
							scoped->setPropagateOnError(true);
						}
						return scoped;
					}
				}
				// Not a valid scoped identifier, create regular identifier
				// Note: We already consumed the first ':', so we can't undo that.
				// This is an edge case that shouldn't normally happen.
			}

			// Check if this is struct construction: StructName { ... } or StructName<T> { ... }
			nextToken = peekNextNonWhitespace(scanner, src);
			// Only treat '<' as generic type args if identifier looks like a struct name (PascalCase)
			// and it's not followed by '=' (which would make it '<=' operator)
			bool identLooksLikeStruct = !identName.empty() && std::isupper(identName[0]);
			bool isGenericBracketIdent = false;
			if (nextToken == '<' && identLooksLikeStruct) {
				// Check if this is '<=' operator by looking at character after '<'
				const char* currentPosIdent = scanner->_str;
				// Skip whitespace and find '<'
				while (*currentPosIdent &&
						(*currentPosIdent == ' ' || *currentPosIdent == '\t' || *currentPosIdent == '\n')) {
					currentPosIdent++;
				}
				if (*currentPosIdent == '<' && *(currentPosIdent + 1) != '=') {
					isGenericBracketIdent = true;
				}
			}
			if (isGenericBracketIdent) {
				// Parse type arguments for generic struct construction
				u8t_scanner_scan(scanner); // Consume '<'
				auto typeArgs = parseTypeArguments(scanner, src, errorReporter);
				// Now expect '{'
				nextToken = peekNextNonWhitespace(scanner, src);
				if (nextToken == '{') {
					u8t_scanner_scan(scanner); // Consume '{'
					return parseStructConstruction(identName, typeArgs, scanner, errorReporter, src, identPos);
				}
				// Not a struct construction after all - fall through to regular identifier
				// but type args were consumed, this is an error
				errorReporter->reportError(scanner, "Expected '{' after generic type arguments");
				return nullptr;
			}
			if (nextToken == '{') {
				u8t_scanner_scan(scanner); // Consume '{'
				return parseStructConstruction(identName, {}, scanner, errorReporter, src, identPos);
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
				node->setPropagateOnError(true);
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
				fastLineColumn(src, ampPos, &line, &column);
				node->setPosition(line, column);
				return node;
			} else {
				errorReporter->reportError(scanner, "Expected function name after '&'");
				return nullptr;
			}
		}

		// Handle array literal: [elem1 elem2 ...]
		if (token == '[') {
			size_t bracketPos = u8t_scanner_token_start(scanner);
			AstNodeArrayLiteral* arrNode = new AstNodeArrayLiteral();
			size_t line, column;
			fastLineColumn(src, bracketPos, &line, &column);
			arrNode->setPosition(line, column);

			// Parse array elements until we hit ']'
			char32_t elemToken;
			while ((elemToken = u8t_scanner_scan(scanner)) != U8T_EOF) {
				if (elemToken == ']') {
					break;
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

} // namespace Qd
