#include <cstring>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_block.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_if.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_program.h>
#include <u8t/scanner.h>
#include <vector>

namespace Qd {
	static IAstNode* parseForStatement(u8t_scanner* scanner);
	static IAstNode* parseIfStatement(u8t_scanner* scanner);

	Ast::~Ast() {
		if (mRoot) {
			delete mRoot;
			mRoot = nullptr;
		}
	}

	static IAstNode* parseFunctionDeclaration(u8t_scanner* scanner) {
		char32_t token = u8t_scanner_scan(scanner);
		if (token != U8T_IDENTIFIER) {
			fprintf(stderr, "Error: Expected function name after 'fn'\n");
			return nullptr;
		}

		size_t n;
		const char* name = u8t_scanner_token_text(scanner, &n);
		AstNodeFunctionDeclaration* func = new AstNodeFunctionDeclaration(name);

		token = u8t_scanner_scan(scanner);
		if (token != '(') {
			fprintf(stderr, "Error: Expected '(' after function name\n");
			delete func;
			return nullptr;
		}

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == ')') {
				break;
			}
		}

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			fprintf(stderr, "Error: Expected '{' after function signature\n");
			delete func;
			return nullptr;
		}

		AstNodeBlock* body = new AstNodeBlock();

		std::vector<IAstNode*> tempNodes;

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);

				if (strcmp(text, "for") == 0) {
					IAstNode* forStmt = parseForStatement(scanner);
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
					IAstNode* ifStmt = parseIfStatement(scanner);
					if (ifStmt) {
						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						ifStmt->setParent(body);
						body->addChild(ifStmt);
					}
				} else {
					AstNodeIdentifier* id = new AstNodeIdentifier(text);
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

	static IAstNode* parseForStatement(u8t_scanner* scanner) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		if (token != U8T_IDENTIFIER) {
			fprintf(stderr, "Error: Expected loop variable after 'for'\n");
			return nullptr;
		}

		const char* loopVar = u8t_scanner_token_text(scanner, &n);
		AstNodeForStatement* forStmt = new AstNodeForStatement(loopVar);

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			fprintf(stderr, "Error: Expected '{' after loop variable\n");
			delete forStmt;
			return nullptr;
		}

		AstNodeBlock* body = new AstNodeBlock();

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);

				if (strcmp(text, "for") == 0) {
					IAstNode* nestedFor = parseForStatement(scanner);
					if (nestedFor) {
						nestedFor->setParent(body);
						body->addChild(nestedFor);
					}
				} else {
					AstNodeIdentifier* id = new AstNodeIdentifier(text);
					id->setParent(body);
					body->addChild(id);
				}
			} else if (token == U8T_INTEGER) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Integer);
				lit->setParent(body);
				body->addChild(lit);
			} else if (token == U8T_FLOAT) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Float);
				lit->setParent(body);
				body->addChild(lit);
			} else if (token == U8T_STRING) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::String);
				lit->setParent(body);
				body->addChild(lit);
			}
		}

		body->setParent(forStmt);
		forStmt->setBody(body);

		return forStmt;
	}

	static IAstNode* parseIfStatement(u8t_scanner* scanner) {
		size_t n;
		char32_t token = u8t_scanner_scan(scanner);

		if (token != '{') {
			fprintf(stderr, "Error: Expected '{' after 'if'\n");
			return nullptr;
		}

		AstNodeIfStatement* ifStmt = new AstNodeIfStatement();
		AstNodeBlock* thenBody = new AstNodeBlock();

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);

				if (strcmp(text, "if") == 0) {
					IAstNode* nestedIf = parseIfStatement(scanner);
					if (nestedIf) {
						nestedIf->setParent(thenBody);
						thenBody->addChild(nestedIf);
					}
				} else {
					AstNodeIdentifier* id = new AstNodeIdentifier(text);
					id->setParent(thenBody);
					thenBody->addChild(id);
				}
			} else if (token == U8T_INTEGER) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Integer);
				lit->setParent(thenBody);
				thenBody->addChild(lit);
			} else if (token == U8T_FLOAT) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::Float);
				lit->setParent(thenBody);
				thenBody->addChild(lit);
			} else if (token == U8T_STRING) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeLiteral* lit = new AstNodeLiteral(text, AstNodeLiteral::LiteralType::String);
				lit->setParent(thenBody);
				thenBody->addChild(lit);
			}
		}

		thenBody->setParent(ifStmt);
		ifStmt->setThenBody(thenBody);

		token = u8t_scanner_scan(scanner);
		if (token == U8T_IDENTIFIER) {
			const char* text = u8t_scanner_token_text(scanner, &n);
			if (strcmp(text, "else") == 0) {
				token = u8t_scanner_scan(scanner);
				if (token == '{') {
					AstNodeBlock* elseBody = new AstNodeBlock();

					while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
						if (token == '}') {
							break;
						}

						if (token == U8T_IDENTIFIER) {
							const char* elseText = u8t_scanner_token_text(scanner, &n);

							if (strcmp(elseText, "if") == 0) {
								IAstNode* nestedIf = parseIfStatement(scanner);
								if (nestedIf) {
									nestedIf->setParent(elseBody);
									elseBody->addChild(nestedIf);
								}
							} else {
								AstNodeIdentifier* id = new AstNodeIdentifier(elseText);
								id->setParent(elseBody);
								elseBody->addChild(id);
							}
						} else if (token == U8T_INTEGER) {
							const char* elseText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(elseText, AstNodeLiteral::LiteralType::Integer);
							lit->setParent(elseBody);
							elseBody->addChild(lit);
						} else if (token == U8T_FLOAT) {
							const char* elseText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(elseText, AstNodeLiteral::LiteralType::Float);
							lit->setParent(elseBody);
							elseBody->addChild(lit);
						} else if (token == U8T_STRING) {
							const char* elseText = u8t_scanner_token_text(scanner, &n);
							AstNodeLiteral* lit = new AstNodeLiteral(elseText, AstNodeLiteral::LiteralType::String);
							lit->setParent(elseBody);
							elseBody->addChild(lit);
						}
					}

					elseBody->setParent(ifStmt);
					ifStmt->setElseBody(elseBody);
				}
			}
		}

		return ifStmt;
	}

	IAstNode* Ast::generate(const char* src) {
		u8t_scanner scanner;
		u8t_scanner_init(&scanner, src);

		if (mRoot) {
			delete mRoot;
		}
		AstProgram* program = new AstProgram();
		mRoot = program;

		char32_t token;
		while ((token = u8t_scanner_scan(&scanner)) != U8T_EOF) {
			size_t n;
			switch (token) {
			case U8T_IDENTIFIER: {
				const char* text = u8t_scanner_token_text(&scanner, &n);

				if (strcmp(text, "fn") == 0) {
					IAstNode* func = parseFunctionDeclaration(&scanner);
					if (func) {
						func->setParent(program);
						program->addChild(func);
					}
				} else {
					printf("Identifier: %s\n", text);
				}
				break;
			}
			case U8T_INTEGER:
				printf("Integer   : %s\n", u8t_scanner_token_text(&scanner, &n));
				break;
			case U8T_STRING:
				printf("String    : %s\n", u8t_scanner_token_text(&scanner, &n));
				break;
			case U8T_FLOAT:
				printf("Float     : %s\n", u8t_scanner_token_text(&scanner, &n));
				break;
			default:
				printf("Token     : %c\n", token);
				break;
			}
		}

		return mRoot;
	}
}
