#include <qc/colors.h>
#include <qd/qd.h>
#include <qdrt/stack.h>

#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <readline/history.h>
#include <readline/readline.h>
#include <string>
#include <unistd.h>
#include <vector>

// Signal handling for crash recovery
static sigjmp_buf g_jmpBuf;
static volatile sig_atomic_t g_inExecution = 0;

static void signalHandler(int sig) {
	if (g_inExecution) {
		siglongjmp(g_jmpBuf, sig);
	}
	// If not in execution, let the default handler run
	signal(sig, SIG_DFL);
	raise(sig);
}

#define QUADRATE_VERSION "0.1.0"

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
	ReplSession() {
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
		printWelcome();

		while (true) {
			std::string prompt = buildPrompt();
			char* input = readline(prompt.c_str());

			if (!input) {
				// EOF (Ctrl+D)
				printf("\n");
				break;
			}

			std::string line = trim(input);
			free(input);

			if (line.empty()) {
				continue;
			}

			add_history(line.c_str());

			// Handle special commands
			if (line == "exit" || line == "quit" || line == ":q") {
				break;
			} else if (line == "help" || line == ":help" || line == ":h") {
				printHelp();
				continue;
			} else if (line == "clear" || line == ":clear") {
				clearStack();
				continue;
			} else if (line == "stack" || line == ":stack") {
				showStack();
				continue;
			} else if (line == "reset" || line == ":reset") {
				reset();
				continue;
			}

			processLine(line);
		}

		printf("Goodbye!\n");
	}

	// Run in piped/non-interactive mode - read from stdin, no prompts
	void runPiped() {
		std::string line;
		while (std::getline(std::cin, line)) {
			line = trim(line);
			if (line.empty()) {
				continue;
			}
			processLine(line);
		}
	}

private:
	qd_context* ctx;
	qd_module* mod;
	std::vector<std::string> functionDefs;
	std::vector<std::string> useStatements;
	std::vector<std::string> history; // Accumulated expressions
	int moduleCounter;
	size_t lastSuccessfulExprCount; // Number of expressions successfully compiled
	size_t expectedStackDepth;		// Expected stack depth based on successful operations

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
		printf("%sQuadrate %s REPL%s\n", COLOR_BOLD, QUADRATE_VERSION, COLOR_RESET);
		printf("Type %shelp%s for available commands, %sexit%s to quit\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("%sTip: Use 'print' to display integer/float values, 'prints' for strings%s\n", COLOR_DIM, COLOR_RESET);
		printf("\n");
	}

	void printHelp() {
		printf("\n");
		printf("%sREPL Commands:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  %shelp%s, %s:help%s     Show this help message\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("  %sexit%s, %squit%s, %s:q%s  Exit the REPL\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET,
				COLOR_GREEN, COLOR_RESET);
		printf("  %sstack%s, %s:stack%s   Show current stack state\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("  %sclear%s, %s:clear%s   Clear the stack\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("  %sreset%s, %s:reset%s   Reset REPL (clear everything)\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN,
				COLOR_RESET);
		printf("\n");
		printf("%sKey Bindings:%s\n", COLOR_BOLD, COLOR_RESET);
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

	void reset() {
		history.clear();
		functionDefs.clear();
		useStatements.clear();
		expectedStackDepth = 0;
		printf("%sREPL reset%s\n", COLOR_DIM, COLOR_RESET);
	}

	void processLine(const std::string& line) {
		std::string trimmedLine = trim(line);

		// Check if this is a use statement
		if (trimmedLine.rfind("use ", 0) == 0) {
			useStatements.push_back(line);
			printf("%sModule imported%s\n", COLOR_DIM, COLOR_RESET);
			return;
		}

		// Check if this is a function definition
		if (trimmedLine.rfind("fn ", 0) == 0) {
			functionDefs.push_back(line);
			printf("%sFunction defined%s\n", COLOR_DIM, COLOR_RESET);
			return;
		}

		// Regular expression - compile and execute
		compileAndExecute(line);
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
		source += "pub fn repl_main( -- ) {\n";
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

		// Line for "pub fn repl_main( -- ) {"
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

		// Get a fresh module for this execution
		qd_module* execMod = qd_get_module(ctx, moduleName.c_str());
		qd_add_script(execMod, source.c_str());

		// Suppress warnings for previously compiled code
		// The new expression starts at line getExpressionLineNumber(lastSuccessfulExprCount)
		if (lastSuccessfulExprCount > 0) {
			qd_set_warning_min_line(execMod, getExpressionLineNumber(lastSuccessfulExprCount));
		}

		qd_build(execMod);

		// Only proceed if compilation succeeded
		if (!qd_is_compiled(execMod)) {
			return; // Don't add to history on compile failure
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

void printVersion() {
	printf("%s\n", QUADRATE_VERSION);
}

void printHelp() {
	printf("quadrate - Quadrate REPL\n\n");
	printf("Interactive Read-Eval-Print Loop for Quadrate.\n\n");
	printf("Usage: quadrate [options]\n\n");
	printf("Options:\n");
	printf("  -h, --help       Show this help message\n");
	printf("  -v, --version    Show version information\n");
	printf("\n");
}

int main(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			printHelp();
			return 0;
		} else if (arg == "-v" || arg == "--version") {
			printVersion();
			return 0;
		} else {
			fprintf(stderr, "quadrate: unknown option: %s\n", arg.c_str());
			fprintf(stderr, "Try 'quadrate --help' for more information.\n");
			return 1;
		}
	}

	// Configure colored output - disable if piped or NO_COLOR is set
	const bool isPiped = !isatty(STDIN_FILENO);
	const bool noColors = std::getenv("NO_COLOR") != nullptr || isPiped;
	Qd::Colors::setEnabled(!noColors);

	// Run the REPL
	ReplSession session;
	if (isPiped) {
		session.runPiped();
	} else {
		session.run();
	}

	return 0;
}
