#include <lexer/scanner.h>
#include <unit-check/uc.h>

TEST(ScannerTest) {
	std::string source = R"(fn main() {
		push -8
		push "Hellö, World!"
	})";

	Qd::Scanner scanner(std::u8string_view(reinterpret_cast<const char8_t*>(source.c_str()), source.size()));
	scanner.lex();
	ASSERT_TRUE(false, "always passes");
}

int main() {
	return UC_PrintResults();
}

