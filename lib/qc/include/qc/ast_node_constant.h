#ifndef QD_QC_AST_NODE_CONSTANT_H
#define QD_QC_AST_NODE_CONSTANT_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeConstant : public IAstNode {
	public:
		AstNodeConstant(const std::string& name, IAstNode* value) : mName(name), mValue(value), mParent(nullptr) {
		}

		~AstNodeConstant() {
			if (mValue) {
				delete mValue;
			}
		}

		IAstNode::Type type() const override {
			return Type::ConstantDeclaration;
		}

		size_t childCount() const override {
			return mValue ? 1 : 0;
		}

		IAstNode* child(size_t index) const override {
			if (index == 0 && mValue) {
				return mValue;
			}
			return nullptr;
		}

		IAstNode* parent() const override {
			return mParent;
		}

		void setParent(IAstNode* parent) override {
			mParent = parent;
		}

		const std::string& name() const {
			return mName;
		}

		IAstNode* value() const {
			return mValue;
		}

	private:
		std::string mName;
		IAstNode* mValue;
		IAstNode* mParent;
	};
}

#endif
