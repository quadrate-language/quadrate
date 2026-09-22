#include <cstring>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_printer.h>
#include <quadrate/qc/numeric_literal.h>
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

TEST(WhileLoopRemoved) {
	Qd::Ast ast;
	const char* src = "fn main() { 1 while { drop 0 } }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	// while is removed; parser should emit error and return null or partial AST
	// Just verify it doesn't crash
	(void)root;
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

TEST(AnnotatedSwitch) {
	// A dispatch table is where a comment per case is most wanted, and the
	// switch parser used to stop at the first one.
	Qd::Ast ast;
	const char* src = "fn k(x:i64 -- r:i64) {\n"
					  "\tswitch {\n"
					  "\t\t1 { 10 }\t// one\n"
					  "\t\t// and the rest\n"
					  "\t\t_ { 0 }\n"
					  "\t}\n"
					  "}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "comments between cases are not cases");
}

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

// Lexical constructs that run to end of file
//
// u8t ends a string token at EOF exactly as it does at a closing quote, and the
// comment reader stops at EOF the same way, so both used to be accepted silently:
// the swallowed text vanished and what was left looked like a clean parse of a
// shorter program. The formatter then re-emitted the comment with a `*/` the author
// never wrote, or took the rest of the file as a module name and grew it by a
// newline on every pass.

TEST(UnterminatedBlockCommentIsAnError) {
	Qd::Ast ast;
	const char* src = "fn main() { /* a }";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "an unterminated /* must be reported");
}

TEST(UnterminatedNestedBlockCommentIsAnError) {
	Qd::Ast ast;
	const char* src = "fn main() { /* a/* b/* c }";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "an unterminated nested /* must be reported");
}

TEST(NestedBlockCommentClosesAtItsOwnEnd) {
	Qd::Ast ast;
	const char* src = "fn t(){/*/**/}*/}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "the inner */ does not close the outer comment");
}

TEST(UnterminatedStringInUseIsAnError) {
	Qd::Ast ast;
	const char* src = "use \"abc";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "an unterminated string in a use path must be reported");
}

TEST(UnterminatedStringInTestNameIsAnError) {
	Qd::Ast ast;
	const char* src = "test \"abc";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "an unterminated string in a test name must be reported");
}

// Tokens that cannot appear where they were being skipped
//
// Each of these used to be swallowed in silence, which left the source brace-count
// one short of what the parser had actually consumed.

TEST(BraceInNamedParameterListIsAnError) {
	Qd::Ast ast;
	const char* src = "fn i({){}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "a brace in a parameter list must be reported");
}

TEST(BraceInAnonymousParameterListIsAnError) {
	Qd::Ast ast;
	const char* src = "fn n(){fn(}){}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "a brace in an anonymous fn parameter list must be reported");
}

TEST(LoneDashInParameterListIsAnError) {
	Qd::Ast ast;
	const char* src = "fn m(){fn(-}){}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "a '-' that is not part of '--' must be reported");
}

TEST(DeferWithoutABlockIsAnError) {
	Qd::Ast ast;
	const char* src = "fn main() {\n\tdefer\n}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "'defer' without a block must be reported");
}

TEST(UnterminatedStructConstructionIsAnError) {
	Qd::Ast ast;
	const char* src = "var g = r{";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "a struct construction that reaches end of file must be reported");
}

TEST(BraceInTypeArgumentsIsAnError) {
	Qd::Ast ast;
	const char* src = "fn i(){fn(i<}>--){}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "a brace in a type argument list must be reported");
}

TEST(AmpersandWithoutAFunctionNameIsAnError) {
	Qd::Ast ast;
	const char* src = "fn t(){p{&}}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "'&' with no function name after it must be reported");
}

TEST(BraceAsAParameterTypeIsAnError) {
	Qd::Ast ast;
	const char* src = "fn a(){fn(n:}--){}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "a parameter type that is not a type name must be reported");
}

TEST(ArrayParameterTypeStillParses) {
	Qd::Ast ast;
	const char* src = "fn f(x:[]i64 -- r:i64) { x len }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "a well-formed []T parameter must still parse");
}

TEST(BareDollarIsAnError) {
	Qd::Ast ast;
	const char* src = "fn s(){$//f{\n}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "'$' not followed by a string must be reported");
}

TEST(StringInterpolationStillParses) {
	Qd::Ast ast;
	const char* src = "fn s() {\n\t42 -> n\n\t$\"n is {n}\" print nl\n}";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "a well-formed interpolated string must still parse");
}

TEST(GenericTypeParameterStillParses) {
	Qd::Ast ast;
	const char* src = "fn f(b:Box<i64> -- r:i64) { b <<v }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "a well-formed generic parameter must still parse");
}

TEST(ScopeOperatorWithoutANameIsAnError) {
	Qd::Ast ast;
	const char* src = "fn m(){i::}}";
	ast.generate(src, false, nullptr);

	ASSERT(ast.hasErrors(), "'::' with no name after it must be reported");
}

TEST(ParseErrorsComeBackInSourceOrder) {
	Qd::Ast ast;
	// The unterminated string is only detected once the file has been read, well after
	// the missing '}' has been reported, so it has to be sorted back into place.
	const char* src = "fn main() {\n\t\"abc\n";
	ast.generate(src, false, nullptr);

	const auto& errors = ast.getErrors();
	ASSERT(errors.size() >= 2, "both the unterminated string and the missing '}' are reported");
	for (size_t i = 1; i < errors.size(); i++) {
		bool ordered = errors[i - 1].line < errors[i].line ||
					   (errors[i - 1].line == errors[i].line && errors[i - 1].column <= errors[i].column);
		ASSERT(ordered, "errors must be reported in source order");
	}
}

TEST(ScopedIdentifierStillParses) {
	Qd::Ast ast;
	const char* src = "fn m() { math::abs }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "a well-formed scoped identifier must still parse");
}

// Float literal spelling (spec 2.3.4)

TEST(FloatLiteralSpellingsThatRead) {
	const char* accepted[] = {"0.5", "3.14", "-0.5", "1e3", "1E3", "1e+3", "1.5e-3", "-2.5E10", "1e-300",
			"1.7976931348623157e+308", "2.2250738585072014e-308", "42"};
	for (const char* text : accepted) {
		double value = 0.0;
		ASSERT(Qd::readFloatLiteral(text, value) == std::errc(), text);
	}

	double value = 0.0;
	ASSERT(Qd::readFloatLiteral("1e3", value) == std::errc() && value == 1000.0, "1e3 is one thousand");
	ASSERT(Qd::readFloatLiteral("-1.5e-3", value) == std::errc() && value == -0.0015, "-1.5e-3 reads exactly");
}

TEST(FloatLiteralSpellingsThatDoNot) {
	// A point with nothing on one side of it, an exponent with no digits, and the
	// separators and hex floats the language has not got.
	const char* rejected[] = {".5", "-.5", "5.", "1.", "1e", "1e+", "1.2.3", "1_000.5", "0x1p3", "", "1f", "e3"};
	for (const char* text : rejected) {
		double value = 0.0;
		ASSERT(Qd::readFloatLiteral(text, value) == std::errc::invalid_argument, text);
	}
}

TEST(FloatLiteralRange) {
	double value = 0.0;
	ASSERT(Qd::readFloatLiteral("1e400", value) == std::errc::result_out_of_range, "1e400 has no f64 to land in");
	ASSERT(Qd::readFloatLiteral("-1e400", value) == std::errc::result_out_of_range, "and neither has -1e400");
	// Underflow is ordinary arithmetic: it lands on a subnormal or on zero.
	ASSERT(Qd::readFloatLiteral("1e-320", value) == std::errc() && value > 0.0, "1e-320 is a subnormal, not an error");
	ASSERT(Qd::readFloatLiteral("1e-400", value) == std::errc() && value == 0.0, "1e-400 underflows to zero");
}

TEST(FloatLiteralTextRecognition) {
	ASSERT(Qd::isFloatLiteralText("1.5"), "a point makes a float");
	ASSERT(Qd::isFloatLiteralText("1e3"), "an exponent makes a float");
	ASSERT(Qd::isFloatLiteralText("-1E3"), "sign and all");
	ASSERT(!Qd::isFloatLiteralText("1000"), "digits alone are an integer");
	ASSERT(!Qd::isFloatLiteralText("0xE1"), "the E of a hex literal is a digit");
	ASSERT(!Qd::isFloatLiteralText("-0xE1"), "sign and all");
	ASSERT(!Qd::isFloatLiteralText("0b1011"), "binary literals are integers");
	ASSERT(!Qd::isFloatLiteralText(""), "nothing is not a float");
}

TEST(ExponentLiteralParsesAsFloat) {
	Qd::Ast ast;
	const char* src = "fn m() { 1e3 print }";
	Qd::IAstNode* root = ast.generate(src, false, nullptr);

	ASSERT(root != nullptr, "root should not be null");
	ASSERT(!ast.hasErrors(), "exponent notation is a float literal, not a parse error");
}

TEST(DigitSeparatorsAreNotANumber) {
	// The language has no digit separator: a `_` is an identifier character, so `1_000` is
	// the integer 1 followed by `_000`, and neither reader takes the spelling as a number.
	const char* rejected[] = {"1_000", "1_000_000", "-1_500", "0xdead_beef", "0b1010_1010", "1_", "_1", "1__0"};
	for (const char* text : rejected) {
		int64_t integer = 0;
		ASSERT(Qd::readIntegerLiteral(text, integer) != std::errc(), text);
	}

	double value = 0.0;
	ASSERT(Qd::readFloatLiteral("1_000.5", value) == std::errc::invalid_argument, "nor is 1_000.5 a float");
	ASSERT(Qd::readFloatLiteral("1_0e1_0", value) == std::errc::invalid_argument, "nor 1_0e1_0");
}

int main() {
	return UC_PrintResults();
}
