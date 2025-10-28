#include <cstring>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_block.h>
#include <qc/ast_node_break.h>
#include <qc/ast_node_constant.h>
#include <qc/ast_node_continue.h>
#include <qc/ast_node_defer.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_instruction.h>
#include <qc/ast_node_label.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_parameter.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_return.h>
#include <qc/ast_node_scoped.h>
#include <qc/ast_node_switch.h>
#include <qc/ast_node_use.h>
#include <qc/error_reporter.h>
#include <u8t/scanner.h>
#include <vector>

namespace Qd {
	// Helper function to check if an identifier is a built-in instruction
	static bool isBuiltInInstruction(const char* name) {
		static const char* instructions[] = {"*", "+", "-", ".", "/", "abs", "add", "div", "dup", "mul", "print",
				"prints", "printsv", "printv", "rot", "sq", "sub", "swap"};
		static const size_t count = sizeof(instructions) / sizeof(instructions[0]);

		for (size_t i = 0; i < count; i++) {
			if (strcmp(name, instructions[i]) == 0) {
				return true;
			}
		}
		return false;
	}

	static IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter);
	static IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter);
	static IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter);

	// Helper to synchronize parser after an error
	// Skips tokens until a synchronization point is found
	[[maybe_unused]] static void synchronize(u8t_scanner* scanner) {
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
						strcmp(text, "if") == 0 || strcmp(text, "for") == 0 || strcmp(text, "switch") == 0 ||
						strcmp(text, "return") == 0) {
					return;
				}
			}
		}
	}

	// Helper to parse a single statement/expression token
	// Returns nullptr if token was a control keyword that was handled
	// Returns a node if it's a literal or identifier
	[[maybe_unused]] static IAstNode* parseSimpleToken(char32_t token, u8t_scanner* scanner, size_t* n) {
		if (token == U8T_INTEGER) {
			const char* text = u8t_scanner_token_text(scanner, n);
			return new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Integer);
		} else if (token == U8T_FLOAT) {
			const char* text = u8t_scanner_token_text(scanner, n);
			return new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Float);
		} else if (token == U8T_STRING) {
			const char* text = u8t_scanner_token_text(scanner, n);
			return new AstNodeLiteral(text, AstNodeLiteral::LiteralType::String);
		} else if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, n);
			if (isBuiltInInstruction(text)) {
				return new AstNodeInstruction(text);
			}
			return new AstNodeIdentifier(text);
		} else if (token == '.') {
			// Handle '.' as alias for 'print' (Forth-style)
			return new AstNodeInstruction(".");
		} else if (token == '/') {
			// Handle '/' as alias for 'div'
			return new AstNodeInstruction("/");
		} else if (token == '*') {
			// Handle '*' as alias for 'mul'
			return new AstNodeInstruction("*");
		} else if (token == '+') {
			// Handle '+' as alias for 'add'
			return new AstNodeInstruction("+");
		} else if (token == '-') {
			// Handle '-' as alias for 'sub'
			return new AstNodeInstruction("-");
		}
		return nullptr;
	}

	// Helper to parse statements inside a block (handles if, break, continue, nested structures)
	// Returns a node that should be added to the parent, or nullptr
	// allowControlFlow: if false, only allows break/continue but not if/for/switch
	static IAstNode* parseBlockStatement(char32_t token, u8t_scanner* scanner, ErrorReporter* errorReporter, size_t* n,
			bool allowControlFlow = true) {
		if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, n);

			// break and continue are always allowed
			if (strcmp(text, "break") == 0) {
				return new AstNodeBreak();
			} else if (strcmp(text, "continue") == 0) {
				return new AstNodeContinue();
			}

			if (allowControlFlow) {
				if (strcmp(text, "if") == 0) {
					return parseIfStatement(scanner, errorReporter);
				} else if (strcmp(text, "for") == 0) {
					return parseForStatement(scanner, errorReporter);
				} else if (strcmp(text, "switch") == 0) {
					return parseSwitchStatement(scanner, errorReporter);
				}
			}

			if (isBuiltInInstruction(text)) {
				return new AstNodeInstruction(text);
			}
			return new AstNodeIdentifier(text);
		}

		return parseSimpleToken(token, scanner, n);
	}

	Ast::~Ast() {
		if (mRoot) {
			delete mRoot;
			mRoot = nullptr;
		}
	}

	static IAstNode* parseFunctionDeclaration(u8t_scanner* scanner, ErrorReporter* errorReporter) {
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected function name after 'fn'");
			synchronize(scanner);
			return nullptr;
		}

		size_t n;
		const char* name = u8t_scanner_token_text(scanner, &n);
		AstNodeFunctionDeclaration* func = new AstNodeFunctionDeclaration(name);

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
					param->setParent(func);
					if (isOutput) {
						func->addOutputParameter(param);
					} else {
						func->addInputParameter(param);
					}
				}
			}
		}

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after function signature");
			// Recovery: create empty body and return partial function
			AstNodeBlock* body = new AstNodeBlock();
			body->setParent(func);
			func->setBody(body);
			synchronize(scanner);
			return func;
		}

		AstNodeBlock* body = new AstNodeBlock();

		std::vector<IAstNode*> tempNodes;
		bool sawColon = false;
		bool sawSlash = false;

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// Handle // line comments
			if (sawSlash && token == '/') {
				sawSlash = false;
				// Skip until end of line
				char32_t c;
				while ((c = u8t_scanner_peek(scanner)) != 0 && c != '\n' && c != '\r') {
					u8t_scanner_scan(scanner);
				}
				continue;
			}

			// Handle /* block comments */
			if (sawSlash && token == '*') {
				sawSlash = false;
				// Skip until */
				bool foundStar = false;
				while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
					if (foundStar && token == '/') {
						break;
					}
					foundStar = (token == '*');
				}
				continue;
			}

			// If we saw a slash but it wasn't a comment, it's a division operator
			if (sawSlash) {
				sawSlash = false;
				// Add division instruction to tempNodes
				AstNodeInstruction* divInstr = new AstNodeInstruction("/");
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
				if (!tempNodes.empty() && tempNodes.back()->type() == IAstNode::Type::Identifier) {
					AstNodeIdentifier* scope = static_cast<AstNodeIdentifier*>(tempNodes.back());
					tempNodes.pop_back();

					// Get the next identifier after ::
					token = u8t_scanner_scan(scanner);
					if (token == U8T_IDENTIFIER) {
						const char* memberName = u8t_scanner_token_text(scanner, &n);
						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scope->name(), memberName);
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
					IAstNode* forStmt = parseForStatement(scanner, errorReporter);
					if (forStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						forStmt->setParent(body);
						body->addChild(forStmt);
					}
				} else if (strcmp(text, "if") == 0) {
					IAstNode* ifStmt = parseIfStatement(scanner, errorReporter);
					if (ifStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						// Check for else clause
						char32_t peek = u8t_scanner_peek(scanner);
						if (peek != 0) {
							token = u8t_scanner_scan(scanner);
							if (token == U8T_IDENTIFIER) {
								const char* elseText = u8t_scanner_token_text(scanner, &n);
								if (strcmp(elseText, "else") == 0) {
									// Parse else block - must have {
									token = u8t_scanner_scan(scanner);
									if (token != '{') {
										errorReporter->reportError(scanner, "Expected '{' after 'else'");
										// Else without block - just ignore the else keyword and continue
										// Put the token back into tempNodes so it gets processed
										if (token == U8T_INTEGER) {
											const char* tokenText = u8t_scanner_token_text(scanner, &n);
											AstNodeLiteral* lit =
													new AstNodeLiteral(tokenText, AstNodeLiteral::LiteralType::Integer);
											tempNodes.push_back(lit);
										} else if (token == U8T_FLOAT) {
											const char* tokenText = u8t_scanner_token_text(scanner, &n);
											AstNodeLiteral* lit =
													new AstNodeLiteral(tokenText, AstNodeLiteral::LiteralType::Float);
											tempNodes.push_back(lit);
										} else if (token == U8T_STRING) {
											const char* tokenText = u8t_scanner_token_text(scanner, &n);
											AstNodeLiteral* lit =
													new AstNodeLiteral(tokenText, AstNodeLiteral::LiteralType::String);
											tempNodes.push_back(lit);
										} else if (token == U8T_IDENTIFIER) {
											const char* tokenText = u8t_scanner_token_text(scanner, &n);
											IAstNode* id =
													isBuiltInInstruction(tokenText)
															? static_cast<IAstNode*>(new AstNodeInstruction(tokenText))
															: static_cast<IAstNode*>(new AstNodeIdentifier(tokenText));
											tempNodes.push_back(id);
										}
									} else {
										AstNodeBlock* elseBody = new AstNodeBlock();

										while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
											if (token == '}') {
												break;
											}

											if (token == U8T_IDENTIFIER) {
												const char* elseBodyText = u8t_scanner_token_text(scanner, &n);

												if (strcmp(elseBodyText, "if") == 0) {
													IAstNode* nestedIf = parseIfStatement(scanner, errorReporter);
													if (nestedIf) {
														nestedIf->setParent(elseBody);
														elseBody->addChild(nestedIf);
													}
												} else if (strcmp(elseBodyText, "break") == 0) {
													AstNodeBreak* breakStmt = new AstNodeBreak();
													breakStmt->setParent(elseBody);
													elseBody->addChild(breakStmt);
												} else if (strcmp(elseBodyText, "continue") == 0) {
													AstNodeContinue* continueStmt = new AstNodeContinue();
													continueStmt->setParent(elseBody);
													elseBody->addChild(continueStmt);
												} else {
													IAstNode* id =
															isBuiltInInstruction(elseBodyText)
																	? static_cast<IAstNode*>(
																			  new AstNodeInstruction(elseBodyText))
																	: static_cast<IAstNode*>(
																			  new AstNodeIdentifier(elseBodyText));
													id->setParent(elseBody);
													elseBody->addChild(id);
												}
											} else if (token == U8T_INTEGER) {
												const char* elseBodyText = u8t_scanner_token_text(scanner, &n);
												AstNodeLiteral* lit = new AstNodeLiteral(
														elseBodyText, AstNodeLiteral::LiteralType::Integer);
												lit->setParent(elseBody);
												elseBody->addChild(lit);
											} else if (token == U8T_FLOAT) {
												const char* elseBodyText = u8t_scanner_token_text(scanner, &n);
												AstNodeLiteral* lit = new AstNodeLiteral(
														elseBodyText, AstNodeLiteral::LiteralType::Float);
												lit->setParent(elseBody);
												elseBody->addChild(lit);
											} else if (token == U8T_STRING) {
												const char* elseBodyText = u8t_scanner_token_text(scanner, &n);
												AstNodeLiteral* lit = new AstNodeLiteral(
														elseBodyText, AstNodeLiteral::LiteralType::String);
												lit->setParent(elseBody);
												elseBody->addChild(lit);
											}
										}

										elseBody->setParent(ifStmt);
										static_cast<AstNodeIfStatement*>(ifStmt)->setElseBody(elseBody);
									}
								} else {
									// Not "else", so this is a new statement - treat it as identifier
									IAstNode* id = isBuiltInInstruction(elseText)
														   ? static_cast<IAstNode*>(new AstNodeInstruction(elseText))
														   : static_cast<IAstNode*>(new AstNodeIdentifier(elseText));
									tempNodes.push_back(id);
								}
							} else {
								// Not an identifier after if, need to handle this token in main loop
								// We've already consumed it, so we need to process it here
								if (token == U8T_INTEGER) {
									const char* tokenText = u8t_scanner_token_text(scanner, &n);
									AstNodeLiteral* lit =
											new AstNodeLiteral(tokenText, AstNodeLiteral::LiteralType::Integer);
									tempNodes.push_back(lit);
								} else if (token == U8T_FLOAT) {
									const char* tokenText = u8t_scanner_token_text(scanner, &n);
									AstNodeLiteral* lit =
											new AstNodeLiteral(tokenText, AstNodeLiteral::LiteralType::Float);
									tempNodes.push_back(lit);
								} else if (token == U8T_STRING) {
									const char* tokenText = u8t_scanner_token_text(scanner, &n);
									AstNodeLiteral* lit =
											new AstNodeLiteral(tokenText, AstNodeLiteral::LiteralType::String);
									tempNodes.push_back(lit);
								}
							}
						}

						ifStmt->setParent(body);
						body->addChild(ifStmt);
					}
				} else if (strcmp(text, "switch") == 0) {
					IAstNode* switchStmt = parseSwitchStatement(scanner, errorReporter);
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
					returnStmt->setParent(body);
					body->addChild(returnStmt);
				} else if (strcmp(text, "defer") == 0) {
					for (auto* node : tempNodes) {
						node->setParent(body);
						body->addChild(node);
					}
					tempNodes.clear();

					AstNodeDefer* deferStmt = new AstNodeDefer();
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
								IAstNode* id = isBuiltInInstruction(deferText)
													   ? static_cast<IAstNode*>(new AstNodeInstruction(deferText))
													   : static_cast<IAstNode*>(new AstNodeIdentifier(deferText));
								deferNodes.push_back(id);
							} else if (token == U8T_INTEGER) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								AstNodeLiteral* lit =
										new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Integer);
								deferNodes.push_back(lit);
							} else if (token == U8T_FLOAT) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Float);
								deferNodes.push_back(lit);
							} else if (token == U8T_STRING) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);
								AstNodeLiteral* lit =
										new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::String);
								deferNodes.push_back(lit);
							} else if (token == ':') {
								char32_t nextChar = u8t_scanner_peek(scanner);
								if (nextChar == ':') {
									u8t_scanner_scan(scanner);
									IAstNode* colonColon = new AstNodeIdentifier("::");
									deferNodes.push_back(colonColon);
								}
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
							deferNodes.push_back(id);
						} else if (token == U8T_INTEGER) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Integer);
							deferNodes.push_back(lit);
						} else if (token == U8T_FLOAT) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Float);
							deferNodes.push_back(lit);
						} else if (token == U8T_STRING) {
							const char* deferText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::String);
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
								if (strcmp(deferText, "for") == 0 || strcmp(deferText, "if") == 0 ||
										strcmp(deferText, "switch") == 0 || strcmp(deferText, "return") == 0 ||
										strcmp(deferText, "defer") == 0 || strcmp(deferText, "break") == 0 ||
										strcmp(deferText, "continue") == 0) {
									IAstNode* id = isBuiltInInstruction(deferText)
														   ? static_cast<IAstNode*>(new AstNodeInstruction(deferText))
														   : static_cast<IAstNode*>(new AstNodeIdentifier(deferText));
									tempNodes.push_back(id);
									break;
								}

								// Mark that we've seen an operator
								hasSeenOperator = true;
								IAstNode* id = isBuiltInInstruction(deferText)
													   ? static_cast<IAstNode*>(new AstNodeInstruction(deferText))
													   : static_cast<IAstNode*>(new AstNodeIdentifier(deferText));
								deferNodes.push_back(id);
							} else if (token == U8T_INTEGER) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								// If we've already seen an operator and the last node was an operator,
								// this literal starts a new statement
								if (hasSeenOperator && !deferNodes.empty() &&
										deferNodes.back()->type() == IAstNode::Type::Identifier) {
									AstNodeLiteral* lit =
											new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Integer);
									tempNodes.push_back(lit);
									break;
								}

								IAstNode* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Integer);
								deferNodes.push_back(lit);
							} else if (token == U8T_FLOAT) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								if (hasSeenOperator && !deferNodes.empty() &&
										deferNodes.back()->type() == IAstNode::Type::Identifier) {
									AstNodeLiteral* lit =
											new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Float);
									tempNodes.push_back(lit);
									break;
								}

								IAstNode* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::Float);
								deferNodes.push_back(lit);
							} else if (token == U8T_STRING) {
								const char* deferText = u8t_scanner_token_text(scanner, &n);

								if (hasSeenOperator && !deferNodes.empty() &&
										deferNodes.back()->type() == IAstNode::Type::Identifier) {
									AstNodeLiteral* lit =
											new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::String);
									tempNodes.push_back(lit);
									break;
								}

								IAstNode* lit = new AstNodeLiteral(deferText, AstNodeLiteral::LiteralType::String);
								deferNodes.push_back(lit);
							} else if (token == ':') {
								char32_t nextChar = u8t_scanner_peek(scanner);
								if (nextChar == ':') {
									u8t_scanner_scan(scanner);
									IAstNode* colonColon = new AstNodeIdentifier("::");
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
					IAstNode* id = isBuiltInInstruction(text) ? static_cast<IAstNode*>(new AstNodeInstruction(text))
															  : static_cast<IAstNode*>(new AstNodeIdentifier(text));
					tempNodes.push_back(id);
				}
			} else if (token == U8T_INTEGER) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Integer);
				tempNodes.push_back(lit);
			} else if (token == U8T_FLOAT) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Float);
				tempNodes.push_back(lit);
			} else if (token == U8T_STRING) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::String);
				tempNodes.push_back(lit);
			} else if (token == '.') {
				// Handle '.' as alias for 'print' (Forth-style)
				AstNodeInstruction* instr = new AstNodeInstruction(".");
				tempNodes.push_back(instr);
			} else if (token == '*') {
				// Handle '*' as alias for 'mul'
				AstNodeInstruction* instr = new AstNodeInstruction("*");
				tempNodes.push_back(instr);
			} else if (token == '+') {
				// Handle '+' as alias for 'add'
				AstNodeInstruction* instr = new AstNodeInstruction("+");
				tempNodes.push_back(instr);
			} else if (token == '-') {
				// Handle '-' as alias for 'sub'
				AstNodeInstruction* instr = new AstNodeInstruction("-");
				tempNodes.push_back(instr);
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

	static IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		if (token != U8T_IDENTIFIER) {
			errorReporter->reportError(scanner, "Expected loop variable after 'for'");
			synchronize(scanner);
			return nullptr;
		}

		const char* loopVar = u8t_scanner_token_text(scanner, &n);
		AstNodeForStatement* forStmt = new AstNodeForStatement(loopVar);

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after loop variable");
			// Recovery: create empty body and return partial for statement
			AstNodeBlock* body = new AstNodeBlock();
			body->setParent(forStmt);
			forStmt->setBody(body);
			synchronize(scanner);
			return forStmt;
		}

		AstNodeBlock* body = new AstNodeBlock();

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n);
			if (node) {
				node->setParent(body);
				body->addChild(node);
			}
		}

		body->setParent(forStmt);
		forStmt->setBody(body);

		return forStmt;
	}

	static IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'if'");
			// Recovery: create empty if statement and synchronize
			AstNodeIfStatement* ifStmt = new AstNodeIfStatement();
			AstNodeBlock* thenBody = new AstNodeBlock();
			thenBody->setParent(ifStmt);
			ifStmt->setThenBody(thenBody);
			synchronize(scanner);
			return ifStmt;
		}

		AstNodeIfStatement* ifStmt = new AstNodeIfStatement();
		AstNodeBlock* thenBody = new AstNodeBlock();

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n);
			if (node) {
				node->setParent(thenBody);
				thenBody->addChild(node);
			}
		}

		thenBody->setParent(ifStmt);
		ifStmt->setThenBody(thenBody);

		// Check for else clause
		char32_t peek = u8t_scanner_peek(scanner);
		if (peek != 0) {
			token = u8t_scanner_scan(scanner);
			if (token == U8T_IDENTIFIER) {
				const char* elseText = u8t_scanner_token_text(scanner, &n);
				if (strcmp(elseText, "else") == 0) {
					// Parse else block - must have {
					token = u8t_scanner_scan(scanner);
					if (token != '{') {
						errorReporter->reportError(scanner, "Expected '{' after 'else'");
					} else {
						AstNodeBlock* elseBody = new AstNodeBlock();

						while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
							if (token == '}') {
								break;
							}

							IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n);
							if (node) {
								node->setParent(elseBody);
								elseBody->addChild(node);
							}
						}

						elseBody->setParent(ifStmt);
						ifStmt->setElseBody(elseBody);
					}
				}
			}
		}

		return ifStmt;
	}

	static IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'switch'");
			// Recovery: create empty switch statement and synchronize
			AstNodeSwitchStatement* switchStmt = new AstNodeSwitchStatement();
			synchronize(scanner);
			return switchStmt;
		}

		AstNodeSwitchStatement* switchStmt = new AstNodeSwitchStatement();

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);

				if (strcmp(text, "case") == 0) {
					token = u8t_scanner_scan(scanner);

					IAstNode* caseValue = nullptr;
					if (token == U8T_INTEGER) {
						const char* valueText = u8t_scanner_token_text(scanner, &n);
						caseValue = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::Integer);
					} else if (token == U8T_FLOAT) {
						const char* valueText = u8t_scanner_token_text(scanner, &n);
						caseValue = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::Float);
					} else if (token == U8T_STRING) {
						const char* valueText = u8t_scanner_token_text(scanner, &n);
						caseValue = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::String);
					} else if (token == U8T_IDENTIFIER) {
						const char* valueText = u8t_scanner_token_text(scanner, &n);
						caseValue = isBuiltInInstruction(valueText)
											? static_cast<IAstNode*>(new AstNodeInstruction(valueText))
											: static_cast<IAstNode*>(new AstNodeIdentifier(valueText));
					}

					if (!caseValue) {
						errorReporter->reportError(scanner, "Expected case value after 'case'");
						continue;
					}

					token = u8t_scanner_scan(scanner);
					if (token != '{') {
						errorReporter->reportError(scanner, "Expected '{' after case value");
						delete caseValue;
						continue;
					}

					AstNodeBlock* caseBody = new AstNodeBlock();
					while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
						if (token == '}') {
							break;
						}

						IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, false);
						if (node) {
							node->setParent(caseBody);
							caseBody->addChild(node);
						}
					}

					AstNodeCase* caseNode = new AstNodeCase(caseValue, false);
					caseBody->setParent(caseNode);
					caseNode->setBody(caseBody);
					caseNode->setParent(switchStmt);
					switchStmt->addCase(caseNode);

				} else if (strcmp(text, "default") == 0) {
					token = u8t_scanner_scan(scanner);
					if (token != '{') {
						errorReporter->reportError(scanner, "Expected '{' after 'default'");
						continue;
					}

					AstNodeBlock* defaultBody = new AstNodeBlock();
					while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
						if (token == '}') {
							break;
						}

						IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, false);
						if (node) {
							node->setParent(defaultBody);
							defaultBody->addChild(node);
						}
					}

					AstNodeCase* defaultCase = new AstNodeCase(nullptr, true);
					defaultBody->setParent(defaultCase);
					defaultCase->setBody(defaultBody);
					defaultCase->setParent(switchStmt);
					switchStmt->addCase(defaultCase);
				}
			}
		}

		return switchStmt;
	}

	IAstNode* Ast::generate(const char* src) {
		u8t_scanner scanner;
		u8t_scanner_init(&scanner, src);

		ErrorReporter errorReporter(src);

		if (mRoot) {
			delete mRoot;
		}
		AstProgram* program = new AstProgram();
		mRoot = program;

		char32_t token;
		bool sawSlash = false;
		while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
			size_t n;

			// Handle // line comments
			if (sawSlash && token == '/') {
				sawSlash = false;
				// Skip until end of line
				char32_t c;
				while ((c = u8t_scanner_peek(&scanner)) != 0 && c != '\n' && c != '\r') {
					u8t_scanner_scan(&scanner);
				}
				continue;
			}

			// Handle /* block comments */
			if (sawSlash && token == '*') {
				sawSlash = false;
				// Skip until */
				bool foundStar = false;
				while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
					if (foundStar && token == '/') {
						break;
					}
					foundStar = (token == '*');
				}
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
					IAstNode* func = parseFunctionDeclaration(&scanner, &errorReporter);
					if (func) {
						func->setParent(program);
						program->addChild(func);
					}
				} else if (strcmp(text, "use") == 0) {
					token = u8t_scanner_scan(&scanner);
					if (token == U8T_IDENTIFIER) {
						const char* moduleName = u8t_scanner_token_text(&scanner, &n);
						AstNodeUse* useStmt = new AstNodeUse(moduleName);
						useStmt->setParent(program);
						program->addChild(useStmt);
					} else {
						errorReporter.reportError(&scanner, "Expected module name after 'use'");
					}
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
								value = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::Integer);
							} else if (token == U8T_FLOAT) {
								const char* valueText = u8t_scanner_token_text(&scanner, &n);
								value = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::Float);
							} else if (token == U8T_STRING) {
								const char* valueText = u8t_scanner_token_text(&scanner, &n);
								value = new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::String);
							}
							if (value) {
								AstNodeConstant* constDecl = new AstNodeConstant(constNameStr, value->value().c_str());
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

		return mRoot;
	}
}
