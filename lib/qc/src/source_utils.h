#ifndef QD_QC_SOURCE_UTILS_H
#define QD_QC_SOURCE_UTILS_H

#include <cstddef>

namespace Qd {

	/**
	 * @brief Calculate line and column from byte position in source
	 *
	 * @param src Source string
	 * @param pos Byte position in the source
	 * @param[out] line Line number (1-based)
	 * @param[out] column Column number (1-based)
	 */
	inline void calculateLineColumn(const char* src, size_t pos, size_t* line, size_t* column) {
		*line = 1;
		*column = 1;
		for (size_t i = 0; i < pos && src[i] != '\0'; i++) {
			if (src[i] == '\n') {
				(*line)++;
				*column = 1;
			} else {
				(*column)++;
			}
		}
	}

	/**
	 * @brief Convert UTF-8 character index to byte offset
	 *
	 * @param src Source string (UTF-8 encoded)
	 * @param charIndex Character (codepoint) index
	 * @return Byte offset in the string
	 */
	inline size_t charIndexToByteOffset(const char* src, size_t charIndex) {
		size_t byteOffset = 0;
		size_t currentCharIndex = 0;

		while (currentCharIndex < charIndex && src[byteOffset] != '\0') {
			unsigned char c = static_cast<unsigned char>(src[byteOffset]);
			if ((c & 0x80) == 0) {
				byteOffset += 1;
			} else if ((c & 0xE0) == 0xC0) {
				byteOffset += 2;
			} else if ((c & 0xF0) == 0xE0) {
				byteOffset += 3;
			} else if ((c & 0xF8) == 0xF0) {
				byteOffset += 4;
			} else {
				byteOffset += 1;
			}
			currentCharIndex++;
		}

		return byteOffset;
	}

	/**
	 * @brief Count UTF-8 characters (codepoints) in a byte range
	 *
	 * @param start Start of byte range
	 * @param end End of byte range
	 * @return Number of UTF-8 characters
	 */
	inline size_t countUtf8Chars(const char* start, const char* end) {
		size_t count = 0;
		const char* p = start;
		while (p < end && *p != '\0') {
			unsigned char c = static_cast<unsigned char>(*p);
			if ((c & 0x80) == 0) {
				p += 1;
			} else if ((c & 0xE0) == 0xC0) {
				p += 2;
			} else if ((c & 0xF0) == 0xE0) {
				p += 3;
			} else if ((c & 0xF8) == 0xF0) {
				p += 4;
			} else {
				p += 1;
			}
			count++;
		}
		return count;
	}

} // namespace Qd

#endif // QD_QC_SOURCE_UTILS_H
