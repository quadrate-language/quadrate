#ifndef QD_QC_AST_NODE_USE_H
#define QD_QC_AST_NODE_USE_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeUse : public IAstNode {
	public:
		AstNodeUse(const std::string& module) : mModule(module), mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::UseStatement;
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

		const std::string& module() const {
			return mModule;
		}

	private:
		std::string mModule;
		IAstNode* mParent;
	};
}

#endif
