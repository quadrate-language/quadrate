/**
 * @file reference.h
 * @brief Every keyword and builtin, with its signature and description, as reference.def gives them.
 *
 * The manual's reference page is generated from reference.def, and the REPL's `:doc`, the
 * language server's hover and its completion read this table, so all of them describe the
 * same language.
 */

#pragma once

#include <string_view>

namespace Qd {

	struct ReferenceEntry {
		const char* name;
		const char* signature; ///< The stack effect; null for a keyword
		const char* description;
	};

	inline constexpr ReferenceEntry REFERENCE[] = {
#define KEYWORD(name, description) {#name, nullptr, description},
#define BUILTIN(name, signature, description) {#name, signature, description},
#include "reference.def"
#undef BUILTIN
#undef KEYWORD
	};

	/// The entry for a keyword or builtin, or null when `name` is neither
	inline const ReferenceEntry* findReference(std::string_view name) {
		for (const auto& entry : REFERENCE) {
			if (name == entry.name) {
				return &entry;
			}
		}
		return nullptr;
	}

} // namespace Qd
