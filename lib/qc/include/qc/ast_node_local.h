#ifndef QD_QC_AST_NODE_LOCAL_H
#define QD_QC_AST_NODE_LOCAL_H

#include "ast_node_base.h"
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
	class AstNodeLocal : public AstNodeBase<IAstNode::Type::LOCAL> {
	public:
		explicit AstNodeLocal(const std::string& name) : mNames{name} {
		}

		explicit AstNodeLocal(const std::vector<std::string>& names) : mNames(names) {
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

		IAstNode* child(size_t) const override {
			return nullptr;
		}

	private:
		std::vector<std::string> mNames;
	};

} // namespace Qd

#endif // QD_QC_AST_NODE_LOCAL_H
