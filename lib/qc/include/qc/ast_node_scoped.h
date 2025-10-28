#ifndef QD_QC_AST_NODE_SCOPED_H
#define QD_QC_AST_NODE_SCOPED_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeScopedIdentifier : public IAstNode {
	public:
		AstNodeScopedIdentifier(const std::string& scope, const std::string& name)
			: mScope(scope), mName(name), mParent(nullptr) {
		}

		IAstNode::Type type() const override {
			return Type::ScopedIdentifier;
		}

		const std::string& scope() const {
			return mScope;
		}

		const std::string& name() const {
			return mName;
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

	private:
		std::string mScope;
		std::string mName;
		IAstNode* mParent;
	};
}

#endif
