#include <cstring>
#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_block.h>
#include <qc/ast_node_for.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_literal.h>
#include <qc/ast_node_program.h>
#include <u8t/scanner.h>
#include <vector>

namespace Qd {
	static IAstNode* parseForStatement(u8t_scanner* scanner);

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
						AstNodeForStatement* forNode = static_cast<AstNodeForStatement*>(forStmt);

						if (tempNodes.size() >= 3) {
							IAstNode* step = tempNodes.back();
							tempNodes.pop_back();
							IAstNode* end = tempNodes.back();
							tempNodes.pop_back();
							IAstNode* start = tempNodes.back();
							tempNodes.pop_back();

							forNode->setStart(start);
							forNode->setEnd(end);
							forNode->setStep(step);
						}

						for (auto* node : tempNodes) {
							node->setParent(body);
							body->addChild(node);
						}
						tempNodes.clear();

						forStmt->setParent(body);
						body->addChild(forStmt);
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
