#ifndef QD_QC_AST_NODE_CASE_H
#define QD_QC_AST_NODE_CASE_H

#include "ast_node.h"
#include <memory>

namespace Qd {
	class AstNodeCase : public IAstNode {
	public:
		AstNodeCase(IAstNode* value, bool isDefault = false)
			: mValue(value), mIsDefault(isDefault), mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeCase() = default;

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
				return mValue.get();
			}
			if (mBody && index == currentIndex++) {
				return mBody.get();
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
			return mValue.get();
		}

		void setBody(IAstNode* body) {
			mBody.reset(body);
		}

		IAstNode* body() const {
			return mBody.get();
		}

	private:
		std::unique_ptr<IAstNode> mValue;
		bool mIsDefault;
		IAstNode* mParent;
		std::unique_ptr<IAstNode> mBody;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
