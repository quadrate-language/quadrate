#ifndef QD_QC_AST_NODE_USE_H
#define QD_QC_AST_NODE_USE_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeUse : public IAstNode {
	public:
		AstNodeUse(const std::string& module) : mModule(module), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::USE_STATEMENT;
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

		const std::string& module() const {
			return mModule;
		}

	private:
		std::string mModule;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
