// SPDX-License-Identifier: GPL-3.0-or-later
// Shared help-page renderer for Quadrate CLI tools.

#include "quadrate/cli/help.h"

#include <algorithm>
#include <iostream>

namespace qdcli {

	namespace {

		// A long-only option is indented by the width of "-x, " so its dashes line
		// up with the long forms of the options that do have a short letter.
		constexpr const char* kShortSlot = "    ";

		std::string formatFlags(char shortFlag, const std::string& longFlag, const std::string& arg) {
			std::string out;
			if (shortFlag != '\0') {
				out += '-';
				out += shortFlag;
				out += ", ";
			} else {
				out += kShortSlot;
			}
			out += longFlag;
			if (!arg.empty()) {
				out += ' ';
				out += arg;
			}
			return out;
		}

		size_t descColumn(const std::vector<std::string>& lefts) {
			size_t widest = 0;
			for (const std::string& left : lefts) {
				widest = std::max(widest, left.size());
			}
			size_t col = 2 + widest + 3;
			col = std::max(col, kMinDescCol);
			col = std::min(col, kMaxDescCol);
			return col;
		}

		void emit(std::ostream& os, const std::string& left, const std::string& right, size_t col) {
			if (right.empty()) {
				os << "  " << left << "\n";
				return;
			}
			std::string padded = "  " + left;
			// An entry wider than the column keeps a two-space gap rather than
			// wrapping, so a long example still reads as one line.
			if (padded.size() + 2 > col) {
				os << padded << "  " << right << "\n";
				return;
			}
			padded.resize(col, ' ');
			os << padded << right << "\n";
		}

	} // namespace

	Help::Help(std::string tool, std::string summary) : tool_(std::move(tool)), summary_(std::move(summary)) {
	}

	Help& Help::description(std::string text) {
		descriptions_.push_back(std::move(text));
		return *this;
	}

	Help& Help::usage(std::string form) {
		usages_.emplace_back(std::move(form), std::string());
		return *this;
	}

	Help& Help::usage(std::string form, std::string comment) {
		usages_.emplace_back(std::move(form), std::move(comment));
		return *this;
	}

	Help& Help::section(std::string title) {
		sections_.push_back(Section{std::move(title), {}});
		return *this;
	}

	Help::Section& Help::current() {
		if (sections_.empty()) {
			sections_.push_back(Section{"Options", {}});
		}
		return sections_.back();
	}

	Help& Help::option(char shortFlag, std::string longFlag, std::string arg, std::string desc) {
		current().lines.push_back(Line{Kind::Entry, formatFlags(shortFlag, longFlag, arg), std::move(desc)});
		return *this;
	}

	Help& Help::option(char shortFlag, std::string longFlag, std::string desc) {
		return option(shortFlag, std::move(longFlag), std::string(), std::move(desc));
	}

	Help& Help::option(std::string longFlag, std::string desc) {
		return option('\0', std::move(longFlag), std::string(), std::move(desc));
	}

	Help& Help::option(std::string longFlag, std::string arg, std::string desc) {
		return option('\0', std::move(longFlag), std::move(arg), std::move(desc));
	}

	Help& Help::standardOptions() {
		option('h', "--help", "Show this help message");
		option('v', "--version", "Show version information");
		option("--no-color", "Disable coloured output");
		return *this;
	}

	Help& Help::item(std::string left, std::string right) {
		current().lines.push_back(Line{Kind::Entry, std::move(left), std::move(right)});
		return *this;
	}

	Help& Help::text(std::string line) {
		current().lines.push_back(Line{line.empty() ? Kind::Blank : Kind::Text, std::move(line), std::string()});
		return *this;
	}

	void Help::print(std::ostream& os) const {
		os << tool_ << " - " << summary_ << "\n";

		for (const std::string& paragraph : descriptions_) {
			os << "\n" << paragraph << "\n";
		}

		if (!usages_.empty()) {
			os << "\n";
			std::vector<std::string> forms;
			forms.reserve(usages_.size());
			for (const auto& [form, comment] : usages_) {
				forms.push_back(tool_ + (form.empty() ? "" : " " + form));
			}
			size_t widest = 0;
			for (const std::string& form : forms) {
				widest = std::max(widest, form.size());
			}
			for (size_t i = 0; i < forms.size(); i++) {
				os << (i == 0 ? "Usage: " : "       ") << forms[i];
				if (!usages_[i].second.empty()) {
					os << std::string(widest - forms[i].size() + 3, ' ') << usages_[i].second;
				}
				os << "\n";
			}
		}

		for (const Section& section : sections_) {
			std::vector<std::string> lefts;
			for (const Line& line : section.lines) {
				if (line.kind == Kind::Entry && !line.right.empty()) {
					lefts.push_back(line.left);
				}
			}
			const size_t col = descColumn(lefts);

			os << "\n" << section.title << ":\n";
			for (const Line& line : section.lines) {
				switch (line.kind) {
				case Kind::Blank:
					os << "\n";
					break;
				case Kind::Text:
					os << "  " << line.left << "\n";
					break;
				case Kind::Entry:
					emit(os, line.left, line.right, col);
					break;
				}
			}
		}
	}

	void Help::print() const {
		print(std::cout);
	}

	int usageError(const std::string& tool, const std::string& message) {
		std::cerr << tool << ": " << message << "\n";
		std::cerr << "Try '" << tool << " --help' for more information.\n";
		return 1;
	}

	int error(const std::string& tool, const std::string& message) {
		std::cerr << tool << ": " << message << "\n";
		return 1;
	}

} // namespace qdcli
