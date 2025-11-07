#ifndef QD_QC_AST_NODE_FUNCTION_H
#define QD_QC_AST_NODE_FUNCTION_H

#include "ast_node.h"
#include <string>
#include <vector>

namespace Qd {
	class AstNodeFunctionDeclaration : public IAstNode {
	public:
		AstNodeFunctionDeclaration(const std::string& name)
			: mName(name), mParent(nullptr), mBody(nullptr), mThrows(false), mLine(0), mColumn(0) {
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
			return Type::FUNCTION_DECLARATION;
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

		void setThrows(bool throws) {
			mThrows = throws;
		}

		bool throws() const {
			return mThrows;
		}

	private:
		std::string mName;
		IAstNode* mParent;
		IAstNode* mBody;
		std::vector<IAstNode*> mInputParameters;
		std::vector<IAstNode*> mOutputParameters;
		bool mThrows;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
