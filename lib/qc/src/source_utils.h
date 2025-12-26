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
			size_t seqLen = 1;
			if ((c & 0x80) == 0) {
				seqLen = 1;
			} else if ((c & 0xE0) == 0xC0) {
				seqLen = 2;
			} else if ((c & 0xF0) == 0xE0) {
				seqLen = 3;
			} else if ((c & 0xF8) == 0xF0) {
				seqLen = 4;
			}
			// Safely advance, stopping at null terminator
			for (size_t i = 0; i < seqLen && src[byteOffset] != '\0'; i++) {
				byteOffset++;
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
			size_t seqLen = 1;
			if ((c & 0x80) == 0) {
				seqLen = 1;
			} else if ((c & 0xE0) == 0xC0) {
				seqLen = 2;
			} else if ((c & 0xF0) == 0xE0) {
				seqLen = 3;
			} else if ((c & 0xF8) == 0xF0) {
				seqLen = 4;
			}
			// Safely advance, stopping at end or null terminator
			for (size_t i = 0; i < seqLen && p < end && *p != '\0'; i++) {
				p++;
			}
			count++;
		}
		return count;
	}

} // namespace Qd

#endif // QD_QC_SOURCE_UTILS_H
