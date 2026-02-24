#ifndef QD_QC_AST_NODE_CONSTANT_H
#define QD_QC_AST_NODE_CONSTANT_H

#include "ast_node_base.h"
#include <string>

namespace Qd {
	class AstNodeConstant : public AstNodeBase<IAstNode::Type::CONSTANT_DECLARATION> {
	public:
		AstNodeConstant(const std::string& name, const char* value, bool isPublic = false)
			: mName(name), mValue(value), mIsPublic(isPublic) {
		}

		size_t childCount() const override {
			return 0;
		}

		IAstNode* child(size_t) const override {
			return nullptr;
		}

		const std::string& name() const {
			return mName;
		}

		const char* value() const {
			return mValue.c_str();
		}

		bool isPublic() const {
			return mIsPublic;
		}

	private:
		std::string mName;
		std::string mValue;
		bool mIsPublic;
	};
}

#endif
