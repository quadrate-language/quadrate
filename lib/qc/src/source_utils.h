#ifndef QD_QC_SOURCE_UTILS_H
#define QD_QC_SOURCE_UTILS_H

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Qd {

	/**
	 * @brief Precomputed line offset table for O(log n) line/column lookup
	 *
	 * Build once at the start of parsing, then use for all position lookups.
	 */
	class SourceLineMap {
	public:
		explicit SourceLineMap(const char* src) : mSrc(src) {
			// Build table of byte offsets for each line start
			mLineStarts.push_back(0); // Line 1 starts at byte 0
			for (size_t i = 0; src[i] != '\0'; i++) {
				if (src[i] == '\n') {
					mLineStarts.push_back(i + 1); // Next line starts after newline
				}
			}
		}

		/**
		 * @brief Get line and column from byte position - O(log n)
		 */
		void getLineColumn(size_t bytePos, size_t* line, size_t* column) const {
			// Binary search for the line containing this byte position
			auto it = std::upper_bound(mLineStarts.begin(), mLineStarts.end(), bytePos);
			size_t lineIndex = (it == mLineStarts.begin())
									   ? 0
									   : static_cast<size_t>(it - mLineStarts.begin() - 1);
			*line = lineIndex + 1; // 1-based line number
			*column = bytePos - mLineStarts[lineIndex] + 1; // 1-based column
		}

	private:
		const char* mSrc;
		std::vector<size_t> mLineStarts;
	};

	/**
	 * @brief Precomputed char-to-byte offset table for O(1) lookup
	 *
	 * For ASCII-heavy source files, this is very compact.
	 * Build once, use for all charIndex -> byteOffset conversions.
	 */
	class CharByteMap {
	public:
		explicit CharByteMap(const char* src) {
			// Build mapping from character index to byte offset
			size_t byteOffset = 0;
			while (src[byteOffset] != '\0') {
				mCharToByteOffset.push_back(byteOffset);
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
				for (size_t i = 0; i < seqLen && src[byteOffset] != '\0'; i++) {
					byteOffset++;
				}
			}
			mCharToByteOffset.push_back(byteOffset); // End position
		}

		/**
		 * @brief Get byte offset from character index - O(1)
		 */
		size_t getByteOffset(size_t charIndex) const {
			if (charIndex >= mCharToByteOffset.size()) {
				return mCharToByteOffset.empty() ? 0 : mCharToByteOffset.back();
			}
			return mCharToByteOffset[charIndex];
		}

	private:
		std::vector<size_t> mCharToByteOffset;
	};

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
