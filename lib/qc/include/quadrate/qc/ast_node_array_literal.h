#ifndef QD_QC_AST_NODE_ARRAY_LITERAL_H
#define QD_QC_AST_NODE_ARRAY_LITERAL_H

#include "ast_node.h"
#include <vector>

namespace Qd {

	/**
	 * @brief AST node representing an array literal
	 *
	 * Example: [1, 2, 3] or ["a", "b", "c"]
	 * Creates an array and pushes it onto the stack
	 */
	class AstNodeArrayLiteral : public IAstNode {
	public:
		AstNodeArrayLiteral() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeArrayLiteral() {
			for (auto* element : mElements) {
				delete element;
			}
		}

		IAstNode::Type type() const override {
			return Type::ARRAY_LITERAL;
		}

		size_t childCount() const override {
			return mElements.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mElements.size()) {
				return mElements[index];
			}
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

		void addElement(IAstNode* element) {
			element->setParent(this);
			mElements.push_back(element);
		}

		const std::vector<IAstNode*>& elements() const {
			return mElements;
		}

	private:
		std::vector<IAstNode*> mElements;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};

} // namespace Qd

#endif
