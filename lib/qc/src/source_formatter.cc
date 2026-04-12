#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <quadrate/qc/ast.h>
#include <quadrate/qc/ast_node.h>
#include <quadrate/qc/ast_node_anonymous_function.h>
#include <quadrate/qc/ast_node_array_literal.h>
#include <quadrate/qc/ast_node_as_cast.h>
#include <quadrate/qc/ast_node_case.h>
#include <quadrate/qc/ast_node_constant.h>
#include <quadrate/qc/ast_node_ctx.h>
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_identifier.h>
#include <quadrate/qc/ast_node_if.h>
#include <quadrate/qc/ast_node_import.h>
#include <quadrate/qc/ast_node_instruction.h>
#include <quadrate/qc/ast_node_literal.h>
#include <quadrate/qc/ast_node_local.h>
#include <quadrate/qc/ast_node_loop.h>
#include <quadrate/qc/ast_node_parameter.h>
#include <quadrate/qc/ast_node_program.h>
#include <quadrate/qc/ast_node_scoped.h>
#include <quadrate/qc/ast_node_string_interpolation.h>
#include <quadrate/qc/ast_node_struct_construction.h>
#include <quadrate/qc/ast_node_struct_declaration.h>
#include <quadrate/qc/ast_node_switch.h>
#include <quadrate/qc/ast_node_test.h>
#include <quadrate/qc/ast_node_use.h>
#include <quadrate/qc/ast_node_while.h>
#include <quadrate/qc/formatter.h>
#include <sstream>
#include <unistd.h>
#include <vector>

#include "ast_node_block.h"
#include "ast_node_comment.h"

namespace Qd {

	// ============================================================
	// FormatOptions implementation
	// ============================================================

	static std::string findConfigFile(const std::string& startDir) {
		std::string dir = startDir;
		if (dir.empty()) {
			char* cwd = getcwd(nullptr, 0);
			if (cwd) {
				dir = cwd;
				free(cwd);
			} else {
				return "";
			}
		}

		while (!dir.empty() && dir != "/") {
			std::string configPath = dir + "/.quadfmt.json";
			std::ifstream f(configPath);
			if (f.good()) {
				return configPath;
			}
			size_t lastSlash = dir.rfind('/');
			if (lastSlash == std::string::npos) {
				break;
			}
			dir = dir.substr(0, lastSlash);
		}
		return "";
	}

	bool FormatOptions::configExists(const std::string& startDir) {
		return !findConfigFile(startDir).empty();
	}

	FormatOptions FormatOptions::loadFromFile(const std::string& startDir) {
		FormatOptions opts;
		std::string configPath = findConfigFile(startDir);
		if (configPath.empty()) {
			return opts;
		}

		std::ifstream f(configPath);
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		size_t pos = content.find("\"sortImports\"");
		if (pos != std::string::npos) {
			pos = content.find(':', pos);
			if (pos != std::string::npos) {
				if (content.find("false", pos) < content.find(',', pos) &&
						content.find("false", pos) < content.find('}', pos)) {
					opts.sortImports = false;
				}
			}
		}

		pos = content.find("\"alignStructFields\"");
		if (pos != std::string::npos) {
			pos = content.find(':', pos);
			if (pos != std::string::npos) {
				if (content.find("false", pos) < content.find(',', pos) &&
						content.find("false", pos) < content.find('}', pos)) {
					opts.alignStructFields = false;
				}
			}
		}

		return opts;
	}

	// ============================================================
	// AST-based formatter
	//
	// Strategy:
	// - Top-level declarations: fully reconstructed from AST
	// - Block bodies: source lines re-indented with brace-depth tracking
	//   This preserves user expression spacing while fixing indentation.
	//   The AST is used for top-level structure (fn signatures, struct decls,
	//   use sorting, import blocks, etc.)
	// ============================================================

	class AstFormatter {
	public:
		AstFormatter(const std::string& source, const FormatOptions& opts) : mSource(source), mOpts(opts), mIndent(0) {
			std::istringstream stream(source);
			std::string line;
			while (std::getline(stream, line)) {
				mSourceLines.push_back(line);
			}
		}

		std::string format(IAstNode* root) {
			if (!root || root->type() != IAstNode::Type::PROGRAM) {
				return mSource;
			}

			// Collect top-level nodes
			std::vector<IAstNode*> children;
			for (size_t i = 0; i < root->childCount(); i++) {
				IAstNode* child = root->child(i);
				// Skip synthetic use nodes (injected by parser at line 0)
				if (child->line() == 0 && child->type() == IAstNode::Type::USE_STATEMENT) {
					continue;
				}
				children.push_back(child);
			}

			// Emit top-level nodes with proper spacing and use sorting
			std::vector<IAstNode*> useBuffer;
			std::vector<IAstNode*> commentBuffer;
			std::string prevType;

			for (size_t i = 0; i < children.size(); i++) {
				IAstNode* node = children[i];

				if (node->type() == IAstNode::Type::USE_STATEMENT) {
					useBuffer.push_back(node);
					continue;
				}

				if (!useBuffer.empty()) {
					flushUseStatements(useBuffer, commentBuffer, prevType);
					useBuffer.clear();
					commentBuffer.clear();
					prevType = "use";
				}

				std::string curType = getTopLevelType(node);

				// Buffer comments that precede use statements
				if (node->type() == IAstNode::Type::COMMENT) {
					bool beforeUse = false;
					for (size_t j = i + 1; j < children.size(); j++) {
						if (children[j]->type() == IAstNode::Type::COMMENT) {
							continue;
						}
						if (children[j]->type() == IAstNode::Type::USE_STATEMENT) {
							beforeUse = true;
						}
						break;
					}
					if (beforeUse) {
						commentBuffer.push_back(node);
						continue;
					}
				}

				// Flush pending comments
				if (!commentBuffer.empty()) {
					for (auto* c : commentBuffer) {
						addTopLevelSpacing(prevType, "comment");
						emitComment(static_cast<AstNodeComment*>(c));
						prevType = "comment";
					}
					commentBuffer.clear();
				}

				addTopLevelSpacing(prevType, curType);
				emitTopLevelNode(node);
				prevType = curType;
			}

			if (!useBuffer.empty()) {
				flushUseStatements(useBuffer, commentBuffer, prevType);
			} else if (!commentBuffer.empty()) {
				for (auto* c : commentBuffer) {
					addTopLevelSpacing(prevType, "comment");
					emitComment(static_cast<AstNodeComment*>(c));
					prevType = "comment";
				}
			}

			return mOutput.str();
		}

	private:
		std::string mSource;
		std::vector<std::string> mSourceLines;
		FormatOptions mOpts;
		std::ostringstream mOutput;
		int mIndent;

		// ============================================================
		// Helpers
		// ============================================================

		static std::string trim(const std::string& s) {
			size_t start = 0;
			while (start < s.length() && std::isspace(static_cast<unsigned char>(s[start]))) {
				start++;
			}
			size_t end = s.length();
			while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
				end--;
			}
			return s.substr(start, end - start);
		}

		const std::string& getSourceLine(size_t lineNum) const {
			static const std::string empty;
			if (lineNum == 0 || lineNum > mSourceLines.size()) {
				return empty;
			}
			return mSourceLines[lineNum - 1];
		}

		void emitIndent() {
			for (int i = 0; i < mIndent; i++) {
				mOutput << '\t';
			}
		}

		void emitIndent(int level) {
			for (int i = 0; i < level; i++) {
				mOutput << '\t';
			}
		}

		// ============================================================
		// Top-level spacing
		// ============================================================

		std::string getTopLevelType(IAstNode* node) {
			switch (node->type()) {
			case IAstNode::Type::USE_STATEMENT:
			case IAstNode::Type::IMPORT_STATEMENT:
				return "use";
			case IAstNode::Type::CONSTANT_DECLARATION:
				return "const";
			case IAstNode::Type::COMMENT: {
				auto* comment = static_cast<AstNodeComment*>(node);
				if (comment->commentType() == AstNodeComment::CommentType::SHEBANG) {
					return "shebang";
				}
				return "comment";
			}
			default:
				return "fn_start";
			}
		}

		void addTopLevelSpacing(const std::string& prevType, const std::string& curType) {
			if (prevType.empty()) {
				return;
			}
			bool needsBlank = false;
			if (prevType == "shebang") {
				needsBlank = true;
			} else if (prevType == "use" && (curType == "const" || curType == "fn_start")) {
				needsBlank = true;
			} else if (prevType == "const" && curType == "fn_start") {
				needsBlank = true;
			} else if (prevType == "fn_start" &&
					   (curType == "fn_start" || curType == "use" || curType == "const" || curType == "comment")) {
				needsBlank = true;
			}
			if (needsBlank) {
				mOutput << '\n';
			}
		}

		// ============================================================
		// Use statement sorting
		// ============================================================

		void flushUseStatements(
				std::vector<IAstNode*>& useNodes, std::vector<IAstNode*>& comments, const std::string& prevType) {
			std::string prevT = prevType;
			for (auto* c : comments) {
				addTopLevelSpacing(prevT, "comment");
				emitComment(static_cast<AstNodeComment*>(c));
				prevT = "comment";
			}
			addTopLevelSpacing(prevT, "use");

			if (mOpts.sortImports) {
				std::vector<std::string> quotedUses;
				std::vector<std::string> bareUses;

				for (auto* node : useNodes) {
					auto* use = static_cast<AstNodeUse*>(node);
					const std::string& mod = use->module();
					if (mod.find('/') != std::string::npos || mod.find(' ') != std::string::npos) {
						quotedUses.push_back(mod);
					} else {
						bareUses.push_back(mod);
					}
				}
				std::sort(quotedUses.begin(), quotedUses.end());
				std::sort(bareUses.begin(), bareUses.end());
				for (const auto& mod : quotedUses) {
					emitUseModule(mod);
				}
				for (const auto& mod : bareUses) {
					emitUseModule(mod);
				}
			} else {
				for (auto* node : useNodes) {
					auto* use = static_cast<AstNodeUse*>(node);
					emitUseModule(use->module());
				}
			}
		}

		void emitUseModule(const std::string& mod) {
			// Quote use paths that contain spaces, slashes, or are .qd file references
			bool needsQuotes = mod.find(' ') != std::string::npos || mod.find('/') != std::string::npos;
			// Also check: if module name contains a dot and ends with .qd but also has a path component
			// (like "local_module.qd"), the original source likely had quotes
			// Actually, the parser stores bare "use helper.qd" and quoted use "file.qd" identically.
			// We quote if it has spaces or slashes (file paths). Bare .qd files stay unquoted.
			if (needsQuotes) {
				mOutput << "use \"" << mod << "\"\n";
			} else {
				mOutput << "use " << mod << "\n";
			}
		}

		// ============================================================
		// Top-level node dispatch
		// ============================================================

		void emitTopLevelNode(IAstNode* node) {
			switch (node->type()) {
			case IAstNode::Type::COMMENT:
				emitComment(static_cast<AstNodeComment*>(node));
				break;
			case IAstNode::Type::USE_STATEMENT:
				emitUseModule(static_cast<AstNodeUse*>(node)->module());
				break;
			case IAstNode::Type::IMPORT_STATEMENT:
				emitImportStatement(static_cast<AstNodeImport*>(node));
				break;
			case IAstNode::Type::CONSTANT_DECLARATION:
				emitConstant(static_cast<AstNodeConstant*>(node));
				break;
			case IAstNode::Type::FUNCTION_DECLARATION:
				emitFunctionDecl(static_cast<AstNodeFunctionDeclaration*>(node));
				break;
			case IAstNode::Type::STRUCT_DECLARATION:
				emitStructDecl(static_cast<AstNodeStructDeclaration*>(node));
				break;
			case IAstNode::Type::ENUM_DECLARATION:
				emitEnumDecl(static_cast<AstNodeEnumDeclaration*>(node));
				break;
			case IAstNode::Type::TEST_DECLARATION:
				emitTestDecl(static_cast<AstNodeTest*>(node));
				break;
			default:
				break;
			}
		}

		// ============================================================
		// Comment
		// ============================================================

		void emitComment(AstNodeComment* node) {
			emitIndent();
			switch (node->commentType()) {
			case AstNodeComment::CommentType::SHEBANG:
				mOutput << "#!" << node->text() << "\n";
				break;
			case AstNodeComment::CommentType::LINE:
				mOutput << "//" << node->text() << "\n";
				break;
			case AstNodeComment::CommentType::BLOCK: {
				// Block comments may span multiple lines; preserve internal formatting
				const std::string& text = node->text();
				mOutput << "/*" << text << "*/\n";
				break;
			}
			}
		}

		// ============================================================
		// Import statement
		// ============================================================

		void emitImportStatement(AstNodeImport* node) {
			mOutput << "import \"" << node->library() << "\" as \"" << node->namespaceName() << "\" {\n";
			mIndent++;
			for (const auto& func : node->functions()) {
				emitIndent();
				if (func->isPublic) {
					mOutput << "pub ";
				}
				mOutput << "fn " << func->name << "(";
				bool hasInputs = !func->inputParameters.empty();
				bool hasOutputs = !func->outputParameters.empty();
				if (!hasInputs && !hasOutputs) {
					// empty
				} else if (!hasInputs && hasOutputs) {
					mOutput << " -- ";
					emitParamList(func->outputParameters);
				} else if (hasInputs && !hasOutputs) {
					emitParamList(func->inputParameters);
				} else {
					emitParamList(func->inputParameters);
					mOutput << " -- ";
					emitParamList(func->outputParameters);
				}
				mOutput << ")";
				if (func->throws) {
					mOutput << "!";
				}
				mOutput << "\n";
			}
			mIndent--;
			mOutput << "}\n";
		}

		void emitParamList(const std::vector<std::unique_ptr<AstNodeParameter>>& params) {
			for (size_t i = 0; i < params.size(); i++) {
				if (i > 0) {
					mOutput << " ";
				}
				mOutput << params[i]->displayString();
			}
		}

		// ============================================================
		// Constant
		// ============================================================

		void emitConstant(AstNodeConstant* node) {
			emitIndent();
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			mOutput << "const " << node->name() << " = " << node->value() << "\n";
		}

		// ============================================================
		// Function declaration
		// ============================================================

		void emitFunctionDecl(AstNodeFunctionDeclaration* node) {
			emitIndent();
			emitFunctionSignature(node);
			mOutput << " {\n";
			mIndent++;
			if (node->body()) {
				emitBlockBody(node->body());
			}
			mIndent--;
			emitIndent();
			mOutput << "}\n";
		}

		void emitFunctionSignature(AstNodeFunctionDeclaration* node) {
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			mOutput << "fn ";
			if (node->hasReceiver()) {
				mOutput << "(" << node->receiverName() << ":" << node->receiverType();
				if (node->hasReceiverTypeParams()) {
					mOutput << "<";
					const auto& rtp = node->receiverTypeParams();
					for (size_t i = 0; i < rtp.size(); i++) {
						if (i > 0) {
							mOutput << ", ";
						}
						mOutput << rtp[i];
					}
					mOutput << ">";
				}
				mOutput << ") ";
			}
			mOutput << node->name();
			if (node->isGeneric()) {
				mOutput << "<";
				const auto& tp = node->typeParams();
				for (size_t i = 0; i < tp.size(); i++) {
					if (i > 0) {
						mOutput << ", ";
					}
					mOutput << tp[i];
				}
				mOutput << ">";
			}
			emitParamSignature(node->inputParameters(), node->outputParameters());
			if (node->throws()) {
				mOutput << "!";
			}
		}

		void emitParamSignature(const std::vector<std::unique_ptr<IAstNode>>& inputs,
				const std::vector<std::unique_ptr<IAstNode>>& outputs) {
			bool hasInputs = !inputs.empty();
			bool hasOutputs = !outputs.empty();
			mOutput << "(";
			if (!hasInputs && !hasOutputs) {
				// fn foo()
			} else if (!hasInputs && hasOutputs) {
				// fn foo( -- result:i64)
				mOutput << " -- ";
				for (size_t i = 0; i < outputs.size(); i++) {
					if (i > 0) {
						mOutput << " ";
					}
					mOutput << static_cast<AstNodeParameter*>(outputs[i].get())->displayString();
				}
			} else if (hasInputs && !hasOutputs) {
				// fn foo(x:i64 b:i64)
				for (size_t i = 0; i < inputs.size(); i++) {
					if (i > 0) {
						mOutput << " ";
					}
					mOutput << static_cast<AstNodeParameter*>(inputs[i].get())->displayString();
				}
			} else {
				for (size_t i = 0; i < inputs.size(); i++) {
					if (i > 0) {
						mOutput << " ";
					}
					mOutput << static_cast<AstNodeParameter*>(inputs[i].get())->displayString();
				}
				mOutput << " -- ";
				for (size_t i = 0; i < outputs.size(); i++) {
					if (i > 0) {
						mOutput << " ";
					}
					mOutput << static_cast<AstNodeParameter*>(outputs[i].get())->displayString();
				}
			}
			mOutput << ")";
		}

		// ============================================================
		// Struct declaration
		// ============================================================

		void emitStructDecl(AstNodeStructDeclaration* node) {
			emitIndent();
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			mOutput << "struct " << node->name();
			if (node->isGeneric()) {
				mOutput << "<";
				const auto& tp = node->typeParams();
				for (size_t i = 0; i < tp.size(); i++) {
					if (i > 0) {
						mOutput << ", ";
					}
					mOutput << tp[i];
				}
				mOutput << ">";
			}
			mOutput << " {\n";
			mIndent++;
			const auto& fields = node->fields();
			if (mOpts.alignStructFields && fields.size() > 2) {
				size_t maxNameLen = 0;
				for (const auto& f : fields) {
					if (f->name().length() > maxNameLen) {
						maxNameLen = f->name().length();
					}
				}
				for (const auto& f : fields) {
					emitIndent();
					mOutput << f->name() << ":";
					size_t padding = maxNameLen - f->name().length() + 1;
					for (size_t p = 0; p < padding; p++) {
						mOutput << ' ';
					}
					mOutput << f->typeName() << "\n";
				}
			} else {
				for (const auto& f : fields) {
					emitIndent();
					mOutput << f->name() << ": " << f->typeName() << "\n";
				}
			}
			mIndent--;
			emitIndent();
			mOutput << "}\n";
		}

		// ============================================================
		// Enum declaration
		// ============================================================

		void emitEnumDecl(AstNodeEnumDeclaration* node) {
			emitIndent();
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			mOutput << "enum " << node->name() << " {\n";
			mIndent++;
			for (const auto& v : node->variants()) {
				emitIndent();
				mOutput << v.name << "\n";
			}
			mIndent--;
			emitIndent();
			mOutput << "}\n";
		}

		// ============================================================
		// Test declaration
		// ============================================================

		void emitTestDecl(AstNodeTest* node) {
			emitIndent();
			mOutput << "test \"" << node->name() << "\" {\n";
			mIndent++;
			if (node->body()) {
				emitBlockBody(node->body());
			}
			mIndent--;
			emitIndent();
			mOutput << "}\n";
		}

		// ============================================================
		// Source line normalization
		//
		// Applied to each source line before emission to normalize
		// operator spacing, anonymous function signatures, and
		// struct constructions.
		// ============================================================

		// Normalize ++ and -- operator spacing: ensure space before and after
		static std::string normalizeIncDecOperators(const std::string& line) {
			std::string result;
			bool inStr = false;
			for (size_t i = 0; i < line.length(); i++) {
				char c = line[i];
				if (c == '"' && (i == 0 || line[i - 1] != '\\')) {
					inStr = !inStr;
				}
				if (inStr) {
					result += c;
					continue;
				}
				// Check for ++ or --
				if ((c == '+' || c == '-') && i + 1 < line.length() && line[i + 1] == c) {
					// Remove trailing whitespace before operator
					while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
						result.pop_back();
					}
					// Add single space before
					if (!result.empty()) {
						result += ' ';
					}
					result += c;
					result += c;
					i++; // Skip second char
					// Skip any whitespace after
					while (i + 1 < line.length() && (line[i + 1] == ' ' || line[i + 1] == '\t')) {
						i++;
					}
					// Add single space after if there's more content
					if (i + 1 < line.length()) {
						result += ' ';
					}
					continue;
				}
				result += c;
			}
			return result;
		}

		// Normalize anonymous function signatures: fn(sig){body}->var → fn (sig) { body } -> var
		static std::string normalizeAnonymousFunction(const std::string& line) {
			// Look for fn( or fn ( patterns that indicate anonymous functions
			std::string result = line;
			{
				size_t pos = 0;
				while (pos < result.length()) {
					// Find "fn" followed by optional space then "("
					size_t fnPos = result.find("fn", pos);
					if (fnPos == std::string::npos) {
						break;
					}

					// Make sure it's a word boundary
					if (fnPos > 0 &&
							(std::isalnum(static_cast<unsigned char>(result[fnPos - 1])) || result[fnPos - 1] == '_')) {
						pos = fnPos + 2;
						continue;
					}

					size_t afterFn = fnPos + 2;
					// Skip spaces
					while (afterFn < result.length() && result[afterFn] == ' ') {
						afterFn++;
					}

					// Must be followed by '(' or '[' (captures)
					if (afterFn >= result.length() || (result[afterFn] != '(' && result[afterFn] != '[')) {
						pos = fnPos + 2;
						continue;
					}

					// Check for captures [...]
					std::string captures;
					if (result[afterFn] == '[') {
						size_t closeB = result.find(']', afterFn);
						if (closeB == std::string::npos) {
							pos = fnPos + 2;
							continue;
						}
						captures = result.substr(afterFn, closeB - afterFn + 1);
						afterFn = closeB + 1;
						while (afterFn < result.length() && result[afterFn] == ' ') {
							afterFn++;
						}
						if (afterFn >= result.length() || result[afterFn] != '(') {
							pos = fnPos + 2;
							continue;
						}
					}

					// Find matching )
					size_t parenStart = afterFn;
					int depth = 0;
					size_t parenEnd = std::string::npos;
					for (size_t j = parenStart; j < result.length(); j++) {
						if (result[j] == '(') {
							depth++;
						} else if (result[j] == ')') {
							depth--;
							if (depth == 0) {
								parenEnd = j;
								break;
							}
						}
					}
					if (parenEnd == std::string::npos) {
						pos = fnPos + 2;
						continue;
					}

					// Extract signature content
					std::string sig = result.substr(parenStart + 1, parenEnd - parenStart - 1);

					// Normalize -- separator in signature
					std::string normSig;
					size_t dashPos = sig.find("--");
					if (dashPos != std::string::npos) {
						std::string before = trim(sig.substr(0, dashPos));
						std::string after = trim(sig.substr(dashPos + 2));
						if (before.empty() && after.empty()) {
							normSig = "";
						} else if (before.empty()) {
							normSig = " -- " + after;
						} else if (after.empty()) {
							normSig = before + " -- ";
						} else {
							normSig = before + " -- " + after;
						}
					} else {
						normSig = trim(sig);
					}

					// Look for { body } after )
					size_t afterParen = parenEnd + 1;
					while (afterParen < result.length() && result[afterParen] == ' ') {
						afterParen++;
					}
					if (afterParen >= result.length() || result[afterParen] != '{') {
						pos = fnPos + 2;
						continue;
					}

					// Find matching }
					size_t braceStart = afterParen;
					depth = 0;
					size_t braceEnd = std::string::npos;
					bool bInStr = false;
					for (size_t j = braceStart; j < result.length(); j++) {
						if (result[j] == '"' && (j == 0 || result[j - 1] != '\\')) {
							bInStr = !bInStr;
						}
						if (bInStr) {
							continue;
						}
						if (result[j] == '{') {
							depth++;
						} else if (result[j] == '}') {
							depth--;
							if (depth == 0) {
								braceEnd = j;
								break;
							}
						}
					}
					if (braceEnd == std::string::npos) {
						pos = fnPos + 2;
						continue;
					}

					std::string body = trim(result.substr(braceStart + 1, braceEnd - braceStart - 1));

					// Get everything after the closing }
					std::string after = result.substr(braceEnd + 1);

					// Rebuild: fn [captures] (sig) { body } + after
					std::string rebuilt = result.substr(0, fnPos) + "fn ";
					if (!captures.empty()) {
						rebuilt += captures + " ";
					}
					// Normalize body: add spaces around standalone operators
					std::string normBody;
					for (size_t b = 0; b < body.length(); b++) {
						char bc = body[b];
						if ((bc == '*' || bc == '/' || bc == '+' || bc == '-' || bc == '%') && b > 0) {
							if (!std::isspace(static_cast<unsigned char>(body[b - 1]))) {
								normBody += ' ';
							}
						}
						normBody += bc;
					}
					// Normalize after: "->var" becomes " -> var"
					std::string normAfter = after;
					if (normAfter.length() >= 2 && normAfter[0] == '-' && normAfter[1] == '>') {
						std::string varName = trim(normAfter.substr(2));
						normAfter = " -> " + varName;
					} else if (normAfter.length() >= 3 && normAfter[0] == ' ' && normAfter[1] == '-' &&
							   normAfter[2] == '>') {
						std::string varName = trim(normAfter.substr(3));
						normAfter = " -> " + varName;
					}
					rebuilt += "(" + normSig + ") { " + trim(normBody) + " }" + normAfter;

					result = rebuilt;
					pos = fnPos + rebuilt.length() - after.length(); // Skip past rebuilt section
					continue;
				}
			}
			return result;
		}

		// Check if line contains a struct construction that should be expanded to multiline
		// Returns the struct construction string to expand, or empty if none
		static bool isStructConstruction(const std::string& line) {
			// Look for UpperCase { field = value ... } pattern
			bool inStr = false;
			for (size_t i = 0; i < line.length(); i++) {
				if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) {
					inStr = !inStr;
				}
				if (inStr) {
					continue;
				}

				if (line[i] == '{' && i > 0) {
					// Check if preceded by an uppercase identifier (possibly scoped)
					size_t nameEnd = i;
					while (nameEnd > 0 && line[nameEnd - 1] == ' ') {
						nameEnd--;
					}
					if (nameEnd == 0) {
						continue;
					}
					size_t nameStart = nameEnd;
					while (nameStart > 0 && (std::isalnum(static_cast<unsigned char>(line[nameStart - 1])) ||
													line[nameStart - 1] == '_' || line[nameStart - 1] == ':')) {
						nameStart--;
					}
					if (nameStart >= nameEnd) {
						continue;
					}
					// Find the last component after ::
					std::string name = line.substr(nameStart, nameEnd - nameStart);
					size_t lastColon = name.rfind("::");
					std::string lastComponent = (lastColon != std::string::npos) ? name.substr(lastColon + 2) : name;
					if (lastComponent.empty() || !std::isupper(static_cast<unsigned char>(lastComponent[0]))) {
						continue;
					}
					// Check it has "field = value" pattern inside
					size_t braceEnd = std::string::npos;
					int depth = 0;
					for (size_t j = i; j < line.length(); j++) {
						if (line[j] == '"' && (j == 0 || line[j - 1] != '\\')) {
							inStr = !inStr;
						}
						if (inStr) {
							continue;
						}
						if (line[j] == '{') {
							depth++;
						} else if (line[j] == '}') {
							depth--;
							if (depth == 0) {
								braceEnd = j;
								break;
							}
						}
					}
					if (braceEnd == std::string::npos) {
						continue;
					}
					std::string content = line.substr(i + 1, braceEnd - i - 1);
					if (content.find('=') != std::string::npos) {
						return true;
					}
				}
			}
			return false;
		}

		// Expand struct constructions to multiline
		// "Point { x = 1 y = 2 } -> p" becomes:
		// "Point {"
		// "\tx = 1"
		// "\ty = 2"
		// "} -> p"
		std::vector<std::string> expandStructConstruction(const std::string& line, int baseIndent) {
			std::vector<std::string> result;
			std::string trimmed = trim(line);

			// Find the struct name and opening brace
			bool inStr = false;
			for (size_t i = 0; i < trimmed.length(); i++) {
				if (trimmed[i] == '"' && (i == 0 || trimmed[i - 1] != '\\')) {
					inStr = !inStr;
				}
				if (inStr) {
					continue;
				}

				if (trimmed[i] == '{' && i > 0) {
					size_t nameEnd = i;
					while (nameEnd > 0 && trimmed[nameEnd - 1] == ' ') {
						nameEnd--;
					}
					if (nameEnd == 0) {
						continue;
					}
					size_t nameStart = nameEnd;
					while (nameStart > 0 && (std::isalnum(static_cast<unsigned char>(trimmed[nameStart - 1])) ||
													trimmed[nameStart - 1] == '_' || trimmed[nameStart - 1] == ':')) {
						nameStart--;
					}
					std::string name = trimmed.substr(nameStart, nameEnd - nameStart);
					size_t lastColon = name.rfind("::");
					std::string lastComponent = (lastColon != std::string::npos) ? name.substr(lastColon + 2) : name;
					if (lastComponent.empty() || !std::isupper(static_cast<unsigned char>(lastComponent[0]))) {
						continue;
					}

					// Find matching }
					size_t braceEnd = std::string::npos;
					int depth = 0;
					bool bInStr = false;
					for (size_t j = i; j < trimmed.length(); j++) {
						if (trimmed[j] == '"' && (j == 0 || trimmed[j - 1] != '\\')) {
							bInStr = !bInStr;
						}
						if (bInStr) {
							continue;
						}
						if (trimmed[j] == '{') {
							depth++;
						} else if (trimmed[j] == '}') {
							depth--;
							if (depth == 0) {
								braceEnd = j;
								break;
							}
						}
					}
					if (braceEnd == std::string::npos) {
						continue;
					}

					std::string content = trim(trimmed.substr(i + 1, braceEnd - i - 1));
					if (content.find('=') == std::string::npos) {
						continue;
					}

					std::string prefix = trimmed.substr(0, nameStart);
					std::string afterBrace = trim(trimmed.substr(braceEnd + 1));

					// Parse fields: "field = value field2 = value2"
					std::vector<std::pair<std::string, std::string>> fields;
					std::string remaining = content;
					while (!remaining.empty()) {
						size_t eqPos = remaining.find('=');
						if (eqPos == std::string::npos) {
							break;
						}
						std::string fieldName = trim(remaining.substr(0, eqPos));
						remaining = trim(remaining.substr(eqPos + 1));

						// Find value: everything up to next "identifier =" or end
						std::string fieldValue;
						size_t nextField = std::string::npos;
						bool fInStr = false;
						int fBraceDepth = 0;
						for (size_t k = 0; k < remaining.length(); k++) {
							if (remaining[k] == '"' && (k == 0 || remaining[k - 1] != '\\')) {
								fInStr = !fInStr;
							}
							if (fInStr) {
								continue;
							}
							if (remaining[k] == '{') {
								fBraceDepth++;
							}
							if (remaining[k] == '}') {
								fBraceDepth--;
							}
							if (fBraceDepth == 0 && std::isspace(static_cast<unsigned char>(remaining[k]))) {
								// Look ahead for identifier followed by =
								size_t ahead = k + 1;
								while (ahead < remaining.length() &&
										std::isspace(static_cast<unsigned char>(remaining[ahead]))) {
									ahead++;
								}
								if (ahead < remaining.length() &&
										std::isalpha(static_cast<unsigned char>(remaining[ahead]))) {
									size_t idEnd = ahead;
									while (idEnd < remaining.length() &&
											(std::isalnum(static_cast<unsigned char>(remaining[idEnd])) ||
													remaining[idEnd] == '_')) {
										idEnd++;
									}
									size_t afterId = idEnd;
									while (afterId < remaining.length() &&
											std::isspace(static_cast<unsigned char>(remaining[afterId]))) {
										afterId++;
									}
									if (afterId < remaining.length() && remaining[afterId] == '=') {
										nextField = ahead;
										break;
									}
								}
							}
						}

						if (nextField != std::string::npos) {
							fieldValue = trim(remaining.substr(0, nextField));
							remaining = trim(remaining.substr(nextField));
						} else {
							fieldValue = trim(remaining);
							remaining.clear();
						}
						fields.push_back({fieldName, fieldValue});
					}

					// Build output lines
					std::string indentStr;
					for (int j = 0; j < baseIndent; j++) {
						indentStr += '\t';
					}
					std::string innerIndent;
					for (int j = 0; j < baseIndent + 1; j++) {
						innerIndent += '\t';
					}

					result.push_back(indentStr + prefix + name + " {");
					for (const auto& field : fields) {
						result.push_back(innerIndent + field.first + " = " + field.second);
					}
					if (!afterBrace.empty()) {
						result.push_back(indentStr + "} " + afterBrace);
					} else {
						result.push_back(indentStr + "}");
					}
					return result;
				}
			}

			// No struct construction found, return line as-is
			result.push_back(line);
			return result;
		}

		// Apply all normalizations to a source line
		std::string normalizeLine(const std::string& line) {
			std::string result = line;
			result = normalizeIncDecOperators(result);
			result = normalizeAnonymousFunction(result);
			return result;
		}

		// ============================================================
		// Block body emission - source line approach with brace tracking
		//
		// Extracts original source lines for the block region and
		// re-indents them using brace depth tracking with proper
		// string/comment awareness. This preserves user expression
		// content while fixing structural indentation.
		// ============================================================

		// Find the closing brace line of a block by scanning source
		size_t findBlockEndLine(IAstNode* blockNode) {
			if (!blockNode) {
				return 0;
			}

			// Start from the block's own line, scan forward for its closing }
			size_t startLine = blockNode->line();

			// Track brace depth to find the matching }
			int depth = 0;
			bool inStr = false;
			bool inLC = false;
			bool inBC = false;

			for (size_t i = startLine; i <= mSourceLines.size(); i++) {
				const std::string& line = getSourceLine(i);
				inLC = false; // Line comments reset each line

				for (size_t j = 0; j < line.length(); j++) {
					char c = line[j];
					if (inLC) {
						break;
					}
					if (!inStr && !inBC && j + 1 < line.length()) {
						if (c == '/' && line[j + 1] == '/') {
							inLC = true;
							break;
						}
						if (c == '/' && line[j + 1] == '*') {
							inBC = true;
							j++;
							continue;
						}
					}
					if (inBC) {
						if (j + 1 < line.length() && c == '*' && line[j + 1] == '/') {
							inBC = false;
							j++;
						}
						continue;
					}
					if (c == '"' && (j == 0 || line[j - 1] != '\\')) {
						inStr = !inStr;
					}
					if (inStr) {
						continue;
					}
					if (c == '{') {
						depth++;
					}
					if (c == '}') {
						depth--;
						if (depth == 0) {
							return i;
						}
					}
				}
			}
			return startLine;
		}

		void emitBlockBody(IAstNode* body) {
			if (!body) {
				return;
			}

			size_t bodyLine = body->line();
			size_t endLine = findBlockEndLine(body);

			// Handle inline bodies: fn main() { body } on a single line
			if (endLine == bodyLine) {
				// Extract content between { and }
				const std::string& line = getSourceLine(bodyLine);
				size_t braceOpen = line.find('{');
				size_t braceClose = line.rfind('}');
				if (braceOpen != std::string::npos && braceClose != std::string::npos && braceClose > braceOpen) {
					std::string content = trim(line.substr(braceOpen + 1, braceClose - braceOpen - 1));
					if (!content.empty()) {
						std::string normalized = normalizeLine(content);
						emitIndent();
						mOutput << normalized << "\n";
					}
				}
				return;
			}

			size_t startLine = bodyLine + 1;

			int braceDepth = 0;
			bool inMultilineString = false;
			bool inBlockComment = false;

			for (size_t i = startLine; i < endLine; i++) {
				const std::string& srcLine = getSourceLine(i);
				std::string trimmed = trim(srcLine);

				// Handle multiline string continuation
				if (inMultilineString) {
					emitIndent(mIndent + braceDepth);
					mOutput << trimmed << "\n";
					for (size_t j = 0; j < srcLine.length(); j++) {
						if (srcLine[j] == '"' && (j == 0 || srcLine[j - 1] != '\\')) {
							inMultilineString = false;
						}
					}
					continue;
				}

				// Handle block comment continuation
				if (inBlockComment) {
					emitIndent(mIndent + braceDepth);
					mOutput << trimmed << "\n";
					if (trimmed.find("*/") != std::string::npos) {
						inBlockComment = false;
					}
					continue;
				}

				if (trimmed.empty()) {
					mOutput << '\n';
					continue;
				}

				// Count leading closing braces to dedent this line
				int leadingCloses = 0;
				{
					size_t pos = 0;
					while (pos < trimmed.length()) {
						if (trimmed[pos] == '}') {
							leadingCloses++;
							pos++;
							// Skip space after }
							while (pos < trimmed.length() && trimmed[pos] == ' ') {
								pos++;
							}
							// "} else {" — stop counting
							if (pos < trimmed.length() && trimmed[pos] != '}') {
								break;
							}
						} else {
							break;
						}
					}
				}

				int lineIndent = braceDepth - leadingCloses;
				if (lineIndent < 0) {
					lineIndent = 0;
				}

				// Apply normalizations to the trimmed line
				std::string normalized = normalizeLine(trimmed);

				// Check for struct construction that should be expanded to multiline
				if (isStructConstruction(normalized)) {
					auto expanded = expandStructConstruction(normalized, mIndent + lineIndent);
					for (const auto& expLine : expanded) {
						mOutput << expLine << "\n";
					}
					// Recount braces from the original line for depth tracking
					// (the expansion doesn't change net brace count)
				} else {
					emitIndent(mIndent + lineIndent);
					mOutput << normalized << "\n";
				}

				// Update brace depth for next line
				bool lineInStr = false;
				bool lineInLC = false;
				bool lineInBC = false;
				for (size_t j = 0; j < trimmed.length(); j++) {
					char c = trimmed[j];
					if (lineInLC) {
						break;
					}
					if (!lineInStr && !lineInBC && j + 1 < trimmed.length()) {
						if (c == '/' && trimmed[j + 1] == '/') {
							lineInLC = true;
							break;
						}
						if (c == '/' && trimmed[j + 1] == '*') {
							lineInBC = true;
							j++;
							continue;
						}
					}
					if (lineInBC) {
						if (j + 1 < trimmed.length() && c == '*' && trimmed[j + 1] == '/') {
							lineInBC = false;
							j++;
						}
						continue;
					}
					if (c == '"' && (j == 0 || trimmed[j - 1] != '\\')) {
						lineInStr = !lineInStr;
					}
					if (lineInStr) {
						continue;
					}
					if (c == '{') {
						braceDepth++;
					}
					if (c == '}') {
						braceDepth--;
					}
				}

				if (lineInStr) {
					inMultilineString = true;
				}
				if (lineInBC) {
					inBlockComment = true;
				}
			}
		}
	};

	// ============================================================
	// Public API
	// ============================================================

	std::string formatSource(const std::string& source, const FormatOptions& opts) {
		Ast ast;
		IAstNode* root = ast.generate(source.c_str(), false, nullptr);

		if (!root || ast.hasErrors()) {
			return source;
		}

		AstFormatter formatter(source, opts);
		return formatter.format(root);
	}

	std::string formatSource(const std::string& source) {
		return formatSource(source, FormatOptions{});
	}

} // namespace Qd
