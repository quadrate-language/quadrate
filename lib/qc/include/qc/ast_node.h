#ifndef QD_QC_AST_NODE_H
#define QD_QC_AST_NODE_H

#include <cstddef>

namespace Qd {
	class IAstNode {
	public:
		enum class Type {
			UNKNOWN,
			PROGRAM,
			BLOCK,
			FUNCTION_DECLARATION,
			VARIABLE_DECLARATION,
			EXPRESSION_STATEMENT,
			IF_STATEMENT,
			FOR_STATEMENT,
			LOOP_STATEMENT,
			SWITCH_STATEMENT,
			CASE_STATEMENT,
			RETURN_STATEMENT,
			BREAK_STATEMENT,
			CONTINUE_STATEMENT,
			DEFER_STATEMENT,
			CTX_STATEMENT,
			BINARY_EXPRESSION,
			UNARY_EXPRESSION,
			LITERAL,
			IDENTIFIER,
			INSTRUCTION,
			SCOPED_IDENTIFIER,
			USE_STATEMENT,
			IMPORT_STATEMENT,
			CONSTANT_DECLARATION,
			LABEL,
			FUNCTION_POINTER_REFERENCE,
			COMMENT,
			LOCAL
		};
		virtual ~IAstNode() = default;

		virtual Type type() const = 0;

		virtual size_t childCount() const = 0;
		virtual IAstNode* child(size_t index) const = 0;
		virtual IAstNode* parent() const = 0;
		virtual void setParent(IAstNode* parent) = 0;

		virtual size_t line() const = 0;
		virtual size_t column() const = 0;
		virtual void setPosition(size_t line, size_t column) = 0;
	};
}

#endif
