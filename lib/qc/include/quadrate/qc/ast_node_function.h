#ifndef QD_QC_AST_NODE_FUNCTION_H
#define QD_QC_AST_NODE_FUNCTION_H

#include "ast_node.h"
#include <memory>
#include <string>
#include <vector>

namespace Qd {
	class AstNodeFunctionDeclaration : public IAstNode {
	public:
		AstNodeFunctionDeclaration(const std::string& name, bool isPublic = false)
			: mName(name), mParent(nullptr), mThrows(false), mIsPublic(isPublic), mLine(0), mColumn(0) {
		}

		AstNodeFunctionDeclaration(
				const std::string& name, const std::vector<std::string>& typeParams, bool isPublic = false)
			: mName(name), mTypeParams(typeParams), mParent(nullptr), mThrows(false), mIsPublic(isPublic), mLine(0),
			  mColumn(0) {
		}

		~AstNodeFunctionDeclaration() = default;

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
			for (const auto& param : mInputParameters) {
				if (index == currentIndex++) {
					return param.get();
				}
			}
			for (const auto& param : mOutputParameters) {
				if (index == currentIndex++) {
					return param.get();
				}
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

		const std::string& name() const {
			return mName;
		}

		void setBody(IAstNode* body) {
			mBody.reset(body);
		}

		IAstNode* body() const {
			return mBody.get();
		}

		void addInputParameter(IAstNode* param) {
			mInputParameters.emplace_back(param);
		}

		void addOutputParameter(IAstNode* param) {
			mOutputParameters.emplace_back(param);
		}

		const std::vector<std::unique_ptr<IAstNode>>& inputParameters() const {
			return mInputParameters;
		}

		const std::vector<std::unique_ptr<IAstNode>>& outputParameters() const {
			return mOutputParameters;
		}

		void setThrows(bool throws) {
			mThrows = throws;
		}

		bool throws() const {
			return mThrows;
		}

		void setPublic(bool isPublic) {
			mIsPublic = isPublic;
		}

		bool isPublic() const {
			return mIsPublic;
		}

		const std::vector<std::string>& typeParams() const {
			return mTypeParams;
		}

		bool isGeneric() const {
			return !mTypeParams.empty();
		}

		void addTypeParam(const std::string& typeParam) {
			mTypeParams.push_back(typeParam);
		}

		// Receiver support for struct methods
		bool hasReceiver() const {
			return mHasReceiver;
		}

		const std::string& receiverName() const {
			return mReceiverName;
		}

		const std::string& receiverType() const {
			return mReceiverType;
		}

		const std::vector<std::string>& receiverTypeParams() const {
			return mReceiverTypeParams;
		}

		bool hasReceiverTypeParams() const {
			return !mReceiverTypeParams.empty();
		}

		void setReceiver(const std::string& name, const std::string& type) {
			mReceiverName = name;
			mReceiverType = type;
			mHasReceiver = true;
		}

		void setReceiver(const std::string& name, const std::string& type, const std::vector<std::string>& typeParams) {
			mReceiverName = name;
			mReceiverType = type;
			mReceiverTypeParams = typeParams;
			mHasReceiver = true;
		}

	private:
		std::string mName;
		std::vector<std::string> mTypeParams;
		IAstNode* mParent;
		std::unique_ptr<IAstNode> mBody;
		std::vector<std::unique_ptr<IAstNode>> mInputParameters;
		std::vector<std::unique_ptr<IAstNode>> mOutputParameters;
		bool mThrows;
		bool mIsPublic;
		size_t mLine;
		size_t mColumn;

		// Receiver information for struct methods
		std::string mReceiverName;
		std::string mReceiverType;
		std::vector<std::string> mReceiverTypeParams;
		bool mHasReceiver = false;
	};
}

#endif
