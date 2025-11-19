#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <llvmgen/generator.h>
#include <qc/ast.h>
#include <qc/colors.h>
#include <qc/semantic_validator.h>
#include <random>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
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
#define MAX_STACK_DISPLAY 5 // Show only top N elements

class ReplSession {
public:
	ReplSession() {
		tempDir = createTempDir();
		lineNumber = 1;
	}

	~ReplSession() {
		cleanup();
	}

	void run() {
		printWelcome();

		while (true) {
			// Build prompt string with color coding and truncation
			std::stringstream promptStr;
			promptStr << COLOR_CYAN << "[";

			size_t startIndex = 0;

			// Truncate if stack is too large
			if (stackValues.size() > MAX_STACK_DISPLAY) {
				promptStr << COLOR_DIM << "..." << COLOR_RESET << " ";
				startIndex = stackValues.size() - MAX_STACK_DISPLAY;
			}

			for (size_t i = startIndex; i < stackValues.size(); i++) {
				if (i > startIndex) {
					promptStr << " ";
				}

				// Color code by type
				const std::string& val = stackValues[i];
				if (val.length() > 0) {
					if (val[0] == '"') {
						// String literal - green
						promptStr << COLOR_GREEN << val << COLOR_RESET;
					} else if (val[0] == '&') {
						// Function pointer - magenta
						promptStr << COLOR_MAGENTA << val << COLOR_RESET;
					} else if (val.find('.') != std::string::npos) {
						// Float - yellow
						promptStr << COLOR_YELLOW << val << COLOR_RESET;
					} else if (val == "?") {
						// Unknown - red
						promptStr << COLOR_RED << val << COLOR_RESET;
					} else {
						// Integer or other - blue
						promptStr << COLOR_BLUE << val << COLOR_RESET;
					}
				}
			}

			promptStr << COLOR_CYAN << "]> " << COLOR_RESET;

			// Use readline for input with history support
			char* input = readline(promptStr.str().c_str());

			if (!input) {
				// EOF (Ctrl+D)
				std::cout << std::endl;
				break;
			}

			std::string line(input);
			free(input);

			// Trim whitespace
			line = trim(line);

			if (line.empty()) {
				continue;
			}

			// Add to history
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

			// Process the line
			if (!processLine(line)) {
				// Error already printed
				continue;
			}

			lineNumber++;
		}

		std::cout << "Goodbye!" << std::endl;
	}

private:
	std::string tempDir;
	std::vector<std::string> history;
	std::vector<std::string> functionDefinitions;
	std::vector<std::string> useStatements;
	std::vector<std::string> stackValues; // Current stack state for display
	int lineNumber;
	size_t lastOutputLineCount = 0;

	std::string createTempDir() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 15);

		std::filesystem::path baseDir = std::filesystem::temp_directory_path();

		for (int attempt = 0; attempt < 10; attempt++) {
			std::stringstream ss;
			ss << "quadrate_repl_";
			for (int i = 0; i < 8; i++) {
				ss << std::hex << dis(gen);
			}

			std::filesystem::path tmpDir = baseDir / ss.str();

			std::error_code ec;
			if (std::filesystem::create_directory(tmpDir, ec)) {
				return tmpDir.string();
			}
		}

		std::cerr << "quadrate: failed to create temporary directory" << std::endl;
		exit(1);
	}

	void cleanup() {
		if (!tempDir.empty()) {
			std::filesystem::remove_all(tempDir);
		}
	}

	std::string trim(const std::string& str) {
		size_t first = str.find_first_not_of(" \t\n\r");
		if (first == std::string::npos) {
			return "";
		}
		size_t last = str.find_last_not_of(" \t\n\r");
		return str.substr(first, (last - first + 1));
	}

	void printWelcome() {
		std::cout << COLOR_BOLD << "Quadrate " << QUADRATE_VERSION << " REPL" << COLOR_RESET << std::endl;
		std::cout << "Type " << COLOR_GREEN << "help" << COLOR_RESET << " for available commands, " << COLOR_GREEN
				  << "exit" << COLOR_RESET << " to quit" << std::endl;
		std::cout << COLOR_DIM << "Tip: Use 'print' to display integer/float values, 'prints' for strings"
				  << COLOR_RESET << std::endl;
		std::cout << std::endl;
	}

	void printHelp() {
		std::cout << std::endl;
		std::cout << COLOR_BOLD << "REPL Commands:" << COLOR_RESET << std::endl;
		std::cout << "  " << COLOR_GREEN << "help" << COLOR_RESET << ", " << COLOR_GREEN << ":help" << COLOR_RESET
				  << "     Show this help message" << std::endl;
		std::cout << "  " << COLOR_GREEN << "exit" << COLOR_RESET << ", " << COLOR_GREEN << "quit" << COLOR_RESET
				  << ", " << COLOR_GREEN << ":q" << COLOR_RESET << "  Exit the REPL" << std::endl;
		std::cout << "  " << COLOR_GREEN << "stack" << COLOR_RESET << ", " << COLOR_GREEN << ":stack" << COLOR_RESET
				  << "   Show current stack state" << std::endl;
		std::cout << "  " << COLOR_GREEN << "clear" << COLOR_RESET << ", " << COLOR_GREEN << ":clear" << COLOR_RESET
				  << "   Clear the stack" << std::endl;
		std::cout << "  " << COLOR_GREEN << "reset" << COLOR_RESET << ", " << COLOR_GREEN << ":reset" << COLOR_RESET
				  << "   Reset REPL (clear everything)" << std::endl;
		std::cout << std::endl;
		std::cout << COLOR_BOLD << "Key Bindings:" << COLOR_RESET << std::endl;
		std::cout << "  " << COLOR_GREEN << "Up/Down Arrow" << COLOR_RESET << "  Navigate command history" << std::endl;
		std::cout << "  " << COLOR_GREEN << "Ctrl+R" << COLOR_RESET << "        Search command history" << std::endl;
		std::cout << "  " << COLOR_GREEN << "Ctrl+D" << COLOR_RESET << "        Exit REPL (EOF)" << std::endl;
		std::cout << std::endl;
		std::cout << COLOR_BOLD << "Usage:" << COLOR_RESET << std::endl;
		std::cout << "  Type Quadrate expressions and they will be evaluated immediately." << std::endl;
		std::cout << "  Use 'print' or 'prints' to see output from your expressions." << std::endl;
		std::cout << std::endl;
		std::cout << COLOR_BOLD << "Examples:" << COLOR_RESET << std::endl;
		std::cout << "  []> 5 3 add print" << std::endl;
		std::cout << "  8" << std::endl;
		std::cout << "  []> 10 dup mul print" << std::endl;
		std::cout << "  100" << std::endl;
		std::cout << "  []> fn double(x:i -- y:i) { dup add }" << std::endl;
		std::cout << "  Function defined" << std::endl;
		std::cout << "  []> 21 double print" << std::endl;
		std::cout << "  42" << std::endl;
		std::cout << std::endl;
	}

	void clearStack() {
		stackValues.clear();
		history.clear();
		lastOutputLineCount = 0;
	}

	void showStack() {
		if (stackValues.empty()) {
			std::cout << COLOR_DIM << "Stack is empty" << COLOR_RESET << std::endl;
			return;
		}

		std::cout << COLOR_BOLD << "Stack (" << stackValues.size() << " items):" << COLOR_RESET << std::endl;
		for (size_t i = 0; i < stackValues.size(); i++) {
			std::cout << "  [" << i << "] " << stackValues[i] << std::endl;
		}
	}

	void reset() {
		stackValues.clear();
		functionDefinitions.clear();
		useStatements.clear();
		history.clear();
		lastOutputLineCount = 0;
		lineNumber = 1;
		std::cout << COLOR_DIM << "REPL reset" << COLOR_RESET << std::endl;
	}

	bool validateUseStatement() {
		// Try to compile a minimal program with the use statement to validate it
		std::stringstream source;

		// Add all use statements
		for (const auto& use : useStatements) {
			source << use << "\n";
		}

		// Add minimal main function
		source << "\nfn main( -- ) { }\n";

		std::string sourceCode = source.str();

		// Parse the source
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(sourceCode.c_str(), false, "<repl>");

		if (!root || ast.hasErrors()) {
			return false;
		}

		// Semantic validation
		Qd::SemanticValidator validator;
		size_t errorCount = validator.validate(root, "<repl>");
		if (errorCount > 0) {
			return false;
		}

		return true;
	}

	bool validateFunctionDefinition() {
		// Try to compile with the function definition to validate it
		std::stringstream source;

		// Add use statements
		for (const auto& use : useStatements) {
			source << use << "\n";
		}
		if (!useStatements.empty()) {
			source << "\n";
		}

		// Add all function definitions
		for (const auto& func : functionDefinitions) {
			source << func << "\n";
		}

		// Add minimal main function
		source << "\nfn main( -- ) { }\n";

		std::string sourceCode = source.str();

		// Parse the source
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(sourceCode.c_str(), false, "<repl>");

		if (!root || ast.hasErrors()) {
			return false;
		}

		// Semantic validation
		Qd::SemanticValidator validator;
		size_t errorCount = validator.validate(root, "<repl>");
		if (errorCount > 0) {
			return false;
		}

		return true;
	}

	bool processLine(const std::string& line) {
		// Check if this is a use statement
		std::string trimmedLine = trim(line);
		if (trimmedLine.rfind("use ", 0) == 0) {
			// Validate the use statement by trying to compile with it
			useStatements.push_back(line);
			bool success = validateUseStatement();
			if (!success) {
				// Remove the invalid use statement
				useStatements.pop_back();
				return false;
			}
			std::cout << COLOR_DIM << "Module imported" << COLOR_RESET << std::endl;
			return true;
		}

		// Check if this is a function definition (starts with "fn ")
		if (trimmedLine.rfind("fn ", 0) == 0) {
			// Validate the function definition by trying to compile with it
			functionDefinitions.push_back(line);
			bool success = validateFunctionDefinition();
			if (!success) {
				// Remove the invalid function definition
				functionDefinitions.pop_back();
				return false;
			}
			std::cout << COLOR_DIM << "Function defined" << COLOR_RESET << std::endl;
			return true;
		}

		// Regular expression - compile and execute
		return compileAndExecute(line);
	}

	void updateStackSimulation(const std::string& code) {
		// Simulate stack operations with actual evaluation when possible
		// Manual tokenization to handle string literals properly
		size_t pos = 0;
		while (pos < code.length()) {
			// Skip whitespace
			while (pos < code.length() && std::isspace(code[pos])) {
				pos++;
			}
			if (pos >= code.length()) {
				break;
			}

			std::string token;

			// Check for string literal
			if (code[pos] == '"') {
				size_t start = pos;
				pos++; // Skip opening quote
				while (pos < code.length() && code[pos] != '"') {
					if (code[pos] == '\\' && pos + 1 < code.length()) {
						pos += 2; // Skip escape sequence
					} else {
						pos++;
					}
				}
				if (pos < code.length()) {
					pos++; // Skip closing quote
				}
				token = code.substr(start, pos - start);
				// Push string literal to stack
				stackValues.push_back(token);
				continue;
			}

			// Regular token (identifier, number, operator)
			size_t start = pos;
			while (pos < code.length() && !std::isspace(code[pos])) {
				pos++;
			}
			token = code.substr(start, pos - start);

			// Check for function pointer (starts with &)
			if (token.length() > 1 && token[0] == '&') {
				// Function pointer - push to stack
				stackValues.push_back(token);
				continue;
			}

			// Try to parse as number
			bool isInt = true;
			bool isFloat = false;

			try {
				if (token.find('.') != std::string::npos) {
					(void)std::stod(token);
					isFloat = true;
					isInt = false;
				} else {
					(void)std::stoll(token);
				}
			} catch (...) {
				isInt = false;
			}

			if (isInt || isFloat) {
				stackValues.push_back(token);
			} else if (token == "+" || token == "-" || token == "*" || token == "/" || token == "%" || token == "add" ||
					   token == "sub" || token == "mul" || token == "div" || token == "mod") {
				if (stackValues.size() >= 2) {
					std::string b = stackValues.back();
					stackValues.pop_back();
					std::string a = stackValues.back();
					stackValues.pop_back();

					// Try to evaluate
					try {
						// Check if either operand is a float
						bool aIsFloat = (a.find('.') != std::string::npos);
						bool bIsFloat = (b.find('.') != std::string::npos);

						if (aIsFloat || bIsFloat) {
							// Use floating-point arithmetic
							double aVal = std::stod(a);
							double bVal = std::stod(b);
							double result = 0.0;
							if (token == "add" || token == "+") {
								result = aVal + bVal;
							} else if (token == "sub" || token == "-") {
								result = aVal - bVal;
							} else if (token == "mul" || token == "*") {
								result = aVal * bVal;
							} else if ((token == "div" || token == "/") && bVal != 0.0) {
								result = aVal / bVal;
							} else if (token == "mod" || token == "%") {
								result = std::fmod(aVal, bVal);
							}
							stackValues.push_back(std::to_string(result));
						} else {
							// Use integer arithmetic
							long long aVal = std::stoll(a);
							long long bVal = std::stoll(b);
							long long result = 0;
							if (token == "add" || token == "+") {
								result = aVal + bVal;
							} else if (token == "sub" || token == "-") {
								result = aVal - bVal;
							} else if (token == "mul" || token == "*") {
								result = aVal * bVal;
							} else if ((token == "div" || token == "/") && bVal != 0) {
								result = aVal / bVal;
							} else if ((token == "mod" || token == "%") && bVal != 0) {
								result = aVal % bVal;
							}
							stackValues.push_back(std::to_string(result));
						}
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "dup") {
				if (!stackValues.empty()) {
					stackValues.push_back(stackValues.back());
				}
			} else if (token == "dup2") {
				if (stackValues.size() >= 2) {
					std::string second = stackValues[stackValues.size() - 2];
					std::string first = stackValues[stackValues.size() - 1];
					stackValues.push_back(second);
					stackValues.push_back(first);
				}
			} else if (token == "drop" || token == "print" || token == "printv" || token == "prints" || token == "." ||
					   token == "call") {
				if (!stackValues.empty()) {
					stackValues.pop_back();
				}
			} else if (token == "drop2") {
				if (stackValues.size() >= 2) {
					stackValues.pop_back();
					stackValues.pop_back();
				}
			} else if (token == "swap") {
				if (stackValues.size() >= 2) {
					std::string top = stackValues.back();
					stackValues.pop_back();
					std::string second = stackValues.back();
					stackValues.pop_back();
					stackValues.push_back(top);
					stackValues.push_back(second);
				}
			} else if (token == "swap2") {
				if (stackValues.size() >= 4) {
					std::string d = stackValues.back();
					stackValues.pop_back();
					std::string c = stackValues.back();
					stackValues.pop_back();
					std::string b = stackValues.back();
					stackValues.pop_back();
					std::string a = stackValues.back();
					stackValues.pop_back();
					stackValues.push_back(c);
					stackValues.push_back(d);
					stackValues.push_back(a);
					stackValues.push_back(b);
				}
			} else if (token == "over") {
				if (stackValues.size() >= 2) {
					stackValues.push_back(stackValues[stackValues.size() - 2]);
				}
			} else if (token == "over2") {
				if (stackValues.size() >= 4) {
					stackValues.push_back(stackValues[stackValues.size() - 4]);
					stackValues.push_back(stackValues[stackValues.size() - 4]);
				}
			} else if (token == "rot") {
				if (stackValues.size() >= 3) {
					std::string c = stackValues.back();
					stackValues.pop_back();
					std::string b = stackValues.back();
					stackValues.pop_back();
					std::string a = stackValues.back();
					stackValues.pop_back();
					stackValues.push_back(b);
					stackValues.push_back(c);
					stackValues.push_back(a);
				}
			} else if (token == "nip") {
				if (stackValues.size() >= 2) {
					std::string top = stackValues.back();
					stackValues.pop_back();
					stackValues.pop_back();
					stackValues.push_back(top);
				}
			} else if (token == "tuck") {
				if (stackValues.size() >= 2) {
					std::string b = stackValues.back();
					stackValues.pop_back();
					std::string a = stackValues.back();
					stackValues.pop_back();
					stackValues.push_back(b);
					stackValues.push_back(a);
					stackValues.push_back(b);
				}
			} else if (token == "clear") {
				stackValues.clear();
			} else if (token == "depth") {
				// Push stack depth onto stack
				stackValues.push_back(std::to_string(stackValues.size()));
			} else if (token == "inc") {
				if (!stackValues.empty()) {
					try {
						long long val = std::stoll(stackValues.back());
						stackValues.pop_back();
						stackValues.push_back(std::to_string(val + 1));
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "dec") {
				if (!stackValues.empty()) {
					try {
						long long val = std::stoll(stackValues.back());
						stackValues.pop_back();
						stackValues.push_back(std::to_string(val - 1));
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "neg") {
				if (!stackValues.empty()) {
					try {
						std::string val = stackValues.back();
						if (val.find('.') != std::string::npos) {
							double d = std::stod(val);
							stackValues.pop_back();
							stackValues.push_back(std::to_string(-d));
						} else {
							long long i = std::stoll(val);
							stackValues.pop_back();
							stackValues.push_back(std::to_string(-i));
						}
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "abs") {
				if (!stackValues.empty()) {
					try {
						std::string val = stackValues.back();
						if (val.find('.') != std::string::npos) {
							double d = std::abs(std::stod(val));
							stackValues.pop_back();
							stackValues.push_back(std::to_string(d));
						} else {
							long long i = std::abs(std::stoll(val));
							stackValues.pop_back();
							stackValues.push_back(std::to_string(i));
						}
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "sq") {
				if (!stackValues.empty()) {
					try {
						std::string val = stackValues.back();
						if (val.find('.') != std::string::npos) {
							double d = std::stod(val);
							stackValues.pop_back();
							stackValues.push_back(std::to_string(d * d));
						} else {
							long long i = std::stoll(val);
							stackValues.pop_back();
							stackValues.push_back(std::to_string(i * i));
						}
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "sqrt") {
				if (!stackValues.empty()) {
					try {
						double d = std::stod(stackValues.back());
						stackValues.pop_back();
						stackValues.push_back(std::to_string(std::sqrt(d)));
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "sin" || token == "cos" || token == "tan") {
				if (!stackValues.empty()) {
					try {
						double d = std::stod(stackValues.back());
						stackValues.pop_back();
						double result;
						if (token == "sin") {
							result = std::sin(d);
						} else if (token == "cos") {
							result = std::cos(d);
						} else {
							result = std::tan(d);
						}
						stackValues.push_back(std::to_string(result));
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "floor" || token == "ceil" || token == "round") {
				if (!stackValues.empty()) {
					try {
						double d = std::stod(stackValues.back());
						stackValues.pop_back();
						double result;
						if (token == "floor") {
							result = std::floor(d);
						} else if (token == "ceil") {
							result = std::ceil(d);
						} else {
							result = std::round(d);
						}
						stackValues.push_back(std::to_string(result));
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else if (token == "min" || token == "max") {
				if (stackValues.size() >= 2) {
					try {
						std::string b = stackValues.back();
						stackValues.pop_back();
						std::string a = stackValues.back();
						stackValues.pop_back();
						bool aIsFloat = (a.find('.') != std::string::npos);
						bool bIsFloat = (b.find('.') != std::string::npos);
						if (aIsFloat || bIsFloat) {
							double aVal = std::stod(a);
							double bVal = std::stod(b);
							double result = (token == "min") ? std::min(aVal, bVal) : std::max(aVal, bVal);
							stackValues.push_back(std::to_string(result));
						} else {
							long long aVal = std::stoll(a);
							long long bVal = std::stoll(b);
							long long result = (token == "min") ? std::min(aVal, bVal) : std::max(aVal, bVal);
							stackValues.push_back(std::to_string(result));
						}
					} catch (...) {
						stackValues.push_back("?");
					}
				}
			} else {
				// Unknown operation - might be a user-defined function or module function
				// Check if it contains "::" (namespaced function call) or looks like a function
				// For now, we can't determine stack effects, so mark with "?"
				// This prevents the stack from getting out of sync
				if (token.find("::") != std::string::npos) {
					// Namespaced function call - we don't know the signature
					// Conservatively assume it might consume/produce values
					// For better UX, just push a placeholder
					stackValues.push_back("?");
				}
				// For other unknown tokens, just ignore them
				// (they might be user functions we haven't analyzed yet)
			}
		}
	}

	bool compileAndExecute(const std::string& userCode) {
		// Save stack state in case we need to rollback
		std::vector<std::string> savedStackValues = stackValues;

		// Convert 'print' to 'printv' for better REPL output (with newlines)
		std::string processedCode = userCode;
		size_t pos = 0;
		while ((pos = processedCode.find("print", pos)) != std::string::npos) {
			// Check if this is 'prints' - don't modify it
			if (pos + 6 < processedCode.length() && processedCode[pos + 5] == 's') {
				pos += 6;
				continue;
			}
			// Check if this is already 'printv' - don't modify it
			if (pos + 6 < processedCode.length() && processedCode[pos + 5] == 'v') {
				pos += 6;
				continue;
			}
			// Check that 'print' is a complete word (not part of another identifier)
			bool isWordStart = (pos == 0 || !isalnum(processedCode[pos - 1]));
			bool isWordEnd = (pos + 5 >= processedCode.length() || !isalnum(processedCode[pos + 5]));
			if (isWordStart && isWordEnd) {
				processedCode.insert(pos + 5, "v");
				pos += 6;
			} else {
				pos += 5;
			}
		}

		// Simulate stack state
		updateStackSimulation(processedCode);

		// Add this line to history
		history.push_back(processedCode);

		// Generate the complete source file by accumulating all history
		std::stringstream source;

		// Add use statements
		for (const auto& use : useStatements) {
			source << use << "\n";
		}
		if (!useStatements.empty()) {
			source << "\n";
		}

		// Add function definitions
		for (const auto& func : functionDefinitions) {
			source << func << "\n";
		}
		if (!functionDefinitions.empty()) {
			source << "\n";
		}

		// Generate main function that executes ALL history
		source << "fn main( -- ) {\n";

		// Add all historical lines
		for (const auto& line : history) {
			source << "\t" << line << "\n";
		}

		// Add stack depth check at the end to sync our simulation
		source << "\tnl \"__DEPTH__\" prints depth printv\n";

		source << "}\n";

		std::string sourceCode = source.str();

		// Parse the source
		Qd::Ast ast;
		Qd::IAstNode* root = ast.generate(sourceCode.c_str(), false, "<repl>");

		if (!root || ast.hasErrors()) {
			std::cerr << COLOR_YELLOW << "Parse error" << COLOR_RESET << std::endl;
			// Rollback changes since compilation failed
			stackValues = savedStackValues;
			history.pop_back();
			return false;
		}

		// Semantic validation
		Qd::SemanticValidator validator;
		size_t errorCount = validator.validate(root, "<repl>");
		if (errorCount > 0) {
			// Errors already printed
			// Rollback changes since compilation failed
			stackValues = savedStackValues;
			history.pop_back();
			return false;
		}

		// Generate LLVM IR and compile
		Qd::LlvmGenerator generator;
		if (!generator.generate(root, "main")) {
			std::cerr << COLOR_YELLOW << "Code generation failed" << COLOR_RESET << std::endl;
			// Rollback changes since compilation failed
			stackValues = savedStackValues;
			history.pop_back();
			return false;
		}

		// Create executable
		std::string exePath = tempDir + "/repl_exec";
		if (!generator.writeExecutable(exePath)) {
			std::cerr << COLOR_YELLOW << "Failed to create executable" << COLOR_RESET << std::endl;
			// Rollback changes since compilation failed
			stackValues = savedStackValues;
			history.pop_back();
			return false;
		}

		// Execute and capture output
		std::string outputFile = tempDir + "/output.txt";
		std::string command = exePath + " > " + outputFile + " 2>&1";
		int exitCode = system(command.c_str());

		if (exitCode != 0) {
			std::cerr << COLOR_YELLOW << "Execution failed with exit code " << (exitCode / 256) << COLOR_RESET
					  << std::endl;
			// Show output
			std::ifstream outFile(outputFile);
			if (outFile.is_open()) {
				std::string line;
				while (std::getline(outFile, line)) {
					std::cerr << line << std::endl;
				}
			}
			// Rollback changes since compilation failed
			stackValues = savedStackValues;
			history.pop_back();
			return false;
		}

		// Read and display output
		// Only show NEW output (not from previous history)
		std::vector<std::string> outputLines;
		std::ifstream outFile(outputFile);
		if (outFile.is_open()) {
			std::string outputLine;
			while (std::getline(outFile, outputLine)) {
				outputLines.push_back(outputLine);
			}
		}

		// The output contains results from ALL history executions
		// We only want to show the NEW output from this line
		// Skip output from previous lines (we track how many output lines we've seen)
		size_t newOutputStart = lastOutputLineCount;

		// Extract actual stack depth and filter out depth markers in one pass
		int actualDepth = -1;
		std::vector<std::string> userOutput;

		for (size_t i = 0; i < outputLines.size(); i++) {
			if (outputLines[i] == "__DEPTH__") {
				// Next line should be "int:N" with the actual depth
				if (i + 1 < outputLines.size()) {
					const std::string& depthLine = outputLines[i + 1];
					if (depthLine.find("int:") == 0) {
						try {
							actualDepth = std::stoi(depthLine.substr(4));
						} catch (...) {
							// Failed to parse depth
						}
					}
				}
				// Skip this line and the next (int:N)
				i++;
			} else {
				userOutput.push_back(outputLines[i]);
			}
		}

		// Sync our simulated stack with actual depth
		if (actualDepth >= 0) {
			size_t simulatedDepth = stackValues.size();
			if (actualDepth != static_cast<int>(simulatedDepth)) {
				// Stack simulation is out of sync - adjust it
				if (actualDepth > static_cast<int>(simulatedDepth)) {
					// Actual stack has more elements - add unknowns
					for (int i = static_cast<int>(simulatedDepth); i < actualDepth; i++) {
						stackValues.push_back("?");
					}
				} else {
					// Actual stack has fewer elements - truncate
					stackValues.resize(static_cast<size_t>(actualDepth));
				}
			}
		}

		// Show only NEW user output
		for (size_t i = newOutputStart; i < userOutput.size(); i++) {
			std::cout << userOutput[i] << std::endl;
		}
		std::cout.flush();

		// Remember total output lines for next time (user output only, depth markers filtered out)
		lastOutputLineCount = userOutput.size();

		return true;
	}
};

void printVersion() {
	std::cout << QUADRATE_VERSION << "\n";
}

void printHelp() {
	std::cout << "quadrate - Quadrate REPL\n\n";
	std::cout << "Interactive Read-Eval-Print Loop for Quadrate.\n\n";
	std::cout << "Usage: quadrate [options]\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help       Show this help message\n";
	std::cout << "  -v, --version    Show version information\n";
	std::cout << "\n";
}

struct Options {
	bool help = false;
	bool version = false;
};

bool parseArgs(int argc, char* argv[], Options& opts) {
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			opts.help = true;
			return true;
		} else if (arg == "-v" || arg == "--version") {
			opts.version = true;
			return true;
		} else {
			std::cerr << "quadrate: unknown option: " << arg << "\n";
			std::cerr << "Try 'quadrate --help' for more information.\n";
			return false;
		}
	}

	return true;
}

int main(int argc, char* argv[]) {
	Options opts;

	if (!parseArgs(argc, argv, opts)) {
		return 1;
	}

	if (opts.help) {
		printHelp();
		return 0;
	}

	if (opts.version) {
		printVersion();
		return 0;
	}

	// Configure colored output
	const bool noColors = std::getenv("NO_COLOR") != nullptr;
	Qd::Colors::setEnabled(!noColors);

	// Run the REPL
	ReplSession session;
	session.run();

	return 0;
}
