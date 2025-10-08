#ifndef QD_QC_AST_NODE_H
#define QD_QC_AST_NODE_H

#include <cstddef>

namespace Qd {
	class IAstNode {
	public:
		enum class Type {
			Unknown,
			Program,
			FunctionDeclaration,
			VariableDeclaration,
			ExpressionStatement,
			IfStatement,
			WhileStatement,
			ReturnStatement,
			BinaryExpression,
			UnaryExpression,
			Literal,
			Identifier
		};
		virtual ~IAstNode() = default;

		virtual Type type() const = 0;

		virtual size_t childCount() const = 0;
		virtual IAstNode* child(size_t index) const = 0;
		virtual IAstNode* parent() const = 0;
		virtual void setParent(IAstNode* parent) = 0;
	};
}

#endif
