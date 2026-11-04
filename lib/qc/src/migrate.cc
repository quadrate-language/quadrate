// `quadfmt --migrate`: rewrites source written when a plain `fn` bound its named inputs and
// `stack fn` left them on the stack, for the language where `fn` leaves them and `def` binds
// them. A function or lambda that reads its inputs by name becomes `def`; `stack fn` becomes `fn`. Renamed
// builtins get their new names -- `printv` is `dump` -- and the operators' old word forms
// their symbols, `add` `+` and `shl` `bits::shl`. `strings::len` is the builtin `len`.
//
// This cannot use the parser: the old code is exactly what the parser no longer reads. It scans
// tokens instead, which is enough, because the rewrite only needs to know where a `fn` starts a
// function or a lambda -- rather than a type, `fn(i64 -- i64)`, or an `import` block's
// declaration -- and whether its parameter list has an input before the `--`.

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <quadrate/qc/formatter.h>

namespace Qd {

	namespace {
		struct Token {
			enum class Kind {
				Identifier,
				String,
				Punct
			} kind;
			std::string text;
			size_t start;
			size_t end;
		};

		std::vector<Token> tokenize(const std::string& src) {
			std::vector<Token> tokens;
			size_t i = 0;
			const size_t n = src.size();
			while (i < n) {
				const char c = src[i];
				if (std::isspace(static_cast<unsigned char>(c))) {
					i++;
				} else if (c == '/' && i + 1 < n && src[i + 1] == '/') {
					while (i < n && src[i] != '\n') {
						i++;
					}
				} else if (c == '/' && i + 1 < n && src[i + 1] == '*') {
					const size_t close = src.find("*/", i + 2);
					i = close == std::string::npos ? n : close + 2;
				} else if (c == '"' || (c == '$' && i + 1 < n && src[i + 1] == '"')) {
					const size_t start = i;
					i += c == '$' ? 2 : 1;
					while (i < n && src[i] != '"') {
						i += src[i] == '\\' ? size_t{2} : size_t{1};
					}
					i = i < n ? i + 1 : n;
					tokens.push_back({Token::Kind::String, src.substr(start, i - start), start, i});
				} else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
					const size_t start = i;
					while (i < n && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_')) {
						i++;
					}
					tokens.push_back({Token::Kind::Identifier, src.substr(start, i - start), start, i});
				} else {
					tokens.push_back({Token::Kind::Punct, std::string(1, c), i, i + 1});
					i++;
				}
			}
			return tokens;
		}

		bool is(const std::vector<Token>& t, size_t k, const char* text) {
			return k < t.size() && t[k].text == text && t[k].kind != Token::Kind::String;
		}

		// The index of the token closing the bracket opened at `open`, or t.size()
		size_t matching(const std::vector<Token>& t, size_t open, const char* opener, const char* closer) {
			int depth = 0;
			for (size_t k = open; k < t.size(); k++) {
				if (is(t, k, opener)) {
					depth++;
				} else if (is(t, k, closer) && --depth == 0) {
					return k;
				}
			}
			return t.size();
		}

		// The input names of the parameter list opened at `open`: each name before a `:`, up to
		// the `--`
		std::set<std::string> inputNames(const std::vector<Token>& t, size_t open) {
			std::set<std::string> names;
			const size_t close = matching(t, open, "(", ")");
			for (size_t k = open + 1; k < close; k++) {
				if (is(t, k, "-") && is(t, k + 1, "-")) {
					break;
				}
				if (t[k].kind == Token::Kind::Identifier && is(t, k + 1, ":") && !is(t, k + 2, ":")) {
					names.insert(t[k].text);
				}
			}
			return names;
		}

		// Whether the body following the parameter list opened at `open` names one of `names`.
		// Under the old meaning a function with inputs bound them, so its body reads them by
		// name; one that does not is already written for the stack, and is left alone. That is
		// also what makes migrating twice harmless.
		bool bodyUsesInputs(const std::vector<Token>& t, size_t open, const std::set<std::string>& names) {
			size_t k = matching(t, open, "(", ")");
			while (k < t.size() && !is(t, k, "{")) {
				k++;
			}
			const size_t close = matching(t, k, "{", "}");
			for (; k < close; k++) {
				if (t[k].kind == Token::Kind::Identifier && names.count(t[k].text) &&
						!(k > 0 && (is(t, k - 1, ":") || is(t, k - 1, "<") || is(t, k - 1, ">")))) {
					return true;
				}
			}
			return false;
		}

		// Builtins renamed or gone, and what is written instead
		const std::unordered_map<std::string, std::string>& renamedWords() {
			static const std::unordered_map<std::string, std::string> words = {{"printv", "dump"}, {"add", "+"},
					{"sub", "-"}, {"mul", "*"}, {"div", "/"}, {"mod", "%"}, {"inc", "++"}, {"dec", "--"}, {"eq", "=="},
					{"neq", "!="}, {"lt", "<"}, {"lte", "<="}, {"gt", ">"}, {"gte", ">="}, {"shl", "bits::shl"},
					{"shr", "bits::shr"}};
			return words;
		}

		// Names the file binds -- `-> x`, and `x:` in a parameter list or a struct -- which mean
		// the binding, not the builtin of that name
		std::set<std::string> boundNames(const std::vector<Token>& t) {
			std::set<std::string> names;
			for (size_t k = 0; k + 1 < t.size(); k++) {
				if (is(t, k, "-") && is(t, k + 1, ">") && k + 2 < t.size() &&
						t[k + 2].kind == Token::Kind::Identifier) {
					names.insert(t[k + 2].text);
				}
				if (t[k].kind == Token::Kind::Identifier && is(t, k + 1, ":") && !is(t, k + 2, ":") &&
						!(k > 0 && is(t, k - 1, ":"))) {
					names.insert(t[k].text);
				}
			}
			return names;
		}

		// Whether the identifier at `k` is a use of the word, rather than a name that only looks
		// like it: qualified, `m::add`; a field, `p <<add`; a definition, `fn add(`; a call, `x add!`
		bool isBareWord(const std::vector<Token>& t, size_t k) {
			if (k > 0) {
				const std::string& prev = t[k - 1].text;
				if (t[k - 1].kind != Token::Kind::String &&
						(prev == ":" || prev == "<" || prev == ">" || prev == "&" || prev == "." || prev == "fn" ||
								prev == "def" || prev == "struct" || prev == "for")) {
					return false;
				}
			}
			return !(is(t, k + 1, "(") || is(t, k + 1, "!") || is(t, k + 1, "?") || is(t, k + 1, ":") ||
					 is(t, k + 1, "=") || is(t, k + 1, "{"));
		}

		// Whether the parameter list opened at `open` has an input: anything before its `--`
		// other than a stack variable, `..a`
		bool hasInputs(const std::vector<Token>& t, size_t open) {
			const size_t close = matching(t, open, "(", ")");
			for (size_t k = open + 1; k < close; k++) {
				if (is(t, k, "-") && is(t, k + 1, "-")) {
					return false;
				}
				if (is(t, k, ".") && is(t, k + 1, ".")) {
					k += 2; // the `..` and the variable's name
					continue;
				}
				return true;
			}
			return false;
		}
	}

	std::string migrateSource(const std::string& source) {
		const std::vector<Token> t = tokenize(source);

		struct Edit {
			size_t start;
			size_t end;
			std::string text;
		};

		std::vector<Edit> edits;
		const std::set<std::string> bound = boundNames(t);
		bool shifted = false;

		int parenDepth = 0;
		size_t importEnd = 0; // the token closing the `import` block being skipped
		for (size_t k = 0; k < t.size(); k++) {
			if (is(t, k, "(")) {
				parenDepth++;
			} else if (is(t, k, ")")) {
				parenDepth--;
			}
			if (is(t, k, "import") && t[k].kind == Token::Kind::Identifier) {
				for (size_t j = k; j < t.size(); j++) {
					if (is(t, j, "{")) {
						importEnd = matching(t, j, "{", "}");
						break;
					}
				}
			}
			// Library functions a builtin replaced: `strings::len` is `len`
			if (is(t, k, "strings") && is(t, k + 1, ":") && is(t, k + 2, ":") && is(t, k + 3, "len") &&
					!is(t, k + 4, "(")) {
				edits.push_back({t[k].start, t[k + 3].end, "len"});
				k += 3;
				continue;
			}
			// Renamed builtins, and the operators' old word forms
			if (t[k].kind == Token::Kind::Identifier) {
				const auto renamed = renamedWords().find(t[k].text);
				if (renamed != renamedWords().end() && !bound.count(t[k].text) && isBareWord(t, k)) {
					edits.push_back({t[k].start, t[k].end, renamed->second});
					shifted = shifted || renamed->second.compare(0, 6, "bits::") == 0;
					continue;
				}
			}
			if (k < importEnd || !is(t, k, "fn") || t[k].kind != Token::Kind::Identifier) {
				continue;
			}

			// A function type: inside a parameter list, or after a `:`, `]`, `<` or `,`
			const std::string prev = k > 0 ? t[k - 1].text : "";
			if (parenDepth > 0 || prev == ":" || prev == "]" || prev == "<" || prev == ",") {
				continue;
			}

			// `stack fn` is `fn` now: drop the word and the space after it, wherever it is among
			// the modifiers -- `pub stack inline fn` as much as `stack fn`
			bool wasStack = false;
			for (size_t m = k; m > 0 && t[m - 1].kind == Token::Kind::Identifier &&
							   (t[m - 1].text == "stack" || t[m - 1].text == "inline" || t[m - 1].text == "pub");
					m--) {
				if (t[m - 1].text == "stack") {
					edits.push_back({t[m - 1].start, t[m].start, ""});
					wasStack = true;
				}
			}
			if (wasStack) {
				continue;
			}

			// Find the parameter list that holds the inputs
			size_t params = t.size();
			size_t j = k + 1;
			if (is(t, j, "[")) {
				j = matching(t, j, "[", "]") + 1; // captures
			}
			if (is(t, j, "(")) {
				const size_t close = matching(t, j, "(", ")");
				if (close + 1 < t.size() && t[close + 1].kind == Token::Kind::Identifier) {
					// A receiver, then the method's name
					j = close + 2;
				} else {
					params = j; // a lambda's own signature
				}
			} else if (j < t.size() && t[j].kind == Token::Kind::Identifier) {
				j++; // the name
			} else {
				continue; // `fn { }`, or not a function at all
			}
			if (params == t.size()) {
				if (is(t, j, "<")) {
					j = matching(t, j, "<", ">") + 1;
				}
				if (!is(t, j, "(")) {
					continue;
				}
				params = j;
			}
			if (hasInputs(t, params) && bodyUsesInputs(t, params, inputNames(t, params))) {
				edits.push_back({t[k].start, t[k].end, "def"});
			}
		}

		// The shifts are bits:: now, so a file that used one needs the module
		bool usesBits = false;
		for (size_t k = 0; k + 1 < t.size(); k++) {
			usesBits = usesBits || (is(t, k, "use") && is(t, k + 1, "bits"));
		}
		if (shifted && !usesBits) {
			size_t at = 0;
			for (size_t k = 0; k + 1 < t.size(); k++) {
				if (is(t, k, "use") && t[k + 1].kind == Token::Kind::Identifier) {
					at = t[k + 1].end;
				}
			}
			edits.push_back({at, at, at == 0 ? "use bits\n\n" : "\nuse bits"});
			std::sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) { return a.start < b.start; });
		}

		std::string out = source;
		for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
			out.replace(it->start, it->end - it->start, it->text);
		}
		return out;
	}

} // namespace Qd
