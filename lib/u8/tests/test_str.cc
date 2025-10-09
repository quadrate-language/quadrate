#include <u8/str.h>
#include <unit-check/uc.h>
#include <vector>

TEST(StrValidTest) {
	std::string str = "Hello!";
	ASSERT(Qd::Str::isValid(str), "String should be valid UTF-8");
}

TEST(StrInvalidTest) {
	std::string invalid = "\xC1\x81";
	ASSERT_FALSE(Qd::Str::isValid(invalid), "String should be invalid UTF-8");
}

TEST(StrAppendTest) {
	std::string str = "Hello!";
	Qd::Str::append(U'ö', str);

	std::string expected = "Hello!\xC3\xB6";
	ASSERT(str == expected, "Strings should be equal");
}

TEST(StrNextTest) {
	std::string str = "Hello!\xC3\xB6";

	std::vector<char32_t> expected = {U'H', U'e', U'l', U'l', U'o', U'!', U'ö'};
	std::vector<char32_t> actual;
	actual.reserve(expected.size());

	std::string::const_iterator itr = str.begin();
	while (itr != str.end()) {
		char32_t cp = Qd::Str::next(itr, str.end());
		actual.push_back(cp);
	}

	ASSERT_EQ(static_cast<int>(expected.size()), static_cast<int>(actual.size()), "Sizes should be equal");
	for (size_t i = 0; i < expected.size(); i++) {
		ASSERT_EQ(static_cast<int>(expected[i]), static_cast<int>(actual[i]), "Codepoints should be equal");
	}
}

int main() {
	return UC_PrintResults();
}
