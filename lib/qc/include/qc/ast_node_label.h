#ifndef QD_QC_AST_NODE_LABEL_H
#define QD_QC_AST_NODE_LABEL_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeLabel : public IAstNode {
	public:
		AstNodeLabel(const std::string& name) : mName(name), mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::Label;
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

	private:
		std::string mName;
		IAstNode* mParent;
	};
}

#endif
