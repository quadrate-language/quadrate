#ifndef QD_QC_AST_NODE_ARRAY_LITERAL_H
#define QD_QC_AST_NODE_ARRAY_LITERAL_H

#include "ast_node.h"
#include <memory>
#include <string>
#include <vector>

namespace Qd {

	/**
	 * @brief AST node representing an array literal
	 *
	 * Two forms share this node, told apart by whether an element type was written hard
	 * against the closing bracket:
	 *
	 *   [1 2 3]       elements      -- elements() are the elements, elementType() is empty
	 *   [10]i64       size          -- elements() are the size expression, elementType() is "i64"
	 *
	 * `[]i64` is the size form with the size left out, which means zero of them, and `[]` is
	 * the element form with no elements, whose element type the runtime adopts on first append.
	 * The size expression reuses the element vector so that every pass that walks children --
	 * the printer, the validators, codegen -- reaches it without knowing about the second form.
	 */
	class AstNodeArrayLiteral : public IAstNode {
	public:
		AstNodeArrayLiteral() : mParent(nullptr), mLine(0), mColumn(0), mHasElementType(false) {
		}

		~AstNodeArrayLiteral() = default;

		IAstNode::Type type() const override {
			return Type::ARRAY_LITERAL;
		}

		size_t childCount() const override {
			return mElements.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mElements.size()) {
				return mElements[index].get();
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
			mElements.emplace_back(element);
		}

		const std::vector<std::unique_ptr<IAstNode>>& elements() const {
			return mElements;
		}

		// Set when a type name followed the ']' directly, making this the `[size]T` form. The
		// nodes in elements() are then the size expression rather than the elements.
		void setElementType(const std::string& type) {
			mElementType = type;
			mHasElementType = true;
		}

		bool hasElementType() const {
			return mHasElementType;
		}

		const std::string& elementType() const {
			return mElementType;
		}

		// The array type this literal produces when it declares one: `[10]i64` is an `[]i64`.
		// Empty for the element form, whose type has to be inferred from the elements instead.
		std::string declaredArrayType() const {
			return mHasElementType ? "[]" + mElementType : std::string();
		}

	private:
		std::vector<std::unique_ptr<IAstNode>> mElements;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
		std::string mElementType;
		bool mHasElementType;
	};

} // namespace Qd

#endif
