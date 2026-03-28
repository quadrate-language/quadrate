#ifndef QD_QC_AST_NODE_DEFER_H
#define QD_QC_AST_NODE_DEFER_H

#include "ast_node_base.h"
#include <memory>
#include <vector>

namespace Qd {
	class AstNodeDefer : public AstNodeBase<IAstNode::Type::DEFER_STATEMENT> {
	public:
		~AstNodeDefer() override = default;

		void addChild(IAstNode* child) {
			mChildren.emplace_back(child);
		}

		size_t childCount() const override {
			return mChildren.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mChildren.size()) {
				return mChildren[index].get();
			}
			return nullptr;
		}

	private:
		std::vector<std::unique_ptr<IAstNode>> mChildren;
	};
}

#endif
