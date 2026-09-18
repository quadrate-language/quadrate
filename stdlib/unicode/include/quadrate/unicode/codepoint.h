/**
 * @file codepoint.h
 * @brief Unicode codepoint primitives shared by the unicode and strings modules
 *
 * One implementation, two callers. `unicode` answers questions about a single codepoint and
 * `strings` about a whole string, but "is this a letter" and "what is this uppercased" have
 * to give the same answer in both or a program gets different results depending on which
 * layer it asked -- which is exactly what happened while `unicode` was ASCII-only and
 * `strings` had been made UTF-8 aware: `"e-acute" strings::is_alpha` was true while
 * `unicode::is_alpha` of the same codepoint was false.
 *
 * Header-only and `static inline`, so neither library has to link against the other.
 */

#ifndef QD_UNICODE_CODEPOINT_H
#define QD_UNICODE_CODEPOINT_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================================
 * UTF-8 encoding
 *
 * Malformed input is decoded rather than rejected. A Quadrate string can come from a file, a
 * socket or an argv, so refusing would turn a data problem into a crash; instead a bad byte
 * is treated as one single-byte character, which costs one character rather than
 * desynchronising everything after it. Every *output* built from these is well-formed.
 * ========================================================================================= */

/* Bytes in the sequence led by `c`. Returns 1 for a continuation or invalid lead byte, so a
 * caller that trusts it still advances and cannot loop forever. */
static inline size_t utf8_seq_len(unsigned char c) {
	if (c < 0x80u) {
		return 1;
	}
	if ((c & 0xE0u) == 0xC0u) {
		return 2;
	}
	if ((c & 0xF0u) == 0xE0u) {
		return 3;
	}
	if ((c & 0xF8u) == 0xF0u) {
		return 4;
	}
	return 1;
}

/* Decode the codepoint at `p`, which has `avail` bytes left. Returns the bytes consumed and
 * writes the codepoint. A truncated or malformed sequence consumes exactly one byte and
 * yields that byte's value. */
static inline size_t utf8_decode(const unsigned char* p, size_t avail, uint32_t* out) {
	if (avail == 0) {
		*out = 0;
		return 0;
	}
	const unsigned char lead = p[0];
	const size_t need = utf8_seq_len(lead);
	if (need == 1 || need > avail) {
		*out = lead;
		return 1;
	}
	for (size_t i = 1; i < need; i++) {
		if ((p[i] & 0xC0u) != 0x80u) {
			*out = lead;
			return 1;
		}
	}
	uint32_t cp = 0;
	if (need == 2) {
		cp = (uint32_t)(lead & 0x1Fu);
	} else if (need == 3) {
		cp = (uint32_t)(lead & 0x0Fu);
	} else {
		cp = (uint32_t)(lead & 0x07u);
	}
	for (size_t i = 1; i < need; i++) {
		cp = (cp << 6) | (uint32_t)(p[i] & 0x3Fu);
	}
	*out = cp;
	return need;
}

/* Encode `cp` into `buf` (at least 4 bytes). Returns the bytes written, or 0 if `cp` is not
 * a codepoint -- above U+10FFFF, or a surrogate, which UTF-8 must not encode. */
static inline size_t utf8_encode(uint32_t cp, char* buf) {
	if (cp < 0x80u) {
		buf[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800u) {
		buf[0] = (char)(0xC0u | (cp >> 6));
		buf[1] = (char)(0x80u | (cp & 0x3Fu));
		return 2;
	}
	if (cp < 0x10000u) {
		if (cp >= 0xD800u && cp <= 0xDFFFu) {
			return 0;
		}
		buf[0] = (char)(0xE0u | (cp >> 12));
		buf[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
		buf[2] = (char)(0x80u | (cp & 0x3Fu));
		return 3;
	}
	if (cp <= 0x10FFFFu) {
		buf[0] = (char)(0xF0u | (cp >> 18));
		buf[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
		buf[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
		buf[3] = (char)(0x80u | (cp & 0x3Fu));
		return 4;
	}
	return 0;
}

/* --------------------------------------------------------------------------------------
 * Case mapping
 *
 * Simple, one-codepoint-to-one-codepoint mapping, derived from the structure of the Unicode
 * blocks rather than from a shipped copy of UnicodeData.txt. The cased scripts are Latin,
 * Greek and Cyrillic, and within those the blocks are laid out regularly enough to compute:
 * either a fixed offset between the upper and lower run, or alternating upper/lower pairs.
 *
 * What this deliberately does not do:
 *   - mappings that change length (U+00DF sharp s uppercases to "SS", U+FB01 fi to "FI").
 *     Those are left unchanged rather than silently mangled; a one-to-one API cannot express
 *     them, and turning one character into two would break every index the caller holds.
 *   - locale-dependent mappings (Turkish dotless i, Lithuanian dot-above).
 *   - Latin Extended-B (U+0180-U+024F), whose layout is genuinely irregular.
 * A codepoint with no mapping here is returned unchanged, which is also the right answer for
 * every script that has no case at all -- CJK, Arabic, Hebrew, Devanagari, Thai.
 * ----------------------------------------------------------------------------------- */

/* True when `cp` sits in a block of alternating Upper/lower pairs whose upper member is even. */
static inline int case_pairs_even_upper(uint32_t cp) {
	return (cp >= 0x0100u && cp <= 0x0137u) || (cp >= 0x014Au && cp <= 0x0177u) || (cp >= 0x01DEu && cp <= 0x01EFu) ||
		   (cp >= 0x01F8u && cp <= 0x021Fu) || (cp >= 0x0222u && cp <= 0x0233u) || (cp >= 0x0246u && cp <= 0x024Fu) ||
		   (cp >= 0x0460u && cp <= 0x0481u) || (cp >= 0x048Au && cp <= 0x04FFu) || (cp >= 0x1E00u && cp <= 0x1E95u) ||
		   (cp >= 0x1EA0u && cp <= 0x1EFFu);
}

/* True when `cp` sits in a block of alternating Upper/lower pairs whose upper member is odd. */
static inline int case_pairs_odd_upper(uint32_t cp) {
	return (cp >= 0x0139u && cp <= 0x0148u) || (cp >= 0x0179u && cp <= 0x017Eu);
}

static inline uint32_t utf8_to_lower_cp(uint32_t cp) {
	if (cp < 0x80u) {
		return (cp >= 'A' && cp <= 'Z') ? cp + 32u : cp;
	}
	/* Latin-1 Supplement: C0-DE are upper, E0-FE lower, skipping D7 (multiplication sign). */
	if (cp >= 0x00C0u && cp <= 0x00DEu && cp != 0x00D7u) {
		return cp + 32u;
	}
	if (cp == 0x0178u) {
		return 0x00FFu; /* Y with diaeresis, whose lowercase sits back in Latin-1 */
	}
	if (case_pairs_even_upper(cp)) {
		return (cp % 2u == 0u) ? cp + 1u : cp;
	}
	if (case_pairs_odd_upper(cp)) {
		return (cp % 2u == 1u) ? cp + 1u : cp;
	}
	/* Greek */
	if (cp == 0x0386u) {
		return 0x03ACu;
	}
	if (cp >= 0x0388u && cp <= 0x038Au) {
		return cp + 37u;
	}
	if (cp == 0x038Cu) {
		return 0x03CCu;
	}
	if (cp >= 0x038Eu && cp <= 0x038Fu) {
		return cp + 63u;
	}
	if ((cp >= 0x0391u && cp <= 0x03A1u) || (cp >= 0x03A3u && cp <= 0x03ABu)) {
		return cp + 32u;
	}
	/* Cyrillic */
	if (cp >= 0x0400u && cp <= 0x040Fu) {
		return cp + 80u;
	}
	if (cp >= 0x0410u && cp <= 0x042Fu) {
		return cp + 32u;
	}
	return cp;
}

static inline uint32_t utf8_to_upper_cp(uint32_t cp) {
	if (cp < 0x80u) {
		return (cp >= 'a' && cp <= 'z') ? cp - 32u : cp;
	}
	if (cp == 0x00FFu) {
		return 0x0178u;
	}
	/* E0-FE are lower, skipping F7 (division sign). DF (sharp s) has no single-codepoint
	 * uppercase and is left alone -- see the note above. */
	if (cp >= 0x00E0u && cp <= 0x00FEu && cp != 0x00F7u) {
		return cp - 32u;
	}
	if (cp == 0x017Fu) {
		return 'S'; /* long s */
	}
	if (case_pairs_even_upper(cp)) {
		return (cp % 2u == 1u) ? cp - 1u : cp;
	}
	if (case_pairs_odd_upper(cp)) {
		return (cp % 2u == 0u) ? cp - 1u : cp;
	}
	/* Greek */
	if (cp == 0x03ACu) {
		return 0x0386u;
	}
	if (cp >= 0x03ADu && cp <= 0x03AFu) {
		return cp - 37u;
	}
	if (cp == 0x03C2u) {
		return 0x03A3u; /* final sigma uppercases to plain sigma */
	}
	if (cp == 0x03CCu) {
		return 0x038Cu;
	}
	if (cp >= 0x03CDu && cp <= 0x03CEu) {
		return cp - 63u;
	}
	if ((cp >= 0x03B1u && cp <= 0x03C1u) || (cp >= 0x03C3u && cp <= 0x03CBu)) {
		return cp - 32u;
	}
	/* Cyrillic */
	if (cp >= 0x0450u && cp <= 0x045Fu) {
		return cp - 80u;
	}
	if (cp >= 0x0430u && cp <= 0x044Fu) {
		return cp - 32u;
	}
	return cp;
}

static inline int utf8_is_upper_cp(uint32_t cp) {
	return utf8_to_lower_cp(cp) != cp;
}

static inline int utf8_is_lower_cp(uint32_t cp) {
	return utf8_to_upper_cp(cp) != cp;
}

/* Letter test. Cased letters answer through the mapping; the uncased scripts need their
 * ranges named, since "unchanged by both mappings" cannot distinguish a Han character from
 * a comma. */
static inline int utf8_is_alpha_cp(uint32_t cp) {
	if (cp < 0x80u) {
		return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
	}
	if (utf8_is_upper_cp(cp) || utf8_is_lower_cp(cp)) {
		return 1;
	}
	return (cp >= 0x00AAu && cp <= 0x00AAu) || (cp >= 0x00B5u && cp <= 0x00B5u) || (cp >= 0x00BAu && cp <= 0x00BAu) ||
		   (cp >= 0x00DFu && cp <= 0x00DFu) || (cp >= 0x0100u && cp <= 0x02AFu) || /* Latin Extended-A/B and IPA */
		   (cp >= 0x0370u && cp <= 0x03FFu) ||									   /* Greek */
		   (cp >= 0x0400u && cp <= 0x052Fu) ||									   /* Cyrillic */
		   (cp >= 0x0531u && cp <= 0x058Fu) ||									   /* Armenian */
		   (cp >= 0x05D0u && cp <= 0x05EAu) ||									   /* Hebrew */
		   (cp >= 0x0620u && cp <= 0x064Au) ||									   /* Arabic */
		   (cp >= 0x0900u && cp <= 0x097Fu) ||									   /* Devanagari */
		   (cp >= 0x0E00u && cp <= 0x0E7Fu) ||									   /* Thai */
		   (cp >= 0x1E00u && cp <= 0x1EFFu) ||									   /* Latin Extended Additional */
		   (cp >= 0x3040u && cp <= 0x30FFu) ||									   /* Hiragana and Katakana */
		   (cp >= 0x4E00u && cp <= 0x9FFFu) ||									   /* CJK Unified Ideographs */
		   (cp >= 0xAC00u && cp <= 0xD7A3u);									   /* Hangul syllables */
}

/* Decimal digits only, as the ASCII-era implementation meant. Other scripts' digits are not
 * interchangeable with these for parsing, so they are deliberately not included. */
static inline int utf8_is_digit_cp(uint32_t cp) {
	return cp >= '0' && cp <= '9';
}

static inline int utf8_is_space_cp(uint32_t cp) {
	if (cp == ' ' || (cp >= 0x09u && cp <= 0x0Du)) {
		return 1;
	}
	return cp == 0x0085u || cp == 0x00A0u || cp == 0x1680u || (cp >= 0x2000u && cp <= 0x200Au) || cp == 0x2028u ||
		   cp == 0x2029u || cp == 0x202Fu || cp == 0x205Fu || cp == 0x3000u;
}
#endif // QD_UNICODE_CODEPOINT_H
