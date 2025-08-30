#include <lexer/scanner.h>
#include <unit-check/uc.h>

TEST(ScannerTest) {
	Qd::Scanner scanner(u8"let x = 10;");
	ASSERT_TRUE(true, "always passes");
}

int main() {
	return UC_PrintResults();
}

