#include <cstring>
#include <qc/ast.h>
#include <qc/ast_printer.h>
#include <unit-check/uc.h>

TEST(SimpleFunctionDeclaration) {
	Qd::Ast ast;
	const char* src = "fn main() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->type() == Qd::IAstNode::Type::Program, "root should be a Program");
	ASSERT(root->childCount() == 1, "program should have 1 child");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "child should be function declaration");
}

TEST(FunctionWithParameters) {
	Qd::Ast ast;
	const char* src = "fn add(a: int b: int -- result: int) {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "should be function");
}

TEST(ScopedIdentifier) {
	Qd::Ast ast;
	const char* src = "fn test() { std::print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* constDecl = root->child(0);
	ASSERT(constDecl->type() == Qd::IAstNode::Type::ConstantDeclaration, "should be constant");

	// Const declaration stores value internally, not as a child node
	ASSERT(constDecl->childCount() == 0, "constDecl should have 0 children");
}

TEST(UseStatement) {
	Qd::Ast ast;
	const char* src = "use std";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* useStmt = root->child(0);
	ASSERT(useStmt->type() == Qd::IAstNode::Type::UseStatement, "should be use statement");
}

TEST(SwitchStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { switch { case 1 { foo } default { bar } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root != nullptr");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* ifStmt = body->child(0);
	ASSERT(ifStmt->type() == Qd::IAstNode::Type::IfStatement, "should be if statement");
}

TEST(IfElseStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { if { foo } else { bar } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 child");

	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);

	ASSERT(body->childCount() == 1, "body should have 1 child");
	Qd::IAstNode* ifStmt = body->child(0);
	ASSERT(ifStmt->type() == Qd::IAstNode::Type::IfStatement, "should be if statement");

	// If statement should have 2 children: then body and else body
	ASSERT(ifStmt->childCount() == 2, "if should have then and else bodies");

	Qd::IAstNode* thenBody = ifStmt->child(0);
	ASSERT(thenBody != nullptr, "then body should exist");
	ASSERT(thenBody->childCount() == 1, "then body should have 1 child");

	Qd::IAstNode* elseBody = ifStmt->child(1);
	ASSERT(elseBody != nullptr, "else body should exist");
	ASSERT(elseBody->childCount() == 1, "else body should have 1 child");
}

TEST(NestedIfElse) {
	Qd::Ast ast;
	const char* src = "fn test() { if { a } else { if { b } else { c } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);
	Qd::IAstNode* outerIf = body->child(0);

	ASSERT(outerIf->type() == Qd::IAstNode::Type::IfStatement, "should be if statement");
	ASSERT(outerIf->childCount() == 2, "outer if should have then and else");

	Qd::IAstNode* outerElse = outerIf->child(1);
	ASSERT(outerElse->childCount() == 1, "outer else should have 1 child");

	Qd::IAstNode* nestedIf = outerElse->child(0);
	ASSERT(nestedIf->type() == Qd::IAstNode::Type::IfStatement, "nested should be if statement");
	ASSERT(nestedIf->childCount() == 2, "nested if should have then and else");
}

TEST(DeeplyNestedIfElse) {
	Qd::Ast ast;
	const char* src = "fn test() { if { a } else { if { b } else { if { c } else { d } } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);
	Qd::IAstNode* level1 = body->child(0);

	ASSERT(level1->childCount() == 2, "level 1 should have then and else");

	Qd::IAstNode* level2 = level1->child(1)->child(0);
	ASSERT(level2->type() == Qd::IAstNode::Type::IfStatement, "level 2 should be if");
	ASSERT(level2->childCount() == 2, "level 2 should have then and else");

	Qd::IAstNode* level3 = level2->child(1)->child(0);
	ASSERT(level3->type() == Qd::IAstNode::Type::IfStatement, "level 3 should be if");
	ASSERT(level3->childCount() == 2, "level 3 should have then and else");
}

TEST(ForStatement) {
	Qd::Ast ast;
	const char* src = "fn test() { for i { foo } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

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

TEST(ErrorRecoveryMissingBraceAfterFunction) {
	Qd::Ast ast;
	const char* src = "fn test()\nfn other() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	// Should recover and parse both functions
	// First function will have error but should create partial node
	// Second function should parse correctly
	ASSERT(root->childCount() >= 1, "should have at least 1 function");
}

TEST(ErrorRecoveryMissingBraceAfterIf) {
	Qd::Ast ast;
	const char* src = "fn test() { if foo }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "should be function");

	// Function should have body even with error in if statement
	Qd::IAstNode* body = func->child(0);
	ASSERT(body != nullptr, "body should not be null");
}

TEST(ErrorRecoveryMissingBraceAfterFor) {
	Qd::Ast ast;
	const char* src = "fn test() { for i foo }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");

	Qd::IAstNode* func = root->child(0);
	ASSERT(func->type() == Qd::IAstNode::Type::FunctionDeclaration, "should be function");

	// Function should have body even with error in for statement
	Qd::IAstNode* body = func->child(0);
	ASSERT(body != nullptr, "body should not be null");
}

TEST(ErrorRecoveryMultipleErrors) {
	Qd::Ast ast;
	const char* src = "fn first() { if bar }\nfn second() { for x }\nfn third() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	// Parser should recover from errors and continue parsing
	// Should have some nodes even with errors
	ASSERT(root->childCount() >= 1, "should have at least 1 function");
}

int main() {
	return UC_PrintResults();
}
