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

TEST(StringLiteral) {
	const char* src = "fn main() { \"hello\" print }";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for string literal");
	ASSERT(irContains(ir, "hello"), "should contain string content");
}

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

// Target triple tests for cross-compilation support

TEST(SetTargetTripleAarch64) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	gen.setTargetTriple("aarch64-linux-gnu");
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with aarch64 target");
	ASSERT(!gen.getIRString().empty(), "should produce IR");
}

TEST(SetTargetTripleX86_64) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	gen.setTargetTriple("x86_64-linux-gnu");
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with x86_64 target");
}

TEST(SetTargetTripleArmMacOS) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	gen.setTargetTriple("aarch64-apple-darwin");
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with aarch64-apple-darwin target");
}

TEST(SetTargetTripleEmptyUsesDefault) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	gen.setTargetTriple(""); // Empty string should use default
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with default target when empty");
}

TEST(SetTargetTripleBeforeGenerate) {
	const char* src = "fn main() { 42 print }";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse successfully");

	Qd::LlvmGenerator gen;
	// Set target triple before generate() - should work
	gen.setTargetTriple("aarch64-linux-gnu");
	gen.setOptimizationLevel(2);
	gen.setStackSize(2048);
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with all settings applied");
}

TEST(TargetTripleWithComplexCode) {
	const char* src = R"(
		struct Point {
			x:i64
			y:i64
		}
		fn add_points(a:Point b:Point -- c:Point) {
			Point {
				x = a @x b @x +
				y = a @y b @y +
			}
		}
		fn main() {
			Point { x = 10 y = 20 } -> p1
			Point { x = 5 y = 15 } -> p2
			p1 p2 add_points -> result
			result @x print nl
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse complex code");

	Qd::SemanticValidator validator;
	size_t errors = validator.validate(root, "test.qd");
	ASSERT(errors == 0, "should validate complex code");

	Qd::LlvmGenerator gen;
	gen.setTargetTriple("aarch64-linux-gnu");
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate complex code for aarch64");
}

TEST(NestedStructAccess) {
	const char* src = R"(
		struct Inner {
			value:i64
		}
		struct Outer {
			inner:Inner
			count:i64
		}
		fn main() {
			Inner { value = 42 } -> i
			Outer { inner = i count = 1 } -> o
			o @inner @value print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for nested struct access");
	ASSERT(irContains(ir, "getelementptr"), "should contain struct field access");
}

TEST(StructFieldAccess) {
	const char* src = R"(
		struct Counter {
			value:i64
		}
		fn main() {
			Counter { value = 10 } -> c
			c @value 1 + print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for struct field access");
}

TEST(StructAsParameter) {
	const char* src = R"(
		struct Point {
			x:i64
			y:i64
		}
		fn distance_squared(p:Point -- d:i64) {
			p @x p @x * p @y p @y * +
		}
		fn main() {
			Point { x = 3 y = 4 } distance_squared print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for struct as parameter");
}

TEST(StructWithStringField) {
	const char* src = R"(
		struct Person {
			name:str
			age:i64
		}
		fn main() {
			Person { name = "Alice" age = 30 } -> p
			p @name print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for struct with string field");
}

TEST(MultipleDeferStatements) {
	const char* src = R"(
		fn main() {
			defer { 1 print }
			defer { 2 print }
			defer { 3 print }
			0 print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for multiple defers");
	// Multiple defers execute in LIFO order
}

TEST(DeferInLoop) {
	const char* src = R"(
		fn main() {
			0 3 1 for i {
				defer { i print }
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for defer in loop");
}

TEST(DeferWithLocalVariable) {
	const char* src = R"(
		fn main() {
			42 -> x
			defer { x print }
			x 1 + -> x
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for defer with local variable");
}

TEST(NestedDeferScopes) {
	const char* src = R"(
		fn main() {
			defer { 1 print }
			1 1 eq if {
				defer { 2 print }
			}
			defer { 3 print }
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for nested defer scopes");
}

TEST(SimpleAnonymousFunction) {
	const char* src = R"(
		fn apply(x:i64 f:ptr -- r:i64) {
			-> f -> x
			x f call
		}
		fn main() {
			fn (n:i64 -- r:i64) { -> n n 2 * } -> doubler
			5 doubler apply print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for anonymous function");
}

TEST(ClosureWithCapture) {
	const char* src = R"(
		fn main() {
			10 -> multiplier
			fn (x:i64 -- r:i64) { -> x x multiplier mul } -> times_mult
			5 times_mult call print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for closure with capture");
}

TEST(HigherOrderFunction) {
	const char* src = R"(
		fn apply(x:i64 f:ptr -- r:i64) {
			-> f -> x
			x f call
		}
		fn double(n:i64 -- r:i64) { -> n n 2 * }
		fn main() {
			fn (n:i64 -- r:i64) { -> n n 2 * } -> doubler
			5 doubler apply print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for higher-order function");
	ASSERT(irContains(ir, "call"), "should contain function calls");
}

TEST(ClosureNoCapture) {
	const char* src = R"(
		fn main() {
			fn (a:i64 b:i64 -- r:i64) { -> b -> a a b add } -> add_fn
			3 4 add_fn call print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for closure without capture");
}

TEST(OptimizationLevelThree) {
	const char* src = R"(
		fn main() {
			0 -> sum
			0 100 1 for i {
				sum i + -> sum
			}
			sum print
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse loop code");

	Qd::SemanticValidator validator;
	ASSERT(validator.validate(root, "test.qd") == 0, "should validate");

	Qd::LlvmGenerator gen;
	gen.setOptimizationLevel(3);
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with O3 optimization");
}

TEST(ConstantFolding) {
	const char* src = R"(
		fn main() {
			2 3 * 4 + print
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, "test.qd");
	ASSERT(root != nullptr, "should parse constant expr");

	Qd::SemanticValidator validator;
	ASSERT(validator.validate(root, "test.qd") == 0, "should validate");

	Qd::LlvmGenerator gen;
	gen.setOptimizationLevel(2);
	bool result = gen.generate(root, "test");
	ASSERT(result, "should generate with constant folding");
	std::string ir = gen.getIRString();
	// With optimization, constant expressions may be folded
	ASSERT(!ir.empty(), "should have IR output");
}

TEST(WhileLoop) {
	const char* src = R"(
		fn main() {
			0 -> i
			i 5 lt while {
				i print
				i 1 + -> i
				i 5 lt
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for while loop");
}

TEST(BreakInLoop) {
	const char* src = R"(
		fn main() {
			0 10 1 for i {
				i 5 eq if { break }
				i print
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for break statement");
}

TEST(ContinueInLoop) {
	const char* src = R"(
		fn main() {
			0 10 1 for i {
				i 2 mod 0 eq if { continue }
				i print
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for continue statement");
}

TEST(SwitchStatement) {
	const char* src = R"(
		fn main() {
			2 switch {
				1 { "one" print }
				2 { "two" print }
				_ { "other" print }
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for switch statement");
}

TEST(NestedControlFlow) {
	const char* src = R"(
		fn main() {
			0 3 1 for i {
				0 3 1 for j {
					i j eq if {
						i print
					}
				}
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for nested control flow");
}

TEST(MethodCallOnStruct) {
	const char* src = R"(
		struct Counter { value:i64 }
		fn (c:Counter) getValue(-- r:i64) { c @value }
		fn main() {
			Counter { value = 5 } -> c
			c getValue print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for method call on struct");
}

TEST(ArrayLiteral) {
	const char* src = R"(
		fn main() {
			[1 2 3] -> arr
			arr 0 nth print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for array literal");
}

TEST(NegationOperator) {
	const char* src = R"(
		fn main() {
			42 neg print
			3.14 neg print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for negation operator");
}

TEST(BitwiseXor) {
	const char* src = R"(
		fn main() {
			5 3 xor print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for xor operation");
}

TEST(ModuloOperator) {
	const char* src = R"(
		fn main() {
			17 5 mod print
			17 5 % print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for modulo operator");
}

TEST(MultipleReturnValuesUsed) {
	const char* src = R"(
		fn divmod(a:i64 b:i64 -- quot:i64 rem:i64) {
			over over div
			rot rot mod
		}
		fn main() {
			17 5 divmod -> rem -> quot
			quot print rem print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for multiple return values");
}

TEST(CtxStatement) {
	const char* src = R"(
		fn main() {
			ctx {
				42 print
			}
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for ctx statement");
}

TEST(LogicalOperations) {
	const char* src = R"(
		fn main() {
			1 0 and print
			1 0 or print
			1 not print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for logical operations");
}

TEST(ShiftOperations) {
	const char* src = R"(
		fn main() {
			8 2 shl print
			8 2 shr print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for shift operations");
}

TEST(IncrementDecrement) {
	const char* src = R"(
		fn main() {
			5 inc print
			5 dec print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for inc/dec");
}

TEST(FloatToIntConversion) {
	const char* src = R"(
		fn main() {
			3.7 cast<i64> print
			42 cast<f64> print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for type conversions");
}

TEST(StackPickRoll) {
	const char* src = R"(
		fn main() {
			1 2 3 2 pick print
			1 2 3 2 roll print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for pick and roll");
}

TEST(StackNipTuck) {
	const char* src = R"(
		fn main() {
			1 2 nip print
			1 2 tuck drop drop print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for nip and tuck");
}

TEST(StructMethodSum) {
	const char* src = R"(
		struct Point { x:i64 y:i64 }
		fn (p:Point) sum(-- s:i64) { p @x p @y add }
		fn main() {
			Point { x = 3 y = 4 } -> pt
			pt sum print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for struct method");
}

TEST(PublicFunction) {
	const char* src = R"(
		pub fn exported(x:i64 -- r:i64) { 2 mul }
		fn main() {
			5 exported print
		}
	)";
	std::string ir = generateIR(src);
	ASSERT(!ir.empty(), "should generate IR for public function");
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
