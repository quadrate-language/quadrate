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
			for (auto* param : mInputParameters) {
				delete param;
			}
			for (auto* param : mOutputParameters) {
				delete param;
			}
		}

		IAstNode::Type type() const override {
			return Type::FunctionDeclaration;
		}

		size_t childCount() const override {
			size_t count = 0;
			count += mInputParameters.size();
			count += mOutputParameters.size();
			if (mBody) {
				count++;
			}
			return count;
		}

		IAstNode* child(size_t index) const override {
			size_t currentIndex = 0;
			for (auto* param : mInputParameters) {
				if (index == currentIndex++) {
					return param;
				}
			}
			for (auto* param : mOutputParameters) {
				if (index == currentIndex++) {
					return param;
				}
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

		const std::string& name() const {
			return mName;
		}

		void setBody(IAstNode* body) {
			mBody = body;
		}

		IAstNode* body() const {
			return mBody;
		}

		void addInputParameter(IAstNode* param) {
			mInputParameters.push_back(param);
		}

		void addOutputParameter(IAstNode* param) {
			mOutputParameters.push_back(param);
		}

		const std::vector<IAstNode*>& inputParameters() const {
			return mInputParameters;
		}

		const std::vector<IAstNode*>& outputParameters() const {
			return mOutputParameters;
		}

	private:
		std::string mName;
		IAstNode* mParent;
		IAstNode* mBody;
		std::vector<IAstNode*> mInputParameters;
		std::vector<IAstNode*> mOutputParameters;
	};
}

#endif
