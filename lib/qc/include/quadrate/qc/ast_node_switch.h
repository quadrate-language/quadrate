#ifndef QD_QC_AST_NODE_SWITCH_H
#define QD_QC_AST_NODE_SWITCH_H

#include "ast_node.h"
#include "ast_node_case.h"
#include <vector>

namespace Qd {
	class AstNodeSwitchStatement : public IAstNode {
	public:
		AstNodeSwitchStatement() : mParent(nullptr), mLine(0), mColumn(0) {
		}

		~AstNodeSwitchStatement() {
			for (auto* caseNode : mCases) {
				delete caseNode;
			}
		}

		IAstNode::Type type() const override {
			return Type::SWITCH_STATEMENT;
		}

		size_t childCount() const override {
			return mCases.size();
		}

		IAstNode* child(size_t index) const override {
			if (index < mCases.size()) {
				return mCases[index];
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

		void addCase(AstNodeCase* caseNode) {
			mCases.push_back(caseNode);
		}

		const std::vector<AstNodeCase*>& cases() const {
			return mCases;
		}

	private:
		IAstNode* mParent;
		std::vector<AstNodeCase*> mCases;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
