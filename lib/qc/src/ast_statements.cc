#include "ast_parse.h"

namespace Qd {

	// Helper function to parse a block body with proper else-handling.
	// When inFunctionBody is true, also handles >>field, ctx blocks, and EOF detection.
	void parseBlockBody(AstNodeBlock* block, u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src,
			bool inFunctionBody) {
		ParseDepthGuard depthGuard;
		if (depthGuard.exceeded()) {
			errorReporter->reportError(scanner, "Block nesting too deep");
			return;
		}
		size_t n;
		char32_t token;
		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection
		bool sawColon = false;
		bool foundClosingBrace = false;
		std::vector<IAstNode*> tempNodes;
		TempNodeGuard guard(tempNodes); // Auto-deletes unflushed nodes on scope exit

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
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
						// The token after `::` has been scanned and cannot be pushed back, so
						// leaving it unreported swallowed it: `fn m(){i::}}` lost the brace
						// that closed the body and still parsed clean.
						errorReporter->reportError(scanner, "Expected a name after '::'");
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
									const char* variantName = u8t_scanner_token_text(scanner, &n);
									memberStr += "::";
									memberStr += variantName;
								}
							}
						}

						AstNodeScopedIdentifier* scoped = new AstNodeScopedIdentifier(scope->name(), memberStr);
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
						// Same as above: the swallowed token is gone either way, so say so.
						errorReporter->reportError(scanner, "Expected a name after '::'");
						// No identifier after ::, put scope back
						tempNodes.push_back(scopeOwner.release());
					}
				}
				continue;
			}

			// Handle comments (// and /* */)
			AstNodeComment* comment = parseComment(scanner, src, slashPos, token);
			if (comment != nullptr) {
				slashPos = SIZE_MAX;
				if (inFunctionBody) {
					// In function bodies, comments are part of the token stream
					tempNodes.push_back(comment);
				} else {
					// In blocks, flush tempNodes before adding comment
					for (auto* node : tempNodes) {
						node->setParent(block);
						block->addChild(node);
					}
					tempNodes.clear();
					comment->setParent(block);
					block->addChild(comment);
				}
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
				foundClosingBrace = true;
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

			// Handle >>field (field set / write) — only in function bodies
			if (inFunctionBody && token == '>') {
				char32_t nextChar = u8t_scanner_peek(scanner);
				if (nextChar == '>') {
					u8t_scanner_scan(scanner); // Consume second >
					char32_t fieldStart = u8t_scanner_peek(scanner);
					if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
							fieldStart == '_') {
						char32_t identToken = u8t_scanner_scan(scanner);
						if (identToken == U8T_IDENTIFIER) {
							std::string fieldName(u8t_scanner_token_text(scanner, &n));
							if (u8t_scanner_peek(scanner) == '!') {
								u8t_scanner_scan(scanner);
								errorReporter->reportError(scanner,
										"'>>field!' has been removed; it only differed from '>>field' by "
										"discarding the struct, so write '>>field drop' instead");
							}
							AstNodeFieldSet* fieldSet = new AstNodeFieldSet(fieldName);
							setNodePosition(fieldSet, scanner, src);
							tempNodes.push_back(fieldSet);
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
					u8t_scanner_scan(scanner); // Consume second <
					char32_t fieldStart = u8t_scanner_peek(scanner);
					if ((fieldStart >= 'a' && fieldStart <= 'z') || (fieldStart >= 'A' && fieldStart <= 'Z') ||
							fieldStart == '_') {
						char32_t identToken = u8t_scanner_scan(scanner);
						if (identToken == U8T_IDENTIFIER) {
							const char* fieldName = u8t_scanner_token_text(scanner, &n);

							// `<<field` reads the struct on top of the stack, whatever put it there -- a local,
							// a call, a construction, a previous `<<`. That operand stays its own node. The
							// parser used to fold a preceding identifier into this node, and every consumer
							// then had to redo name resolution to learn whether it had been a variable, a
							// function or a struct type; the validator got it wrong for functions.
							AstNodeFieldAccess* fieldAccess = new AstNodeFieldAccess(fieldName);
							setNodePosition(fieldAccess, scanner, src);
							tempNodes.push_back(fieldAccess);
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
					noteUnterminatedString(scanner, src);
					const char* strText = u8t_scanner_token_text(scanner, &sn);
					std::string raw(strText);
					// Strip outer quotes
					if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
						raw = raw.substr(1, raw.size() - 2);
					}
					// Flush existing temp nodes first
					for (auto* node : tempNodes) {
						node->setParent(block);
						block->addChild(node);
					}
					tempNodes.clear();
					parseStringInterpolation(raw, scanner, src, tempNodes);
					continue;
				} else {
					// `$` only ever introduces an interpolated string (spec 2121), and the token
					// after it has already been scanned, so it reaches parseBlockStatement out
					// of the loop's context -- without the comment detection, among other
					// things, which is how `fn s(){$//f{` swallowed a brace and still parsed
					// clean. Report the `$`, then process what followed as before.
					errorReporter->reportError(scanner, "Expected a string after '$'");
					IAstNode* extraNode = parseBlockStatement(nextToken, scanner, errorReporter, &n, src);
					if (extraNode) {
						tempNodes.push_back(extraNode);
					}
					continue;
				}
			}

			// Check if this token is a keyword
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
							parseBlockBody(elseBody, scanner, errorReporter, src, inFunctionBody);

							elseBody->setParent(ifStmt);
							ifStmt->setElseBody(elseBody);
						}
						continue; // Skip adding else as a regular statement
					} else {
						errorReporter->reportError(scanner, "'else' without preceding 'if'");
						continue;
					}
				} else if (strcmp(tokenText, "fn") == 0 || strcmp(tokenText, "stack") == 0) {
					// Anonymous function: fn (params -- outputs) { body }, or
					// stack fn (params -- outputs) { body } to leave the inputs on the stack.
					// tokenText points into the scanner's buffer, which parsing invalidates.
					const bool isFn = strcmp(tokenText, "fn") == 0;
					IAstNode* anonFunc = tryParseAnonymousFunction(scanner, errorReporter, src, tokenText);
					if (anonFunc) {
						tempNodes.push_back(anonFunc);
						continue;
					}
					if (isFn) {
						errorReporter->reportError(scanner, "Function declarations not allowed inside blocks. "
															"Did you mean 'fn (...) { }' for an anonymous function?");
						continue;
					}
					// A plain identifier that happens to be called `stack`: fall through to the
					// normal identifier handling below.
				}
			}

			// In function bodies, handle standalone '?' and '!' as postfix error operators
			if (inFunctionBody && token == '?' && !tempNodes.empty()) {
				IAstNode* prevNode = tempNodes.back();
				if (prevNode->type() == IAstNode::Type::IDENTIFIER) {
					static_cast<AstNodeIdentifier*>(prevNode)->setPropagateOnError(true);
				} else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
					static_cast<AstNodeInstruction*>(prevNode)->setPropagateOnError(true);
				} else if (prevNode->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
					static_cast<AstNodeScopedIdentifier*>(prevNode)->setPropagateOnError(true);
				}
				continue;
			}

			if (inFunctionBody && token == '!' && !tempNodes.empty()) {
				// Check if next token is '=' for '!='
				char32_t nextToken = u8t_scanner_peek(scanner);
				if (nextToken != '=') {
					IAstNode* prevNode = tempNodes.back();
					if (prevNode->type() == IAstNode::Type::IDENTIFIER) {
						static_cast<AstNodeIdentifier*>(prevNode)->setAbortOnError(true);
					} else if (prevNode->type() == IAstNode::Type::INSTRUCTION) {
						static_cast<AstNodeInstruction*>(prevNode)->setAbortOnError(true);
					} else if (prevNode->type() == IAstNode::Type::SCOPED_IDENTIFIER) {
						static_cast<AstNodeScopedIdentifier*>(prevNode)->setAbortOnError(true);
					} else {
						// Fall back to creating 'not' instruction
						AstNodeInstruction* instr = new AstNodeInstruction("not");
						setNodePosition(instr, scanner, src);
						tempNodes.push_back(instr);
					}
					continue;
				}
			}

			IAstNode* node = parseBlockStatement(token, scanner, errorReporter, &n, src);
			if (node) {
				// `cond while { body }`: the condition is the run of expression nodes immediately
				// before the keyword. It stops at the previous binding or statement, so the `0 -> i`
				// in `0 -> i  i 5 < while { ... }` stays put and only `i 5 <` is re-evaluated. A run
				// that does not leave exactly one value is reported by the validator, which is where
				// stack effects are known.
				if (node->type() == IAstNode::Type::WHILE_STATEMENT) {
					AstNodeWhileStatement* whileStmt = static_cast<AstNodeWhileStatement*>(node);
					AstNodeBlock* condition = static_cast<AstNodeBlock*>(whileStmt->condition());
					size_t start = tempNodes.size();
					while (start > 0 && isConditionExpressionNode(tempNodes[start - 1]->type())) {
						start--;
					}
					for (size_t i = start; i < tempNodes.size(); i++) {
						tempNodes[i]->setParent(condition);
						condition->addChild(tempNodes[i]);
					}
					tempNodes.resize(start);
				}
				tempNodes.push_back(node);
			}
		}

		// Check if we hit EOF without finding closing brace (function bodies only)
		if (inFunctionBody && !foundClosingBrace) {
			errorReporter->reportError(scanner, "Expected '}' to close function body (reached end of file)");
		}

		// Flush remaining tempNodes to block (block takes ownership)
		for (auto* node : tempNodes) {
			node->setParent(block);
			block->addChild(node);
		}
		tempNodes.clear(); // Prevent guard from double-deleting
	}

	IAstNode* parseForStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
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

	IAstNode* parseLoopStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
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

	IAstNode* parseWhileStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
		char32_t token = u8t_scanner_scan(scanner);

		AstNodeWhileStatement* whileStmt = new AstNodeWhileStatement();
		setNodePosition(whileStmt, scanner, src);
		AstNodeBlock* condition = new AstNodeBlock();
		setNodePosition(condition, scanner, src);
		condition->setParent(whileStmt);
		whileStmt->setCondition(condition);

		AstNodeBlock* body = new AstNodeBlock();
		setNodePosition(body, scanner, src);

		if (token != '{') {
			errorReporter->reportError(scanner, "Expected '{' after 'while'");
			body->setParent(whileStmt);
			whileStmt->setBody(body);
			synchronize(scanner);
			return whileStmt;
		}

		parseBlockBody(body, scanner, errorReporter, src);

		body->setParent(whileStmt);
		whileStmt->setBody(body);

		// The condition block is filled by the caller in parseBlockBody, which is the only place
		// the preceding expression nodes are still reachable.
		return whileStmt;
	}

	IAstNode* parseIfStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
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

	IAstNode* parseSwitchStatement(u8t_scanner* scanner, ErrorReporter* errorReporter, const char* src) {
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

		size_t slashPos = SIZE_MAX; // Position of first slash for comment detection

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			// A comment between cases belongs to no case, so it is read and
			// dropped. Without this a dispatch table cannot be annotated, which
			// is the one place a table most wants it.
			AstNodeComment* comment = parseComment(scanner, src, slashPos, token);
			if (comment != nullptr) {
				slashPos = SIZE_MAX;
				delete comment;
				continue;
			}

			if (token == '/') {
				slashPos = u8t_scanner_token_start(scanner);
				continue; // Wait for next token to see if it's a comment
			}

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
			std::unique_ptr<IAstNode> caseValue;
			if (token == U8T_INTEGER) {
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				caseValue.reset(new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::INTEGER));
				setNodePosition(caseValue.get(), scanner, src);
			} else if (token == U8T_FLOAT) {
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				caseValue.reset(new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::FLOAT));
				setNodePosition(caseValue.get(), scanner, src);
			} else if (token == U8T_STRING) {
				noteUnterminatedString(scanner, src);
				const char* valueText = u8t_scanner_token_text(scanner, &n);
				caseValue.reset(new AstNodeLiteral(valueText, AstNodeLiteral::LiteralType::STRING));
				setNodePosition(caseValue.get(), scanner, src);
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
							caseValue.reset(new AstNodeScopedIdentifier(scopeName, nameText));
							setNodePosition(caseValue.get(), scanner, src);
						}
					}
				} else {
					// Handle boolean literals and result constants in switch cases
					caseValue.reset(tryCreateBooleanLiteral(valueText, scanner, src));
					if (!caseValue) {
						caseValue.reset(isBuiltInInstruction(valueText)
												? static_cast<IAstNode*>(new AstNodeInstruction(valueText))
												: static_cast<IAstNode*>(new AstNodeIdentifier(valueText)));
						setNodePosition(caseValue.get(), scanner, src);
					}
				}
			}

			if (!caseValue) {
				errorReporter->reportError(scanner, "Expected case value in switch statement");
				continue;
			}

			token = u8t_scanner_scan(scanner);
			if (token != '{') {
				errorReporter->reportError(scanner, "Expected '{' after case value");
				continue;
			}

			AstNodeBlock* caseBody = new AstNodeBlock();
			setNodePosition(caseBody, scanner, src);
			parseBlockBody(caseBody, scanner, errorReporter, src);

			AstNodeCase* caseNode = new AstNodeCase(caseValue.release(), false);
			setNodePosition(caseNode, scanner, src);
			caseBody->setParent(caseNode);
			caseNode->setBody(caseBody);
			caseNode->setParent(switchStmt);
			switchStmt->addCase(caseNode);
		}

		return switchStmt;
	}

} // namespace Qd
