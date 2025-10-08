#include <qc/ast.h>
#include <qc/ast_node.h>
#include <qc/ast_node_program.h>
#include <qc/ast_node_function.h>
#include <qc/ast_node_identifier.h>
#include <qc/ast_node_block.h>
#include <u8t/scanner.h>
#include <cstring>

namespace Qd {
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

		token = u8t_scanner_scan(scanner);
		if (token != ')') {
			fprintf(stderr, "Error: Expected ')' after parameters\n");
			delete func;
			return nullptr;
		}

		token = u8t_scanner_scan(scanner);
		if (token != '{') {
			fprintf(stderr, "Error: Expected '{' after function signature\n");
			delete func;
			return nullptr;
		}

		AstNodeBlock* body = new AstNodeBlock();

		while ((token = u8t_scanner_scan(scanner)) != U8T_EOF) {
			if (token == '}') {
				break;
			}

			if (token == U8T_IDENTIFIER) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				AstNodeIdentifier* id = new AstNodeIdentifier(text);
				id->setParent(body);
				body->addChild(id);
			} else if (token == U8T_INTEGER || token == U8T_FLOAT || token == U8T_STRING) {
				const char* text = u8t_scanner_token_text(scanner, &n);
				printf("Literal in body: %s\n", text);
			}
		}

		body->setParent(func);
		func->setBody(body);

		return func;
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
