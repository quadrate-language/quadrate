#include <qc/ast.h>
#include <qc/ast_printer.h>
#include <unit-check/uc.h>
#include <cstring>

TEST(SimpleFunctionDeclaration) {
	Qd::Ast ast;
	const char* src = "fn main() {}";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->type() == Qd::IAstNode::Type::Program, "root should be a Program");
	ASSERT(root->childCount() == 1, "program should have 1 child");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "child should be function declaration");
}

TEST(FunctionWithParameters) {
	Qd::Ast ast;
	const char* src = "fn add(a: int b: int -- result: int) {}";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "should be function");
}

TEST(ScopedIdentifier) {
	Qd::Ast ast;
	const char* src = "fn test() { std::print }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "should be function");

	// Function should have a body
	Qd::IAstNode* body = func->child(0);
	ASSERT(body != nullptr, "body should not be null");
	ASSERT(body->childCount() == 1, "body should have 1 child");

	// Body should contain scoped identifier
	Qd::IAstNode* scoped = body->child(0);
	ASSERT(scoped->type() == Qd::IAstNode::Type::ScopedIdentifier, "should be scoped identifier");
}

TEST(LineComment) {
	Qd::Ast ast;
	const char* src = "fn test() { // comment\nfoo }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	// Should have 1 child (foo), comment should be skipped
	ASSERT(body->childCount() == 1, "body should have 1 child");

	Qd::IAstNode* id = body->child(0);
	ASSERT(id->type() == Qd::IAstNode::Type::Identifier, "should be identifier");
}

TEST(BlockComment) {
	Qd::Ast ast;
	const char* src = "fn test() { /* block comment */ foo }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	// Should have 1 child (foo), comment should be skipped
	ASSERT(body->childCount() == 1, "body should have 1 child");
}

TEST(BreakStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { for i { break } }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* funcBody = func->child(0);
	ASSERT(funcBody->childCount() == 1, "funcBody should have 1 child");

	Qd::IAstNode* forStmt = funcBody->child(0);
	ASSERT(forStmt->type() == Qd::IAstNode::Type::ForStatement, "should be for");

	Qd::IAstNode* forBody = forStmt->child(0);
	ASSERT(forBody->childCount() == 1, "forBody should have 1 child");

	Qd::IAstNode* breakStmt = forBody->child(0);
	ASSERT(breakStmt->type() == Qd::IAstNode::Type::BreakStatement, "should be break");
}

TEST(ContinueStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { for i { continue } }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* funcBody = func->child(0);
	Qd::IAstNode* forStmt = funcBody->child(0);
	Qd::IAstNode* forBody = forStmt->child(0);

	ASSERT(forBody->childCount() == 1, "forBody should have 1 child");
	Qd::IAstNode* continueStmt = forBody->child(0);
	ASSERT(continueStmt->type() == Qd::IAstNode::Type::ContinueStatement, "should be continue");
}

TEST(DeferStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { defer close }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* deferStmt = body->child(0);
	ASSERT(deferStmt->type() == Qd::IAstNode::Type::DeferStatement, "should be defer");

	// Defer should contain the identifier
	ASSERT(deferStmt->childCount() == 1, "deferStmt should have 1 child");
	Qd::IAstNode* id = deferStmt->child(0);
	ASSERT(id->type() == Qd::IAstNode::Type::Identifier, "should be identifier");
}

TEST(DeferBlock) {
	Qd::Ast ast;
	const char* src = "fn test() { defer { close } }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* deferStmt = body->child(0);
	ASSERT(deferStmt->type() == Qd::IAstNode::Type::DeferStatement, "should be defer");

	// Defer should contain the identifier
	ASSERT(deferStmt->childCount() == 1, "deferStmt should have 1 child");
}

TEST(ReturnStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { return }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* returnStmt = body->child(0);
	ASSERT(returnStmt->type() == Qd::IAstNode::Type::ReturnStatement, "should be return");
}

TEST(ConstDeclaration) {
	Qd::Ast ast;
	const char* src = "const PI = 3.14";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* constDecl = root->child(0);
	ASSERT(constDecl->type() == Qd::IAstNode::Type::ConstantDeclaration, "should be constant");

	// Const should have a value
	ASSERT(constDecl->childCount() == 1, "constDecl should have 1 child");
	Qd::IAstNode* value = constDecl->child(0);
	ASSERT(value->type() == Qd::IAstNode::Type::Literal, "should be literal");
}

TEST(UseStatement) {
	Qd::Ast ast;
	const char* src = "use std";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* useStmt = root->child(0);
	ASSERT(useStmt->type() == Qd::IAstNode::Type::UseStatement, "should be use statement");
}

TEST(SwitchStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { switch { case 1 { foo } default { bar } } }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* switchStmt = body->child(0);
	ASSERT(switchStmt->type() == Qd::IAstNode::Type::SwitchStatement, "should be switch");

	// Switch should have 2 cases (case 1 and default)
	ASSERT(switchStmt->childCount() == 2, "switch should have 2 cases");
}

TEST(IfStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { if { foo } }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* ifStmt = body->child(0);
	ASSERT(ifStmt->type() == Qd::IAstNode::Type::IfStatement, "should be if statement");
}

TEST(ForStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { for i { foo } }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* forStmt = body->child(0);
	ASSERT(forStmt->type() == Qd::IAstNode::Type::ForStatement, "should be for");
}

TEST(Literals) {
	Qd::Ast ast;
	const char* src = "fn test() { 42 3.14 \"hello\" }";
	Qd::IAstNode* root = ast.generate(src);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 3, "body should have 3 children");

	Qd::IAstNode* intLit = body->child(0);
	ASSERT(intLit->type() == Qd::IAstNode::Type::Literal, "should be literal");

	Qd::IAstNode* floatLit = body->child(1);
	ASSERT(floatLit->type() == Qd::IAstNode::Type::Literal, "should be literal");

	Qd::IAstNode* strLit = body->child(2);
	ASSERT(strLit->type() == Qd::IAstNode::Type::Literal, "should be literal");
}

int main() {
	return UC_PrintResults();
}
