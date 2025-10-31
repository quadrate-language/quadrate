#ifndef QD_QC_AST_NODE_FUNCTION_POINTER_H
#define QD_QC_AST_NODE_FUNCTION_POINTER_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeFunctionPointerReference : public IAstNode {
	public:
		AstNodeFunctionPointerReference(const std::string& functionName)
			: mFunctionName(functionName), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::FUNCTION_POINTER_REFERENCE;
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

		const std::string& functionName() const {
			return mFunctionName;
		}

	private:
		std::string mFunctionName;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
