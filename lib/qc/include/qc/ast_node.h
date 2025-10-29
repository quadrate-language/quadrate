#ifndef QD_QC_AST_NODE_H
#define QD_QC_AST_NODE_H

#include <cstddef>

namespace Qd {
	class IAstNode {
	public:
		enum class Type {
			Unknown,
			Program,
			Block,
			FunctionDeclaration,
			VariableDeclaration,
			ExpressionStatement,
			IfStatement,
			ForStatement,
			SwitchStatement,
			CaseStatement,
			ReturnStatement,
			BreakStatement,
			ContinueStatement,
			DeferStatement,
			BinaryExpression,
			UnaryExpression,
			Literal,
			Identifier,
			Instruction,
			ScopedIdentifier,
			UseStatement,
			ConstantDeclaration,
			Label
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
