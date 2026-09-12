#include <quadrate/cli/cli.h>
#include <quadrate/platform/platform.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qc/instructions.h>
#include <quadrate/qc/semantic_validator.h>
#include <quadrate/qd/qd.h>
#include <quadrate/rt/stack.h>

#include <algorithm>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <pwd.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "version.h"

// History file path
static std::string g_historyFile;

static bool isWordChar(char c) {
	return isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

struct ReferenceEntry {
	const char* name;
	const char* signature;
	const char* description;
};

static const ReferenceEntry g_reference[] = {
#define KEYWORD(name, description) {#name, nullptr, description},
#define BUILTIN(name, signature, description) {#name, signature, description},
#include <quadrate/qc/reference.def>
#undef BUILTIN
#undef KEYWORD
};

static const char* const g_keywords[] = {"fn", "pub", "inline", "if", "else", "for", "loop", "break", "continue",
		"return", "use", "struct", "packed", "enum", "const", "var", "defer", "switch", "case", "test", "type", "as",
		"null", "true", "false", "Ok", "Err"};

extern "C" int exe_path_platform_get(char* buffer, size_t buffer_size);

#ifndef QUADRATE_SOURCE_ROOT
#define QUADRATE_SOURCE_ROOT ""
#endif

#ifdef QD_PLATFORM_HAIKU
#define QD_DATA_DIR_NAME "data"
#else
#define QD_DATA_DIR_NAME "share"
#endif

static std::string findStdlibRoot() {
	namespace fs = std::filesystem;

	auto usable = [](const fs::path& path) {
		std::error_code ec;
		return fs::is_directory(path, ec);
	};

	if (const char* root = getenv("QUADRATE_ROOT"); root && usable(root)) {
		return root;
	}

	if (const char* libDir = getenv("QUADRATE_LIBDIR")) {
		fs::path share = fs::path(libDir) / ".." / QD_DATA_DIR_NAME / "quadrate";
		if (usable(share)) {
			return share.string();
		}
	}

	char exePathBuf[4096];
	const int len = exe_path_platform_get(exePathBuf, sizeof(exePathBuf));
	if (len > 0 && static_cast<size_t>(len) < sizeof(exePathBuf)) {
		std::error_code ec;
		fs::path exePath = fs::canonical(exePathBuf, ec);
		if (!ec) {
			fs::path share = exePath.parent_path() / ".." / QD_DATA_DIR_NAME / "quadrate";
			if (usable(share)) {
				return share.string();
			}
		}
	}

	if (const char* home = getenv("HOME")) {
		fs::path path = fs::path(home) / "quadrate";
		if (usable(path)) {
			return path.string();
		}
	}

	if (QUADRATE_SOURCE_ROOT[0] != '\0') {
		fs::path stdlib = fs::path(QUADRATE_SOURCE_ROOT) / "stdlib";
		if (usable(stdlib)) {
			return stdlib.string();
		}
	}

	return "";
}

static void collectPublicDeclarations(
		const std::filesystem::path& file, const std::string& module, std::map<std::string, std::string>& out) {
	std::ifstream in(file);
	if (!in.good()) {
		return;
	}

	std::string line;
	while (std::getline(in, line)) {
		size_t indent = 0;
		while (indent < line.size() && isspace(static_cast<unsigned char>(line[indent])) != 0) {
			indent++;
		}
		if (line.compare(indent, 4, "pub ") != 0) {
			continue;
		}

		size_t pos = indent + 4;
		auto skipKeyword = [&](const char* keyword) {
			const size_t n = strlen(keyword);
			if (line.compare(pos, n, keyword) != 0 || pos + n >= line.size() ||
					isspace(static_cast<unsigned char>(line[pos + n])) == 0) {
				return false;
			}
			pos += n;
			while (pos < line.size() && isspace(static_cast<unsigned char>(line[pos])) != 0) {
				pos++;
			}
			return true;
		};

		skipKeyword("inline");
		if (!skipKeyword("fn") && !skipKeyword("const") && !skipKeyword("struct") && !skipKeyword("enum") &&
				!skipKeyword("type")) {
			continue;
		}

		const size_t start = pos;
		while (pos < line.size() && isWordChar(line[pos])) {
			pos++;
		}
		if (pos == start) {
			continue;
		}

		std::string declaration = line.substr(indent, line.find('{') - indent);
		while (!declaration.empty() && isspace(static_cast<unsigned char>(declaration.back())) != 0) {
			declaration.pop_back();
		}
		out.emplace(module + "::" + line.substr(start, pos - start), declaration);
	}
}

static const std::map<std::string, std::string>& stdlibDeclarations() {
	static const std::map<std::string, std::string> declarations = [] {
		namespace fs = std::filesystem;
		std::map<std::string, std::string> out;

		const std::string root = findStdlibRoot();
		if (root.empty()) {
			return out;
		}

		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(root, ec)) {
			if (!entry.is_directory(ec)) {
				continue;
			}
			const std::string module = entry.path().filename().string();
			if (module.empty() || module[0] == '.' || module[0] == '_') {
				continue;
			}

			std::error_code walkEc;
			for (const auto& file : fs::recursive_directory_iterator(entry.path(), walkEc)) {
				if (file.path().extension() == ".qd") {
					collectPublicDeclarations(file.path(), module, out);
				}
			}
		}
		return out;
	}();
	return declarations;
}

static const std::vector<std::string>& completionCandidates() {
	static const std::vector<std::string> candidates = [] {
		std::vector<std::string> all(std::begin(g_keywords), std::end(g_keywords));
		all.insert(all.end(), std::begin(Qd::BUILTIN_INSTRUCTIONS), std::end(Qd::BUILTIN_INSTRUCTIONS));
		for (const auto& entry : g_reference) {
			if (strcmp(entry.name, "_") != 0) {
				all.push_back(entry.name);
			}
		}

		for (const auto& [name, declaration] : stdlibDeclarations()) {
			all.push_back(name.substr(0, name.find(':') + 2));
			all.push_back(name);
		}

		std::sort(all.begin(), all.end());
		all.erase(std::unique(all.begin(), all.end()), all.end());
		return all;
	}();
	return candidates;
}

// Generator function for readline completion
static char* completionGenerator(const char* text, int state) {
	static size_t index;
	static size_t len;

	if (!state) {
		index = 0;
		len = strlen(text);
	}

	const std::vector<std::string>& candidates = completionCandidates();
	while (index < candidates.size()) {
		const std::string& name = candidates[index++];
		if (name.compare(0, len, text, len) == 0) {
			return strdup(name.c_str());
		}
	}

	return nullptr;
}

// Completion function for readline
static char** completionFunction(const char* text, int start, int end) {
	(void)start;
	(void)end;
	rl_attempted_completion_over = 1; // Don't fall back to filename completion
	return rl_completion_matches(text, completionGenerator);
}

// Get history file path
static std::string getHistoryFilePath() {
	const char* home = getenv("HOME");
	if (!home) {
		struct passwd* pw = getpwuid(getuid());
		if (pw) {
			home = pw->pw_dir;
		}
	}
	if (home) {
#ifdef QD_PLATFORM_HAIKU
		// Haiku uses ~/config/settings/ for user settings
		return std::string(home) + "/config/settings/quadrepl_history";
#else
		return std::string(home) + "/.quadrepl_history";
#endif
	}
	return "";
}

// Signal handling for crash recovery
static sigjmp_buf g_jmpBuf;
static volatile sig_atomic_t g_inExecution = 0;

// Readline hook for auto-indentation
static std::string g_pendingIndent;

static int insertIndentHook() {
	if (!g_pendingIndent.empty()) {
		rl_insert_text(g_pendingIndent.c_str());
	}
	return 0;
}

static void signalHandler(int sig) {
	if (g_inExecution) {
		siglongjmp(g_jmpBuf, sig);
	}
	// If not in execution, let the default handler run
	signal(sig, SIG_DFL);
	raise(sig);
}

// ANSI color codes
#define COLOR_RESET (Qd::Colors::isEnabled() ? "\033[0m" : "")
#define COLOR_BOLD (Qd::Colors::isEnabled() ? "\033[1m" : "")
#define COLOR_DIM (Qd::Colors::isEnabled() ? "\033[2m" : "")
#define COLOR_GREEN (Qd::Colors::isEnabled() ? "\033[32m" : "")
#define COLOR_YELLOW (Qd::Colors::isEnabled() ? "\033[33m" : "")
#define COLOR_BLUE (Qd::Colors::isEnabled() ? "\033[34m" : "")
#define COLOR_CYAN (Qd::Colors::isEnabled() ? "\033[36m" : "")
#define COLOR_MAGENTA (Qd::Colors::isEnabled() ? "\033[35m" : "")
#define COLOR_RED (Qd::Colors::isEnabled() ? "\033[31m" : "")

// Stack display settings
#define MAX_STACK_DISPLAY 5
#define HISTORY_LIMIT 5000
#define MAX_STRING_DISPLAY 20

static std::string rewriteWords(
		const std::string& code, const std::function<bool(const std::string&, std::string&)>& rewrite) {
	std::string out;
	const size_t n = code.size();
	size_t i = 0;

	while (i < n) {
		const char c = code[i];

		if (c == '"') {
			out += c;
			i++;
			while (i < n) {
				if (code[i] == '\\' && i + 1 < n) {
					out += code[i];
					out += code[i + 1];
					i += 2;
					continue;
				}
				out += code[i];
				i++;
				if (code[i - 1] == '"') {
					break;
				}
			}
			continue;
		}

		if (c == '/' && i + 1 < n && code[i + 1] == '/') {
			while (i < n && code[i] != '\n') {
				out += code[i];
				i++;
			}
			continue;
		}

		if (c == '/' && i + 1 < n && code[i + 1] == '*') {
			out += "/*";
			i += 2;
			while (i + 1 < n && !(code[i] == '*' && code[i + 1] == '/')) {
				out += code[i];
				i++;
			}
			if (i + 1 < n) {
				out += "*/";
				i += 2;
			} else {
				out.append(code, i, std::string::npos);
				i = n;
			}
			continue;
		}

		if (isWordChar(c)) {
			const size_t start = i;
			while (i < n && isWordChar(code[i])) {
				i++;
			}
			const std::string word = code.substr(start, i - start);
			std::string replacement;
			out += rewrite(word, replacement) ? replacement : word;
			continue;
		}

		out += c;
		i++;
	}

	return out;
}

class ReplSession {
public:
	ReplSession(bool shouldPrintOnExit) : printOnExit(shouldPrintOnExit) {
		ctx = qd_create_context(1024);
		mod = qd_get_module(ctx, "repl");
		moduleCounter = 0;
	}

	~ReplSession() {
		if (ctx) {
			qd_free_context(ctx);
		}
	}

	void run() {
		// Initialize readline completion
		rl_attempted_completion_function = completionFunction;

		// Load history from file
		g_historyFile = getHistoryFilePath();
		if (!g_historyFile.empty()) {
			read_history(g_historyFile.c_str());
			stifle_history(HISTORY_LIMIT);
		}

		printWelcome();

		std::string accumulated; // Accumulated multiline input
		int braceDepth = 0;		 // Track unbalanced braces

		while (true) {
			std::string prompt;
			std::string indent;
			if (braceDepth > 0) {
				// Continuation prompt with indentation indicator
				prompt = paint(COLOR_DIM) + "...> " + paint(COLOR_RESET);
				// Build indentation string (one tab per brace level)
				for (int i = 0; i < braceDepth; i++) {
					indent += "\t";
				}
			} else {
				prompt = buildPrompt();
			}

			// Set up pre-input hook to insert indentation
			if (!indent.empty()) {
				g_pendingIndent = indent;
				rl_startup_hook = insertIndentHook;
			}

			char* input = readline(prompt.c_str());

			// Clear the hook
			rl_startup_hook = nullptr;
			g_pendingIndent.clear();

			if (!input) {
				// EOF (Ctrl+D)
				printf("\n");
				if (printOnExit) {
					printStackToStdout();
				}
				saveHistory();
				break;
			}

			std::string line = input;
			free(input);

			// If in multiline mode, accumulate the line
			if (braceDepth > 0) {
				accumulated += "\n" + line;
			} else {
				accumulated = line;
			}

			// Update brace depth
			braceDepth = countUnbalancedBraces(accumulated);

			// If braces are still unbalanced, continue reading
			if (braceDepth > 0) {
				continue;
			}

			// Braces are balanced (or no braces) - process the complete input
			std::string completeInput = trim(accumulated);
			accumulated.clear();

			if (completeInput.empty()) {
				continue;
			}

			add_history(completeInput.c_str());

			if (!handleCompleteInput(completeInput)) {
				break;
			}
		}
	}

	bool hasUserFunction(const std::string& name) {
		for (const auto& def : definitions) {
			std::string d = trim(def);
			if (d.compare(0, 4, "pub ") == 0) {
				d = trim(d.substr(4));
			}
			if (d.compare(0, 3, "fn ") != 0) {
				continue;
			}
			std::string rest = trim(d.substr(3));
			if (rest.compare(0, name.size(), name) == 0 && rest.size() > name.size() &&
					(rest[name.size()] == '(' || rest[name.size()] == ' ' || rest[name.size()] == '<')) {
				return true;
			}
		}
		return false;
	}

	static std::pair<std::string, std::string> splitCommand(const std::string& input) {
		const size_t space = input.find_first_of(" \t");
		if (space == std::string::npos) {
			return {input, ""};
		}
		return {input.substr(0, space), input.substr(space + 1)};
	}

	bool handleCompleteInput(const std::string& completeInput) {
		auto isMeta = [&](std::initializer_list<const char*> names) {
			for (const char* name : names) {
				if (completeInput != name) {
					continue;
				}
				if (name[0] == ':' || !hasUserFunction(name)) {
					return true;
				}
			}
			return false;
		};

		const auto [word, argument] = splitCommand(completeInput);
		auto isMetaWithArgument = [&](std::initializer_list<const char*> names) {
			for (const char* name : names) {
				if (word == name) {
					return true;
				}
			}
			return false;
		};

		if (isMeta({"exit", "quit", ":q", ":quit", ":exit"})) {
			if (printOnExit) {
				printStackToStdout();
			}
			saveHistory();
			return false;
		} else if (isMeta({"help", ":help", ":h"})) {
			printHelp();
			return true;
		} else if (isMeta({"clear", ":clear"})) {
			clearStack();
			return true;
		} else if (isMeta({"stack", ":stack"})) {
			showStack();
			return true;
		} else if (isMeta({"type", "types", ":type", ":types"})) {
			showTypes();
			return true;
		} else if (isMeta({"reset", ":reset"})) {
			reset();
			return true;
		} else if (isMetaWithArgument({":doc", ":d"})) {
			const std::string name = trim(argument);
			if (name.empty()) {
				printf("%sUsage: :doc <name>%s\n", COLOR_RED, COLOR_RESET);
			} else {
				printDoc(name);
			}
			return true;
		} else if (isMetaWithArgument({".save", ":save"})) {
			const std::string filename = trim(argument);
			if (filename.empty()) {
				printf("%sUsage: :save <filename>%s\n", COLOR_RED, COLOR_RESET);
			} else {
				saveSession(filename);
			}
			return true;
		} else if (isMetaWithArgument({".load", ":load"})) {
			const std::string filename = trim(argument);
			if (filename.empty()) {
				printf("%sUsage: :load <filename>%s\n", COLOR_RED, COLOR_RESET);
			} else {
				loadSession(filename);
			}
			return true;
		}

		sessionHistory.push_back(completeInput);
		processLine(completeInput);
		return true;
	}

	// Count unbalanced braces in input (returns > 0 if more { than })
	int countUnbalancedBraces(const std::string& input) {
		int depth = 0;
		bool inString = false;
		bool inLineComment = false;
		bool inBlockComment = false;

		for (size_t i = 0; i < input.length(); i++) {
			char c = input[i];
			char next = (i + 1 < input.length()) ? input[i + 1] : '\0';

			// Handle newlines (reset line comment)
			if (c == '\n') {
				inLineComment = false;
				continue;
			}

			// Skip if in line comment
			if (inLineComment) {
				continue;
			}

			// Check for block comment end
			if (inBlockComment) {
				if (c == '*' && next == '/') {
					inBlockComment = false;
					i++; // skip '/'
				}
				continue;
			}

			// Check for comment start
			if (c == '/' && next == '/') {
				inLineComment = true;
				i++; // skip second '/'
				continue;
			}
			if (c == '/' && next == '*') {
				inBlockComment = true;
				i++; // skip '*'
				continue;
			}

			// Handle strings
			if (c == '"' && !inString) {
				inString = true;
				continue;
			}
			if (c == '"' && inString) {
				// Check for escape
				size_t backslashes = 0;
				for (size_t j = i; j > 0 && input[j - 1] == '\\'; j--) {
					backslashes++;
				}
				if (backslashes % 2 == 0) {
					inString = false;
				}
				continue;
			}

			// Skip string contents
			if (inString) {
				continue;
			}

			// Count braces
			if (c == '{') {
				depth++;
			} else if (c == '}') {
				depth--;
			}
		}

		return depth > 0 ? depth : 0;
	}

	// Read values from stdin and push onto stack, then run interactive REPL
	void runWithPipedInput() {
		std::string accumulated;
		std::string line;
		bool keepGoing = true;
		while (keepGoing && std::getline(std::cin, line)) {
			if (accumulated.empty()) {
				accumulated = line;
			} else {
				accumulated += "\n" + line;
			}
			if (countUnbalancedBraces(accumulated) > 0) {
				continue;
			}
			std::string completeInput = trim(accumulated);
			accumulated.clear();
			if (completeInput.empty()) {
				continue;
			}
			keepGoing = handleCompleteInput(completeInput);
		}
		if (!trim(accumulated).empty() && keepGoing) {
			handleCompleteInput(trim(accumulated));
		}
		if (!keepGoing) {
			return;
		}

		// Reopen stdin from terminal for interactive input
		if (!freopen("/dev/tty", "r", stdin)) {
			// Can't reopen terminal - print stack and exit
			if (printOnExit) {
				printStackToStdout();
			}
			return;
		}

		// Re-enable colors for interactive mode. Only meaningful when the output
		// streams really are terminals, which is what the default already tests.
		if (!qdcli::noColor() && !mNoColorFlag && isatty(STDOUT_FILENO) && isatty(STDERR_FILENO)) {
			Qd::Colors::setEnabled(true);
		}

		// Now run the interactive REPL with the pre-populated stack
		run();
	}

	// Print stack values to stdout (space-separated, compatible with 'read' instruction)
	void printStackToStdout() {
		size_t depth = userStackDepth();
		for (size_t i = 0; i < depth; i++) {
			if (i > 0) {
				printf(" ");
			}
			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);

			switch (elem.type) {
			case QD_STACK_TYPE_INT:
				printf("%lld", static_cast<long long>(elem.value.i));
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("%g", elem.value.f);
				break;
			case QD_STACK_TYPE_STR:
				printf("%s", (elem.value.s && elem.value.s->data) ? elem.value.s->data : "");
				break;
			case QD_STACK_TYPE_PTR:
				printf("%p", elem.value.p);
				break;
			default:
				break;
			}
		}
		if (depth > 0) {
			printf("\n");
		}
	}

private:
	struct Binding {
		std::string name;
		std::string type;
	};

	static std::string declaredType(const qd_stack_element_t& elem, const std::string& structType) {
		switch (elem.type) {
		case QD_STACK_TYPE_INT:
			return "i64";
		case QD_STACK_TYPE_FLOAT:
			return "f64";
		case QD_STACK_TYPE_STR:
			return "str";
		case QD_STACK_TYPE_PTR:
			return structType.empty() ? "ptr" : structType;
		default:
			return "any";
		}
	}

	qd_context* ctx;
	qd_module* mod;
	std::vector<std::string> definitions;
	std::vector<std::string> useStatements;
	std::vector<std::string> sessionHistory; // Commands for .save/.load
	std::vector<std::string> sessionStack;
	std::vector<Binding> sessionLocals;
	int moduleCounter;
	bool printOnExit;
	bool mNoColorFlag = false;

public:
	void setNoColor(bool noColor) {
		mNoColorFlag = noColor;
	}

private:
	std::string trim(const std::string& str) {
		size_t first = str.find_first_not_of(" \t\n\r");
		if (first == std::string::npos) {
			return "";
		}
		size_t last = str.find_last_not_of(" \t\n\r");
		return str.substr(first, (last - first + 1));
	}

	void clearStack() {
		const size_t visible = userStackDepth();
		for (size_t i = 0; i < visible; i++) {
			qd_stack_remove_at(ctx->st, 0, nullptr);
		}
		sessionStack.clear();
	}

	void dropEverything() {
		while (!qd_stack_is_empty(ctx->st)) {
			qd_stack_pop(ctx->st, nullptr);
		}
		sessionStack.clear();
		sessionLocals.clear();
	}

	std::string formatValue(const qd_stack_element_t& elem, const char*& color, size_t maxString) {
		char buffer[64];

		switch (elem.type) {
		case QD_STACK_TYPE_INT:
			color = COLOR_BLUE;
			return std::to_string(elem.value.i);
		case QD_STACK_TYPE_FLOAT:
			color = COLOR_YELLOW;
			snprintf(buffer, sizeof(buffer), "%g", elem.value.f);
			return buffer;
		case QD_STACK_TYPE_STR: {
			color = COLOR_GREEN;
			std::string text = (elem.value.s && elem.value.s->data) ? elem.value.s->data : "";
			if (text.size() > maxString) {
				text = text.substr(0, maxString) + "...";
			}
			return "\"" + text + "\"";
		}
		case QD_STACK_TYPE_PTR:
			color = COLOR_MAGENTA;
			return "ptr";
		default:
			color = COLOR_RED;
			return "?";
		}
	}

	static std::string paint(const char* color) {
		std::string escaped(color);
		if (escaped.empty()) {
			return escaped;
		}
		return std::string(1, RL_PROMPT_START_IGNORE) + escaped + std::string(1, RL_PROMPT_END_IGNORE);
	}

	std::string buildPrompt() {
		std::string prompt = paint(COLOR_CYAN) + "[";

		const size_t depth = userStackDepth();
		size_t startIndex = 0;
		if (depth > MAX_STACK_DISPLAY) {
			prompt += paint(COLOR_DIM) + "..." + paint(COLOR_RESET) + " ";
			startIndex = depth - MAX_STACK_DISPLAY;
		}

		for (size_t i = startIndex; i < depth; i++) {
			if (i > startIndex) {
				prompt += " ";
			}

			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);

			const char* color = COLOR_RESET;
			const std::string text = formatValue(elem, color, MAX_STRING_DISPLAY);
			prompt += paint(color) + text + paint(COLOR_RESET);
		}

		prompt += paint(COLOR_CYAN) + "]> " + paint(COLOR_RESET);
		return prompt;
	}

	void printWelcome() {
		printf("%sQuadrate REPL %s%s\n", COLOR_BOLD, QUADRATE_VERSION, COLOR_RESET);
		printf("Type %shelp%s for available commands, %sexit%s to quit\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("%sTip: the prompt shows the stack. 'print' writes the top value out, ':doc <name>' explains a name%s\n",
				COLOR_DIM, COLOR_RESET);
		printf("\n");
	}

	void saveSession(const std::string& filename) {
		std::ofstream file(filename);
		if (!file.good()) {
			printf("%sError: Could not open file '%s' for writing%s\n", COLOR_RED, filename.c_str(), COLOR_RESET);
			return;
		}

		// Save session history
		for (const auto& cmd : sessionHistory) {
			file << cmd << "\n";
		}

		printf("%sSession saved to '%s' (%zu commands)%s\n", COLOR_GREEN, filename.c_str(), sessionHistory.size(),
				COLOR_RESET);
	}

	void loadSession(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.good()) {
			printf("%sError: Could not open file '%s' for reading%s\n", COLOR_RED, filename.c_str(), COLOR_RESET);
			return;
		}

		std::string line;
		size_t count = 0;
		while (std::getline(file, line)) {
			if (!line.empty()) {
				processLine(line);
				sessionHistory.push_back(line);
				count++;
			}
		}

		printf("%sLoaded %zu commands from '%s'%s\n", COLOR_GREEN, count, filename.c_str(), COLOR_RESET);
	}

	void printHelp() {
		auto command = [](const char* names, const char* description) {
			printf("  %s%-18s%s %s\n", COLOR_GREEN, names, COLOR_RESET, description);
		};

		printf("\n");
		printf("%sREPL Commands:%s\n", COLOR_BOLD, COLOR_RESET);
		command("help, :help, :h", "Show this help message");
		command("exit, quit, :q", "Exit the REPL");
		command("stack, :stack", "Show current stack values");
		command("type, :type", "Show stack types (i64, f64, str, ptr)");
		command("clear, :clear", "Clear the stack");
		command("reset, :reset", "Reset the REPL: stack, definitions and imports");
		command(":doc <name>", "Show a signature; with no exact match, search names");
		command(":save <file>", "Save session history to file");
		command(":load <file>", "Load and execute commands from file");
		printf("\n");
		printf("%sKey Bindings:%s\n", COLOR_BOLD, COLOR_RESET);
		command("Tab", "Complete keywords, builtins and standard library names");
		command("Up/Down Arrow", "Navigate command history");
		command("Ctrl+R", "Search command history");
		command("Ctrl+D", "Exit REPL (EOF)");
		printf("\n");
		printf("%sUsage:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  Expressions are evaluated as you type them, and whatever they leave\n");
		printf("  on the stack stays there - the prompt shows it. Use 'print' to write\n");
		printf("  a value to the screen (which also pops it).\n");
		printf("\n");
		printf("%sExamples:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  []> 5 3\n");
		printf("  [5 3]> add\n");
		printf("  [8]> dup mul\n");
		printf("  [64]> print\n");
		printf("  64\n");
		printf("  []> fn double(x:i64 -- y:i64) { x x add }\n");
		printf("  Function defined\n");
		printf("  []> 21 double print\n");
		printf("  42\n");
		printf("\n");
	}

	void printDoc(const std::string& query) {
		size_t matches = 0;

		for (const auto& entry : g_reference) {
			if (query != entry.name) {
				continue;
			}
			matches++;
			if (entry.signature) {
				printf("  %s%s%s %s%s%s\n", COLOR_BOLD, entry.name, COLOR_RESET, COLOR_DIM, entry.signature,
						COLOR_RESET);
			} else {
				printf("  %s%s%s %s(keyword)%s\n", COLOR_BOLD, entry.name, COLOR_RESET, COLOR_DIM, COLOR_RESET);
			}
			printf("    %s\n", entry.description);
		}

		const auto& declarations = stdlibDeclarations();
		if (auto it = declarations.find(query); it != declarations.end()) {
			matches++;
			printf("  %s%s%s\n", COLOR_BOLD, it->second.c_str(), COLOR_RESET);
		}

		for (const auto& definition : definitions) {
			const std::string declaration = trim(definition.substr(0, definition.find('{')));
			if (declaration.find(query) != std::string::npos) {
				matches++;
				printf("  %s%s%s %s(this session)%s\n", COLOR_BOLD, declaration.c_str(), COLOR_RESET, COLOR_DIM,
						COLOR_RESET);
			}
		}

		if (matches > 0) {
			return;
		}

		for (const auto& name : completionCandidates()) {
			if (name.find(query) == std::string::npos || name.size() < 2 ||
					name.compare(name.size() - 2, 2, "::") == 0) {
				continue;
			}
			matches++;
			if (matches == 1) {
				printf("%sNo exact match for '%s'. Names containing it:%s\n", COLOR_DIM, query.c_str(), COLOR_RESET);
			}
			printf("  %s\n", name.c_str());
		}

		if (matches == 0) {
			printf("%sNothing found for '%s'%s\n", COLOR_DIM, query.c_str(), COLOR_RESET);
		}
	}

	void showStack() {
		const size_t depth = userStackDepth();
		if (depth == 0) {
			printf("%sStack is empty%s\n", COLOR_DIM, COLOR_RESET);
			return;
		}

		printf("%sStack (%zu items):%s\n", COLOR_BOLD, depth, COLOR_RESET);
		for (size_t i = 0; i < depth; i++) {
			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);
			const char* color = COLOR_RESET;
			const std::string text = formatValue(elem, color, std::string::npos);
			printf("  [%zu] %s%s%s\n", i, color, text.c_str(), COLOR_RESET);
		}
	}

	void showTypes() {
		size_t depth = userStackDepth();
		if (depth == 0) {
			printf("%sStack is empty%s\n", COLOR_DIM, COLOR_RESET);
			return;
		}

		printf("%sStack types (%zu items):%s\n", COLOR_BOLD, depth, COLOR_RESET);
		for (size_t i = 0; i < depth; i++) {
			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);
			printf("  [%zu] ", i);

			switch (elem.type) {
			case QD_STACK_TYPE_INT:
				printf("%si64%s\n", COLOR_BLUE, COLOR_RESET);
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("%sf64%s\n", COLOR_YELLOW, COLOR_RESET);
				break;
			case QD_STACK_TYPE_STR:
				printf("%sstr%s\n", COLOR_GREEN, COLOR_RESET);
				break;
			case QD_STACK_TYPE_PTR:
				printf("%sptr%s\n", COLOR_MAGENTA, COLOR_RESET);
				break;
			default:
				printf("%s?%s\n", COLOR_RED, COLOR_RESET);
				break;
			}
		}
	}

	void reset() {
		dropEverything();
		definitions.clear();
		useStatements.clear();
		printf("%sREPL reset%s\n", COLOR_DIM, COLOR_RESET);
	}

	void saveHistory() {
		if (!g_historyFile.empty()) {
			write_history(g_historyFile.c_str());
		}
	}

	static const char* declarationKind(const std::string& line) {
		static const std::pair<const char*, const char*> kinds[] = {
				{"fn ", "Function"},
				{"inline fn ", "Function"},
				{"struct ", "Struct"},
				{"packed struct ", "Struct"},
				{"enum ", "Enum"},
				{"const ", "Constant"},
				{"type ", "Type alias"},
				{"var ", "Global"},
		};

		const std::string rest = line.compare(0, 4, "pub ") == 0 ? line.substr(4) : line;
		for (const auto& [prefix, name] : kinds) {
			if (rest.compare(0, strlen(prefix), prefix) == 0) {
				return name;
			}
		}
		return nullptr;
	}

	void processLine(const std::string& line) {
		const std::string trimmedLine = trim(line);

		if (trimmedLine.rfind("use ", 0) == 0) {
			if (validateDefinition(line)) {
				useStatements.push_back(line);
				printf("%sModule imported%s\n", COLOR_DIM, COLOR_RESET);
			}
			return;
		}

		if (const char* kind = declarationKind(trimmedLine)) {
			if (validateDefinition(line)) {
				definitions.push_back(line);
				printf("%s%s defined%s\n", COLOR_DIM, kind, COLOR_RESET);
			}
			return;
		}

		compileAndExecute(line);
	}

	// Validate a function or use statement by trying to compile it
	bool validateDefinition(const std::string& definition) {
		// Build a minimal source with the definition
		std::string testSource;

		// Add existing use statements
		for (const auto& use : useStatements) {
			testSource += use + "\n";
		}

		// Add existing function definitions
		for (const auto& func : definitions) {
			testSource += func + "\n";
		}

		// Add the new definition
		testSource += definition + "\n";

		// Add a minimal main function if we don't have one
		testSource += "fn __repl_validate_main__() { }\n";

		// Try to parse
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(testSource.c_str(), false, "repl");

		if (!root || ast.hasErrors()) {
			// Print errors
			for (const auto& error : ast.getErrors()) {
				printf("%s%s%s\n", COLOR_RED, error.message.c_str(), COLOR_RESET);
			}
			return false;
		}

		return true;
	}

	std::string convertPrint(const std::string& code) {
		return rewriteWords(code, [](const std::string& word, std::string& out) {
			if (word == "print" || word == "printv" || word == "prints" || word == "printsv") {
				out = word + " nl";
				return true;
			}
			return false;
		});
	}

	std::vector<std::string> topLevelBindings(const std::string& code) {
		std::vector<std::string> names;

		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(("pub fn repl_main() {\n" + code + "\n}\n").c_str(), false, "repl");
		if (!root || ast.hasErrors()) {
			return names;
		}

		for (size_t i = 0; i < root->childCount(); i++) {
			Qd::IAstNode* node = root->child(i);
			if (!node || node->type() != Qd::IAstNode::Type::FUNCTION_DECLARATION) {
				continue;
			}
			auto* body = static_cast<Qd::AstNodeFunctionDeclaration*>(node)->body();
			if (!body) {
				continue;
			}
			for (size_t j = 0; j < body->childCount(); j++) {
				Qd::IAstNode* child = body->child(j);
				if (!child || child->type() != Qd::IAstNode::Type::LOCAL) {
					continue;
				}
				for (const auto& name : static_cast<Qd::AstNodeLocal*>(child)->names()) {
					if (name != "_") {
						names.push_back(name);
					}
				}
			}
		}
		return names;
	}

	static bool isAnonymousType(const std::string& type) {
		return type == "i64" || type == "f64" || type == "str" || type == "ptr" || type == "any";
	}

	std::string buildSource(const std::string& code, const std::vector<std::string>& stackTypes,
			const std::vector<Binding>& locals, const std::vector<Binding>& carried, size_t outputs) {
		bool named = false;
		for (const auto& type : stackTypes) {
			named = named || !isAnonymousType(type);
		}
		for (const auto& local : locals) {
			named = named || !isAnonymousType(local.type);
		}

		std::string source;

		for (const auto& use : useStatements) {
			source += use + "\n";
		}
		if (!useStatements.empty()) {
			source += "\n";
		}

		for (const auto& definition : definitions) {
			source += definition + "\n";
		}
		if (!definitions.empty()) {
			source += "\n";
		}

		source += "pub fn repl_main(";
		for (size_t i = 0; i < stackTypes.size(); i++) {
			source += named ? "_s" + std::to_string(i) + ":" + stackTypes[i] + " " : stackTypes[i] + " ";
		}
		for (const auto& local : locals) {
			source += named ? local.name + ":" + local.type + " " : local.type + " ";
		}
		source += "--";
		for (size_t i = 0; i < outputs; i++) {
			source += " r" + std::to_string(i) + ":any";
		}
		source += ") {\n";

		if (named) {
			if (!stackTypes.empty()) {
				source += "\t";
				for (size_t i = 0; i < stackTypes.size(); i++) {
					source += "_s" + std::to_string(i) + " ";
				}
				source += "\n";
			}
		} else if (!locals.empty()) {
			source += "\t";
			for (size_t i = locals.size(); i > 0; i--) {
				source += "-> " + locals[i - 1].name + " ";
			}
			source += "\n";
		}

		source += "\t" + code + "\n";

		if (!carried.empty()) {
			source += "\t";
			for (const auto& local : carried) {
				source += local.name + " ";
			}
			source += "\n";
		}

		source += "}\n";
		return source;
	}

	size_t bodyLineNumber() const {
		size_t line = 2;
		line += useStatements.size() + (useStatements.empty() ? 0 : 1);
		line += definitions.size() + (definitions.empty() ? 0 : 1);
		return line + 1;
	}

	bool analyseBody(
			const std::string& source, std::vector<std::string>& structTypes, std::vector<Qd::ErrorInfo>& parseErrors) {
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(source.c_str(), false, "repl");
		if (!root || ast.hasErrors()) {
			parseErrors = ast.getErrors();
			return false;
		}

		Qd::SemanticValidator validator;
		validator.setStoreErrors(true);
		validator.setDisplayFilename("repl");
		validator.validate(root, "repl", true, false);

		if (validator.finalStackFunction() != "repl_main") {
			return false;
		}

		const auto& names = validator.finalStackStructTypes();
		structTypes.assign(validator.finalStackTypes().size(), "");
		for (size_t i = 0; i < structTypes.size() && i < names.size(); i++) {
			structTypes[i] = names[i];
		}
		return true;
	}

	void recordState(const std::vector<std::string>& structTypes) {
		const size_t depth = qd_stack_size(ctx->st);
		const size_t localCount = std::min(sessionLocals.size(), depth);
		const size_t visible = depth - localCount;

		sessionStack.clear();
		for (size_t i = 0; i < depth; i++) {
			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);
			const std::string structType = i < structTypes.size() ? structTypes[i] : "";

			if (i < visible) {
				sessionStack.push_back(declaredType(elem, structType));
			} else {
				sessionLocals[i - visible].type = declaredType(elem, structType);
			}
		}
	}

	size_t userStackDepth() const {
		const size_t depth = qd_stack_size(ctx->st);
		return depth > sessionLocals.size() ? depth - sessionLocals.size() : 0;
	}

	void compileAndExecute(const std::string& code) {
		const std::string processedCode = convertPrint(code);

		const std::vector<std::string>& stackTypes = sessionStack;
		const std::vector<Binding>& locals = sessionLocals;

		std::vector<Binding> carried = locals;
		for (const auto& name : topLevelBindings(processedCode)) {
			const bool known =
					std::any_of(carried.begin(), carried.end(), [&](const Binding& b) { return b.name == name; });
			if (!known) {
				carried.push_back({name, "any"});
			}
		}

		std::vector<std::string> structTypes;
		std::vector<Qd::ErrorInfo> parseErrors;
		if (!analyseBody(buildSource(processedCode, stackTypes, locals, carried, 0), structTypes, parseErrors)) {
			structTypes.clear();
		}
		if (!parseErrors.empty()) {
			for (const auto& error : parseErrors) {
				printf("%s%s%s\n", COLOR_RED, error.message.c_str(), COLOR_RESET);
			}
			return;
		}

		const std::string source = buildSource(processedCode, stackTypes, locals, carried, structTypes.size());
		const std::string moduleName = "repl_" + std::to_string(moduleCounter++);

		int savedStderr = dup(STDERR_FILENO);
		int devNull = open("/dev/null", O_WRONLY);

		dup2(devNull, STDERR_FILENO);
		qd_module* execMod = qd_get_module(ctx, moduleName.c_str());
		qd_add_script(execMod, source.c_str());
		qd_set_warning_min_line(execMod, bodyLineNumber());
		qd_build(execMod);

		dup2(savedStderr, STDERR_FILENO);
		close(savedStderr);
		close(devNull);

		if (!qd_is_compiled(execMod)) {
			const std::string failName = "repl_" + std::to_string(moduleCounter++);
			qd_module* failMod = qd_get_module(ctx, failName.c_str());
			qd_add_script(failMod, source.c_str());
			qd_set_warning_min_line(failMod, bodyLineNumber());
			qd_build(failMod);
			return;
		}

		const std::string funcCall = moduleName + "::repl_main";

		// Set up signal handlers for crash recovery
		struct sigaction sa, oldAbrt, oldFpe, oldSegv;
		sa.sa_handler = signalHandler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;

		sigaction(SIGABRT, &sa, &oldAbrt);
		sigaction(SIGFPE, &sa, &oldFpe);
		sigaction(SIGSEGV, &sa, &oldSegv);

		int sig = sigsetjmp(g_jmpBuf, 1);
		if (sig == 0) {
			// Normal execution path
			g_inExecution = 1;
			qd_execute(ctx, funcCall.c_str());
			g_inExecution = 0;

			sessionLocals = carried;
			recordState(structTypes);
		} else {
			// Returned from signal handler - execution crashed
			g_inExecution = 0;
			printf("%sExecution error (signal %d)%s\n", COLOR_RED, sig, COLOR_RESET);

			dropEverything();
		}

		// Restore original signal handlers
		sigaction(SIGABRT, &oldAbrt, nullptr);
		sigaction(SIGFPE, &oldFpe, nullptr);
		sigaction(SIGSEGV, &oldSegv, nullptr);
	}
};

int main(int argc, char* argv[]) {
	bool printOnExit = false;

	qdcli::BaseOptions base;
	bool parsed = qdcli::parseArgs(argc, argv, base, "quadrepl", [&](const char* arg, int&, int, char*[]) -> bool {
		std::string a(arg);
		if (a == "-p" || a == "--print") {
			printOnExit = true;
			return true;
		}
		return false;
	});

	if (!parsed) {
		return 1;
	}
	if (base.help) {
		printf("quadrepl - Quadrate REPL\n\n");
		printf("Interactive Read-Eval-Print Loop for Quadrate.\n\n");
		printf("Usage: quadrepl [options]\n\n");
		printf("Options:\n");
		printf("  -h, --help       Show this help message\n");
		printf("  -v, --version    Show version information\n");
		printf("  -p, --print      Print stack to stdout on exit (implied when stdin is a pipe)\n");
		printf("      --no-color   Disable coloured output\n");
		printf("\nPiping:\n");
		printf("  echo \"1 2 add\" | quadrepl   Evaluate and print what is left on the stack\n\n");
		return 0;
	}
	if (base.version) {
		qdcli::printVersion("quadrepl");
		return 0;
	}

	// Configure colored output - disable if piped or NO_COLOR is set. The default
	// already accounts for NO_COLOR and redirected output; piped *stdin* is the
	// extra condition only the REPL cares about, so it can only subtract.
	const bool isPiped = !isatty(STDIN_FILENO);
	if (qdcli::noColor() || base.noColor || isPiped) {
		Qd::Colors::setEnabled(false);
	}

	if (isPiped) {
		printOnExit = true;
	}

	// Run the REPL
	ReplSession session(printOnExit);
	session.setNoColor(base.noColor);
	if (isPiped) {
		session.runWithPipedInput();
	} else {
		session.run();
	}

	return 0;
}
