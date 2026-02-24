#include <cstring>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_printer.h>
#include <unit-check/uc.h>

// Empty and Whitespace Inputs

TEST(EmptyInput) {
	Qd::Ast ast;
	const char* src = "";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null for empty input");
	ASSERT(root->type() == Qd::IAstNode::Type::PROGRAM, "root should be a Program");
	ASSERT(root->childCount() == 0, "empty program should have 0 children");
}

TEST(WhitespaceOnly) {
	Qd::Ast ast;
	const char* src = "   \t\n\r\n   ";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null for whitespace input");
	ASSERT(root->childCount() == 0, "whitespace-only program should have 0 children");
}

TEST(OnlyNewlines) {
	Qd::Ast ast;
	const char* src = "\n\n\n\n\n";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 0, "newlines-only should have 0 children");
}

// Comment Edge Cases

TEST(CommentOnly) {
	Qd::Ast ast;
	const char* src = "// this is just a comment";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	// Comment at top level might or might not be captured
	(void)root->childCount();
}

TEST(BlockCommentOnly) {
	Qd::Ast ast;
	const char* src = "/* this is a block comment */";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(NestedBlockComments) {
	Qd::Ast ast;
	// Note: nested block comments may or may not be supported
	const char* src = "fn main() { /* outer /* inner */ still outer */ 42 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(CommentWithSpecialChars) {
	Qd::Ast ast;
	const char* src = "fn main() { // comment with \" and ' and /* and */ symbols\n 42 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(EmptyBlockComment) {
	Qd::Ast ast;
	const char* src = "fn main() { /**/ 42 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

// String Edge Cases

TEST(EmptyString) {
	Qd::Ast ast;
	const char* src = "fn main() { \"\" print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(StringWithEscapes) {
	Qd::Ast ast;
	const char* src = "fn main() { \"hello\\nworld\\ttab\\\"quote\" print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(StringWithBackslash) {
	Qd::Ast ast;
	const char* src = "fn main() { \"path\\\\to\\\\file\" print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(LongString) {
	Qd::Ast ast;
	// Create a string with 1000 characters
	std::string src = "fn main() { \"";
	for (int i = 0; i < 1000; i++) {
		src += "a";
	}
	src += "\" print }";
	Qd::IAstNode* root = ast.generate(src.c_str(), false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

// Number Edge Cases

TEST(ZeroInteger) {
	Qd::Ast ast;
	const char* src = "fn main() { 0 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(NegativeInteger) {
	Qd::Ast ast;
	const char* src = "fn main() { -42 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(LargeInteger) {
	Qd::Ast ast;
	const char* src = "fn main() { 9223372036854775807 print }"; // INT64_MAX
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(ZeroFloat) {
	Qd::Ast ast;
	const char* src = "fn main() { 0.0 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(FloatWithExponent) {
	Qd::Ast ast;
	const char* src = "fn main() { 1.5e10 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(NegativeExponent) {
	Qd::Ast ast;
	const char* src = "fn main() { 1.5e-10 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(MultipleLeadingZeros) {
	Qd::Ast ast;
	const char* src = "fn main() { 00042 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	// Parser behavior may vary - just ensure it doesn't crash
	ASSERT(root != nullptr, "root should not be null");
}

// Identifier Edge Cases

TEST(SingleCharIdentifier) {
	Qd::Ast ast;
	const char* src = "fn a() { b }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(LongIdentifier) {
	Qd::Ast ast;
	std::string src = "fn ";
	// Create a 200-char identifier
	for (int i = 0; i < 200; i++) {
		src += "a";
	}
	src += "() {}";
	Qd::IAstNode* root = ast.generate(src.c_str(), false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(IdentifierWithUnderscore) {
	Qd::Ast ast;
	const char* src = "fn my_func_name() { var_name }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(IdentifierStartingWithUnderscore) {
	Qd::Ast ast;
	const char* src = "fn _private() { _value }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(IdentifierWithNumbers) {
	Qd::Ast ast;
	const char* src = "fn func123() { var456 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

// Deep Nesting

TEST(DeeplyNestedBlocks) {
	Qd::Ast ast;
	// 10 levels of nested if statements
	const char* src = "fn main() { if { if { if { if { if { if { if { if { if { if { 42 } } } } } } } } } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(DeeplyNestedLoops) {
	Qd::Ast ast;
	const char* src = "fn main() { for { for { for { for { for { 42 } } } } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(MixedNestedControl) {
	Qd::Ast ast;
	const char* src = "fn main() { if { for { if { for { switch { 1 { break } } } } } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

// Function Declaration Edge Cases

TEST(FunctionNoParams) {
	Qd::Ast ast;
	const char* src = "fn test() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(FunctionManyParams) {
	Qd::Ast ast;
	const char* src = "fn test(a:i64 b:i64 c:i64 d:i64 e:i64 f:i64 g:i64 h:i64 -- r:i64) {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(FunctionManyOutputs) {
	Qd::Ast ast;
	const char* src = "fn test( -- a:i64 b:i64 c:i64 d:i64 e:i64) {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(FunctionOnlyOutputs) {
	Qd::Ast ast;
	const char* src = "fn producer( -- x:i64) { 42 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(MultipleFunctions) {
	Qd::Ast ast;
	const char* src = "fn a() {} fn b() {} fn c() {} fn d() {} fn e() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 5, "should have 5 functions");
}

TEST(PublicFunction) {
	Qd::Ast ast;
	const char* src = "pub fn exported() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

// Generic Functions

TEST(GenericFunctionSingleParam) {
	Qd::Ast ast;
	const char* src = "fn identity<T>(x:T -- y:T) { }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(GenericFunctionMultipleParams) {
	Qd::Ast ast;
	const char* src = "fn pair<T U>(a:T b:U -- x:T y:U) { }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

// Struct Definitions

TEST(EmptyStruct) {
	Qd::Ast ast;
	const char* src = "struct Empty {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 struct");
}

TEST(StructWithFields) {
	Qd::Ast ast;
	const char* src = "struct Point { x:i64 y:i64 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 struct");
}

TEST(StructManyFields) {
	Qd::Ast ast;
	const char* src = "struct Big { a:i64 b:i64 c:i64 d:i64 e:i64 f:i64 g:i64 h:i64 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 struct");
}

// Constant Declarations

TEST(IntConstant) {
	Qd::Ast ast;
	const char* src = "const ANSWER = 42";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 constant");
}

TEST(FloatConstant) {
	Qd::Ast ast;
	const char* src = "const PI = 3.14159265359";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 constant");
}

TEST(StringConstant) {
	Qd::Ast ast;
	const char* src = "const GREETING = \"Hello, World!\"";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 constant");
}

TEST(MultipleConstants) {
	Qd::Ast ast;
	const char* src = "const A = 1 const B = 2 const C = 3";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 3, "should have 3 constants");
}

// Use Statements

TEST(SingleUse) {
	Qd::Ast ast;
	const char* src = "use std";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 use statement");
}

TEST(MultipleUse) {
	Qd::Ast ast;
	const char* src = "use std\nuse math\nuse io";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 3, "should have 3 use statements");
}

// Anonymous Functions

TEST(SimpleAnonymousFunction) {
	Qd::Ast ast;
	const char* src = "fn main() { fn(x:i64 -- y:i64) { dup mul } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() == 1, "should have 1 function");
}

TEST(AnonymousFunctionNoParams) {
	Qd::Ast ast;
	const char* src = "fn main() { fn( -- x:i64) { 42 } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

// Control Flow Edge Cases

TEST(IfWithoutElse) {
	Qd::Ast ast;
	const char* src = "fn main() { 1 if { 42 print } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	Qd::IAstNode* func = root->child(0);
	Qd::IAstNode* body = func->child(0);
	Qd::IAstNode* ifStmt = body->child(1); // After the literal '1'
	ASSERT(ifStmt->type() == Qd::IAstNode::Type::IF_STATEMENT, "should be if statement");
	ASSERT(ifStmt->childCount() == 1, "if without else should have 1 child (then body)");
}

TEST(ElseIfChain) {
	Qd::Ast ast;
	const char* src = "fn main() { if { 1 } else { if { 2 } else { if { 3 } else { 4 } } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(SwitchManyCase) {
	Qd::Ast ast;
	const char* src = "fn main() { switch { 1 { a } 2 { b } 3 { c } 4 { d } 5 { e } _ { f } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(SwitchDefaultOnly) {
	Qd::Ast ast;
	const char* src = "fn main() { switch { _ { fallback } } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(WhileLoop) {
	Qd::Ast ast;
	const char* src = "fn main() { 1 while { drop 0 } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(InfiniteLoop) {
	Qd::Ast ast;
	const char* src = "fn main() { loop { break } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

TEST(ForWithIterator) {
	Qd::Ast ast;
	const char* src = "fn main() { 0 10 1 for i { i print } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
}

// Error Recovery

TEST(MissingClosingBrace) {
	Qd::Ast ast;
	const char* src = "fn test() { 42";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null even with error");
	ASSERT(ast.hasErrors(), "should have errors for missing brace");
}

TEST(MissingOpeningBrace) {
	Qd::Ast ast;
	const char* src = "fn test() 42 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	// Parser should report an error
}

TEST(UnterminatedString) {
	Qd::Ast ast;
	const char* src = "fn main() { \"unterminated }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(ast.hasErrors(), "should have errors for unterminated string");
}

TEST(MissingFunctionName) {
	Qd::Ast ast;
	const char* src = "fn () {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(ast.hasErrors(), "should have errors for missing function name");
}

TEST(RecoveryAfterError) {
	Qd::Ast ast;
	const char* src = "fn bad( fn good() { 42 }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	// Parser should try to recover and parse second function
}

TEST(MultipleConsecutiveErrors) {
	Qd::Ast ast;
	const char* src = "fn { fn { fn good() {}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(ast.hasErrors(), "should have errors");
}

// Mixed Complex Programs

TEST(CompleteProgram) {
	Qd::Ast ast;
	const char* src = R"(
		use std

		const MAX = 100

		struct Point {
			x:i64
			y:i64
		}

		fn add_points(a:Point b:Point -- r:Point) {
			a.x b.x add -> rx
			a.y b.y add -> ry
			rx ry Point
		}

		fn main() {
			10 20 Point -> p1
			30 40 Point -> p2
			p1 p2 add_points -> result
			result.x print nl
			result.y print nl
		}
	)";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(root->childCount() >= 4, "should have use, const, struct, and functions");
}

int main() {
	return UC_PrintResults();
}
