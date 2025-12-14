/**
 * @file test_llvmgen.cc
 * @brief Unit tests for the LLVM code generator
 */

#include <cstring>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/semantic_validator.h>
#include <string>
#include <unit-check/uc.h>

// Helper function to generate IR from source code
std::string generateIR(const char* src) {
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	if (!root || ast.hasErrors()) {
		return "";
	}

	// Validate first
	Qd::SemanticValidator validator;
	if (validator.validate(root, "test.qd") > 0) {
		return "";
	}

	Qd::LlvmGenerator gen;
	if (!gen.generate(root, "test")) {
		return "";
	}

	return gen.getIRString();
}

// Helper to check if IR contains a pattern
bool irContains(const std::string& ir, const std::string& pattern) {
	return ir.find(pattern) != std::string::npos;
}

// ========== Basic Generation Tests ==========

TEST(EmptyMainFunction) {
	const char* src = "fn main() { }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for empty main");
	ASSERT(irContains(ir, "define"), "should contain function definition");
	ASSERT(irContains(ir, "main"), "should reference main function");
}

TEST(SimplePrint) {
	const char* src = "fn main() { 42 print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for simple print");
	ASSERT(irContains(ir, "42"), "should contain literal 42");
}

// ========== Arithmetic Tests ==========

TEST(IntegerAddition) {
	const char* src = "fn main() { 10 20 add print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for addition");
	ASSERT(irContains(ir, "10") || irContains(ir, "20"), "should contain operands");
}

TEST(IntegerMultiplication) {
	const char* src = "fn main() { 5 6 mul print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for multiplication");
}

TEST(IntegerSubtraction) {
	const char* src = "fn main() { 100 30 sub print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for subtraction");
}

TEST(IntegerDivision) {
	const char* src = "fn main() { 100 5 div print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for division");
}

TEST(ChainedArithmetic) {
	const char* src = "fn main() { 1 2 add 3 mul 4 sub print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for chained arithmetic");
}

// ========== Float Tests ==========

TEST(FloatLiteral) {
	const char* src = "fn main() { 3.14 print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for float literal");
}

TEST(FloatArithmetic) {
	const char* src = "fn main() { 1.5 2.5 add print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for float arithmetic");
}

// ========== String Tests ==========

TEST(StringLiteral) {
	const char* src = "fn main() { \"hello\" print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for string literal");
	ASSERT(irContains(ir, "hello"), "should contain string content");
}

// ========== Stack Operation Tests ==========

TEST(StackDup) {
	const char* src = "fn main() { 42 dup add print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for dup operation");
}

TEST(StackSwap) {
	const char* src = "fn main() { 10 20 swap sub print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for swap operation");
}

TEST(StackDrop) {
	const char* src = "fn main() { 10 20 drop print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for drop operation");
}

TEST(StackOver) {
	const char* src = "fn main() { 10 20 over add add print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for over operation");
}

TEST(StackRot) {
	const char* src = "fn main() { 1 2 3 rot add add print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for rot operation");
}

// ========== Control Flow Tests ==========

TEST(IfStatementTrue) {
	const char* src = R"(
		fn main() {
			1 if {
				100 print
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for if statement");
	ASSERT(irContains(ir, "br"), "should contain branch instruction");
}

TEST(IfElseStatement) {
	const char* src = R"(
		fn main() {
			0 if {
				100 print
			} else {
				200 print
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for if-else statement");
}

TEST(ForLoop) {
	const char* src = R"(
		fn main() {
			0 5 1 for i {
				i print
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for for loop");
}

TEST(LoopStatement) {
	const char* src = R"(
		fn main() {
			0 -> count
			loop {
				count 3 gte if {
					break
				}
				count inc -> count
			}
			count print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for loop statement");
}

// ========== Function Tests ==========

TEST(SimpleFunctionCall) {
	const char* src = R"(
		fn add_one( x:i64 -- y:i64 ) {
			1 +
		}
		fn main() {
			41 add_one print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for function call");
	ASSERT(irContains(ir, "add_one"), "should reference add_one function");
}

TEST(FunctionMultipleParams) {
	const char* src = R"(
		fn add_three( a:i64 b:i64 c:i64 -- sum:i64 ) {
			+ +
		}
		fn main() {
			1 2 3 add_three print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for multi-param function");
}

TEST(FunctionMultipleReturns) {
	const char* src = R"(
		fn get_pair( -- a:i64 b:i64 ) {
			10 20
		}
		fn main() {
			get_pair add print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for multi-return function");
}

TEST(RecursiveFunction) {
	const char* src = R"(
		fn factorial( n:i64 -- r:i64 ) {
			-> num
			num 1 lte if {
				1
			} else {
				num num 1 sub factorial mul
			}
		}
		fn main() {
			5 factorial print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for recursive function");
	ASSERT(irContains(ir, "factorial"), "should reference factorial function");
}

// ========== Local Variable Tests ==========

TEST(LocalVariable) {
	const char* src = R"(
		fn main() {
			42 -> x
			x print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for local variable");
	ASSERT(irContains(ir, "alloca"), "should contain alloca for local");
}

TEST(MultipleLocals) {
	const char* src = R"(
		fn main() {
			10 -> a
			20 -> b
			a b add print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for multiple locals");
}

// ========== Comparison Tests ==========

TEST(IntegerComparison) {
	const char* src = R"(
		fn main() {
			10 20 lt print
			10 10 eq print
			20 10 gt print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for comparisons");
	ASSERT(irContains(ir, "icmp"), "should contain integer compare");
}

// ========== Struct Tests ==========

TEST(StructDefinition) {
	const char* src = R"(
		struct Point {
			x:i64
			y:i64
		}
		fn main() {
			Point {
				x = 10
				y = 20
			} -> p
			p @x p @y add print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for struct");
	ASSERT(irContains(ir, "Point") || irContains(ir, "getelementptr"), "should reference struct");
}

// ========== Defer Tests ==========

TEST(DeferStatement) {
	const char* src = R"(
		fn main() {
			defer {
				1 print
			}
			2 print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for defer");
}

// ========== Constant Tests ==========

TEST(ConstantDefinition) {
	const char* src = R"(
		const answer = 42
		fn main() {
			answer print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR with constant");
	ASSERT(irContains(ir, "42"), "should contain constant value");
}

// ========== Boolean Tests ==========

TEST(BooleanOperations) {
	const char* src = R"(
		fn main() {
			1 1 and print
			1 0 or print
			1 not print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for boolean operations");
}

// ========== Error Handling Tests ==========

TEST(InvalidCodeReturnsEmpty) {
	const char* src = "fn main() { undefined_func }";
	std::string ir = generateIR(src);
	ASSERT(ir.empty(), "invalid code should return empty IR");
}

TEST(SyntaxErrorReturnsEmpty) {
	const char* src = "fn main( { }";
	std::string ir = generateIR(src);
	ASSERT(ir.empty(), "syntax error should return empty IR");
}

// ========== Optimization Level Tests ==========

TEST(OptimizationLevelZero) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	gen.setOptimizationLevel(0);
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with O0");
	ASSERT(!gen.getIRString().empty(), "should produce IR");
}

TEST(OptimizationLevelTwo) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	gen.setOptimizationLevel(2);
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with O2");
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
