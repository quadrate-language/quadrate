#ifndef QD_QC_AST_NODE_TYPE_ALIAS_H
#define QD_QC_AST_NODE_TYPE_ALIAS_H

#include "ast_node.h"
#include <string>

namespace Qd {
	class AstNodeTypeAlias : public IAstNode {
	public:
		AstNodeTypeAlias(const std::string& name, const std::string& targetType, bool isPublic = false)
			: mName(name), mTargetType(targetType), mIsPublic(isPublic), mParent(nullptr), mLine(0), mColumn(0) {
		}

		IAstNode::Type type() const override {
			return Type::TYPE_ALIAS_DECLARATION;
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

		const std::string& targetType() const {
			return mTargetType;
		}

		bool isPublic() const {
			return mIsPublic;
		}

	private:
		std::string mName;
		std::string mTargetType;
		bool mIsPublic;
		IAstNode* mParent;
		size_t mLine;
		size_t mColumn;
	};
}

#endif
