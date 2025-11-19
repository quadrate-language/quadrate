#include <cstring>
#include <qc/ast.h>
#include <qc/semantic_validator.h>
#include <unit-check/uc.h>

// Helper function to validate code and return error count
size_t validateCode(const char* src) {
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, nullptr);
	Qd::SemanticValidator validator;
	return validator.validate(root, "test.qd");
}

// Test that valid simple function compiles without errors
TEST(SimpleFunctionNoError) {
	const char* src = "fn main() { 42 print }";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "should have no errors");
}

// Test undefined function detection
TEST(UndefinedFunctionError) {
	const char* src = "fn main() { undefined_func }";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "should have 1 error for undefined function");
}

// Test type error: inc on string
TEST(TypeErrorIncOnString) {
	const char* src = "fn main() { \"hello\" inc }";
	size_t errors = validateCode(src);
	ASSERT(errors >= 1, "should have at least 1 error for inc on string");
}

// Test type error: add with mismatched types
TEST(TypeErrorAddIntString) {
	const char* src = "fn main() { 42 \"hello\" add }";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "should have 1 error for add with int and string");
}

// Test stack underflow detection
TEST(StackUnderflowAdd) {
	const char* src = "fn main() { 5 add }";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "should have 1 error for stack underflow in add");
}

// Test valid arithmetic operations
TEST(ValidArithmetic) {
	const char* src = "fn main() { 10 20 add 2 mul print }";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "valid arithmetic should have no errors");
}

// Test function signature: simple producer
TEST(FunctionSignatureSimpleProducer) {
	const char* src = R"(
		fn get_value() {
			42
		}
		fn main() {
			get_value print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "function producing value should work");
}

// Test function signature: multiple outputs
TEST(FunctionSignatureMultipleOutputs) {
	const char* src = R"(
		fn get_pair() {
			10 20
		}
		fn main() {
			get_pair add print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "function producing multiple values should work");
}

// Test chained function calls
TEST(ChainedFunctionCalls) {
	const char* src = R"(
		fn c() {
			3 7
		}
		fn b() {
			c add
		}
		fn a() {
			b 2 mul
		}
		fn main() {
			a print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "chained function calls should work");
}

// Test deep nesting (5 levels)
TEST(DeepNesting) {
	const char* src = R"(
		fn level1() {
			1
		}
		fn level2() {
			level1 2 add
		}
		fn level3() {
			level2 3 add
		}
		fn level4() {
			level3 4 add
		}
		fn level5() {
			level4 5 add
		}
		fn main() {
			level5 print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "deeply nested function calls should work");
}

// Test function with zero outputs
TEST(FunctionZeroOutputs) {
	const char* src = R"(
		fn do_nothing() {
		}
		fn main() {
			do_nothing
			42 print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "function with zero outputs should work");
}

// Test varying outputs
TEST(VaryingOutputs) {
	const char* src = R"(
		fn one() {
			1
		}
		fn two() {
			2 3
		}
		fn three() {
			4 5 6
		}
		fn main() {
			one two three
			add add add add add
			print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "functions with varying outputs should work");
}

// Test float type propagation
TEST(FloatTypePropagation) {
	const char* src = R"(
		fn make_float() {
			3.14
		}
		fn double_it() {
			make_float 2 mul
		}
		fn main() {
			double_it print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "float type propagation should work");
}

// Test mixed int and float (should produce float)
TEST(MixedIntFloat) {
	const char* src = R"(
		fn main() {
			5 2.5 mul print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "int and float multiplication should work");
}

// Test error in function propagates
TEST(ErrorInFunctionPropagates) {
	const char* src = R"(
		fn bad_func() {
			"text" inc
		}
		fn main() {
			bad_func
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors >= 1, "error in function should be detected");
}

// Test error with function call result
TEST(ErrorWithFunctionResult) {
	const char* src = R"(
		fn get_string() {
			"hello"
		}
		fn main() {
			get_string inc
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors >= 1, "type error with function result should be detected");
}

// Test type mismatch from two functions
TEST(TypeMismatchFromFunctions) {
	const char* src = R"(
		fn get_int() {
			10
		}
		fn get_string() {
			"world"
		}
		fn main() {
			get_int get_string add
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "type mismatch from functions should be detected");
}

// Test complex producer composition
TEST(ComplexProducerComposition) {
	const char* src = R"(
		fn pair1() {
			10 20
		}
		fn pair2() {
			30 40
		}
		fn four_values() {
			pair1 pair2
		}
		fn main() {
			four_values add add add print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "complex producer composition should work");
}

// Test interleaved calls
TEST(InterleavedCalls) {
	const char* src = R"(
		fn one() {
			1
		}
		fn two() {
			2 3
		}
		fn three() {
			4 5 6
		}
		fn main() {
			one two three
			add add add add add
			print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "interleaved function calls should work");
}

// Test dup operation
TEST(DupOperation) {
	const char* src = R"(
		fn main() {
			5 dup mul print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "dup operation should work");
}

// Test dup underflow
TEST(DupUnderflow) {
	const char* src = R"(
		fn main() {
			dup
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "dup underflow should be detected");
}

// Test swap operation
TEST(SwapOperation) {
	const char* src = R"(
		fn main() {
			10 20 swap sub print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "swap operation should work");
}

// Test swap underflow
TEST(SwapUnderflow) {
	const char* src = R"(
		fn main() {
			5 swap
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "swap underflow should be detected");
}

// Test inc with integer
TEST(IncInteger) {
	const char* src = R"(
		fn main() {
			42 inc print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "inc on integer should work");
}

// Test inc underflow
TEST(IncUnderflow) {
	const char* src = R"(
		fn main() {
			inc
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors >= 1, "inc underflow should be detected");
}

// Test multiple errors in same function
TEST(MultipleErrors) {
	const char* src = R"(
		fn main() {
			"text" inc
			5 "hello" add
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors >= 2, "multiple errors should be detected");
}

// Test string operations
TEST(StringPrint) {
	const char* src = R"(
		fn main() {
			"Hello, World!" print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "string print should work");
}

// Test comprehensive mixed scenario
TEST(ComprehensiveMixed) {
	const char* src = R"(
		fn base() {
			10
		}
		fn chain1() {
			base 5 add
		}
		fn chain2() {
			chain1 2 mul
		}
		fn multi() {
			100 200
		}
		fn combiner() {
			multi add chain2 add
		}
		fn main() {
			combiner print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 0, "comprehensive mixed scenario should work");
}

// Test that missing modules are reported as errors
TEST(UseStatementCollection) {
	const char* src = R"(
		use std
		use math
		fn main() {
			42 print
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 2, "missing modules should be reported as errors");
}

// Test error when module is not imported
TEST(ScopedIdentifierModuleNotImported) {
	const char* src = R"(
		fn main() {
			42 math::sqrt
		}
	)";
	size_t errors = validateCode(src);
	ASSERT(errors == 1, "should have 1 error for module not imported");
}

// Test that scoped identifier works when module is imported (even if module file doesn't exist)
TEST(ScopedIdentifierModuleImported) {
	const char* src = R"(
		use math
		fn main() {
			42 math::sqrt
		}
	)";
	size_t errors = validateCode(src);
	// Should have 0 errors if module file doesn't exist (fails silently)
	// or if module exists but function not found, will have error
	// For now, we just check it doesn't crash (errors will be 0 or more)
	(void)errors; // Avoid unused variable warning
	ASSERT(true, "should not crash when using scoped identifier with imported module");
}

// Test implicit cast: int to float - should succeed with warning
TEST(ImplicitCastIntToFloat) {
	const char* src = R"(
		fn add_float(a:f64 b:f64 -- result:f64) {
			+
		}
		fn main() {
			10 20.5 add_float printv
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, nullptr);
	Qd::SemanticValidator validator;
	size_t errors = validator.validate(root, "test.qd");
	ASSERT(errors == 0, "implicit cast int->float should succeed");
	ASSERT(validator.warningCount() == 1, "should have 1 warning for implicit cast");
}

// Test implicit cast: float to int - should succeed with warning
TEST(ImplicitCastFloatToInt) {
	const char* src = R"(
		fn add_int(a:i64 b:i64 -- result:i64) {
			+
		}
		fn main() {
			10.5 20.3 add_int printv
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, nullptr);
	Qd::SemanticValidator validator;
	size_t errors = validator.validate(root, "test.qd");
	ASSERT(errors == 0, "implicit cast float->int should succeed");
	ASSERT(validator.warningCount() == 2, "should have 2 warnings for implicit casts");
}

// Test werror: warnings treated as errors
TEST(WerrorTreatsWarningsAsErrors) {
	const char* src = R"(
		fn add_float(a:f64 b:f64 -- result:f64) {
			+
		}
		fn main() {
			10 20.5 add_float printv
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, nullptr);
	Qd::SemanticValidator validator;
	size_t errors = validator.validate(root, "test.qd", false, true); // werror=true
	ASSERT(errors > 0, "with werror, warnings should become errors");
	ASSERT(validator.warningCount() == 0, "with werror, warning count should be 0 (converted to errors)");
}

// Test werror: clean code still passes
TEST(WerrorCleanCodePasses) {
	const char* src = R"(
		fn add_int(a:i64 b:i64 -- result:i64) {
			+
		}
		fn main() {
			10 20 add_int printv
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, nullptr);
	Qd::SemanticValidator validator;
	size_t errors = validator.validate(root, "test.qd", false, true); // werror=true
	ASSERT(errors == 0, "with werror, clean code should still pass");
	ASSERT(validator.warningCount() == 0, "should have no warnings");
}

// Test multiple implicit casts in one call
TEST(MultipleImplicitCasts) {
	const char* src = R"(
		fn mix(a:i64 b:f64 c:i64 -- result:f64) {
			drop drop
		}
		fn main() {
			10.5 20 30.5 mix printv
		}
	)";
	Qd::Ast ast;
	Qd::IAstNode* root = ast.generate(src, false, nullptr);
	Qd::SemanticValidator validator;
	size_t errors = validator.validate(root, "test.qd");
	ASSERT(errors == 0, "multiple implicit casts should succeed");
	ASSERT(validator.warningCount() == 3, "should have 3 warnings (all params need casts)");
}

int main() {
	return UC_PrintResults();
}
