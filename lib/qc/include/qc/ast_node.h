#ifndef QD_QC_AST_NODE_H
#define QD_QC_AST_NODE_H

#include <cstddef>
#include <iterator>

namespace Qd {
	class IAstNode;

	// Iterator for traversing AST node children
	class AstChildIterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = IAstNode*;
		using difference_type = std::ptrdiff_t;
		using pointer = IAstNode**;
		using reference = IAstNode*;

		AstChildIterator(const IAstNode* node, size_t index) : mNode(node), mIndex(index) {}

		inline IAstNode* operator*() const;
		AstChildIterator& operator++() {
			++mIndex;
			return *this;
		}
		AstChildIterator operator++(int) {
			auto tmp = *this;
			++mIndex;
			return tmp;
		}
		bool operator==(const AstChildIterator& other) const { return mIndex == other.mIndex; }
		bool operator!=(const AstChildIterator& other) const { return mIndex != other.mIndex; }

	private:
		const IAstNode* mNode;
		size_t mIndex;
	};

	// Range for iterating over AST node children with range-based for
	class AstChildRange {
	public:
		explicit AstChildRange(const IAstNode* node) : mNode(node) {}

		inline AstChildIterator begin() const;
		inline AstChildIterator end() const;
		inline size_t size() const;
		inline bool empty() const;

	private:
		const IAstNode* mNode;
	};

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
			WHILE_STATEMENT,
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
			LOCAL,
			STRUCT_DECLARATION,
			STRUCT_FIELD,
			STRUCT_CONSTRUCTION,
			FIELD_ACCESS,
			FIELD_SET,
			ARRAY_LITERAL,
			ARRAY_INDEX,
			TEST_DECLARATION,
			ANONYMOUS_FUNCTION
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

		// Returns a range for iterating over children with range-based for
		AstChildRange children() const { return AstChildRange(this); }
	};

	// Inline implementations (must be after IAstNode is fully defined)
	inline IAstNode* AstChildIterator::operator*() const { return mNode->child(mIndex); }
	inline AstChildIterator AstChildRange::begin() const { return AstChildIterator(mNode, 0); }
	inline AstChildIterator AstChildRange::end() const { return AstChildIterator(mNode, mNode->childCount()); }
	inline size_t AstChildRange::size() const { return mNode->childCount(); }
	inline bool AstChildRange::empty() const { return mNode->childCount() == 0; }
}

#endif
