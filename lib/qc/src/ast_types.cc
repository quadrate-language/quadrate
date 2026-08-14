#include "ast_parse.h"
#include <quadrate/qc/ast_node_array_literal.h>

namespace Qd {

	IAstNode* parseEnumDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected enum name after 'enum'");
			synchronize(scanner);
			return nullptr;
		}

		const char* name = u8t_scanner_token_text(scanner, &n);
		auto enumDecl = std::make_unique<AstNodeEnumDeclaration>(name, isPublic);
		setNodePosition(enumDecl.get(), scanner, src);

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after enum name");
			synchronize(scanner);
			return nullptr;
		}

		int64_t nextValue = 0;
		size_t slashPos = SIZE_MAX;
		size_t lastVariantLine = 0; // for deciding whether a comment trails a variant
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			// Handle comments
			auto comment = std::unique_ptr<AstNodeComment>(parseComment(scanner, src, slashPos, token));
			if (comment != nullptr) {
				// Keep it for the formatter, tagged with where it sits relative to the variants.
				enumDecl->addBodyComment(comment->text(), comment->commentType() == AstNodeComment::CommentType::BLOCK,
						enumDecl->variants().size(), lastVariantLine != 0 && comment->line() == lastVariantLine);
				slashPos = SIZE_MAX;
				continue;
			}
			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
				continue;
			}
			slashPos = SIZE_MAX;

			if (token == U8T_IDENTIFIER) {
				const char* variantName = u8t_scanner_token_text(scanner, &n);
				std::string variantNameStr(variantName);
				lastVariantLine = currentScannerLine(scanner, src);

				// Check for explicit value: Name = <integer>
				char32_t peek = peekNextNonWhitespace(scanner, src);
				if (peek == '=') {
					u8t_scanner_scan(scanner); // Consume '='
					token = u8t_scanner_scan(scanner);
					bool negative = false;
					if (token == '-') {
						negative = true;
						token = u8t_scanner_scan(scanner);
					}
					if (token == U8T_INTEGER) {
						const char* valText = u8t_scanner_token_text(scanner, &n);
						int64_t val = static_cast<int64_t>(strtoll(valText, nullptr, 0));
						if (negative) {
							val = -val;
						}
						nextValue = val;
					} else {
						errorReporter->reportError(scanner, "Expected integer value after '=' in enum");
					}
				}

				enumDecl->addVariant(variantNameStr, nextValue);
				nextValue++;
			} else {
				// Skip comments or unexpected tokens
				continue;
			}
		}

		return enumDecl.release();
	}

	IAstNode* parseStructDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected struct name after 'struct'");
			synchronize(scanner);
			return nullptr;
		}

		const char* name = u8t_scanner_token_text(scanner, &n);
		auto structDecl = std::make_unique<AstNodeStructDeclaration>(name, isPublic);
		setNodePosition(structDecl.get(), scanner, src);

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
			return nullptr;
		}

		// Parse struct fields
		size_t slashPos = SIZE_MAX;
		size_t lastFieldLine = 0; // for deciding whether a comment trails a field
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle comments, the same way the enum and struct-construction loops do. Without
			// this a trailing `// note` on a field line reaches the field parser as a stray
			// token and reports "Expected ':' after field name".
			auto comment = std::unique_ptr<AstNodeComment>(parseComment(scanner, src, slashPos, token));
			if (comment != nullptr) {
				// Keep it for the formatter, tagged with where it sits relative to the fields.
				structDecl->addBodyComment(comment->text(),
						comment->commentType() == AstNodeComment::CommentType::BLOCK, structDecl->fields().size(),
						lastFieldLine != 0 && comment->line() == lastFieldLine);
				slashPos = SIZE_MAX;
				continue;
			}
			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
				continue;
			}
			slashPos = SIZE_MAX;

			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* fieldName = u8t_scanner_token_text(scanner, &n);
				std::string fieldNameStr(fieldName);
				lastFieldLine = currentScannerLine(scanner, src);

				// Expect ':' and then type
				token = u8t_scanner_scan(scanner);
				if (token != ':') {
					errorReporter->reportError(scanner, "Expected ':' after field name");
					continue;
				}

				token = u8t_scanner_scan(scanner);
				if (token != U8T_IDENTIFIER && token != '*' && token != '[') {
					errorReporter->reportError(scanner, "Expected type after ':'");
					continue;
				}

				std::string fieldType;
				if (token == '[') {
					// Array type: []T
					token = u8t_scanner_scan(scanner);
					if (token == ']') {
						token = u8t_scanner_scan(scanner);
						if (token == U8T_IDENTIFIER) {
							const char* elemType = u8t_scanner_token_text(scanner, &n);
							fieldType = "[]" + std::string(elemType);
						} else {
							errorReporter->reportError(scanner, "Expected element type after '[]'");
							continue;
						}
					} else {
						errorReporter->reportError(scanner, "Expected ']' after '['");
						continue;
					}
				} else if (token == '*') {
					// Pointer type: *StructName
					fieldType = "*";
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* typeName = u8t_scanner_token_text(scanner, &n);
						fieldType += typeName;

						// Check for qualified name: *module::StructName
						char32_t peekChar = u8t_scanner_peek(scanner);
						if (peekChar == ':') {
							u8t_scanner_scan(scanner); // Consume first ':'
							char32_t secondColon = u8t_scanner_peek(scanner);
							if (secondColon == ':') {
								u8t_scanner_scan(scanner); // Consume second ':'
								char32_t nameToken = u8t_scanner_scan(scanner);
								if (nameToken == U8T_IDENTIFIER) {
									const char* qualName = u8t_scanner_token_text(scanner, &n);
									fieldType += "::";
									fieldType += qualName;
								} else {
									errorReporter->reportError(scanner, "Expected struct name after '::'");
								}
							}
							// Note: If second char wasn't ':', we've consumed a single ':'
							// which is an error in this context, but that will be handled
							// on the next field parse attempt
						}
					} else {
						errorReporter->reportError(scanner, "Expected type name after '*'");
						continue;
					}
				} else {
					// Regular type
					const char* typeName = u8t_scanner_token_text(scanner, &n);
					fieldType = typeName;

					// Check for fn(...) type: fn(i64 -- i64)
					if (fieldType == "fn") {
						char32_t fnPeek = u8t_scanner_peek(scanner);
						if (fnPeek == '(') {
							u8t_scanner_scan(scanner); // consume '('
							fieldType = "fn(";
							bool needSpace = false;
							while (true) {
								char32_t ftok = u8t_scanner_scan(scanner);
								if (ftok == U8T_EOF || ftok == ')') {
									break;
								}
								if (ftok == '-') {
									char32_t next = u8t_scanner_peek(scanner);
									if (next == '-') {
										u8t_scanner_scan(scanner);
										if (needSpace) {
											fieldType += " ";
										}
										fieldType += "-- ";
										needSpace = false;
										continue;
									}
								}
								if (ftok == U8T_IDENTIFIER) {
									size_t fn;
									const char* ft = u8t_scanner_token_text(scanner, &fn);
									if (needSpace) {
										fieldType += " ";
									}
									fieldType += ft;
									needSpace = true;
								}
							}
							if (!fieldType.empty() && fieldType.back() == ' ') {
								fieldType.pop_back();
							}
							fieldType += ")";
						}
					}

					// Check for qualified name: module::StructName
					char32_t peekChar = u8t_scanner_peek(scanner);
					if (peekChar == ':') {
						u8t_scanner_scan(scanner); // Consume first ':'
						char32_t secondColon = u8t_scanner_peek(scanner);
						if (secondColon == ':') {
							u8t_scanner_scan(scanner); // Consume second ':'
							char32_t nameToken = u8t_scanner_scan(scanner);
							if (nameToken == U8T_IDENTIFIER) {
								const char* qualName = u8t_scanner_token_text(scanner, &n);
								fieldType += "::";
								fieldType += qualName;
							} else {
								errorReporter->reportError(scanner, "Expected struct name after '::'");
							}
						}
						// Note: If second char wasn't ':', we've consumed a single ':'
						// which is an error in this context, but that will be handled
						// on the next field parse attempt
					}
				}

				AstNodeStructField* field = new AstNodeStructField(fieldNameStr, fieldType);
				setNodePosition(field, scanner, src);
				field->setParent(structDecl.get());

				// Check for default value: field:type = expr
				// For simplicity, only support single-token defaults (literals or identifiers)
				char32_t peekDefault = peekNextNonWhitespace(scanner, src);
				if (peekDefault == '=') {
					u8t_scanner_scan(scanner); // Consume '='

					std::vector<IAstNode*> defaultNodes;

					// Parse the default value (single token for now)
					char32_t valToken = u8t_scanner_scan(scanner);
					if (valToken == U8T_INTEGER) {
						const char* numText = u8t_scanner_token_text(scanner, &n);
						AstNodeLiteral* numNode = new AstNodeLiteral(numText, AstNodeLiteral::LiteralType::INTEGER);
						setNodePosition(numNode, scanner, src);
						defaultNodes.push_back(numNode);
					} else if (valToken == U8T_FLOAT) {
						const char* numText = u8t_scanner_token_text(scanner, &n);
						AstNodeLiteral* numNode = new AstNodeLiteral(numText, AstNodeLiteral::LiteralType::FLOAT);
						setNodePosition(numNode, scanner, src);
						defaultNodes.push_back(numNode);
					} else if (valToken == U8T_STRING) {
						const char* strText = u8t_scanner_token_text(scanner, &n);
						AstNodeLiteral* strNode = new AstNodeLiteral(strText, AstNodeLiteral::LiteralType::STRING);
						setNodePosition(strNode, scanner, src);
						defaultNodes.push_back(strNode);
					} else if (valToken == U8T_IDENTIFIER) {
						const char* idText = u8t_scanner_token_text(scanner, &n);
						// Check for boolean/null literals
						if (auto* boolNode = tryCreateBooleanLiteral(idText, scanner, src)) {
							defaultNodes.push_back(boolNode);
						} else {
							AstNodeIdentifier* ident = new AstNodeIdentifier(idText);
							setNodePosition(ident, scanner, src);
							defaultNodes.push_back(ident);
						}
					} else if (valToken == '-') {
						// Negative number
						char32_t numToken = u8t_scanner_scan(scanner);
						if (numToken == U8T_INTEGER) {
							const char* numText = u8t_scanner_token_text(scanner, &n);
							std::string negNum = std::string("-") + numText;
							AstNodeLiteral* numNode = new AstNodeLiteral(negNum, AstNodeLiteral::LiteralType::INTEGER);
							setNodePosition(numNode, scanner, src);
							defaultNodes.push_back(numNode);
						} else if (numToken == U8T_FLOAT) {
							const char* numText = u8t_scanner_token_text(scanner, &n);
							std::string negNum = std::string("-") + numText;
							AstNodeLiteral* numNode = new AstNodeLiteral(negNum, AstNodeLiteral::LiteralType::FLOAT);
							setNodePosition(numNode, scanner, src);
							defaultNodes.push_back(numNode);
						}
					}

					if (!defaultNodes.empty()) {
						field->setDefaultValue(std::move(defaultNodes));
					}
				}

				structDecl->addField(field);
			}
		}

		return structDecl.release();
	}

	// Parse struct construction body: StructName { field1: expr1 field2: expr2 ... }
	// or StructName<T> { field1: expr1 ... } for generic structs
	// Called after the opening '{' has been consumed
	AstNodeStructConstruction* parseStructConstruction(const std::string& structName,
			const std::vector<std::string>& typeArgs, u8t_scanner* scanner, ErrorReporter* errorReporter,
			const char* src, size_t startPos) {
		ParseDepthGuard depthGuard;
		if (depthGuard.exceeded()) {
			errorReporter->reportError(scanner, "Struct construction nesting too deep");
			return nullptr;
		}
		AstNodeStructConstruction* structConstruct = new AstNodeStructConstruction(structName, typeArgs);
		size_t line, column;
		size_t startPosByte = fastCharToByteOffset(src, startPos);
		fastLineColumn(src, startPosByte, &line, &column);
		structConstruct->setPosition(line, column);

		std::string currentFieldName;
		std::vector<IAstNode*> currentFieldNodes;
		size_t slashPos = SIZE_MAX;

		char32_t token;
		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle > and >> in struct construction
			if (token == '>') {
				char32_t nextChar = u8t_scanner_peek(scanner);
				if (nextChar == '>') {
					u8t_scanner_scan(scanner); // Consume second >
					char32_t fieldStart = u8t_scanner_peek(scanner);
					if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
							fieldStart == '_') {
						char32_t identToken = u8t_scanner_scan(scanner);
						if (identToken == U8T_IDENTIFIER) {
							size_t fn;
							std::string fieldName(u8t_scanner_token_text(scanner, &fn));
							bool noReturn = (u8t_scanner_peek(scanner) == '!');
							if (noReturn) {
								u8t_scanner_scan(scanner);
							}
							// >>field in struct construction = field set (write)
							AstNodeFieldSet* fieldSet = new AstNodeFieldSet("", fieldName, noReturn);
							setNodePosition(fieldSet, scanner, src);
							currentFieldNodes.push_back(fieldSet);
							continue;
						}
					}
				}
				// Single > — treat as comparison
				AstNodeInstruction* instr = new AstNodeInstruction(">");
				setNodePosition(instr, scanner, src);
				currentFieldNodes.push_back(instr);
				continue;
			}

			// Handle < and << in struct construction
			if (token == '<') {
				char32_t nextChar = u8t_scanner_peek(scanner);
				if (nextChar == '<') {
					u8t_scanner_scan(scanner); // Consume second <
					char32_t fieldStart = u8t_scanner_peek(scanner);
					if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
							fieldStart == '_') {
						char32_t identToken = u8t_scanner_scan(scanner);
						if (identToken == U8T_IDENTIFIER) {
							size_t fn;
							const char* fieldName = u8t_scanner_token_text(scanner, &fn);
							// <<field in struct construction = field read
							if (!currentFieldNodes.empty() &&
									currentFieldNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
								std::unique_ptr<IAstNode> varOwner(currentFieldNodes.back());
								currentFieldNodes.pop_back();
								auto* varIdent = static_cast<AstNodeIdentifier*>(varOwner.get());
								std::string varName = varIdent->name();
								if (varName == "error") {
									varName = "__global_error__";
								}
								AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess(varName, fieldName);
								setNodePosition(fieldAccess, scanner, src);
								currentFieldNodes.push_back(fieldAccess);
								// varOwner auto-deletes old node
							} else if (!currentFieldNodes.empty() &&
									   currentFieldNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
								AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
								setNodePosition(fieldAccess, scanner, src);
								currentFieldNodes.push_back(fieldAccess);
							} else {
								AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
								setNodePosition(fieldAccess, scanner, src);
								currentFieldNodes.push_back(fieldAccess);
							}
							continue;
						}
					}
				}
				// Single < — treat as comparison
				AstNodeInstruction* instr = new AstNodeInstruction("<");
				setNodePosition(instr, scanner, src);
				currentFieldNodes.push_back(instr);
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

				// Check for common mistake: using ':' instead of '=' for field initializers
				// But don't trigger on '::' (scope operator like math::Vec3)
				if (nextToken == ':') {
					// Save identifier text BEFORE any more scanning (scanner buffer gets reused)
					std::string identText(text);

					// Peek ahead to check if it's '::' (scope operator)
					u8t_scanner_scan(scanner); // Consume the first ':'
					char32_t afterColon = peekNextNonWhitespace(scanner, src);
					if (afterColon != ':') {
						// Single ':' - this is likely the user mistake
						size_t errPos = u8t_scanner_token_start(scanner);
						size_t errLine, errColumn;
						size_t errPosByte = fastCharToByteOffset(src, errPos);
						fastLineColumn(src, errPosByte, &errLine, &errColumn);
						std::string errorMsg = "Use '=' instead of ':' for struct field initializers. ";
						errorMsg += "Expected '" + identText + " = value', not '" + identText + ": value'";
						errorReporter->reportError(errLine, errColumn, errorMsg.c_str());

						// Save previous field if any
						if (!currentFieldName.empty()) {
							structConstruct->addFieldInit(currentFieldName, std::move(currentFieldNodes));
							currentFieldNodes.clear();
						}
						currentFieldName = identText;
						continue;
					}
					// It's '::' - treat current identifier as scoped
					// We already consumed first ':', now consume second ':' and continue as scoped identifier
					u8t_scanner_scan(scanner); // Consume second ':'
					// Now handle as scoped identifier - get the next identifier
					char32_t scopedToken = u8t_scanner_scan(scanner);
					if (scopedToken == U8T_IDENTIFIER) {
						size_t scopedN;
						const char* scopedText = u8t_scanner_token_text(scanner, &scopedN);
						std::string fullName = identText + "::" + std::string(scopedText);
						// Check if followed by '{' (nested struct construction)
						char32_t afterScoped = peekNextNonWhitespace(scanner, src);
						if (afterScoped == '{') {
							size_t nestedPos = u8t_scanner_token_start(scanner);
							u8t_scanner_scan(scanner); // Consume '{'
							AstNodeStructConstruction* nested =
									parseStructConstruction(fullName, {}, scanner, errorReporter, src, nestedPos);
							if (nested) {
								currentFieldNodes.push_back(nested);
							}
						} else {
							// Scoped identifier (function call or constant)
							AstNodeScopedIdentifier* scoped =
									new AstNodeScopedIdentifier(identText.c_str(), scopedText);
							setNodePosition(scoped, scanner, src);
							currentFieldNodes.push_back(scoped);
						}
					}
					continue;
				}

				// Not a field name, parse as expression element

				// Check for anonymous error literal: error { code = X message = Y }
				if (strcmp(text, "error") == 0) {
					char32_t errNextToken = peekNextNonWhitespace(scanner, src);
					if (errNextToken == '{') {
						size_t errorPos = u8t_scanner_token_start(scanner);
						u8t_scanner_scan(scanner); // Consume '{'
						AstNodeStructConstruction* errorConstruct =
								parseStructConstruction("__error__", {}, scanner, errorReporter, src, errorPos);
						if (errorConstruct) {
							currentFieldNodes.push_back(errorConstruct);
						}
						continue;
					}
				}

				// Check if it's a struct construction (nested) - StructName { ... } or StructName<T> { ... }
				// Only treat '<' as generic type args if identifier starts with uppercase (struct names)
				char32_t nextNonWs = peekNextNonWhitespace(scanner, src);
				std::vector<std::string> nestedTypeArgs;
				bool textLooksLikeStruct = text != nullptr && text[0] != '\0' && std::isupper(text[0]);
				if (nextNonWs == '<' && textLooksLikeStruct) {
					u8t_scanner_scan(scanner); // Consume '<'
					nestedTypeArgs = parseTypeArguments(scanner, src, errorReporter);
					nextNonWs = peekNextNonWhitespace(scanner, src);
				}
				if (nextNonWs == '{') {
					size_t nestedPos = u8t_scanner_token_start(scanner);
					std::string nestedName(text);
					u8t_scanner_scan(scanner); // Consume '{'
					AstNodeStructConstruction* nested =
							parseStructConstruction(nestedName, nestedTypeArgs, scanner, errorReporter, src, nestedPos);
					if (nested) {
						currentFieldNodes.push_back(nested);
					}
					continue;
				}

				// Check for scoped identifier (module::something)
				if (nextToken == ':') {
					// Already handled above for field names
				}

				// Check for boolean/null literals (true, false, Ok, Err, null)
				if (auto* boolNode = tryCreateBooleanLiteral(text, scanner, src)) {
					currentFieldNodes.push_back(boolNode);
					continue;
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

							// Check for nested struct: module::StructName { ... } or module::StructName<T> { ... }
							// Only treat '<' as generic type args if member name starts with uppercase (struct names)
							// and it's not followed by '=' (which would make it '<=' operator)
							char32_t afterMember = peekNextNonWhitespace(scanner, src);
							std::vector<std::string> scopedNestedTypeArgs;
							bool memberLooksLikeStruct3 =
									memberName != nullptr && memberName[0] != '\0' && std::isupper(memberName[0]);
							bool isGenericBracket4 = false;
							if (afterMember == '<' && memberLooksLikeStruct3) {
								// Check if this is '<=' operator by looking at character after '<'
								const char* currentPos4 = scanner->_str;
								// Skip whitespace and find '<'
								while (*currentPos4 &&
										(*currentPos4 == ' ' || *currentPos4 == '\t' || *currentPos4 == '\n')) {
									currentPos4++;
								}
								if (*currentPos4 == '<' && *(currentPos4 + 1) != '=') {
									isGenericBracket4 = true;
								}
							}
							if (isGenericBracket4) {
								u8t_scanner_scan(scanner); // Consume '<'
								scopedNestedTypeArgs = parseTypeArguments(scanner, src, errorReporter);
								afterMember = peekNextNonWhitespace(scanner, src);
							}
							if (afterMember == '{') {
								size_t nestedPos = u8t_scanner_token_start(scanner);
								u8t_scanner_scan(scanner); // Consume '{'
								AstNodeStructConstruction* nested = parseStructConstruction(
										fullName, scopedNestedTypeArgs, scanner, errorReporter, src, nestedPos);
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
								scoped->setPropagateOnError(true);
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
						ident->setPropagateOnError(true);
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
					size_t ampPosByte = fastCharToByteOffset(src, ampPos);
					fastLineColumn(src, ampPosByte, &line, &column);
					node->setPosition(line, column);
					currentFieldNodes.push_back(node);
				}
				// If not followed by identifier, silently ignore (error will be caught by semantic validator)
			} else if (token == '[') {
				// Array literal inside struct construction: [elem1 elem2 ...]
				size_t bracketPos = u8t_scanner_token_start(scanner);
				AstNodeArrayLiteral* arrNode = new AstNodeArrayLiteral();
				size_t arrLine, arrColumn;
				size_t bracketPosByte = fastCharToByteOffset(src, bracketPos);
				fastLineColumn(src, bracketPosByte, &arrLine, &arrColumn);
				arrNode->setPosition(arrLine, arrColumn);

				char32_t elemToken;
				while ((elemToken = u8t_scanner_scan(scanner)) != U8T_EOF) {
					if (elemToken == ']') {
						break;
					}
					size_t en;
					if (elemToken == U8T_INTEGER) {
						const char* etext = u8t_scanner_token_text(scanner, &en);
						AstNodeLiteral* lit = new AstNodeLiteral(etext, AstNodeLiteral::LiteralType::INTEGER);
						setNodePosition(lit, scanner, src);
						arrNode->addElement(lit);
					} else if (elemToken == U8T_FLOAT) {
						const char* etext = u8t_scanner_token_text(scanner, &en);
						AstNodeLiteral* lit = new AstNodeLiteral(etext, AstNodeLiteral::LiteralType::FLOAT);
						setNodePosition(lit, scanner, src);
						arrNode->addElement(lit);
					} else if (elemToken == U8T_STRING) {
						const char* etext = u8t_scanner_token_text(scanner, &en);
						AstNodeLiteral* lit = new AstNodeLiteral(etext, AstNodeLiteral::LiteralType::STRING);
						setNodePosition(lit, scanner, src);
						arrNode->addElement(lit);
					} else if (elemToken == U8T_IDENTIFIER) {
						const char* etext = u8t_scanner_token_text(scanner, &en);
						if (auto* boolNode = tryCreateBooleanLiteral(etext, scanner, src)) {
							arrNode->addElement(boolNode);
						} else {
							AstNodeIdentifier* ident = new AstNodeIdentifier(etext);
							setNodePosition(ident, scanner, src);
							arrNode->addElement(ident);
						}
					} else if (elemToken == '[') {
						// Nested array literal — recursively parse
						// Push back and let the outer loop handle it
						// For simplicity, create a nested array literal inline
						AstNodeArrayLiteral* nested = new AstNodeArrayLiteral();
						setNodePosition(nested, scanner, src);
						char32_t nestedToken;
						while ((nestedToken = u8t_scanner_scan(scanner)) != U8T_EOF) {
							if (nestedToken == ']') {
								break;
							}
							size_t nn;
							if (nestedToken == U8T_INTEGER) {
								const char* nt = u8t_scanner_token_text(scanner, &nn);
								AstNodeLiteral* lit = new AstNodeLiteral(nt, AstNodeLiteral::LiteralType::INTEGER);
								setNodePosition(lit, scanner, src);
								nested->addElement(lit);
							} else if (nestedToken == U8T_FLOAT) {
								const char* nt = u8t_scanner_token_text(scanner, &nn);
								AstNodeLiteral* lit = new AstNodeLiteral(nt, AstNodeLiteral::LiteralType::FLOAT);
								setNodePosition(lit, scanner, src);
								nested->addElement(lit);
							} else if (nestedToken == U8T_STRING) {
								const char* nt = u8t_scanner_token_text(scanner, &nn);
								AstNodeLiteral* lit = new AstNodeLiteral(nt, AstNodeLiteral::LiteralType::STRING);
								setNodePosition(lit, scanner, src);
								nested->addElement(lit);
							}
						}
						arrNode->addElement(nested);
					}
				}
				currentFieldNodes.push_back(arrNode);
			} else if (token == ',') {
				// Commas are not part of struct instantiation syntax
				size_t errPos = u8t_scanner_token_start(scanner);
				size_t errLine, errColumn;
				size_t errPosByte = fastCharToByteOffset(src, errPos);
				fastLineColumn(src, errPosByte, &errLine, &errColumn);
				errorReporter->reportError(errLine, errColumn,
						"Commas are not used in struct instantiation. "
						"Use whitespace or newlines to separate field initializers.");
			} else {
				// Handle operators
				IAstNode* opNode = tryParseOperatorAlias(token, scanner, src);
				if (opNode != nullptr) {
					currentFieldNodes.push_back(opNode);
				}
			}
		}

		// Clean up any remaining nodes that weren't added to a field
		// (can happen with malformed input)
		for (IAstNode* node : currentFieldNodes) {
			auto cleanup = std::unique_ptr<IAstNode>(node);
		}
		currentFieldNodes.clear();

		return structConstruct;
	}

	IAstNode* parseTypeAliasDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected type alias name after 'type'");
			synchronize(scanner);
			return nullptr;
		}

		const char* name = u8t_scanner_token_text(scanner, &n);
		std::string aliasName(name);

		// Expect '='
		token = u8t_scanner_scan(scanner);
		if (token != '=') {
			errorReporter->reportError(scanner, "Expected '=' after type alias name");
			synchronize(scanner);
			return nullptr;
		}

		// Parse the target type
		token = u8t_scanner_scan(scanner);
		std::string targetType;

		if (token == '[') {
			// Array type: []T
			token = u8t_scanner_scan(scanner);
			if (token == ']') {
				token = u8t_scanner_scan(scanner);
				if (token == U8T_IDENTIFIER) {
					const char* elemType = u8t_scanner_token_text(scanner, &n);
					targetType = "[]" + std::string(elemType);
				}
			}
		} else if (token == U8T_IDENTIFIER) {
			const char* typeName = u8t_scanner_token_text(scanner, &n);
			targetType = typeName;

			// Check for fn(...) type
			if (targetType == "fn") {
				char32_t peek = u8t_scanner_peek(scanner);
				if (peek == '(') {
					u8t_scanner_scan(scanner); // consume '('
					targetType = "fn(";
					bool needSpace = false;
					while (true) {
						char32_t ftok = u8t_scanner_scan(scanner);
						if (ftok == U8T_EOF || ftok == ')') {
							break;
						}
						if (ftok == '-') {
							char32_t next = u8t_scanner_peek(scanner);
							if (next == '-') {
								u8t_scanner_scan(scanner);
								if (needSpace) {
									targetType += " ";
								}
								targetType += "-- ";
								needSpace = false;
								continue;
							}
						}
						if (ftok == U8T_IDENTIFIER) {
							size_t fn;
							const char* ft = u8t_scanner_token_text(scanner, &fn);
							if (needSpace) {
								targetType += " ";
							}
							targetType += ft;
							needSpace = true;
						}
					}
					if (!targetType.empty() && targetType.back() == ' ') {
						targetType.pop_back();
					}
					targetType += ")";
				}
			}

			// Check for qualified name: module::Type
			if (targetType.find("fn(") == std::string::npos) {
				char32_t peek = u8t_scanner_peek(scanner);
				if (peek == ':') {
					u8t_scanner_scan(scanner);
					char32_t peek2 = u8t_scanner_peek(scanner);
					if (peek2 == ':') {
						u8t_scanner_scan(scanner);
						token = u8t_scanner_scan(scanner);
						if (token == U8T_IDENTIFIER) {
							const char* qualName = u8t_scanner_token_text(scanner, &n);
							targetType += "::";
							targetType += qualName;
						}
					}
				}
			}
		}

		if (targetType.empty()) {
			errorReporter->reportError(scanner, "Expected type after '=' in type alias");
			synchronize(scanner);
			return nullptr;
		}

		auto alias = std::make_unique<AstNodeTypeAlias>(aliasName, targetType, isPublic);
		setNodePosition(alias.get(), scanner, src);
		return alias.release();
	}

} // namespace Qd
