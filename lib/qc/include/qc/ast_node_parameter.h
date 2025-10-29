#ifndef QD_QC_AST_NODE_PARAMETER_H
#define QD_QC_AST_NODE_PARAMETER_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeParameter : public IAstNode {
	public:
		AstNodeParameter(const std::string& name, const std::string& type, bool isOutput)
			: mName(name), mType(type), mIsOutput(isOutput), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::VARIABLE_DECLARATION;
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
		size_t mLine;
		size_t mColumn;
	};
}

#endif
