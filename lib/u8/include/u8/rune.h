#ifndef QD_U8_RUNE_H
#define QD_U8_RUNE_H

namespace Qd {
	typedef char32_t Rune;

	inline bool isDigit(Rune r) {
		return r >= U'0' && r <= U'9';
	}
}

#endif
