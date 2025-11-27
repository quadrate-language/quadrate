#include <qd/qd.h>
#include <qc/colors.h>
#include <qdrt/stack.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <readline/history.h>
#include <readline/readline.h>
#include <string>
#include <vector>

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

private:
	qd_context* ctx;
	qd_module* mod;
	std::vector<std::string> functionDefs;
	std::vector<std::string> useStatements;
	std::vector<std::string> history; // Accumulated expressions
	int moduleCounter;

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
		printf("Type %shelp%s for available commands, %sexit%s to quit\n",
			   COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("%sTip: Use 'print' to display integer/float values, 'prints' for strings%s\n",
			   COLOR_DIM, COLOR_RESET);
		printf("\n");
	}

	void printHelp() {
		printf("\n");
		printf("%sREPL Commands:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("  %shelp%s, %s:help%s     Show this help message\n",
			   COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("  %sexit%s, %squit%s, %s:q%s  Exit the REPL\n",
			   COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("  %sstack%s, %s:stack%s   Show current stack state\n",
			   COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("  %sclear%s, %s:clear%s   Clear the stack\n",
			   COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
		printf("  %sreset%s, %s:reset%s   Reset REPL (clear everything)\n",
			   COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
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
		// Convert 'print' to 'printv' for better REPL output
		std::string processedCode = code;
		size_t pos = 0;
		while ((pos = processedCode.find("print", pos)) != std::string::npos) {
			if (pos + 6 < processedCode.length() && processedCode[pos + 5] == 's') {
				pos += 6;
				continue;
			}
			if (pos + 6 < processedCode.length() && processedCode[pos + 5] == 'v') {
				pos += 6;
				continue;
			}
			bool isWordStart = (pos == 0 || !isalnum(processedCode[pos - 1]));
			bool isWordEnd = (pos + 5 >= processedCode.length() || !isalnum(processedCode[pos + 5]));
			if (isWordStart && isWordEnd) {
				processedCode.insert(pos + 5, "v");
				pos += 6;
			} else {
				pos += 5;
			}
		}
		return processedCode;
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

		// Execute the main function
		std::string funcCall = moduleName + "::repl_main";
		qd_execute(ctx, funcCall.c_str());

		// Add to history on success
		history.push_back(processedCode);
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

	// Configure colored output
	const bool noColors = std::getenv("NO_COLOR") != nullptr;
	Qd::Colors::setEnabled(!noColors);

	// Run the REPL
	ReplSession session;
	session.run();

	return 0;
}
