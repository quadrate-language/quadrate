#ifndef QC_AST_NODE_LOCAL_H
#define QC_AST_NODE_LOCAL_H

#include <qc/ast_node.h>
#include <string>
#include <vector>

namespace Qd {

	/**
	 * @brief AST node representing local variable declaration(s)
	 *
	 * Syntax: -> variableName
	 *         -> var1 var2 var3  (multiple assignment)
	 *
	 * Single assignment pops the top value from the stack and stores it in a named local variable.
	 * Multiple assignment pops multiple values: -> a b c pops 3 values, assigns top to a, next to b, etc.
	 * References to the variable name later will push a copy of the value to the stack.
	 */
	class AstNodeLocal : public IAstNode {
	public:
		explicit AstNodeLocal(const std::string& name) : mNames{name}, mParent(nullptr), mLine(0), mColumn(0) {
		}

		explicit AstNodeLocal(const std::vector<std::string>& names)
			: mNames(names), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeLocal() override = default;

		Type type() const override {
			return Type::LOCAL;
		}

		// For backwards compatibility - returns first name
		const std::string& name() const {
			return mNames[0];
		}

		// Get all names for multiple assignment
		const std::vector<std::string>& names() const {
			return mNames;
		}

		// Check if this is a multiple assignment
		bool isMultiple() const {
			return mNames.size() > 1;
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
		std::vector<std::string> mNames;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

} // namespace Qd

#endif // QC_AST_NODE_LOCAL_H
