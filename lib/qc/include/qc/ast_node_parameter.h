#ifndef QD_QC_AST_NODE_PARAMETER_H
#define QD_QC_AST_NODE_PARAMETER_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeParameter : public IAstNode {
	public:
		AstNodeParameter(const std::string& name, const std::string& type, bool isOutput)
			: mName(name), mType(type), mIsOutput(isOutput), mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::VariableDeclaration;
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

		const std::string& name() const {
			return mName;
		}

		const std::string& typeString() const {
			return mType;
		}

		bool isOutput() const {
			return mIsOutput;
		}

	private:
		std::string mName;
		std::string mType;
		bool mIsOutput;
		IAstNode* mParent;
	};
}

#endif
