#ifndef QD_QC_AST_NODE_STRING_INTERPOLATION_H
#define QD_QC_AST_NODE_STRING_INTERPOLATION_H

#include "ast_node.h"
#include <string>

namespace Qd {

	/**
	 * @brief AST node representing a string interpolation expression
	 *
	 * Preserves the original $"..." template for formatter round-tripping.
	 * Expanded to sb::new/append/finish nodes after parsing but before
	 * semantic validation, so downstream passes see the expanded form.
	 *
	 * Example: $"hello {name}, you are {age} years old"
	 * Template stored: "hello {name}, you are {age} years old"
	 */
	class AstNodeStringInterpolation : public IAstNode {
	public:
		AstNodeStringInterpolation(const std::string& templateStr)
			: mTemplate(templateStr), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeStringInterpolation() = default;

		IAstNode::Type type() const override {
			return Type::STRING_INTERPOLATION;
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
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

		const std::string& templateString() const {
			return mTemplate;
		}

	private:
		std::string mTemplate;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

} // namespace Qd

#endif
