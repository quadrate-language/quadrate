#include <quadrate/cli/cli.h>
#include <quadrate/platform/platform.h>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/colors.h>
#include <quadrate/qd/qd.h>
#include <quadrate/rt/stack.h>

#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
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

// Completions for tab completion
static const char* g_keywords[] = {"fn", "pub", "if", "else", "for", "loop", "break", "continue", "return", "use",
		"struct", "packed", "enum", "const", "var", "defer", "switch", "case", "test", "as", "null", "true", "false",
		"Ok", "Err", nullptr};

static const char* g_builtins[] = {
		// Stack operations
		"dup", "drop", "swap", "over", "rot", "nip", "pick", "roll",
		// Arithmetic
		"add", "sub", "mul", "div", "mod", "neg", "inc", "dec",
		// Comparison
		"eq", "ne", "lt", "gt", "le", "ge",
		// Logic
		"and", "or", "not", "xor",
		// Bitwise
		"shl", "shr", "band", "bor", "bnot", "bxor",
		// I/O
		"print", "prints", "printv", "printsv", "nl", "read",
		// Type conversion
		"i2f", "f2i", "i2s", "f2s",
		// Arrays
		"makei", "makef", "makes", "make", "len", "nth", "set", "append", nullptr};

static const char* g_modules[] = {
		"math::", "strings::", "io::", "fmt::", "os::", "mem::", "time::", "thread::", "flag::", "path::", "rand::",
		"unicode::", "strconv::", "bytes::", "bits::", "signal::", "term::", "limits::", "testing::", "sb::", nullptr};

// Module function completions (module::function format)
static const char* g_moduleFunctions[] = {
		// math module
		"math::abs", "math::sqrt", "math::sin", "math::cos", "math::tan", "math::asin", "math::acos", "math::atan",
		"math::atan2", "math::exp", "math::log", "math::log10", "math::log2", "math::pow", "math::floor", "math::ceil",
		"math::round", "math::trunc", "math::min", "math::max", "math::clamp", "math::lerp", "math::sq", "math::hypot",
		"math::PI", "math::E", "math::TAU",
		// strings module
		"strings::len", "strings::concat", "strings::substr", "strings::index", "strings::contains",
		"strings::starts_with", "strings::ends_with", "strings::trim", "strings::ltrim", "strings::rtrim",
		"strings::upper", "strings::lower", "strings::replace", "strings::split", "strings::join", "strings::repeat",
		"strings::reverse", "strings::compare", "strings::char_at", "strings::from_char",
		// io module
		"io::open", "io::close", "io::read", "io::write", "io::read_line", "io::read_file", "io::write_file",
		"io::append_file", "io::exists", "io::remove", "io::rename", "io::mkdir", "io::rmdir", "io::ReadOnly",
		"io::WriteOnly", "io::ReadWrite", "io::Append", "io::Create", "io::Truncate",
		// fmt module
		"fmt::sprintf", "fmt::printf", "fmt::pad_left", "fmt::pad_right",
		// os module
		"os::args", "os::getenv", "os::setenv", "os::exit", "os::exec", "os::system", "os::getcwd", "os::chdir",
		"os::hostname", "os::username", "os::pid", "os::ppid",
		// mem module
		"mem::alloc", "mem::realloc", "mem::free", "mem::copy", "mem::set", "mem::zero",
		// time module
		"time::now", "time::sleep", "time::since", "time::format", "time::parse", "time::year", "time::month",
		"time::day", "time::hour", "time::minute", "time::second", "time::weekday",
		// thread module
		"thread::spawn", "thread::join", "thread::sleep", "thread::yield", "thread::id", "thread::mutex_create",
		"thread::mutex_lock", "thread::mutex_unlock", "thread::mutex_destroy",
		// path module
		"path::join", "path::dir", "path::base", "path::ext", "path::abs", "path::rel", "path::clean", "path::is_abs",
		"path::split", "path::match",
		// rand module
		"rand::int", "rand::float", "rand::range", "rand::bytes", "rand::shuffle", "rand::choice", "rand::seed",
		// strconv module
		"strconv::atoi", "strconv::atof", "strconv::itoa", "strconv::ftoa", "strconv::parse_int",
		"strconv::parse_float",
		// bytes module
		"bytes::from_str", "bytes::to_str", "bytes::len", "bytes::get", "bytes::set", "bytes::slice",
		// bits module
		"bits::set", "bits::clear", "bits::toggle", "bits::test", "bits::count", "bits::reverse",
		// signal module
		"signal::handle", "signal::ignore", "signal::reset", "signal::raise",
		// term module
		"term::width", "term::height", "term::clear", "term::move", "term::color", "term::reset", "term::is_tty",
		"term::raw", "term::cooked",
		// limits module
		"limits::I8_MIN", "limits::I8_MAX", "limits::I16_MIN", "limits::I16_MAX", "limits::I32_MIN", "limits::I32_MAX",
		"limits::I64_MIN", "limits::I64_MAX", "limits::F32_MIN", "limits::F32_MAX", "limits::F64_MIN",
		"limits::F64_MAX",
		// sb (string builder) module
		"sb::new", "sb::write", "sb::writeln", "sb::string", "sb::len", "sb::clear", "sb::free", nullptr};

// Generator function for readline completion
static char* completionGenerator(const char* text, int state) {
	static int listIndex;
	static size_t len;
	static int phase; // 0=keywords, 1=builtins, 2=modules, 3=module functions
	const char* name;

	if (!state) {
		listIndex = 0;
		phase = 0;
		len = strlen(text);
	}

	while (phase < 4) {
		const char** list;
		switch (phase) {
		case 0:
			list = g_keywords;
			break;
		case 1:
			list = g_builtins;
			break;
		case 2:
			list = g_modules;
			break;
		case 3:
			list = g_moduleFunctions;
			break;
		default:
			return nullptr;
		}

		while ((name = list[listIndex]) != nullptr) {
			listIndex++;
			if (strncmp(name, text, len) == 0) {
				return strdup(name);
			}
		}
		phase++;
		listIndex = 0;
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
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_DIM "\033[2m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_CYAN "\033[36m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_RED "\033[31m"

// Stack display settings
#define MAX_STACK_DISPLAY 5

class ReplSession {
public:
	ReplSession(bool shouldPrintOnExit) : printOnExit(shouldPrintOnExit) {
		ctx = qd_create_context(1024);
		mod = qd_get_module(ctx, "repl");
		moduleCounter = 0;
		lastSuccessfulExprCount = 0;
		expectedStackDepth = 0;
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
		}

		printWelcome();

		std::string accumulated; // Accumulated multiline input
		int braceDepth = 0;		 // Track unbalanced braces

		while (true) {
			std::string prompt;
			std::string indent;
			if (braceDepth > 0) {
				// Continuation prompt with indentation indicator
				prompt = std::string(COLOR_DIM) + "...> " + COLOR_RESET;
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

			// Handle special commands
			if (completeInput == "exit" || completeInput == "quit" || completeInput == ":q") {
				if (printOnExit) {
					printStackToStdout();
				}
				saveHistory();
				break;
			} else if (completeInput == "help" || completeInput == ":help" || completeInput == ":h") {
				printHelp();
				continue;
			} else if (completeInput == "clear" || completeInput == ":clear") {
				clearStack();
				continue;
			} else if (completeInput == "stack" || completeInput == ":stack") {
				showStack();
				continue;
			} else if (completeInput == "type" || completeInput == ":type" || completeInput == ":types") {
				showTypes();
				continue;
			} else if (completeInput == "reset" || completeInput == ":reset") {
				reset();
				continue;
			} else if (completeInput.substr(0, 6) == ".save ") {
				std::string filename = trim(completeInput.substr(6));
				if (filename.empty()) {
					printf("%sUsage: .save <filename>%s\n", COLOR_RED, COLOR_RESET);
				} else {
					saveSession(filename);
				}
				continue;
			} else if (completeInput.substr(0, 6) == ".load ") {
				std::string filename = trim(completeInput.substr(6));
				if (filename.empty()) {
					printf("%sUsage: .load <filename>%s\n", COLOR_RED, COLOR_RESET);
				} else {
					loadSession(filename);
				}
				continue;
			}

			// Record successful commands for session history
			sessionHistory.push_back(completeInput);
			processLine(completeInput);
		}
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
		// Read all piped input
		std::string allInput;
		std::string line;
		while (std::getline(std::cin, line)) {
			line = trim(line);
			if (line.empty()) {
				continue;
			}
			if (!allInput.empty()) {
				allInput += " ";
			}
			allInput += line;
		}

		// Process piped input as Quadrate code
		if (!allInput.empty()) {
			processLine(allInput);
		}

		// Reopen stdin from terminal for interactive input
		if (!freopen("/dev/tty", "r", stdin)) {
			// Can't reopen terminal - print stack and exit
			if (printOnExit) {
				printStackToStdout();
			}
			return;
		}

		// Re-enable colors for interactive mode (unless NO_COLOR is set)
		if (!qdcli::noColor()) {
			Qd::Colors::setEnabled(true);
		}

		// Now run the interactive REPL with the pre-populated stack
		run();
	}

	// Parse a line of space-separated values and push them onto the stack
	void pushValuesFromLine(const std::string& line) {
		size_t pos = 0;
		size_t len = line.length();

		while (pos < len) {
			// Skip whitespace
			while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) {
				pos++;
			}
			if (pos >= len) {
				break;
			}

			// Check for quoted string
			if (line[pos] == '"') {
				pos++; // skip opening quote
				std::string str;
				while (pos < len && line[pos] != '"') {
					if (line[pos] == '\\' && pos + 1 < len) {
						pos++;
						switch (line[pos]) {
						case 'n':
							str += '\n';
							break;
						case 't':
							str += '\t';
							break;
						case 'r':
							str += '\r';
							break;
						case '\\':
							str += '\\';
							break;
						case '"':
							str += '"';
							break;
						default:
							str += line[pos];
							break;
						}
					} else {
						str += line[pos];
					}
					pos++;
				}
				if (pos < len) {
					pos++; // skip closing quote
				}
				qd_push_s(ctx, str.c_str());
			} else {
				// Read until whitespace
				size_t start = pos;
				while (pos < len && line[pos] != ' ' && line[pos] != '\t') {
					pos++;
				}
				std::string token = line.substr(start, pos - start);

				// Try to parse as number
				if (token.empty()) {
					continue;
				}

				// Check for float (contains '.' or 'e'/'E')
				bool isFloat = token.find('.') != std::string::npos || token.find('e') != std::string::npos ||
							   token.find('E') != std::string::npos;

				if (isFloat) {
					try {
						double val = std::stod(token);
						qd_push_f(ctx, val);
					} catch (...) {
						// Not a valid float, push as string
						qd_push_s(ctx, token.c_str());
					}
				} else {
					try {
						long long val = std::stoll(token);
						qd_push_i(ctx, val);
					} catch (...) {
						// Not a valid int, push as string
						qd_push_s(ctx, token.c_str());
					}
				}
			}
		}
	}

	// Print stack values to stdout (space-separated, compatible with 'read' instruction)
	void printStackToStdout() {
		size_t depth = getStackDepth();
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
	qd_context* ctx;
	qd_module* mod;
	std::vector<std::string> functionDefs;
	std::vector<std::string> useStatements;
	std::vector<std::string> history;		 // Accumulated expressions
	std::vector<std::string> sessionHistory; // Commands for .save/.load
	int moduleCounter;
	size_t lastSuccessfulExprCount; // Number of expressions successfully compiled
	size_t expectedStackDepth;		// Expected stack depth based on successful operations
	bool printOnExit;				// Whether to print stack on exit (-p flag)

	std::string trim(const std::string& str) {
		size_t first = str.find_first_not_of(" \t\n\r");
		if (first == std::string::npos) {
			return "";
		}
		size_t last = str.find_last_not_of(" \t\n\r");
		return str.substr(first, (last - first + 1));
	}

	size_t getStackDepth() {
		return qd_stack_size(ctx->st);
	}

	void clearStack() {
		// Clear expression history
		history.clear();
		// Also clear the context stack immediately
		while (!qd_stack_is_empty(ctx->st)) {
			qd_stack_element_t elem;
			qd_stack_pop(ctx->st, &elem);
		}
	}

	std::string buildPrompt() {
		std::string prompt;
		prompt += COLOR_CYAN;
		prompt += "[";

		size_t depth = getStackDepth();
		size_t startIndex = 0;

		if (depth > MAX_STACK_DISPLAY) {
			prompt += COLOR_DIM;
			prompt += "...";
			prompt += COLOR_RESET;
			prompt += " ";
			startIndex = depth - MAX_STACK_DISPLAY;
		}

		for (size_t i = startIndex; i < depth; i++) {
			if (i > startIndex) {
				prompt += " ";
			}

			// Get value at position (from bottom, index 0 = bottom)
			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);

			switch (elem.type) {
			case QD_STACK_TYPE_INT:
				prompt += COLOR_BLUE;
				prompt += std::to_string(elem.value.i);
				prompt += COLOR_RESET;
				break;
			case QD_STACK_TYPE_FLOAT:
				prompt += COLOR_YELLOW;
				prompt += std::to_string(elem.value.f);
				prompt += COLOR_RESET;
				break;
			case QD_STACK_TYPE_STR:
				prompt += COLOR_GREEN;
				prompt += "\"";
				if (elem.value.s && elem.value.s->data) {
					prompt += elem.value.s->data;
				}
				prompt += "\"";
				prompt += COLOR_RESET;
				break;
			case QD_STACK_TYPE_PTR:
				prompt += COLOR_MAGENTA;
				prompt += "ptr";
				prompt += COLOR_RESET;
				break;
			default:
				prompt += COLOR_RED;
				prompt += "?";
				prompt += COLOR_RESET;
				break;
			}
		}

		prompt += COLOR_CYAN;
		prompt += "]> ";
		prompt += COLOR_RESET;

		return prompt;
	}

	void printWelcome() {
		printf("%sQuadrate REPL %s%s\n", COLOR_BOLD, QUADRATE_VERSION, COLOR_RESET);
		printf("Type %shelp%s for available commands, %sexit%s to quit\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("%sTip: Use 'print' to display integer/float values, 'prints' for strings%s\n", COLOR_DIM, COLOR_RESET);
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
		printf("\n");
		printf("%sREPL Commands:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  %shelp%s, %s:help%s     Show this help message\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("  %sexit%s, %squit%s, %s:q%s  Exit the REPL\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET,
				COLOR_GREEN, COLOR_RESET);
		printf("  %sstack%s, %s:stack%s   Show current stack values\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("  %stype%s, %s:type%s    Show stack types (i64, f64, str, ptr)\n", COLOR_GREEN, COLOR_RESET,
				COLOR_GREEN, COLOR_RESET);
		printf("  %sclear%s, %s:clear%s   Clear the stack\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("  %sreset%s, %s:reset%s   Reset REPL (clear everything)\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("  %s.save <file>%s    Save session history to file\n", COLOR_GREEN, COLOR_RESET);
		printf("  %s.load <file>%s    Load and execute commands from file\n", COLOR_GREEN, COLOR_RESET);
		printf("\n");
		printf("%sKey Bindings:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  %sTab%s           Auto-complete keywords, builtins, module functions\n", COLOR_GREEN, COLOR_RESET);
		printf("  %sUp/Down Arrow%s  Navigate command history\n", COLOR_GREEN, COLOR_RESET);
		printf("  %sCtrl+R%s        Search command history\n", COLOR_GREEN, COLOR_RESET);
		printf("  %sCtrl+D%s        Exit REPL (EOF)\n", COLOR_GREEN, COLOR_RESET);
		printf("\n");
		printf("%sUsage:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  Type Quadrate expressions and they will be evaluated immediately.\n");
		printf("  Use 'print' or 'prints' to see output from your expressions.\n");
		printf("\n");
		printf("%sExamples:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  []> 5 3 add print\n");
		printf("  8\n");
		printf("  []> 10 dup mul print\n");
		printf("  100\n");
		printf("  []> fn double(x:i64 -- y:i64) { dup add }\n");
		printf("  Function defined\n");
		printf("  []> 21 double print\n");
		printf("  42\n");
		printf("\n");
	}

	void showStack() {
		size_t depth = getStackDepth();
		if (depth == 0) {
			printf("%sStack is empty%s\n", COLOR_DIM, COLOR_RESET);
			return;
		}

		printf("%sStack (%zu items):%s\n", COLOR_BOLD, depth, COLOR_RESET);
		for (size_t i = 0; i < depth; i++) {
			qd_stack_element_t elem;
			qd_stack_element(ctx->st, i, &elem);
			printf("  [%zu] ", i);

			switch (elem.type) {
			case QD_STACK_TYPE_INT:
				printf("%lld\n", static_cast<long long>(elem.value.i));
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("%f\n", elem.value.f);
				break;
			case QD_STACK_TYPE_STR:
				printf("\"%s\"\n", (elem.value.s && elem.value.s->data) ? elem.value.s->data : "");
				break;
			case QD_STACK_TYPE_PTR:
				printf("ptr:%p\n", elem.value.p);
				break;
			default:
				printf("?\n");
				break;
			}
		}
	}

	void showTypes() {
		size_t depth = getStackDepth();
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
		history.clear();
		functionDefs.clear();
		useStatements.clear();
		expectedStackDepth = 0;
		printf("%sREPL reset%s\n", COLOR_DIM, COLOR_RESET);
	}

	void saveHistory() {
		if (!g_historyFile.empty()) {
			write_history(g_historyFile.c_str());
		}
	}

	void processLine(const std::string& line) {
		std::string trimmedLine = trim(line);

		// Check if this is a use statement
		if (trimmedLine.rfind("use ", 0) == 0) {
			// Validate use statement by trying to parse it
			if (validateDefinition(line)) {
				useStatements.push_back(line);
				printf("%sModule imported%s\n", COLOR_DIM, COLOR_RESET);
			}
			return;
		}

		// Check if this is a function definition
		if (trimmedLine.rfind("fn ", 0) == 0) {
			// Validate function definition by trying to parse it
			if (validateDefinition(line)) {
				functionDefs.push_back(line);
				printf("%sFunction defined%s\n", COLOR_DIM, COLOR_RESET);
			}
			return;
		}

		// Regular expression - compile and execute
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
		for (const auto& func : functionDefs) {
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
		// In REPL, automatically add newlines after print operations for better output
		// Convert 'prints' to 'prints nl'
		// Convert 'print' to 'print nl'
		// Convert '.' to '. nl'
		std::string processedCode = code;

		// Handle '.' -> '. nl'
		size_t pos = 0;
		while ((pos = processedCode.find('.', pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isalnum(processedCode[pos - 1]));
			bool isWordEnd = (pos + 1 >= processedCode.length() || !isalnum(processedCode[pos + 1]));
			// Check it's not a decimal point
			bool isDecimalPoint = (pos > 0 && isdigit(processedCode[pos - 1])) ||
								  (pos + 1 < processedCode.length() && isdigit(processedCode[pos + 1]));
			if (isWordStart && isWordEnd && !isDecimalPoint) {
				processedCode.insert(pos + 1, " nl");
				pos += 4; // ". nl"
			} else {
				pos += 1;
			}
		}

		// Handle 'prints' -> 'prints nl' (before handling 'print')
		pos = 0;
		while ((pos = processedCode.find("prints", pos)) != std::string::npos) {
			// Skip 'printsv'
			if (pos + 7 < processedCode.length() && processedCode[pos + 6] == 'v') {
				pos += 7;
				continue;
			}
			bool isWordStart = (pos == 0 || !isalnum(processedCode[pos - 1]));
			bool isWordEnd = (pos + 6 >= processedCode.length() || !isalnum(processedCode[pos + 6]));
			if (isWordStart && isWordEnd) {
				processedCode.insert(pos + 6, " nl");
				pos += 9; // "prints nl"
			} else {
				pos += 6;
			}
		}

		// Handle 'printv' -> 'printv nl' (before handling 'print')
		pos = 0;
		while ((pos = processedCode.find("printv", pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isalnum(processedCode[pos - 1]));
			bool isWordEnd = (pos + 6 >= processedCode.length() || !isalnum(processedCode[pos + 6]));
			if (isWordStart && isWordEnd) {
				processedCode.insert(pos + 6, " nl");
				pos += 9; // "printv nl"
			} else {
				pos += 6;
			}
		}

		// Handle 'print' -> 'print nl' (but not 'prints' or 'printv' which we already handled)
		pos = 0;
		while ((pos = processedCode.find("print", pos)) != std::string::npos) {
			// Skip 'prints' (already has 'nl' appended)
			if (pos + 6 < processedCode.length() && processedCode[pos + 5] == 's') {
				pos += 6;
				continue;
			}
			// Skip 'printv' (already has 'nl' appended)
			if (pos + 6 < processedCode.length() && processedCode[pos + 5] == 'v') {
				pos += 6;
				continue;
			}
			bool isWordStart = (pos == 0 || !isalnum(processedCode[pos - 1]));
			bool isWordEnd = (pos + 5 >= processedCode.length() || !isalnum(processedCode[pos + 5]));
			if (isWordStart && isWordEnd) {
				processedCode.insert(pos + 5, " nl");
				pos += 8; // "print nl"
			} else {
				pos += 5;
			}
		}
		return processedCode;
	}

	// Strip print side effects from code for storage in history.
	// This prevents duplicate output when history is re-executed.
	// - '.' (print and pop) -> 'drop' (just pop, same stack effect)
	// - 'print' / 'printv' (print and pop) -> 'drop'
	// - 'prints' / 'printsv' (print stack, no pop) -> '' (remove, no stack effect)
	// - 'nl' (print newline) -> '' (remove, no stack effect)
	std::string stripPrintForHistory(const std::string& code) {
		std::string result = code;

		// Helper to check word boundaries
		auto isWordChar = [](char c) { return isalnum(c) || c == '_'; };

		// Replace '.' with 'drop' (must check it's not part of a number like 3.14)
		size_t pos = 0;
		while ((pos = result.find('.', pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isWordChar(result[pos - 1]));
			bool isWordEnd = (pos + 1 >= result.length() || !isWordChar(result[pos + 1]));
			// Also check it's not a decimal point (preceded by digit and followed by digit)
			bool isDecimalPoint =
					(pos > 0 && isdigit(result[pos - 1])) || (pos + 1 < result.length() && isdigit(result[pos + 1]));
			if (isWordStart && isWordEnd && !isDecimalPoint) {
				result.replace(pos, 1, "drop");
				pos += 4;
			} else {
				pos += 1;
			}
		}

		// Replace 'printv' with 'drop' (before 'print' to avoid partial match)
		pos = 0;
		while ((pos = result.find("printv", pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isWordChar(result[pos - 1]));
			bool isWordEnd = (pos + 6 >= result.length() || !isWordChar(result[pos + 6]));
			if (isWordStart && isWordEnd) {
				result.replace(pos, 6, "drop");
				pos += 4;
			} else {
				pos += 6;
			}
		}

		// Replace 'print' with 'drop' (but not 'prints' or 'printsv')
		pos = 0;
		while ((pos = result.find("print", pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isWordChar(result[pos - 1]));
			bool isWordEnd = (pos + 5 >= result.length() || !isWordChar(result[pos + 5]));
			if (isWordStart && isWordEnd) {
				result.replace(pos, 5, "drop");
				pos += 4;
			} else {
				pos += 5;
			}
		}

		// Remove 'printsv' (before 'prints' to avoid partial match)
		pos = 0;
		while ((pos = result.find("printsv", pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isWordChar(result[pos - 1]));
			bool isWordEnd = (pos + 7 >= result.length() || !isWordChar(result[pos + 7]));
			if (isWordStart && isWordEnd) {
				result.erase(pos, 7);
			} else {
				pos += 7;
			}
		}

		// Remove 'prints'
		pos = 0;
		while ((pos = result.find("prints", pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isWordChar(result[pos - 1]));
			bool isWordEnd = (pos + 6 >= result.length() || !isWordChar(result[pos + 6]));
			if (isWordStart && isWordEnd) {
				result.erase(pos, 6);
			} else {
				pos += 6;
			}
		}

		// Remove 'nl'
		pos = 0;
		while ((pos = result.find("nl", pos)) != std::string::npos) {
			bool isWordStart = (pos == 0 || !isWordChar(result[pos - 1]));
			bool isWordEnd = (pos + 2 >= result.length() || !isWordChar(result[pos + 2]));
			if (isWordStart && isWordEnd) {
				result.erase(pos, 2);
			} else {
				pos += 2;
			}
		}

		return result;
	}

	std::string buildSource(const std::vector<std::string>& expressions) {
		std::string source;

		// Add use statements
		for (const auto& use : useStatements) {
			source += use + "\n";
		}
		if (!useStatements.empty()) {
			source += "\n";
		}

		// Add function definitions
		for (const auto& func : functionDefs) {
			source += func + "\n";
		}
		if (!functionDefs.empty()) {
			source += "\n";
		}

		// Generate main function with all expressions
		source += "pub fn repl_main() {\n";
		for (const auto& expr : expressions) {
			source += "\t" + expr + "\n";
		}
		source += "}\n";

		return source;
	}

	// Calculate the line number where the Nth expression starts in the generated source
	// (accounting for package declaration added by qd_build)
	size_t getExpressionLineNumber(size_t exprIndex) {
		// Line 1: package repl_N (added by qd_build)
		// Line 2: (empty line after package)
		size_t line = 2;

		// Use statements
		line += useStatements.size();
		if (!useStatements.empty()) {
			line += 1; // Empty line after use statements
		}

		// Function definitions
		line += functionDefs.size();
		if (!functionDefs.empty()) {
			line += 1; // Empty line after function definitions
		}

		// Line for "pub fn repl_main() {"
		line += 1;

		// Expressions are on subsequent lines (1-indexed, exprIndex is 0-indexed)
		line += exprIndex + 1;

		return line;
	}

	void compileAndExecute(const std::string& code) {
		// Convert print to printv for the new code
		std::string processedCode = convertPrint(code);

		// Create a unique module name for this execution
		std::string moduleName = "repl_" + std::to_string(moduleCounter++);

		// Try compiling with the new expression added to history
		std::vector<std::string> testHistory = history;
		testHistory.push_back(processedCode);

		std::string source = buildSource(testHistory);

		// Suppress stderr during compilation attempts (errors printed on final failure)
		// Save stderr, redirect to /dev/null for probing
		int savedStderr = dup(STDERR_FILENO);
		int devNull = open("/dev/null", O_WRONLY);

		// First try: compile as-is (silently)
		dup2(devNull, STDERR_FILENO);
		qd_module* execMod = qd_get_module(ctx, moduleName.c_str());
		qd_add_script(execMod, source.c_str());
		if (lastSuccessfulExprCount > 0) {
			qd_set_warning_min_line(execMod, getExpressionLineNumber(lastSuccessfulExprCount));
		}
		qd_build(execMod);

		// If compilation failed, try adding print+drop for leftover stack values
		if (!qd_is_compiled(execMod)) {
			for (int drops = 1; drops <= 8; drops++) {
				std::string cleanup = "print nl";
				for (int i = 1; i < drops; i++) {
					cleanup += " drop";
				}

				std::vector<std::string> retryHistory = history;
				retryHistory.push_back(processedCode);
				retryHistory.push_back(cleanup);
				std::string retrySource = buildSource(retryHistory);

				std::string retryName = "repl_" + std::to_string(moduleCounter++);
				execMod = qd_get_module(ctx, retryName.c_str());
				qd_add_script(execMod, retrySource.c_str());
				if (lastSuccessfulExprCount > 0) {
					qd_set_warning_min_line(execMod, getExpressionLineNumber(lastSuccessfulExprCount));
				}
				qd_build(execMod);

				if (qd_is_compiled(execMod)) {
					moduleName = retryName;
					processedCode += "\n" + cleanup;
					break;
				}
			}
		}

		// Restore stderr
		dup2(savedStderr, STDERR_FILENO);
		close(savedStderr);
		close(devNull);

		// If still not compiled, re-compile with errors visible
		if (!qd_is_compiled(execMod)) {
			// Recompile once more with stderr visible so user sees the error
			std::string failName = "repl_" + std::to_string(moduleCounter++);
			qd_module* failMod = qd_get_module(ctx, failName.c_str());
			qd_add_script(failMod, source.c_str());
			if (lastSuccessfulExprCount > 0) {
				qd_set_warning_min_line(failMod, getExpressionLineNumber(lastSuccessfulExprCount));
			}
			qd_build(failMod);
			return;
		}

		// Clear the stack before execution (we re-execute all history)
		while (!qd_stack_is_empty(ctx->st)) {
			qd_stack_element_t elem;
			qd_stack_pop(ctx->st, &elem);
		}

		// Execute the main function with crash protection
		std::string funcCall = moduleName + "::repl_main";

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

			// Check if execution actually succeeded by comparing stack depth
			// Some operations like '.' silently fail without aborting
			size_t actualDepth = getStackDepth();

			// Add to history on success (strip print effects to avoid duplicates on re-execution)
			std::string historyEntry = stripPrintForHistory(processedCode);

			// Only add to history if we have something meaningful to add
			std::string trimmed = historyEntry;
			// Trim whitespace
			size_t start = trimmed.find_first_not_of(" \t\n\r");
			size_t end = trimmed.find_last_not_of(" \t\n\r");
			if (start != std::string::npos) {
				trimmed = trimmed.substr(start, end - start + 1);
			} else {
				trimmed = "";
			}

			// Only add to history if the entry would be safe to replay.
			// Count how many 'drop' operations are in the history entry and ensure
			// we'll have enough stack depth on replay.
			size_t dropCount = 0;
			size_t pos = 0;
			while ((pos = trimmed.find("drop", pos)) != std::string::npos) {
				// Check it's a word boundary
				bool isWordStart = (pos == 0 || !isalnum(trimmed[pos - 1]));
				bool isWordEnd = (pos + 4 >= trimmed.length() || !isalnum(trimmed[pos + 4]));
				if (isWordStart && isWordEnd) {
					dropCount++;
				}
				pos += 4;
			}

			// Only add if we have enough depth for the drops
			// expectedStackDepth is the depth BEFORE this expression
			// actualDepth is the depth AFTER this expression
			// The expression is safe to add if actualDepth >= 0 (which it always is)
			// and if we wouldn't underflow on replay
			if (!trimmed.empty() && (dropCount == 0 || actualDepth + dropCount <= expectedStackDepth + 100)) {
				// The condition above is a sanity check. The real check is:
				// did the execution actually modify the stack as expected?
				// If we're adding drops but the stack didn't shrink, something failed.
				bool executionSucceeded = true;
				if (dropCount > 0) {
					// If we added drops, the actual depth should reflect that
					// If actual depth > expected - drops, a drop failed silently
					// Actually, we need to think about pushes too...
					// Simpler: if actualDepth equals expectedStackDepth and we're adding drops,
					// those drops didn't actually execute (stack underflow)
					if (actualDepth == expectedStackDepth && dropCount > 0) {
						// The drops didn't execute - don't add them to history
						executionSucceeded = false;
					}
				}

				if (executionSucceeded) {
					history.push_back(historyEntry);
					lastSuccessfulExprCount = history.size();
					expectedStackDepth = actualDepth;
				}
			} else if (trimmed.empty()) {
				// Empty entry, just update expected depth
				expectedStackDepth = actualDepth;
			}
		} else {
			// Returned from signal handler - execution crashed
			g_inExecution = 0;
			printf("%sExecution error (signal %d)%s\n", COLOR_RED, sig, COLOR_RESET);

			// Clear the stack to recover to a clean state
			while (!qd_stack_is_empty(ctx->st)) {
				qd_stack_element_t elem;
				qd_stack_pop(ctx->st, &elem);
			}
			// Don't add to history on crash
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
		printf("  -p, --print      Print stack to stdout on exit\n");
		printf("\nPiping:\n");
		printf("  echo \"1 2 3\" | quadrepl      Start with values on stack\n");
		printf("  echo \"1 2 add\" | quadrepl -p  Compute and print result\n\n");
		return 0;
	}
	if (base.version) {
		qdcli::printVersion("quadrepl");
		return 0;
	}

	// Configure colored output - disable if piped or NO_COLOR is set
	const bool isPiped = !isatty(STDIN_FILENO);
	const bool noColors = qdcli::noColor() || isPiped;
	Qd::Colors::setEnabled(!noColors);

	// Run the REPL
	ReplSession session(printOnExit);
	if (isPiped) {
		session.runWithPipedInput();
	} else {
		session.run();
	}

	return 0;
}
