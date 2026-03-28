#include "ast_parse.h"

namespace Qd {

	/**
	 * Parse an anonymous function: fn (params -- outputs) { body }
	 * Called when 'fn' keyword is followed by '(' (no identifier name).
	 * The 'fn' keyword has already been consumed.
	 *
	 * Captures are detected automatically during semantic analysis - any variable
	 * referenced inside the anonymous function that is defined in an enclosing
	 * scope will be captured implicitly.
	 */
	IAstNode* parseAnonymousFunction(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		auto func = std::make_unique<AstNodeAnonymousFunction>();
		setNodePosition(func.get(), scanner, src);

		char32_t token = u8t_scanner_scan(scanner);
		if (token != '(') {
			errorReporter->reportError(scanner, "Expected '(' after 'fn' for anonymous function");
			return nullptr;
		}

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
						std::string paramTypeStr(paramType);
						// Check for qualified type name (module::Type)
						char32_t peek1 = u8t_scanner_peek(scanner);
						if (peek1 == ':') {
							u8t_scanner_scan(scanner); // consume first ':'
							char32_t peek2 = u8t_scanner_peek(scanner);
							if (peek2 == ':') {
								u8t_scanner_scan(scanner); // consume second ':'
								token = u8t_scanner_scan(scanner);
								if (token == U8T_IDENTIFIER) {
									const char* structName = u8t_scanner_token_text(scanner, &n);
									paramTypeStr = paramTypeStr + "::" + structName;
								}
							}
						}
						// Check for generic type parameters: Type<T> or Type<T, U>
						char32_t peekAngle = u8t_scanner_peek(scanner);
						if (peekAngle == '<') {
							u8t_scanner_scan(scanner); // consume '<'
							paramTypeStr += "<";
							int depth = 1;
							while (depth > 0) {
								token = u8t_scanner_scan(scanner);
								if (token == '<') {
									depth++;
									paramTypeStr += "<";
								} else if (token == '>') {
									depth--;
									paramTypeStr += ">";
								} else if (token == ',') {
									paramTypeStr += ",";
								} else if (token == U8T_IDENTIFIER) {
									paramTypeStr += std::string(u8t_scanner_token_text(scanner, &n));
								} else if (token == U8T_EOF) {
									break; // Prevent infinite loop on malformed input
								}
							}
						}
						AstNodeParameter* param = new AstNodeParameter(paramNameStr, paramTypeStr, isOutput);
						setNodePosition(param, scanner, src);
						param->setParent(func.get());
						if (isOutput) {
							func->addOutputParameter(param);
						} else {
							func->addInputParameter(param);
						}
					}
				} else if (isTypeName(paramNameStr)) {
					// Unnamed typed parameter (e.g., fn (i64 -- i64) { ... })
					AstNodeParameter* param = new AstNodeParameter("", paramNameStr, isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func.get());
					if (isOutput) {
						func->addOutputParameter(param);
					} else {
						func->addInputParameter(param);
					}
				} else if (paramPeek == '<') {
					// Unnamed generic type parameter (e.g., fn (Vec<i64> -- ) { ... })
					std::string typeStr = paramNameStr;
					u8t_scanner_scan(scanner); // consume '<'
					typeStr += "<";
					int depth = 1;
					while (depth > 0) {
						token = u8t_scanner_scan(scanner);
						if (token == '<') {
							depth++;
							typeStr += "<";
						} else if (token == '>') {
							depth--;
							typeStr += ">";
						} else if (token == ',') {
							typeStr += ",";
						} else if (token == U8T_IDENTIFIER) {
							typeStr += std::string(u8t_scanner_token_text(scanner, &n));
						} else if (token == U8T_EOF) {
							break;
						}
					}
					AstNodeParameter* param = new AstNodeParameter("", typeStr, isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func.get());
					if (isOutput) {
						func->addOutputParameter(param);
					} else {
						func->addInputParameter(param);
					}
				} else {
					// Untyped parameter - use empty string as type
					AstNodeParameter* param = new AstNodeParameter(paramNameStr, "", isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func.get());
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
			return nullptr;
		}

		// Parse the body
		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);
		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(func.get());
		func->setBody(body);

		return func.release();
	}

	IAstNode* parseFunctionDeclaration(
			u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src, bool isPublic) {
		// Check for receiver syntax: fn (receiver:Type) name(...) or fn (receiver:Type<T>) name(...)
		std::string receiverName;
		std::string receiverType;
		std::vector<std::string> receiverTypeParams;
		bool hasReceiver = false;

		char32_t token = u8t_scanner_scan(scanner);

		// If first token is '(', this is receiver syntax
		if (token == '(') {
			hasReceiver = true;

			// Parse receiver name
			token = u8t_scanner_scan(scanner);
			if (token != U8T_IDENTIFIER) {
				errorReporter->reportError(scanner, "Expected receiver name after '(' in method declaration");
				synchronize(scanner);
				return nullptr;
			}
			size_t n;
			receiverName = u8t_scanner_token_text(scanner, &n);

			// Expect ':'
			token = u8t_scanner_scan(scanner);
			if (token != ':') {
				errorReporter->reportError(scanner, "Expected ':' after receiver name in method declaration");
				synchronize(scanner);
				return nullptr;
			}

			// Parse receiver type (may be qualified like module::Type)
			token = u8t_scanner_scan(scanner);
			if (token != U8T_IDENTIFIER) {
				errorReporter->reportError(scanner, "Expected receiver type after ':' in method declaration");
				synchronize(scanner);
				return nullptr;
			}
			receiverType = u8t_scanner_token_text(scanner, &n);

			// Handle qualified types: module::Type
			while (true) {
				char32_t peek1 = u8t_scanner_peek(scanner);
				if (peek1 == ':') {
					u8t_scanner_scan(scanner); // consume first ':'
					char32_t peek2 = u8t_scanner_peek(scanner);
					if (peek2 == ':') {
						u8t_scanner_scan(scanner); // consume second ':'
						token = u8t_scanner_scan(scanner);
						if (token == U8T_IDENTIFIER) {
							receiverType += "::" + std::string(u8t_scanner_token_text(scanner, &n));
						} else {
							errorReporter->reportError(scanner, "Expected type name after '::' in receiver type");
							synchronize(scanner);
							return nullptr;
						}
					} else {
						break;
					}
				} else {
					break;
				}
			}

			// Handle generic type parameters: Type<T> or Type<T, U>
			char32_t peekAngle = u8t_scanner_peek(scanner);
			if (peekAngle == '<') {
				u8t_scanner_scan(scanner); // consume '<'
				while (true) {
					token = u8t_scanner_scan(scanner);
					if (token == '>') {
						break;
					}
					if (token == U8T_IDENTIFIER) {
						receiverTypeParams.push_back(std::string(u8t_scanner_token_text(scanner, &n)));
						char32_t peekComma = u8t_scanner_peek(scanner);
						if (peekComma == ',') {
							u8t_scanner_scan(scanner); // consume ','
						}
					} else if (token != ',') {
						errorReporter->reportError(scanner, "Expected type parameter or '>' in receiver type");
						synchronize(scanner);
						return nullptr;
					}
				}
			}

			// Expect ')'
			token = u8t_scanner_scan(scanner);
			if (token != ')') {
				errorReporter->reportError(scanner, "Expected ')' after receiver type in method declaration");
				synchronize(scanner);
				return nullptr;
			}

			// Now scan the actual function name
			token = u8t_scanner_scan(scanner);
		}

		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected function name after 'fn'");
			synchronize(scanner);
			return nullptr;
		}

		size_t n;
		const char* name = u8t_scanner_token_text(scanner, &n);
		auto func = std::make_unique<AstNodeFunctionDeclaration>(name, isPublic);
		setNodePosition(func.get(), scanner, src);

		// Set receiver if present
		if (hasReceiver) {
			func->setReceiver(receiverName, receiverType, receiverTypeParams);
		}

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
						std::string paramTypeStr(paramType);
						// Check for qualified type name (module::Type)
						char32_t peek1 = u8t_scanner_peek(scanner);
						if (peek1 == ':') {
							u8t_scanner_scan(scanner); // consume first ':'
							char32_t peek2 = u8t_scanner_peek(scanner);
							if (peek2 == ':') {
								u8t_scanner_scan(scanner); // consume second ':'
								token = u8t_scanner_scan(scanner);
								if (token == U8T_IDENTIFIER) {
									const char* structName = u8t_scanner_token_text(scanner, &n);
									paramTypeStr = paramTypeStr + "::" + structName;
								}
							}
						}
						// Check for generic type parameters: Type<T> or Type<T, U>
						char32_t peekAngle = u8t_scanner_peek(scanner);
						if (peekAngle == '<') {
							u8t_scanner_scan(scanner); // consume '<'
							paramTypeStr += "<";
							int depth = 1;
							while (depth > 0) {
								token = u8t_scanner_scan(scanner);
								if (token == '<') {
									depth++;
									paramTypeStr += "<";
								} else if (token == '>') {
									depth--;
									paramTypeStr += ">";
								} else if (token == ',') {
									paramTypeStr += ",";
								} else if (token == U8T_IDENTIFIER) {
									paramTypeStr += std::string(u8t_scanner_token_text(scanner, &n));
								} else if (token == U8T_EOF) {
									break; // Prevent infinite loop on malformed input
								}
							}
						}
						AstNodeParameter* param = new AstNodeParameter(paramNameStr, paramTypeStr, isOutput);
						setNodePosition(param, scanner, src);
						param->setParent(func.get());
						if (isOutput) {
							func->addOutputParameter(param);
						} else {
							func->addInputParameter(param);
						}
					}
				} else if (isTypeName(paramNameStr)) {
					// Unnamed typed parameter (e.g., fn foo(i64 f64 -- i64))
					AstNodeParameter* param = new AstNodeParameter("", paramNameStr, isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func.get());
					if (isOutput) {
						func->addOutputParameter(param);
					} else {
						func->addInputParameter(param);
					}
				} else if (paramPeek == '<') {
					// Unnamed generic type parameter (e.g., fn foo(Vec<i64> -- ))
					std::string typeStr = paramNameStr;
					u8t_scanner_scan(scanner); // consume '<'
					typeStr += "<";
					int depth = 1;
					while (depth > 0) {
						token = u8t_scanner_scan(scanner);
						if (token == '<') {
							depth++;
							typeStr += "<";
						} else if (token == '>') {
							depth--;
							typeStr += ">";
						} else if (token == ',') {
							typeStr += ",";
						} else if (token == U8T_IDENTIFIER) {
							typeStr += std::string(u8t_scanner_token_text(scanner, &n));
						} else if (token == U8T_EOF) {
							break;
						}
					}
					AstNodeParameter* param = new AstNodeParameter("", typeStr, isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func.get());
					if (isOutput) {
						func->addOutputParameter(param);
					} else {
						func->addInputParameter(param);
					}
				} else {
					// Untyped parameter - use empty string as type
					AstNodeParameter* param = new AstNodeParameter(paramNameStr, "", isOutput);
					setNodePosition(param, scanner, src);
					param->setParent(func.get());
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
			body->setParent(func.get());
			func->setBody(body);
			synchronize(scanner);
			return func.release();
		}

		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		std::vector<IAstNode*> tempNodes;
		TempNodeGuard guard(tempNodes); // Auto-deletes unflushed nodes on scope exit
		bool sawColon = false;
		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection
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

				// Triple scope: extend existing ScopedIdentifier (module::Enum::Variant)
				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
					std::unique_ptr<IAstNode> existingOwner(tempNodes.back());
					tempNodes.pop_back();
					auto* existing = static_cast<AstNodeScopedIdentifier*>(existingOwner.get());
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* extraName = u8t_scanner_token_text(scanner, &n);
						std::string newName = existing->name() + "::" + extraName;
						AstNodeScopedIdentifier* extended = new AstNodeScopedIdentifier(existing->scope(), newName);
						setNodePosition(extended, scanner, src);
						tempNodes.push_back(extended);
					} else {
						// Put it back — unique_ptr must release ownership
						tempNodes.push_back(existingOwner.release());
					}
					continue;
				}

				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
					std::unique_ptr<IAstNode> scopeOwner(tempNodes.back());
					tempNodes.pop_back();
					auto* scope = static_cast<AstNodeIdentifier*>(scopeOwner.get());

					// Get the next identifier after ::
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* memberName = u8t_scanner_token_text(scanner, &n);
						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scope->name(), memberName);
						setNodePosition(scoped, scanner, src);
						// scopeOwner auto-deletes old node
						// Check for '!' or '?' suffix
						char32_t nextToken = u8t_scanner_peek(scanner);
						if (nextToken == '!') {
							u8t_scanner_scan(scanner); // Consume the '!'
							scoped->setAbortOnError(true);
						} else if (nextToken == '?') {
							u8t_scanner_scan(scanner); // Consume the '?'
							scoped->setPropagateOnError(true);
						}
						tempNodes.push_back(scoped);
					} else {
						// No identifier after ::, put tokens back
						tempNodes.push_back(scopeOwner.release());
						// Can't really handle this case properly without putback
					}
				}
				continue;
			}

			sawColon = (token == ':');
			if (sawColon) {
				continue; // Wait for next token to see if it's another colon
			}

			// Handle >>field (field set / write, Factor-style)
			if (token == '>') {
				char32_t nextChar = u8t_scanner_peek(scanner);
				if (nextChar == '>') {
					u8t_scanner_scan(scanner); // Consume second >
					char32_t fieldStart = u8t_scanner_peek(scanner);
					if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
							fieldStart == '_') {
						char32_t identToken = u8t_scanner_scan(scanner);
						if (identToken == U8T_IDENTIFIER) {
							std::string fieldName(u8t_scanner_token_text(scanner, &n));
							bool noReturn = (u8t_scanner_peek(scanner) == '!');
							if (noReturn) {
								u8t_scanner_scan(scanner);
							}
							AstNodeFieldSet* fieldSet = new AstNodeFieldSet("", fieldName, noReturn);
							setNodePosition(fieldSet, scanner, src);
							tempNodes.push_back(fieldSet);
							continue;
						}
					}
					errorReporter->reportError(scanner, "Expected field name after '>>'");
					continue;
				}
			}

			// Handle <<field (field read, Factor-style)
			if (token == '<') {
				char32_t nextChar = u8t_scanner_peek(scanner);
				if (nextChar == '<') {
					u8t_scanner_scan(scanner); // Consume second <
					char32_t fieldStart = u8t_scanner_peek(scanner);
					if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
							fieldStart == '_') {
						char32_t identToken = u8t_scanner_scan(scanner);
						if (identToken == U8T_IDENTIFIER) {
							const char* fieldName = u8t_scanner_token_text(scanner, &n);

							if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
								// We have: identifier <<field
								std::unique_ptr<IAstNode> varOwner(tempNodes.back());
								tempNodes.pop_back();
								auto* varIdent = static_cast<AstNodeIdentifier*>(varOwner.get());

								// Special handling for 'error <<field' - access global error struct
								std::string varName = varIdent->name();
								if (varName == "error") {
									varName = "__global_error__";
								}
								AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess(varName, fieldName);
								setNodePosition(fieldAccess, scanner, src);
								tempNodes.push_back(fieldAccess);
								// varOwner auto-deletes old node
							} else if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
								// Chained field access: previous <<field followed by <<field2
								AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
								setNodePosition(fieldAccess, scanner, src);
								tempNodes.push_back(fieldAccess);
							} else {
								// Stack-based field access: <<field after struct construction, function call, etc.
								AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess("", fieldName);
								setNodePosition(fieldAccess, scanner, src);
								tempNodes.push_back(fieldAccess);
							}
							continue;
						}
					}
					errorReporter->reportError(scanner, "Expected field name after '<<'");
					continue;
				}
			}

			// Handle $ string interpolation: $"hello {name}"
			if (token == '$') {
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_STRING) {
					size_t sn;
					const char* strText = u8t_scanner_token_text(scanner, &sn);
					std::string raw(strText);
					if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
						raw = raw.substr(1, raw.size() - 2);
					}
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();
					parseStringInterpolation(raw, scanner, src, tempNodes);
					continue;
				} else {
					// Not a string after $ — treat $ as unknown and process the consumed token
					IAstNode* extraNode = parseBlockStatement(nextToken, scanner, errorReporter, &n, src);
					if (extraNode) {
						tempNodes.push_back(extraNode);
					}
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
					errorReporter->reportError(
							scanner, "'while' has been removed; use 'loop' with 'if'/'break' instead");
					synchronize(scanner);
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
						errorReporter->reportError(scanner, "Function declarations not allowed inside blocks. "
															"Did you mean 'fn (...) { }' for an anonymous function?");
					}
				} else if (strcmp(text, "as") == 0) {
					// Parse type narrowing cast: 'as TypeName'
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						std::string typeName(u8t_scanner_token_text(scanner, &n));
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
									typeName += u8t_scanner_token_text(scanner, &n);
								} else {
									errorReporter->reportError(scanner, "Expected type name after '::'");
								}
							}
						}
						AstNodeAsCast* asCast = new AstNodeAsCast(typeName);
						setNodePosition(asCast, scanner, src);
						tempNodes.push_back(asCast);
					} else {
						errorReporter->reportError(scanner, "Expected type name after 'as'");
					}
				} else if (strcmp(text, "ctx") == 0) {
					// Parse ctx block
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					auto ctxStmt = std::make_unique<AstNodeCtx>();
					setNodePosition(ctxStmt.get(), scanner, src);
					token = u8t_scanner_scan(scanner);

					// ctx requires a block
					if (token != '{') {
						errorReporter->reportError(scanner, "Expected '{' after 'ctx'");
					} else {
						// Parse ctx block inline
						// ctx blocks can contain control flow statements
						std::vector<IAstNode*> ctxTempNodes;
						TempNodeGuard ctxGuard(ctxTempNodes);
						size_t ctxSlashPos = SIZE_MAX;
						bool ctxSawColon = false;

						while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
							// Handle comments
							AstNodeComment* ctxComment = parseComment(scanner, src, ctxSlashPos, token);
							if (ctxComment != nullptr) {
								ctxSlashPos = SIZE_MAX;
								for (auto* node : ctxTempNodes) {
									node->setParent(ctxStmt.get());
									ctxStmt->addChild(node);
								}
								ctxTempNodes.clear();
								ctxComment->setParent(ctxStmt.get());
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

							// Handle >>field / >>field! (field set / write)
							if (token == '>') {
								char32_t nextChar = u8t_scanner_peek(scanner);
								if (nextChar == '>') {
									u8t_scanner_scan(scanner);
									char32_t fieldStart = u8t_scanner_peek(scanner);
									if ((fieldStart >= 'a' && fieldStart <= 'z') ||
											(fieldStart >= 'A' && fieldStart <= 'Z') || fieldStart == '_') {
										char32_t identToken = u8t_scanner_scan(scanner);
										if (identToken == U8T_IDENTIFIER) {
											std::string fieldName(u8t_scanner_token_text(scanner, &n));
											bool noReturn = (u8t_scanner_peek(scanner) == '!');
											if (noReturn) {
												u8t_scanner_scan(scanner);
											}
											AstNodeFieldSet* fieldSet = new AstNodeFieldSet("", fieldName, noReturn);
											setNodePosition(fieldSet, scanner, src);
											ctxTempNodes.push_back(fieldSet);
											continue;
										}
									}
									errorReporter->reportError(scanner, "Expected field name after '>>'");
									continue;
								}
							}

							// Handle <<field (field read)
							if (token == '<') {
								char32_t nextChar = u8t_scanner_peek(scanner);
								if (nextChar == '<') {
									u8t_scanner_scan(scanner);
									char32_t fieldStart = u8t_scanner_peek(scanner);
									if ((fieldStart >= 'a' && fieldStart <= 'z') ||
											(fieldStart >= 'A' && fieldStart <= 'Z') || fieldStart == '_') {
										char32_t identToken = u8t_scanner_scan(scanner);
										if (identToken == U8T_IDENTIFIER) {
											const char* fieldName = u8t_scanner_token_text(scanner, &n);
											if (!ctxTempNodes.empty() &&
													ctxTempNodes.back()->type() == IAstNode::Type::IDENTIFIER) {
												std::unique_ptr<IAstNode> varOwner(ctxTempNodes.back());
												ctxTempNodes.pop_back();
												auto* varIdent = static_cast<AstNodeIdentifier*>(varOwner.get());
												std::string varName = varIdent->name();
												if (varName == "error") {
													varName = "__global_error__";
												}
												AstNodeFieldAccess* fa = new AstNodeFieldAccess(varName, fieldName);
												setNodePosition(fa, scanner, src);
												ctxTempNodes.push_back(fa);
												// varOwner auto-deletes old node
											} else if (!ctxTempNodes.empty() &&
													   ctxTempNodes.back()->type() == IAstNode::Type::FIELD_ACCESS) {
												AstNodeFieldAccess* fa = new AstNodeFieldAccess("", fieldName);
												setNodePosition(fa, scanner, src);
												ctxTempNodes.push_back(fa);
											} else {
												AstNodeFieldAccess* fa = new AstNodeFieldAccess("", fieldName);
												setNodePosition(fa, scanner, src);
												ctxTempNodes.push_back(fa);
											}
											continue;
										}
									}
									errorReporter->reportError(scanner, "Expected field name after '<<'");
									continue;
								}
							}

							// Check if this token is an "else" or "fn" keyword
							if (token == U8T_IDENTIFIER) {
								const char* tokenText = u8t_scanner_token_text(scanner, &n);
								if (strcmp(tokenText, "else") == 0) {
									// Flush ctxTempNodes before handling else
									for (auto* node : ctxTempNodes) {
										node->setParent(ctxStmt.get());
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
								} else if (strcmp(tokenText, "fn") == 0) {
									// Anonymous function: fn (params -- outputs) { body }
									char32_t nextToken = peekNextNonWhitespace(scanner, src);
									if (nextToken == '(') {
										IAstNode* anonFunc = parseAnonymousFunction(scanner, errorReporter, src);
										if (anonFunc) {
											ctxTempNodes.push_back(anonFunc);
										}
										continue;
									} else {
										errorReporter->reportError(scanner,
												"Function declarations not allowed inside blocks. "
												"Did you mean 'fn (...) { }' for an anonymous function?");
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
							node->setParent(ctxStmt.get());
							ctxStmt->addChild(node);
						}
						ctxTempNodes.clear(); // Prevent guard from double-deleting

						ctxStmt->setParent(body);
						body->addChild(ctxStmt.release());
					}
					continue; // Skip fallthrough after ctx parsing
				} else {
					// Handle boolean/result constants (true, false, Ok, Err)
					if (auto* boolNode = tryCreateBooleanLiteral(text, scanner, src)) {
						tempNodes.push_back(boolNode);
					} else if (isBuiltInInstruction(text)) {
						std::string instrName(text);
						// Check for generic type parameter: instruction<Type> or instruction<module::Type>
						// Use peekNextChar (no whitespace skip) to distinguish make<T> from len <
						char32_t nextCh = peekNextChar(scanner, src);
						if (nextCh == '<') {
							u8t_scanner_scan(scanner); // Consume '<'
							char32_t typeToken = u8t_scanner_scan(scanner);
							if (typeToken == U8T_IDENTIFIER) {
								std::string typeParam = u8t_scanner_token_text(scanner, &n);

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
											typeParam += "::";
											typeParam += qualName;
										} else {
											errorReporter->reportError(scanner, "Expected struct name after '::'");
										}
									}
								}

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

						// Check for anonymous error literal: error { code = X message = Y }
						if (identName == "error") {
							char32_t errNextToken = peekNextNonWhitespace(scanner, src);
							if (errNextToken == '{') {
								u8t_scanner_scan(scanner); // Consume '{'
								// Parse as anonymous error struct using special name __error__
								AstNodeStructConstruction* errorConstruct =
										parseStructConstruction("__error__", {}, scanner, errorReporter, src, identPos);
								if (errorConstruct) {
									tempNodes.push_back(errorConstruct);
								}
								continue;
							} else if (errNextToken == '@') {
								// error @field - access global error struct
								u8t_scanner_scan(scanner); // Consume '@'
								char32_t fieldToken = u8t_scanner_scan(scanner);
								if (fieldToken == U8T_IDENTIFIER) {
									const char* fieldName = u8t_scanner_token_text(scanner, &n);
									// Create special field access for global error
									AstNodeFieldAccess* errorField =
											new AstNodeFieldAccess("__global_error__", fieldName);
									setNodePosition(errorField, scanner, src);
									tempNodes.push_back(errorField);
								} else {
									errorReporter->reportError(scanner, "Expected field name after 'error @'");
								}
								continue;
							}
						}

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

									// Check for struct construction: module::StructName { ... } or
									// module::StructName<T> { ... } Only treat '<' as generic type args if member name
									// starts with uppercase (struct names) and it's not followed by '=' (which would
									// make it '<=' operator)
									char32_t afterMember = peekNextNonWhitespace(scanner, src);
									std::vector<std::string> typeArgs;
									bool memberLooksLikeStruct2 = memberName != nullptr && memberName[0] != '\0' &&
																  std::isupper(memberName[0]);
									bool isGenericBracket2 = false;
									if (afterMember == '<' && memberLooksLikeStruct2) {
										// Check if this is '<=' operator by looking at character after '<'
										const char* currentPos2 = scanner->_str;
										// Skip whitespace and find '<'
										while (*currentPos2 &&
												(*currentPos2 == ' ' || *currentPos2 == '\t' || *currentPos2 == '\n')) {
											currentPos2++;
										}
										if (*currentPos2 == '<' && *(currentPos2 + 1) != '=') {
											isGenericBracket2 = true;
										}
									}
									if (isGenericBracket2) {
										u8t_scanner_scan(scanner); // Consume '<'
										typeArgs = parseTypeArguments(scanner, src, errorReporter);
										afterMember = peekNextNonWhitespace(scanner, src);
									}
									if (afterMember == '{') {
										u8t_scanner_scan(scanner); // Consume '{'
										AstNodeStructConstruction* structConstruct = parseStructConstruction(
												fullName, typeArgs, scanner, errorReporter, src, identPos);
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
										scoped->setPropagateOnError(true);
									}
									tempNodes.push_back(scoped);
									continue;
								}
							}
						}

						// Check for struct construction: StructName { ... } or StructName<T> { ... }
						// Only treat '<' as generic type args if identifier starts with uppercase (struct names)
						// and it's not followed by '=' (which would make it '<=' operator)
						nextToken = peekNextNonWhitespace(scanner, src);
						std::vector<std::string> typeArgs;
						bool looksLikeStructName = !identName.empty() && std::isupper(identName[0]);
						bool isGenericBracket3 = false;
						if (nextToken == '<' && looksLikeStructName) {
							// Check if this is '<=' operator by looking at character after '<'
							const char* currentPos3 = scanner->_str;
							// Skip whitespace and find '<'
							while (*currentPos3 &&
									(*currentPos3 == ' ' || *currentPos3 == '\t' || *currentPos3 == '\n')) {
								currentPos3++;
							}
							if (*currentPos3 == '<' && *(currentPos3 + 1) != '=') {
								isGenericBracket3 = true;
							}
						}
						if (isGenericBracket3) {
							u8t_scanner_scan(scanner); // Consume '<'
							typeArgs = parseTypeArguments(scanner, src, errorReporter);
							nextToken = peekNextNonWhitespace(scanner, src);
						}
						if (nextToken == '{') {
							u8t_scanner_scan(scanner); // Consume '{'
							AstNodeStructConstruction* structConstruct =
									parseStructConstruction(identName, typeArgs, scanner, errorReporter, src, identPos);
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
							id->setPropagateOnError(true);
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
				size_t tokenEndByte = fastCharToByteOffset(src, tokenEndChar);
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
						size_t tokenStartByte = fastCharToByteOffset(src, tokenStart);
						fastLineColumn(src, tokenStartByte, &line, &column);
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
					// Handle '!' as fallible call marker on previous node
					// If previous node is an identifier or instruction, mark it as a fallible call
					if (!tempNodes.empty()) {
						IAstNode* prevNode = tempNodes.back();
						if (prevNode->type() == IAstNode::Type::IDENTIFIER) {
							static_cast<AstNodeIdentifier*>(prevNode)->setAbortOnError(true);
						} else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
							static_cast<AstNodeInstruction*>(prevNode)->setAbortOnError(true);
						} else if (prevNode->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
							static_cast<AstNodeScopedIdentifier*>(prevNode)->setAbortOnError(true);
						} else {
							// Fall back to creating 'not' instruction for other cases
							AstNodeInstruction* instr = new AstNodeInstruction("not");
							setNodePosition(instr, scanner, src);
							tempNodes.push_back(instr);
						}
					} else {
						// No previous node, treat as 'not' instruction
						AstNodeInstruction* instr = new AstNodeInstruction("not");
						setNodePosition(instr, scanner, src);
						tempNodes.push_back(instr);
					}
				}
			} else if (token == '?') {
				// Handle '?' as error propagation marker on previous node
				if (!tempNodes.empty()) {
					IAstNode* prevNode = tempNodes.back();
					if (prevNode->type() == IAstNode::Type::IDENTIFIER) {
						static_cast<AstNodeIdentifier*>(prevNode)->setPropagateOnError(true);
					} else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
						static_cast<AstNodeInstruction*>(prevNode)->setPropagateOnError(true);
					} else if (prevNode->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
						static_cast<AstNodeScopedIdentifier*>(prevNode)->setPropagateOnError(true);
					}
				}
			} else if (token == '&') {
				// Handle '&' for function pointer references
				size_t ampPos = u8t_scanner_token_start(scanner);
				char32_t nextToken = u8t_scanner_scan(scanner);
				if (nextToken == U8T_IDENTIFIER) {
					const char* functionName = u8t_scanner_token_text(scanner, &n);
					AstNodeFunctionPointerReference* funcPtr = new AstNodeFunctionPointerReference(functionName);
					size_t line, column;
					fastLineColumn(src, ampPos, &line, &column);
					funcPtr->setPosition(line, column);
					tempNodes.push_back(funcPtr);
				} else {
					errorReporter->reportError(scanner, "Expected function name after '&'");
				}
			} else if (token == '[') {
				// Handle array literal: [elem1 elem2 ...]
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
		tempNodes.clear(); // Prevent guard from double-deleting

		body->setParent(func.get());
		func->setBody(body);

		return func.release();
	}

	IAstNode* parseTestDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
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

		auto test = std::make_unique<AstNodeTest>(testName);
		setNodePosition(test.get(), scanner, src);

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after test name");
			synchronize(scanner);
			return nullptr;
		}

		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(test.get());
		test->setBody(body);

		return test.release();
	}

} // namespace Qd
