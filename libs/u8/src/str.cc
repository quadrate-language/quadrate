#include <u8/str.h>
#include "utf8.h"

namespace U8 {
	bool Str::isValid(const std::string& str) {
		return utf8::is_valid(str.begin(), str.end());
	}

	void Str::append(char32_t c, std::string& str) {
		utf8::append(c, std::back_inserter(str));
	}

	char32_t Str::next(std::string::const_iterator& itr, std::string::const_iterator end) {
		return utf8::next(itr, end);
	}
}

