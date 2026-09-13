// SPDX-License-Identifier: GPL-3.0-or-later
// Shared help-page renderer for Quadrate CLI tools.
//
// Every tool's --help is built here rather than hand-written, so the ten tools
// keep one layout: a title line, a description paragraph, a Usage block, an
// Options block that always opens with the same three flags, any number of
// tool-specific sections, and Examples. Column alignment is computed rather
// than hardcoded, which is what let the widths drift apart before.

#ifndef QDCLI_HELP_H
#define QDCLI_HELP_H

#include <iosfwd>
#include <string>
#include <vector>

namespace qdcli {

	// The description column for a block of aligned entries. Computed as
	// 2 (indent) + widest left-hand side + 3, clamped to these bounds: the floor
	// keeps the standard option trio in the same column across the tools that
	// have no wider flag, and the ceiling stops one long example from pushing
	// every description in its section off to the right.
	inline constexpr size_t kMinDescCol = 19;
	inline constexpr size_t kMaxDescCol = 42;

	class Help {
	public:
		// summary is the short noun phrase after the dash on the title line,
		// e.g. "Quadrate compiler". Keep it a name, not a sentence.
		Help(std::string tool, std::string summary);

		// One or more paragraphs under the title. Each call is its own paragraph.
		Help& description(std::string text);

		// Usage forms, in order. The first is prefixed "Usage: ", the rest are
		// indented to line up under it. Pass the form without the tool name;
		// it is prepended.
		Help& usage(std::string form);
		// A usage form with a trailing comment, rendered aligned after the form.
		Help& usage(std::string form, std::string comment);

		// Opens a new section. Options/items added after this land under it.
		Help& section(std::string title);

		// An option entry. Pass the short form without the dash ('h'), or 0 for
		// none — a long-only option is indented into the same column as one with
		// a short form. longFlag includes its dashes; arg is the placeholder
		// ("<N>", "<dir>") or empty.
		Help& option(char shortFlag, std::string longFlag, std::string arg, std::string desc);
		Help& option(char shortFlag, std::string longFlag, std::string desc);
		Help& option(std::string longFlag, std::string desc);
		Help& option(std::string longFlag, std::string arg, std::string desc);

		// The three flags every Quadrate tool accepts, in the order every tool
		// lists them. Call right after section("Options").
		Help& standardOptions();

		// A left/right entry that is not an option: a command, an example, a
		// table row. Alignment is shared with options in the same section.
		Help& item(std::string left, std::string right);

		// A line of prose inside the current section, indented to the entry
		// indent. Empty text emits a blank line.
		Help& text(std::string line = "");

		void print(std::ostream& os) const;
		void print() const;

	private:
		enum class Kind {
			Entry,
			Text,
			Blank
		};

		struct Line {
			Kind kind = Kind::Entry;
			std::string left;
			std::string right;
		};

		struct Section {
			std::string title;
			std::vector<Line> lines;
		};

		Section& current();

		std::string tool_;
		std::string summary_;
		std::vector<std::string> descriptions_;
		std::vector<std::pair<std::string, std::string>> usages_;
		std::vector<Section> sections_;
	};

	// "tool: message" on stderr, followed by the standard referral line. Use for
	// argument and usage errors; returns 1 so callers can `return usageError(...)`.
	int usageError(const std::string& tool, const std::string& message);

	// "tool: message" on stderr, with no referral line. Use for runtime failures
	// (unreadable file, failed subprocess) where --help would not help.
	int error(const std::string& tool, const std::string& message);

} // namespace qdcli

#endif // QDCLI_HELP_H
