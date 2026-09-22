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
#include <quadrate/qc/ast_node_defer.h>
#include <quadrate/qc/ast_node_enum.h>
#include <quadrate/qc/ast_node_field_access.h>
#include <quadrate/qc/ast_node_field_set.h>
#include <quadrate/qc/ast_node_for.h>
#include <quadrate/qc/ast_node_function.h>
#include <quadrate/qc/ast_node_function_pointer.h>
#include <quadrate/qc/ast_node_global_var.h>
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
#include <quadrate/qc/ast_node_type_alias.h>
#include <quadrate/qc/ast_node_use.h>
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
			std::vector<IAstNode*> commentBuffer;
			std::string prevType;
			size_t prevLine = 0;

			for (size_t i = 0; i < children.size(); i++) {
				IAstNode* node = children[i];

				if (node->type() == IAstNode::Type::USE_STATEMENT) {
					std::vector<UseUnit> units;
					size_t last = collectUseGroup(children, i, units);
					flushUseStatements(units, commentBuffer, prevType, prevLine);
					commentBuffer.clear();
					prevType = "use";
					prevLine = units.back().use->line();
					i = last;
					continue;
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
						addTopLevelSpacing(prevType, "comment", prevLine, c->line());
						emitComment(static_cast<AstNodeComment*>(c));
						prevType = "comment";
						prevLine = c->line();
					}
					commentBuffer.clear();
				}

				addTopLevelSpacing(prevType, curType, prevLine, node->line());
				AstNodeComment* trailing = nullptr;
				if (i + 1 < children.size() && takesTrailingComment(node)) {
					trailing = trailingCommentOf(node, children[i + 1]);
				}
				emitTopLevelNode(node);
				if (takesTrailingComment(node)) {
					emitTrailingComment(trailing);
					mOutput << "\n";
				}
				if (trailing) {
					i++;
				}
				prevType = curType;
				prevLine = node->line();
			}

			if (!commentBuffer.empty()) {
				for (auto* c : commentBuffer) {
					addTopLevelSpacing(prevType, "comment");
					emitComment(static_cast<AstNodeComment*>(c));
					prevType = "comment";
				}
			}

			// A block whose braces never balanced means the formatter and the parser
			// disagree about where that block ends, and reconstructing it from the wrong
			// end invents structure: `fn n(){f{{}}` parses clean but counts one `}` short,
			// and the rebuilt body came back with a brace the author never wrote. Hand the
			// source back untouched instead -- the same answer the formatter already gives
			// for input it cannot parse.
			if (mBailOut) {
				return mSource;
			}

			return mOutput.str();
		}

	private:
		std::string mSource;
		std::vector<std::string> mSourceLines;
		FormatOptions mOpts;
		std::ostringstream mOutput;
		int mIndent;
		bool mBailOut = false;

		// ============================================================
		// Helpers
		// ============================================================

		// Whether the quote at `i` opens or closes a string, given whether the scan is
		// already inside one. Outside a string nothing is escaped, so a `"` always opens one
		// -- the backslash in `t{\"}}` is a stray character, not an escape. Inside one, an
		// odd run of backslashes before the quote escapes it; testing only the character
		// before read the closing quote of `"a\\"` as escaped, because that backslash is
		// itself escaped.
		static bool quoteToggles(const std::string& s, size_t i, bool inStr) {
			if (s[i] != '"') {
				return false;
			}
			if (!inStr) {
				return true;
			}
			size_t backslashes = 0;
			while (backslashes < i && s[i - 1 - backslashes] == '\\') {
				backslashes++;
			}
			return backslashes % 2 == 0;
		}

		static std::string ltrim(const std::string& s) {
			size_t start = 0;
			while (start < s.length() && std::isspace(static_cast<unsigned char>(s[start]))) {
				start++;
			}
			return s.substr(start);
		}

		static std::string rtrim(const std::string& s) {
			size_t end = s.length();
			while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
				end--;
			}
			return s.substr(0, end);
		}

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
			case IAstNode::Type::TYPE_ALIAS_DECLARATION:
				return "const";
			case IAstNode::Type::GLOBAL_VAR_DECLARATION:
				return "var";
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

		void addTopLevelSpacing(
				const std::string& prevType, const std::string& curType, size_t prevLine = 0, size_t curLine = 0) {
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

			// Preserve blank lines from original source between same-type declarations.
			// If the original source had a blank line between prevLine and curLine, keep it.
			if (!needsBlank && prevLine > 0 && curLine > prevLine) {
				for (size_t i = prevLine; i < curLine; i++) {
					const std::string& line = getSourceLine(i);
					if (trim(line).empty()) {
						needsBlank = true;
						break;
					}
				}
			}

			if (needsBlank) {
				mOutput << '\n';
			}
		}

		// ============================================================
		// Use statement sorting
		// ============================================================

		struct UseUnit {
			AstNodeUse* use = nullptr;
			std::vector<AstNodeComment*> leading;
			AstNodeComment* trailing = nullptr;
		};

		static bool takesTrailingComment(IAstNode* node) {
			switch (node->type()) {
			case IAstNode::Type::CONSTANT_DECLARATION:
			case IAstNode::Type::GLOBAL_VAR_DECLARATION:
			case IAstNode::Type::TYPE_ALIAS_DECLARATION:
				return true;
			default:
				return false;
			}
		}

		static AstNodeComment* trailingCommentOf(IAstNode* node, IAstNode* next) {
			if (next->type() != IAstNode::Type::COMMENT || next->line() == 0 || next->line() != node->line()) {
				return nullptr;
			}
			auto* comment = static_cast<AstNodeComment*>(next);
			if (comment->commentType() == AstNodeComment::CommentType::LINE) {
				return comment;
			}
			if (comment->commentType() == AstNodeComment::CommentType::BLOCK &&
					comment->text().find('\n') == std::string::npos) {
				return comment;
			}
			return nullptr;
		}

		void emitTrailingComment(AstNodeComment* comment) {
			if (!comment) {
				return;
			}
			if (comment->commentType() == AstNodeComment::CommentType::BLOCK) {
				mOutput << " /*" << comment->text() << "*/";
			} else {
				mOutput << " //" << comment->text();
			}
		}

		size_t collectUseGroup(const std::vector<IAstNode*>& children, size_t first, std::vector<UseUnit>& units) {
			std::vector<AstNodeComment*> pending;
			size_t last = first;
			for (size_t j = first; j < children.size(); j++) {
				IAstNode* child = children[j];
				if (child->type() == IAstNode::Type::USE_STATEMENT) {
					UseUnit unit;
					unit.use = static_cast<AstNodeUse*>(child);
					unit.leading = pending;
					pending.clear();
					units.push_back(unit);
					last = j;
					continue;
				}
				if (child->type() != IAstNode::Type::COMMENT) {
					break;
				}
				if (pending.empty() && !units.empty() && !units.back().trailing) {
					AstNodeComment* trailing = trailingCommentOf(units.back().use, child);
					if (trailing) {
						units.back().trailing = trailing;
						last = j;
						continue;
					}
				}
				pending.push_back(static_cast<AstNodeComment*>(child));
			}
			return last;
		}

		void flushUseStatements(std::vector<UseUnit>& units, std::vector<IAstNode*>& comments,
				const std::string& prevType, size_t prevLine = 0) {
			std::string prevT = prevType;
			size_t prevL = prevLine;
			for (auto* c : comments) {
				addTopLevelSpacing(prevT, "comment", prevL, c->line());
				emitComment(static_cast<AstNodeComment*>(c));
				prevT = "comment";
				prevL = c->line();
			}
			size_t firstUseLine = units.empty() ? 0 : units.front().use->line();
			if (!units.empty() && !units.front().leading.empty()) {
				firstUseLine = units.front().leading.front()->line();
			}
			addTopLevelSpacing(prevT, "use", prevL, firstUseLine);

			if (mOpts.sortImports) {
				std::stable_sort(units.begin(), units.end(), [](const UseUnit& a, const UseUnit& b) {
					bool aQuoted = useModuleNeedsQuotes(a.use->module());
					bool bQuoted = useModuleNeedsQuotes(b.use->module());
					if (aQuoted != bQuoted) {
						return aQuoted;
					}
					return a.use->module() < b.use->module();
				});
			}
			for (size_t u = 0; u < units.size(); u++) {
				const UseUnit& unit = units[u];
				if (u > 0 && !unit.leading.empty()) {
					size_t commentLine = unit.leading.front()->line();
					if (commentLine > 1 && trim(getSourceLine(commentLine - 1)).empty()) {
						mOutput << '\n';
					}
				}
				for (auto* c : unit.leading) {
					emitComment(c);
				}
				emitUseModule(unit.use->module());
				emitTrailingComment(unit.trailing);
				mOutput << "\n";
			}
		}

		// The parser stores `use helper.qd` and `use "helper.qd"` identically, so the
		// quotes have to be decided from the name alone. Bare is only safe for what the
		// parser reads back bare -- an identifier, optionally suffixed `.qd`. Testing
		// instead for the characters that are known to need quoting let a path the
		// heuristic had not met through unquoted, and `use "a b/c"` losing its quotes is
		// a file that no longer resolves.
		static bool useModuleNeedsQuotes(const std::string& mod) {
			std::string name = mod;
			if (name.size() > 3 && name.compare(name.size() - 3, 3, ".qd") == 0) {
				name = name.substr(0, name.size() - 3);
			}
			if (name.empty() || (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_')) {
				return true;
			}
			for (char c : name) {
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
					return true;
				}
			}
			return false;
		}

		void emitUseModule(const std::string& mod) {
			if (useModuleNeedsQuotes(mod)) {
				mOutput << "use \"" << mod << "\"";
			} else {
				mOutput << "use " << mod;
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
				mOutput << "\n";
				break;
			case IAstNode::Type::IMPORT_STATEMENT:
				emitImportStatement(static_cast<AstNodeImport*>(node));
				break;
			case IAstNode::Type::CONSTANT_DECLARATION:
				emitConstant(static_cast<AstNodeConstant*>(node));
				break;
			case IAstNode::Type::GLOBAL_VAR_DECLARATION:
				emitGlobalVar(static_cast<AstNodeGlobalVar*>(node));
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
			case IAstNode::Type::TYPE_ALIAS_DECLARATION:
				emitTypeAlias(static_cast<AstNodeTypeAlias*>(node));
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
			// Preserve original source text for import blocks to keep doc comments.
			// Only normalize the -- separator in fn signatures.
			size_t startLine = node->line();
			BlockRange range = scanBlockRange(startLine, 0);
			if (!range.found || !startsItsLine(node)) {
				// Same as the struct case: without a matching `}` every line but the first
				// would be dropped, so hand the source back instead.
				mBailOut = true;
				return;
			}
			size_t endLine = range.closeLine;

			// Emit lines from source, normalizing fn signatures for --
			for (size_t i = startLine; i <= endLine; i++) {
				// Only as far as the block's own `}`, as in emitStructDecl: whatever follows
				// it on that line is the next declaration, and is emitted as itself.
				std::string line = i == endLine ? getSourceLine(i).substr(0, range.closeCol + 1) : getSourceLine(i);

				// Check if this line has a fn declaration without --
				// Pattern: "pub fn name(params)" or "fn name(params)" where params exist but no --
				size_t fnPos = line.find("fn ");
				if (fnPos != std::string::npos) {
					size_t parenOpen = line.find('(', fnPos);
					size_t parenClose = line.rfind(')');
					if (parenOpen != std::string::npos && parenClose != std::string::npos && parenClose > parenOpen) {
						std::string sig = line.substr(parenOpen + 1, parenClose - parenOpen - 1);
						std::string trimmedSig = trim(sig);
						// If there are params but no --, add it
						if (!trimmedSig.empty() && trimmedSig.find("--") == std::string::npos) {
							// Has inputs, no outputs, no -- : add " -- " before closing paren
							std::string before = line.substr(0, parenClose);
							std::string after = line.substr(parenClose);
							line = before + " -- " + after;
						}
					}
				}

				mOutput << line << "\n";
			}
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
			mOutput << "const " << node->name() << " = " << node->value();
		}

		void emitGlobalVar(AstNodeGlobalVar* node) {
			emitIndent();
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			mOutput << "var " << node->name();
			if (node->hasExplicitType()) {
				mOutput << ":" << node->typeName();
			}
			mOutput << " = " << node->sourceExpr();
		}

		void emitTypeAlias(AstNodeTypeAlias* node) {
			emitIndent();
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			mOutput << "type " << node->name() << " = " << node->targetType();
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
			if (node->isInline()) {
				mOutput << "inline ";
			}
			// Canonical modifier order is pub, inline, stack; the parser accepts any order
			if (node->isStack()) {
				mOutput << "stack ";
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
				// fn foo(x:i64 b:i64 -- )
				for (size_t i = 0; i < inputs.size(); i++) {
					if (i > 0) {
						mOutput << " ";
					}
					mOutput << static_cast<AstNodeParameter*>(inputs[i].get())->displayString();
				}
				mOutput << " -- ";
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
			// Check if any field has a default value — if so, preserve original source
			// since reconstructing default value expressions from the AST is complex.
			const auto& fields = node->fields();
			bool hasDefaults = false;
			for (const auto& f : fields) {
				if (f->hasDefaultValue()) {
					hasDefaults = true;
					break;
				}
			}

			if (hasDefaults) {
				// Preserve original source lines for struct with default values
				size_t startLine = node->line();
				BlockRange range = scanBlockRange(startLine, 0);
				if (!range.found || !startsItsLine(node)) {
					// Falling back to `endLine = startLine` here emitted the declaration's
					// first line and dropped every field under it.
					mBailOut = true;
					return;
				}
				bool inStr = false;
				int bcDepth = 0;
				for (size_t i = startLine; i < range.closeLine; i++) {
					mOutput << spaceFieldColons(getSourceLine(i), inStr, bcDepth) << "\n";
				}
				// Only as far as the declaration's own `}`. Emitting the whole closing line
				// copied out whatever followed it -- in `struct P{e:i=c}fn i(){}` the function
				// after it, which was then emitted again as itself.
				mOutput << spaceFieldColons(
								   getSourceLine(range.closeLine).substr(0, range.closeCol + 1), inStr, bcDepth)
						<< "\n";
				return;
			}

			emitIndent();
			if (node->isPublic()) {
				mOutput << "pub ";
			}
			if (node->isPacked()) {
				mOutput << "packed ";
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
			size_t maxNameLen = 0;
			const bool align = mOpts.alignStructFields && fields.size() > 2;
			if (align) {
				for (const auto& f : fields) {
					if (f->name().length() > maxNameLen) {
						maxNameLen = f->name().length();
					}
				}
			}
			const auto& comments = node->bodyComments();
			for (size_t i = 0; i < fields.size(); i++) {
				emitBodyCommentsBefore(comments, i);
				const auto& f = fields[i];
				emitIndent();
				if (align) {
					mOutput << f->name() << ":";
					size_t padding = maxNameLen - f->name().length() + 1;
					for (size_t p = 0; p < padding; p++) {
						mOutput << ' ';
					}
					mOutput << f->typeName();
				} else {
					mOutput << f->name() << ": " << f->typeName();
				}
				emitTrailingBodyComment(comments, i + 1);
				mOutput << "\n";
			}
			emitBodyCommentsBefore(comments, fields.size());
			mIndent--;
			emitIndent();
			mOutput << "}\n";
		}

		static std::string spaceFieldColons(const std::string& line, bool& inStr, int& bcDepth) {
			std::string result;
			for (size_t j = 0; j < line.length(); j++) {
				char c = line[j];
				if (bcDepth > 0) {
					result += c;
					if (j + 1 < line.length() && c == '/' && line[j + 1] == '*') {
						bcDepth++;
						result += line[++j];
					} else if (j + 1 < line.length() && c == '*' && line[j + 1] == '/') {
						bcDepth--;
						result += line[++j];
					}
					continue;
				}
				if (quoteToggles(line, j, inStr)) {
					inStr = !inStr;
					result += c;
					continue;
				}
				if (inStr) {
					result += c;
					continue;
				}
				if (c == '/' && j + 1 < line.length() && line[j + 1] == '/') {
					result += line.substr(j);
					break;
				}
				if (c == '/' && j + 1 < line.length() && line[j + 1] == '*') {
					bcDepth = 1;
					result += c;
					result += line[++j];
					continue;
				}
				result += c;
				if (c == ':' && j > 0 && j + 1 < line.length() && line[j + 1] != ':' && line[j - 1] != ':' &&
						!std::isspace(static_cast<unsigned char>(line[j + 1])) &&
						(std::isalnum(static_cast<unsigned char>(line[j - 1])) || line[j - 1] == '_')) {
					result += ' ';
				}
			}
			return result;
		}

		// Comments recorded inside a struct or enum body. `index` is the number of fields (or
		// variants) that precede them; a trailing comment shares its line with the item before it
		// and is emitted by emitTrailingBodyComment instead.
		template <typename BodyCommentT>
		void emitBodyCommentsBefore(const std::vector<BodyCommentT>& comments, size_t index) {
			for (const auto& c : comments) {
				if (c.trailing || commentIndex(c) != index) {
					continue;
				}
				emitIndent();
				mOutput << (c.isBlock ? "/*" : "//") << c.text << (c.isBlock ? "*/" : "") << "\n";
			}
		}

		template <typename BodyCommentT>
		void emitTrailingBodyComment(const std::vector<BodyCommentT>& comments, size_t index) {
			for (const auto& c : comments) {
				if (!c.trailing || commentIndex(c) != index) {
					continue;
				}
				mOutput << " " << (c.isBlock ? "/*" : "//") << c.text << (c.isBlock ? "*/" : "");
				return; // at most one can share the line
			}
		}

		static size_t commentIndex(const AstNodeStructDeclaration::BodyComment& c) {
			return c.afterFieldIdx;
		}

		static size_t commentIndex(const AstNodeEnumDeclaration::BodyComment& c) {
			return c.afterVariantIdx;
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
			const auto& variants = node->variants();
			const auto& comments = node->bodyComments();
			for (size_t i = 0; i < variants.size(); i++) {
				emitBodyCommentsBefore(comments, i);
				emitIndent();
				mOutput << variants[i].name;
				if (variants[i].hasExplicitValue) {
					mOutput << " = " << variants[i].valueText;
				}
				emitTrailingBodyComment(comments, i + 1);
				mOutput << "\n";
			}
			emitBodyCommentsBefore(comments, variants.size());
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
				if (quoteToggles(line, i, inStr)) {
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

					// A `:` before it makes this a function-pointer type, not an anonymous
					// function: `g:fn(i64 -- i64)`. The rewrite below puts a space after the
					// `fn`, which is fine for a value but not for a type -- the parser only
					// reads `fn` as a type when the `(` is glued to it, so `g:fn (i64 -- i64)`
					// no longer parses. Leave type annotations alone.
					if (fnPos > 0 && result[fnPos - 1] == ':') {
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
						if (quoteToggles(result, j, bInStr)) {
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
					// Normalize after: "->var" becomes " -> var". With no name after the arrow
					// the space would trail the line, which the next pass trims away, so the
					// formatter would never reach a fixed point.
					std::string normAfter = after;
					std::string varName;
					bool hasArrow = false;
					if (normAfter.length() >= 2 && normAfter[0] == '-' && normAfter[1] == '>') {
						varName = trim(normAfter.substr(2));
						hasArrow = true;
					} else if (normAfter.length() >= 3 && normAfter[0] == ' ' && normAfter[1] == '-' &&
							   normAfter[2] == '>') {
						varName = trim(normAfter.substr(3));
						hasArrow = true;
					}
					if (hasArrow) {
						normAfter = varName.empty() ? " ->" : " -> " + varName;
					}
					rebuilt += "(" + normSig + ") { " + trim(normBody) + " }" + normAfter;

					result = rebuilt;
					// Resume just past what was rebuilt. `rebuilt` is the whole line, so its
					// length minus the tail's is already that index -- adding `fnPos` skipped
					// the rest of the line, and a second `fn(){}` on it was only normalised on
					// the pass after. The tail may have been rewritten, so measure `normAfter`.
					pos = rebuilt.length() - normAfter.length();
					continue;
				}
			}
			return result;
		}

		// The '=' of a field initializer, as opposed to the one inside ==, !=, <= or >=.
		// A switch arm labelled with a constant reads exactly like a struct construction --
		// `Bool { b v <<inum 0 != if { … } }` is an uppercase name, a brace and an '=' -- so
		// without this test the arm was expanded as if it were one, and re-emitted as
		// `field = value` with a space driven through the operator: `0 ! = if`, which does
		// not compile. `make format` could turn a working file into a broken one.
		static size_t findFieldAssignEquals(const std::string& s, size_t from = 0) {
			for (size_t i = from; i < s.length(); i++) {
				if (s[i] != '=') {
					continue;
				}
				if (i + 1 < s.length() && s[i + 1] == '=') {
					i++; // ==, and skip its second character
					continue;
				}
				if (i > 0 && (s[i - 1] == '=' || s[i - 1] == '!' || s[i - 1] == '<' || s[i - 1] == '>')) {
					continue;
				}
				return i;
			}
			return std::string::npos;
		}

		// Check if line contains a struct construction that should be expanded to multiline
		// Returns the struct construction string to expand, or empty if none
		static bool isStructConstruction(const std::string& line) {
			// Look for UpperCase { field = value ... } pattern
			bool inStr = false;
			for (size_t i = 0; i < line.length(); i++) {
				if (quoteToggles(line, i, inStr)) {
					inStr = !inStr;
				}
				if (inStr) {
					continue;
				}
				// Skip line comments — braces in comments aren't struct constructions
				if (i + 1 < line.length() && line[i] == '/' && line[i + 1] == '/') {
					return false;
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
					// Only expand if the struct construction starts at the beginning of the line.
					// Inline constructions embedded in larger expressions stay compact.
					if (nameStart > 0) {
						continue;
					}
					// Check it has "field = value" pattern inside
					size_t braceEnd = std::string::npos;
					int depth = 0;
					for (size_t j = i; j < line.length(); j++) {
						if (quoteToggles(line, j, inStr)) {
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
					if (findFieldAssignEquals(content) != std::string::npos) {
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
		static bool isPlainIdentifier(const std::string& s) {
			if (s.empty() || (!std::isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_')) {
				return false;
			}
			for (char c : s) {
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
					return false;
				}
			}
			return true;
		}

		// Whether every brace in `s` outside a string literal is matched.
		static bool bracesBalanced(const std::string& s) {
			int depth = 0;
			bool inStr = false;
			for (size_t i = 0; i < s.length(); i++) {
				if (quoteToggles(s, i, inStr)) {
					inStr = !inStr;
				}
				if (inStr) {
					continue;
				}
				if (s[i] == '{') {
					depth++;
				} else if (s[i] == '}') {
					depth--;
					if (depth < 0) {
						return false;
					}
				}
			}
			return depth == 0 && !inStr;
		}

		std::vector<std::string> expandStructConstruction(const std::string& line, int baseIndent) {
			std::vector<std::string> result;
			std::string trimmed = trim(line);

			// A comment ends the code on the line, and the braces inside it are text. In
			// `V{x=//}` the `}` is commented out and the construction carries on to the next
			// line; expanding to the first `}` regardless closed it here and emitted a brace
			// the author never wrote. A comment that begins after the construction's own `}`
			// is past `codeEnd` and still lands in `afterBrace` as before.
			size_t commentStart = findCommentStart(trimmed);
			size_t codeEnd = commentStart == std::string::npos ? trimmed.length() : commentStart;

			// Find the struct name and opening brace
			//
			// Only a construction the line itself opens can be expanded. One nested inside
			// another brace group has that group's braces around it, and putting its fields
			// on lines of their own splits those across lines: `Ok { P { x = 1 y = 2 } -> p }`
			// came back as `Ok { P {` / fields / `} -> p }`, which the next pass indents
			// differently, so `quadfmt -c` reported the file forever.
			bool inStr = false;
			int lineDepth = 0;
			for (size_t i = 0; i < codeEnd; i++) {
				if (quoteToggles(trimmed, i, inStr)) {
					inStr = !inStr;
				}
				if (inStr) {
					continue;
				}
				if (trimmed[i] == '}') {
					lineDepth--;
					continue;
				}
				if (trimmed[i] == '{') {
					lineDepth++;
					// Exactly one: deeper means the construction is nested inside another
					// group, and zero or less means the line has already closed more than it
					// opened -- `O{=}}P{x=0}}` reaches `P` at depth -1, where the expansion's
					// idea of the line's shape no longer matches the re-indenter's.
					if (lineDepth != 1) {
						continue;
					}
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
					for (size_t j = i; j < codeEnd; j++) {
						if (quoteToggles(trimmed, j, bInStr)) {
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
					if (findFieldAssignEquals(content) == std::string::npos) {
						continue;
					}

					std::string prefix = trimmed.substr(0, nameStart);
					std::string afterBrace = trim(trimmed.substr(braceEnd + 1));

					// Parse fields: "field = value field2 = value2"
					std::vector<std::pair<std::string, std::string>> fields;
					std::string remaining = content;
					while (!remaining.empty()) {
						size_t eqPos = findFieldAssignEquals(remaining);
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
							if (quoteToggles(remaining, k, fInStr)) {
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
									if (afterId < remaining.length() && remaining[afterId] == '=' &&
											findFieldAssignEquals(remaining, afterId) == afterId) {
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

					// The expansion puts each field on a line of its own, so it can only be
					// applied to fields that survive being separated. A name that is not an
					// identifier or a value whose braces do not balance -- `V{r{=0 y=}}` splits
					// into `r{ = 0` and `y = }` -- leaves the following lines at a brace depth
					// the next pass reads differently, so the formatter never reaches a fixed
					// point; an empty name or value came back as `\t = `, whose trailing space
					// the next pass trims; and anything left in `remaining` would be dropped
					// outright. Leave such a line alone.
					// What follows the construction on the line has to leave the line's brace
					// accounting alone too: `fn t(){f{\nP{x=0}}}` put `} }` on one line, and
					// the re-indenter reads two leading closes there where the expansion meant
					// one, so the line dedented further on every pass.
					std::string afterBraceCode =
							codeEnd > braceEnd + 1 ? trimmed.substr(braceEnd + 1, codeEnd - braceEnd - 1) : "";
					bool expandable = !fields.empty() && remaining.empty() && bracesBalanced(afterBraceCode);
					for (const auto& field : fields) {
						if (!isPlainIdentifier(field.first) || field.second.empty() || !bracesBalanced(field.second)) {
							expandable = false;
							break;
						}
					}
					if (!expandable) {
						continue;
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

			// No struct construction the expansion can spell. Return nothing rather than the
			// line: the caller emitted whatever came back verbatim, so a line that reached
			// one of the bail-outs above came out at column 0, having lost its indent.
			return {};
		}

		static size_t findCommentStart(const std::string& line) {
			bool inStr = false;
			for (size_t i = 0; i < line.length(); i++) {
				char c = line[i];
				if (quoteToggles(line, i, inStr)) {
					inStr = !inStr;
					continue;
				}
				if (inStr) {
					continue;
				}
				if (c == '/' && i + 1 < line.length() && (line[i + 1] == '/' || line[i + 1] == '*')) {
					return i;
				}
			}
			return std::string::npos;
		}

		// Apply all normalizations to a source line
		std::string normalizeLine(const std::string& line) {
			size_t commentPos = findCommentStart(line);
			std::string code = commentPos == std::string::npos ? line : line.substr(0, commentPos);
			std::string comment = commentPos == std::string::npos ? std::string() : line.substr(commentPos);
			if (commentPos == 0) {
				return line;
			}
			std::string result = code;
			result = normalizeIncDecOperators(result);
			result = normalizeAnonymousFunction(result);
			return result + comment;
		}

		// ============================================================
		// Block body emission - source line approach with brace tracking
		//
		// Extracts original source lines for the block region and
		// re-indents them using brace depth tracking with proper
		// string/comment awareness. This preserves user expression
		// content while fixing structural indentation.
		// ============================================================

		// Where a block's own braces sit in the source. Both ends are needed, not just
		// the closing line: a brace is not always the last thing on its line, and
		// whatever shares the line with it is body content that has to be emitted.
		// `found` is false when the braces never balance (malformed input), so callers
		// can fall back rather than read a sentinel line number as a real one.
		struct BlockRange {
			size_t openLine = 0;
			size_t openCol = 0;
			size_t closeLine = 0;
			size_t closeCol = 0;
			bool found = false;
		};

		// Whether nothing precedes the declaration on its line but its own leading keywords.
		// Both source-preserving emitters copy from the start of the line, so anything else
		// there is copied with it: in `struct P{}struct P{e:i=c}` the second declaration
		// copied out the first one's text, and in `type e=r struct P{e:i=c}` the alias. A
		// declaration that does not start its line is handed back instead.
		bool startsItsLine(IAstNode* node) const {
			if (node->column() == 0) {
				return false;
			}
			const std::string& line = getSourceLine(node->line());
			size_t end = node->column() - 1;
			if (end > line.length()) {
				return false;
			}
			size_t i = 0;
			bool sawKeyword = false;
			while (i < end) {
				while (i < end && std::isspace(static_cast<unsigned char>(line[i]))) {
					i++;
				}
				if (i == end) {
					break;
				}
				size_t wordStart = i;
				while (i < end && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
					i++;
				}
				if (i == wordStart) {
					return false; // punctuation: something else on the line
				}
				std::string word = line.substr(wordStart, i - wordStart);
				if (word == "struct" || word == "import") {
					sawKeyword = true;
				} else if (word != "pub" && word != "packed") {
					return false;
				}
			}
			// The keyword has to be on this line too: the node's position is its name, and
			// in `struct\nP{e:i=c}` the copy would have started below the `struct`.
			return sawKeyword;
		}

		// Find a block's own `{` and its matching `}` by scanning source, skipping
		// braces inside strings and comments.
		BlockRange findBlockRange(IAstNode* blockNode) {
			if (!blockNode) {
				return BlockRange{};
			}

			size_t startLine = blockNode->line();

			// The parser positions a block node on its own `{`, so the exact brace is
			// known and does not have to be guessed at. Taking the first `{` on the line
			// instead read the one inside the parameter list of `fn i({){}` as the body's
			// and re-emitted the signature's leftovers as the body.
			size_t startCol = 0;
			const std::string& openLine = getSourceLine(startLine);
			size_t col = blockNode->column();
			if (col >= 1 && col - 1 < openLine.length() && openLine[col - 1] == '{') {
				startCol = col - 1;
			}

			return scanBlockRange(startLine, startCol);
		}

		// Scan forward from (startLine, startCol) for the first `{` and the `}` that
		// matches it, skipping braces inside strings and comments.
		BlockRange scanBlockRange(size_t startLine, size_t startCol) {
			BlockRange range;

			// Track brace depth to find the matching }
			int depth = 0;
			bool inStr = false;
			bool inLC = false;
			int bcDepth = 0;

			for (size_t i = startLine; i <= mSourceLines.size(); i++) {
				const std::string& line = getSourceLine(i);
				inLC = false; // Line comments reset each line

				for (size_t j = (i == startLine ? startCol : 0); j < line.length(); j++) {
					char c = line[j];
					if (inLC) {
						break;
					}
					if (!inStr && bcDepth == 0 && j + 1 < line.length()) {
						if (c == '/' && line[j + 1] == '/') {
							inLC = true;
							break;
						}
						if (c == '/' && line[j + 1] == '*') {
							bcDepth = 1;
							j++;
							continue;
						}
					}
					// Block comments nest, so stopping at the first `*/` is wrong: in
					// `fn t(){/*/**/}*/}` that `*/` is the inner comment's, and treating it
					// as the outer one's made the `}` after it look like the body's close.
					if (bcDepth > 0) {
						if (j + 1 < line.length() && c == '/' && line[j + 1] == '*') {
							bcDepth++;
							j++;
						} else if (j + 1 < line.length() && c == '*' && line[j + 1] == '/') {
							bcDepth--;
							j++;
						}
						continue;
					}
					if (quoteToggles(line, j, inStr)) {
						inStr = !inStr;
					}
					if (inStr) {
						continue;
					}
					if (c == '{') {
						depth++;
						if (depth == 1) {
							range.openLine = i;
							range.openCol = j;
						}
					}
					if (c == '}') {
						depth--;
						if (depth == 0) {
							range.closeLine = i;
							range.closeCol = j;
							range.found = range.openLine != 0;
							return range;
						}
					}
				}
			}
			return range;
		}

		// Advance a block-comment nesting depth over one line, by the lexer's rule that
		// `/*` nests. Updates `depth` and returns the index just past the `*/` that closed
		// it, or the line's length when the comment runs on. The caller has to scan from
		// there: whatever follows a closing `*/` is code, and leaving it unscanned lost
		// both the brace it opened and the comment it started.
		static size_t advanceBlockCommentDepth(const std::string& line, int& depth) {
			size_t j = 0;
			for (; j + 1 < line.length() && depth > 0; j++) {
				if (line[j] == '/' && line[j + 1] == '*') {
					depth++;
					j++;
				} else if (line[j] == '*' && line[j + 1] == '/') {
					depth--;
					j++;
				}
			}
			return depth == 0 ? j : line.length();
		}

		// Scan `line` from `from`, carrying the running brace depth forward and reporting
		// what the line leaves open: a string, or a block comment at `bcDepth`. A line
		// comment ends the scan.
		static void scanLineState(const std::string& line, size_t from, int& braceDepth, bool& inStr, int& bcDepth) {
			bool inLineComment = false;
			for (size_t j = from; j < line.length(); j++) {
				char c = line[j];
				if (inLineComment) {
					break;
				}
				if (!inStr && bcDepth == 0 && j + 1 < line.length()) {
					if (c == '/' && line[j + 1] == '/') {
						inLineComment = true;
						break;
					}
					if (c == '/' && line[j + 1] == '*') {
						bcDepth = 1;
						j++;
						continue;
					}
				}
				if (bcDepth > 0) {
					if (j + 1 < line.length() && c == '/' && line[j + 1] == '*') {
						bcDepth++;
						j++;
					} else if (j + 1 < line.length() && c == '*' && line[j + 1] == '/') {
						bcDepth--;
						j++;
					}
					continue;
				}
				if (quoteToggles(line, j, inStr)) {
					inStr = !inStr;
				}
				if (inStr) {
					continue;
				}
				if (c == '{') {
					braceDepth++;
				}
				if (c == '}') {
					braceDepth--;
				}
			}
		}

		void emitBlockBody(IAstNode* body) {
			if (!body) {
				return;
			}

			BlockRange range = findBlockRange(body);

			// The braces the caller emits are not part of the body, but anything sharing
			// a line with them is. Collect the body as a list of lines -- the tail of the
			// `{` line, the lines between, the head of the `}` line -- so the one loop
			// below handles every shape. Dropping those two partial lines is what lost
			// the whole body of `fn main()\n {0 5 1 for it {`: the `for` header shared a
			// line with the brace that opened the body.
			std::vector<std::string> bodyLines;
			if (range.found && range.closeLine == range.openLine) {
				// Inline body: fn main() { body } on a single line.
				const std::string& line = getSourceLine(range.openLine);
				std::string content = trim(line.substr(range.openCol + 1, range.closeCol - range.openCol - 1));
				if (!content.empty()) {
					bodyLines.push_back(content);
				}
			} else {
				if (!range.found) {
					// No matching `}` anywhere in the source. Where the body ends is a
					// guess from here on, so stop and let format() return the source.
					mBailOut = true;
					return;
				}
				size_t openLine = range.openLine;
				size_t closeLine = range.closeLine;

				std::string tail = getSourceLine(openLine).substr(range.openCol + 1);
				if (!trim(tail).empty()) {
					bodyLines.push_back(tail);
				}
				for (size_t i = openLine + 1; i < closeLine; i++) {
					bodyLines.push_back(getSourceLine(i));
				}
				std::string head = getSourceLine(closeLine).substr(0, range.closeCol);
				if (!trim(head).empty()) {
					bodyLines.push_back(head);
				}
			}

			int braceDepth = 0;
			bool inMultilineString = false;
			int blockCommentDepth = 0;
			std::string blockCommentBase;
			int blockCommentIndent = 0;

			for (const std::string& srcLine : bodyLines) {
				std::string trimmed = trim(srcLine);

				// Handle multiline string continuation
				if (inMultilineString) {
					int emittedAt = mIndent + braceDepth;
					// Find where the string closes and hand the rest of the line to the normal
					// scanner: it is code, and leaving it unread lost the brace, string or
					// comment it opened -- in `"/*` the block comment that followed.
					size_t resume = srcLine.length();
					for (size_t j = 0; j < srcLine.length(); j++) {
						if (quoteToggles(srcLine, j, inMultilineString)) {
							inMultilineString = false;
							resume = j + 1;
							break;
						}
					}
					if (inMultilineString) {
						mOutput << srcLine << "\n";
					} else {
						mOutput << srcLine.substr(0, resume) << rtrim(srcLine.substr(resume)) << "\n";
					}
					if (!inMultilineString && resume < srcLine.length()) {
						int tailBcDepth = 0;
						scanLineState(srcLine, resume, braceDepth, inMultilineString, tailBcDepth);
						if (tailBcDepth > 0) {
							blockCommentDepth = tailBcDepth;
							blockCommentBase = srcLine.substr(0, srcLine.find_first_not_of(" \t"));
							blockCommentIndent = emittedAt;
						}
					}
					continue;
				}

				// Handle block comment continuation
				if (blockCommentDepth > 0) {
					if (trimmed.empty()) {
						mOutput << '\n';
					} else {
						// At the level the comment's opening line was emitted at, not at the
						// depth after it: when that line also opened a brace -- `r{/*` -- the
						// two differ, and the continuation then sat one level deeper than the
						// base it is measured against, so it gained a tab on every pass.
						emitIndent(blockCommentIndent);
						if (!blockCommentBase.empty() &&
								srcLine.compare(0, blockCommentBase.size(), blockCommentBase) == 0) {
							mOutput << srcLine.substr(blockCommentBase.size()) << "\n";
						} else {
							mOutput << trimmed << "\n";
						}
					}
					size_t resume = advanceBlockCommentDepth(trimmed, blockCommentDepth);
					if (blockCommentDepth == 0 && resume < trimmed.length()) {
						// The comment closed part way along: the rest of the line is code.
						bool tailInStr = false;
						int tailBcDepth = 0;
						scanLineState(trimmed, resume, braceDepth, tailInStr, tailBcDepth);
						if (tailInStr) {
							inMultilineString = true;
						}
						if (tailBcDepth > 0) {
							blockCommentDepth = tailBcDepth;
							blockCommentBase = srcLine.substr(0, srcLine.find_first_not_of(" \t"));
						}
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

				{
					int probeDepth = 0;
					bool probeInStr = false;
					int probeBcDepth = 0;
					scanLineState(trimmed, 0, probeDepth, probeInStr, probeBcDepth);
					if (probeInStr) {
						trimmed = ltrim(srcLine);
					}
				}

				// Apply normalizations to the trimmed line
				std::string normalized = normalizeLine(trimmed);

				// Check for struct construction that should be expanded to multiline.
				// An empty expansion means the line is not one, so emit it as an ordinary
				// line. (The expansion does not change the net brace count, so the depth
				// tracking below reads the original line either way.)
				std::vector<std::string> expanded;
				if (isStructConstruction(normalized)) {
					expanded = expandStructConstruction(normalized, mIndent + lineIndent);
				}
				if (!expanded.empty()) {
					for (const auto& expLine : expanded) {
						mOutput << expLine << "\n";
					}
				} else {
					emitIndent(mIndent + lineIndent);
					mOutput << normalized << "\n";
				}

				// Update brace depth for next line
				bool lineInStr = false;
				int lineBcDepth = 0;
				scanLineState(trimmed, 0, braceDepth, lineInStr, lineBcDepth);

				if (lineInStr) {
					inMultilineString = true;
				}
				if (lineBcDepth > 0) {
					blockCommentDepth = lineBcDepth;
					blockCommentBase = srcLine.substr(0, srcLine.find_first_not_of(" \t"));
					blockCommentIndent = mIndent + lineIndent;
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
