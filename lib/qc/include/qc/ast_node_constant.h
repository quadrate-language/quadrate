#ifndef QD_QC_AST_NODE_CONSTANT_H
#define QD_QC_AST_NODE_CONSTANT_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeConstant : public IAstNode {
	public:
		AstNodeConstant(const std::string& name, const char* value) : mName(name), mValue(value), mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::ConstantDeclaration;
		}

		size_t childCount() const override {
			return 1;
		}

		IAstNode* child(size_t /*index*/) const override {
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

		const char* value() const {
			return mValue.c_str();
		}

	private:
		std::string mName;
		std::string mValue;
		IAstNode* mParent;
	};
}

#endif
