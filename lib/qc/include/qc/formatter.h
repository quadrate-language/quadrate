#ifndef QD_QC_FORMATTER_H
#define QD_QC_FORMATTER_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class Formatter {
	public:
		Formatter();

		// Format an AST node and return formatted source code
		std::string format(const IAstNode* node);

		// Configuration
		void setIndentWidth(int width);
		void setMaxLineLength(int length);

	private:
		int mIndentWidth;
		int mMaxLineLength;
		int mCurrentIndent;
		std::string mOutput;

		void formatNode(const IAstNode* node);
		void formatProgram(const IAstNode* node);
		void formatFunction(const IAstNode* node);
		void formatBlock(const IAstNode* node);
		void formatBlockInline(const IAstNode* node);
		void formatIf(const IAstNode* node);
		void formatFor(const IAstNode* node);
		void formatSwitch(const IAstNode* node);
		void formatCase(const IAstNode* node);
		void formatDefer(const IAstNode* node);
		void formatUse(const IAstNode* node);
		void formatConstant(const IAstNode* node);
		void formatInstruction(const IAstNode* node);
		void formatLiteral(const IAstNode* node);
		void formatIdentifier(const IAstNode* node);
		void formatScopedIdentifier(const IAstNode* node);
		void formatBreak(const IAstNode* node);
		void formatContinue(const IAstNode* node);
		void formatReturn(const IAstNode* node);
		void formatComment(const IAstNode* node);
		bool isInlineNode(const IAstNode* node);

		void indent();
		void dedent();
		void writeIndent();
		void write(const std::string& text);
		void writeLine(const std::string& text = "");
		void newLine();
	};
}

#endif
