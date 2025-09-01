#include <lexer/scanner.h>
#include <unit-check/uc.h>

TEST(ScannerTest) {
	std::string source = R"(fn main() {
		push -8
		push "Hellåäö漢"
	})";

	Qd::Scanner scanner(source);
	scanner.lex();
	ASSERT_TRUE(false, "always passes");
}

int main() {
	return UC_PrintResults();
}

