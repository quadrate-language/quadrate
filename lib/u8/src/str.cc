#include "utf8.h"
#include <u8/str.h>

namespace Qd {
	bool Str::isValid(const std::string& str) {
		return utf8::is_valid(str.begin(), str.end());
	}

	void Str::append(Rune r, std::string& str) {
		utf8::append(r, std::back_inserter(str));
	}

	char32_t Str::next(std::string::const_iterator& itr, std::string::const_iterator end) {
		return utf8::next(itr, end);
	}
}
