#ifndef QD_QC_AST_NODE_COMMENT_H
#define QD_QC_AST_NODE_COMMENT_H

#include <qc/ast_node.h>
#include <string>

namespace Qd {
	class AstNodeComment : public IAstNode {
	public:
		enum class CommentType {
			LINE, // Single-line comment //
			BLOCK // Multi-line comment /* */
		};

		AstNodeComment(const std::string& text, CommentType commentType)
			: mText(text), mCommentType(commentType), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::COMMENT;
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

		const std::string& text() const {
			return mText;
		}

		CommentType commentType() const {
			return mCommentType;
		}

	private:
		std::string mText;
		CommentType mCommentType;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
