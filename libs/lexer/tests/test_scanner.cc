#include <lexer/scanner.h>
#include <unit-check/uc.h>

TEST(ScannerTest) {
	std::string source = R"(fn main() {
		push -8
		push "Hellåäö漢"
	})";

	Qd::Scanner scanner(source);
	std::vector<Qd::Token> tokens;
	scanner.lex(tokens);

	printf("Tokens: %d\n", tokens.size());
	for (auto token : tokens) {
		printf("Token: %s\n", token.value.c_str());
	}
	fflush(stdout);
	ASSERT_TRUE(false, "always passes");
}

int main() {
	return UC_PrintResults();
}

