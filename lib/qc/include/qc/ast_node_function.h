#ifndef QD_QC_AST_NODE_FUNCTION_H
#define QD_QC_AST_NODE_FUNCTION_H

#include "ast_node.h"
#include <string>
#include <vector>

namespace Qd {
	class AstNodeFunctionDeclaration : public IAstNode {
	public:
		AstNodeFunctionDeclaration(const std::string& name) : mName(name), mParent(nullptr), mBody(nullptr) {
		}

		~AstNodeFunctionDeclaration() {
			if (mBody) {
				delete mBody;
			}
			for (auto* param : mParameters) {
				delete param;
			}
		}

		IAstNode::Type type() const override {
			return Type::FunctionDeclaration;
		}

		size_t childCount() const override {
			return mBody ? 1 : 0;
		}

		IAstNode* child(size_t index) const override {
			if (index == 0 && mBody) {
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

		const std::string& name() const {
			return mName;
		}

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

		void addParameter(IAstNode* param) {
			mParameters.push_back(param);
		}

		const std::vector<IAstNode*>& parameters() const {
			return mParameters;
		}

	private:
		std::string mName;
		IAstNode* mParent;
		IAstNode* mBody;
		std::vector<IAstNode*> mParameters;
	};
}

#endif
