#ifndef U8_STR_H
#define U8_STR_H

#include <string>

namespace U8 {
	typedef char32_t Rune;

	class Str {
	public:
		static bool isValid(const std::string& str);
		static void append(Rune r, std::string& str);
		static char32_t next(std::string::const_iterator& itr, std::string::const_iterator end);
	};
}

#endif

