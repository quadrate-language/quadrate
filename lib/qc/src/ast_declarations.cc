#include "ast_parse.h"

namespace Qd {

	// Try to parse a fn(...) type annotation.
	// Called after scanning an identifier that equals "fn".
	// If the next token is '(', consumes until matching ')' and returns "fn(i64 -- i64)" etc.
	// Otherwise returns empty string (nothing consumed).
	static std::string tryParseFnType(u8t_scanner* scanner) {
		char32_t peek = u8t_scanner_peek(scanner);
		if (peek != '(') {
			return "";
		}

		u8t_scanner_scan(scanner); // consume '('
		std::string typeStr = "fn(";
		bool needSpace = false;

		while (true) {
			char32_t tok = u8t_scanner_scan(scanner);
			if (tok == U8T_EOF || tok == ')') {
				break;
			}

			if (tok == '-') {
				char32_t next = u8t_scanner_peek(scanner);
				if (next == '-') {
					u8t_scanner_scan(scanner); // consume second '-'
					if (needSpace) {
						typeStr += " ";
					}
					typeStr += "-- ";
					needSpace = false;
					continue;
				}
			}

			if (tok == U8T_IDENTIFIER) {
				size_t n;
				const char* text = u8t_scanner_token_text(scanner, &n);
				if (needSpace) {
					typeStr += " ";
				}
				typeStr += text;
				needSpace = true;
			}
		}
		// Trim trailing space
		if (!typeStr.empty() && typeStr.back() == ' ') {
			typeStr.pop_back();
		}
		typeStr += ")";
		return typeStr;
	}

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
					if (token == '[') {
						// Array type: []T
						token = u8t_scanner_scan(scanner);
						if (token == ']') {
							token = u8t_scanner_scan(scanner);
							if (token == U8T_IDENTIFIER) {
								const char* elemType = u8t_scanner_token_text(scanner, &n);
								std::string paramTypeStr = "[]" + std::string(elemType);
								AstNodeParameter* param = new AstNodeParameter(paramNameStr, paramTypeStr, isOutput);
								setNodePosition(param, scanner, src);
								param->setParent(func.get());
								if (isOutput) {
									func->addOutputParameter(param);
								} else {
									func->addInputParameter(param);
								}
							}
						}
					} else if (token == U8T_IDENTIFIER) {
						const char* paramType = u8t_scanner_token_text(scanner, &n);
						std::string paramTypeStr(paramType);
						// Check for fn(...) type: fn(i64 -- i64)
						if (paramTypeStr == "fn") {
							std::string fnType = tryParseFnType(scanner);
							if (!fnType.empty()) {
								AstNodeParameter* param = new AstNodeParameter(paramNameStr, fnType, isOutput);
								setNodePosition(param, scanner, src);
								param->setParent(func.get());
								if (isOutput) {
									func->addOutputParameter(param);
								} else {
									func->addInputParameter(param);
								}
								continue;
							}
						}
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
			} else if (token == '[') {
				// Unnamed array type parameter: []T
				token = u8t_scanner_scan(scanner);
				if (token == ']') {
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* elemType = u8t_scanner_token_text(scanner, &n);
						std::string typeStr = "[]" + std::string(elemType);
						AstNodeParameter* param = new AstNodeParameter("", typeStr, isOutput);
						setNodePosition(param, scanner, src);
						param->setParent(func.get());
						if (isOutput) {
							func->addOutputParameter(param);
						} else {
							func->addInputParameter(param);
						}
					}
				}
			} else if (token == U8T_IDENTIFIER) {
				// Check for unnamed fn(...) type parameter
				const char* typeName = u8t_scanner_token_text(scanner, &n);
				if (strcmp(typeName, "fn") == 0) {
					std::string fnType = tryParseFnType(scanner);
					if (!fnType.empty()) {
						AstNodeParameter* param = new AstNodeParameter("", fnType, isOutput);
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
					if (token == '[') {
						// Array type: []T
						token = u8t_scanner_scan(scanner);
						if (token == ']') {
							token = u8t_scanner_scan(scanner);
							if (token == U8T_IDENTIFIER) {
								const char* elemType = u8t_scanner_token_text(scanner, &n);
								std::string paramTypeStr = "[]" + std::string(elemType);
								AstNodeParameter* param = new AstNodeParameter(paramNameStr, paramTypeStr, isOutput);
								setNodePosition(param, scanner, src);
								param->setParent(func.get());
								if (isOutput) {
									func->addOutputParameter(param);
								} else {
									func->addInputParameter(param);
								}
							}
						}
					} else if (token == U8T_IDENTIFIER) {
						const char* paramType = u8t_scanner_token_text(scanner, &n);
						std::string paramTypeStr(paramType);
						// Check for fn(...) type: fn(i64 -- i64)
						if (paramTypeStr == "fn") {
							std::string fnType = tryParseFnType(scanner);
							if (!fnType.empty()) {
								AstNodeParameter* param = new AstNodeParameter(paramNameStr, fnType, isOutput);
								setNodePosition(param, scanner, src);
								param->setParent(func.get());
								if (isOutput) {
									func->addOutputParameter(param);
								} else {
									func->addInputParameter(param);
								}
								continue;
							}
						}
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
			} else if (token == '[') {
				// Unnamed array type parameter: []T
				token = u8t_scanner_scan(scanner);
				if (token == ']') {
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* elemType = u8t_scanner_token_text(scanner, &n);
						std::string typeStr = "[]" + std::string(elemType);
						AstNodeParameter* param = new AstNodeParameter("", typeStr, isOutput);
						setNodePosition(param, scanner, src);
						param->setParent(func.get());
						if (isOutput) {
							func->addOutputParameter(param);
						} else {
							func->addInputParameter(param);
						}
					}
				}
			} else if (token == U8T_IDENTIFIER) {
				// Check for unnamed fn(...) type parameter
				const char* typeName = u8t_scanner_token_text(scanner, &n);
				if (strcmp(typeName, "fn") == 0) {
					std::string fnType = tryParseFnType(scanner);
					if (!fnType.empty()) {
						AstNodeParameter* param = new AstNodeParameter("", fnType, isOutput);
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

		// Parse function body — uses shared block parser with function context flag
		parseBlockBody(body, scanner, errorReporter, src, true);
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
