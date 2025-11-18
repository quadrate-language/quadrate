#ifndef QC_AST_NODE_LOCAL_H
#define QC_AST_NODE_LOCAL_H

#include <qc/ast_node.h>
#include <string>

namespace Qd {

	/**
	 * @brief AST node representing a local variable declaration
	 *
	 * Syntax: -> variableName
	 *
	 * This pops the top value from the stack and stores it in a named local variable.
	 * References to the variable name later will push a copy of the value to the stack.
	 */
	class AstNodeLocal : public IAstNode {
	public:
		explicit AstNodeLocal(const std::string& name) : mName(name), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeLocal() override = default;

		Type type() const override {
			return Type::LOCAL;
		}

		const std::string& name() const {
			return mName;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t /*index*/) const override {
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

		size_t line() const override {
			return mLine;
		}

		size_t column() const override {
			return mColumn;
		}

		void setPosition(size_t line, size_t column) override {
			mLine = line;
			mColumn = column;
		}

	private:
		std::string mName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

} // namespace Qd

#endif // QC_AST_NODE_LOCAL_H
