#ifndef QD_QC_AST_NODE_CASE_H
#define QD_QC_AST_NODE_CASE_H

#include "ast_node.h"

namespace Qd {
	class AstNodeCase : public IAstNode {
	public:
		AstNodeCase(IAstNode* value, bool isDefault = false)
			: mValue(value), mIsDefault(isDefault), mParent(nullptr), mBody(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeCase() {
			if (mValue) {
				delete mValue;
			}
			if (mBody) {
				delete mBody;
			}
		}

		IAstNode::Type type() const override {
			return Type::CASE_STATEMENT;
		}

		size_t childCount() const override {
			size_t count = 0;
			if (mValue) {
				count++;
			}
			if (mBody) {
				count++;
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t currentIndex = 0;
			if (mValue && index == currentIndex++) {
				return mValue;
			}
			if (mBody && index == currentIndex++) {
				return mBody;
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

		bool isDefault() const {
			return mIsDefault;
		}

		IAstNode* value() const {
			return mValue;
		}

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

	private:
		IAstNode* mValue;
		bool mIsDefault;
		IAstNode* mParent;
		IAstNode* mBody;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
