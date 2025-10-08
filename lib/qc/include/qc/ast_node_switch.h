#ifndef QD_QC_AST_NODE_SWITCH_H
#define QD_QC_AST_NODE_SWITCH_H

#include "ast_node.h"
#include <vector>

namespace Qd {
	class AstNodeCase : public IAstNode {
	public:
		AstNodeCase(IAstNode* value, bool isDefault = false)
			: mValue(value), mIsDefault(isDefault), mParent(nullptr), mBody(nullptr) {
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
			return Type::CaseStatement;
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
	};

	class AstNodeSwitchStatement : public IAstNode {
	public:
		AstNodeSwitchStatement() : mParent(nullptr) {
		}

		~AstNodeSwitchStatement() {
			for (auto* caseNode : mCases) {
				delete caseNode;
			}
		}

		IAstNode::Type type() const override {
			return Type::SwitchStatement;
		}

		size_t childCount() const override {
			return mCases.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mCases.size()) {
				return mCases[index];
			}
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

		void addCase(AstNodeCase* caseNode) {
			mCases.push_back(caseNode);
		}

		const std::vector<AstNodeCase*>& cases() const {
			return mCases;
		}

	private:
		IAstNode* mParent;
		std::vector<AstNodeCase*> mCases;
	};
}

#endif
